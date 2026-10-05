// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArenaDuelWeaponComponent.h"
#include "../Characters/ArenaDuelCharacter.h"
#include "ArenaDuelWeaponTarget.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	static TArray<FArenaDuelWeaponDefinition> MakeDefinitions()
	{
		FArenaDuelWeaponDefinition Arc;
		Arc.Id = EArenaDuelWeaponId::ArcRifle;
		Arc.DisplayName = TEXT("Arc Rifle");
		FArenaDuelWeaponDefinition SMG = Arc;
		SMG.Id = EArenaDuelWeaponId::ShadeSMG; SMG.DisplayName = TEXT("Shade SMG"); SMG.MagazineCapacity = 32; SMG.ReserveCapacity = 128; SMG.RoundsPerMinute = 900.0f; SMG.BaseSpreadDegrees = 0.65f; SMG.MovementSpreadDegrees = 1.8f;
		FArenaDuelWeaponDefinition DMR = Arc;
		DMR.Id = EArenaDuelWeaponId::RuneDMR; DMR.DisplayName = TEXT("Rune DMR"); DMR.MagazineCapacity = 12; DMR.ReserveCapacity = 48; DMR.RoundsPerMinute = 280.0f; DMR.BaseSpreadDegrees = 0.08f; DMR.MovementSpreadDegrees = 0.55f; DMR.bAutomatic = false;
		FArenaDuelWeaponDefinition Shotgun = Arc;
		Shotgun.Id = EArenaDuelWeaponId::HexShotgun; Shotgun.DisplayName = TEXT("Hex Shotgun"); Shotgun.MagazineCapacity = 6; Shotgun.ReserveCapacity = 30; Shotgun.RoundsPerMinute = 75.0f; Shotgun.BaseSpreadDegrees = 5.0f; Shotgun.MovementSpreadDegrees = 2.0f; Shotgun.Pellets = 8; Shotgun.bAutomatic = false;
		return { Arc, SMG, DMR, Shotgun };
	}

	static EArenaDuelShotResult ClassifyHit(const FHitResult& Hit)
	{
		if (!Hit.GetActor()) return EArenaDuelShotResult::World;
		if (Hit.BoneName == TEXT("head") || (Hit.Component.IsValid() && Hit.Component->ComponentHasTag(TEXT("HeadHitZone")))) return EArenaDuelShotResult::Head;
		if (Hit.GetActor()->IsA<AArenaDuelWeaponTarget>() || Hit.GetActor()->IsA<APawn>()) return EArenaDuelShotResult::Body;
		return EArenaDuelShotResult::World;
	}
}

UArenaDuelWeaponComponent::UArenaDuelWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	WeaponDefinitions = MakeDefinitions();
	FirstPersonWeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FirstPersonWeaponMesh"));
	FirstPersonWeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FirstPersonWeaponMesh->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded()) FirstPersonWeaponMesh->SetStaticMesh(CubeMesh.Object);
}

void UArenaDuelWeaponComponent::BeginPlay()
{
	Super::BeginPlay();
	if (AArenaDuelCharacter* Character = Cast<AArenaDuelCharacter>(GetOwner()))
	{
		FirstPersonWeaponMesh->AttachToComponent(Character->GetFirstPersonCamera(), FAttachmentTransformRules::KeepRelativeTransform);
		FirstPersonWeaponMesh->SetVisibility(Character->IsLocallyControlled());
	}
	if (GetOwnerRole() == ROLE_Authority) InitializeRuntimeAmmo();
	RefreshWeaponVisual();
}

void UArenaDuelWeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UArenaDuelWeaponComponent, EquippedWeaponIndex);
	DOREPLIFETIME(UArenaDuelWeaponComponent, RuntimeAmmo);
	DOREPLIFETIME(UArenaDuelWeaponComponent, bReloading);
}

const FArenaDuelWeaponDefinition& UArenaDuelWeaponComponent::GetCurrentDefinition() const
{
	static const FArenaDuelWeaponDefinition Fallback;
	return WeaponDefinitions.IsValidIndex(EquippedWeaponIndex) ? WeaponDefinitions[EquippedWeaponIndex] : Fallback;
}
const FArenaDuelWeaponRuntimeState* UArenaDuelWeaponComponent::GetCurrentRuntimeState() const { return RuntimeAmmo.IsValidIndex(EquippedWeaponIndex) ? &RuntimeAmmo[EquippedWeaponIndex] : nullptr; }
FArenaDuelWeaponRuntimeState* UArenaDuelWeaponComponent::GetMutableCurrentRuntimeState() { return RuntimeAmmo.IsValidIndex(EquippedWeaponIndex) ? &RuntimeAmmo[EquippedWeaponIndex] : nullptr; }
void UArenaDuelWeaponComponent::InitializeRuntimeAmmo()
{
	RuntimeAmmo.SetNum(WeaponDefinitions.Num());
	for (int32 Index = 0; Index < WeaponDefinitions.Num(); ++Index) { RuntimeAmmo[Index].MagazineAmmo = WeaponDefinitions[Index].MagazineCapacity; RuntimeAmmo[Index].ReserveAmmo = WeaponDefinitions[Index].ReserveCapacity; }
}
EArenaDuelWeaponId UArenaDuelWeaponComponent::GetCurrentWeaponId() const { return GetCurrentDefinition().Id; }
FName UArenaDuelWeaponComponent::GetCurrentWeaponName() const { return GetCurrentDefinition().DisplayName; }
int32 UArenaDuelWeaponComponent::GetCurrentMagazineAmmo() const { const FArenaDuelWeaponRuntimeState* State = GetCurrentRuntimeState(); return State ? State->MagazineAmmo : GetCurrentDefinition().MagazineCapacity; }
int32 UArenaDuelWeaponComponent::GetReserveAmmo() const { const FArenaDuelWeaponRuntimeState* State = GetCurrentRuntimeState(); return State ? State->ReserveAmmo : GetCurrentDefinition().ReserveCapacity; }
float UArenaDuelWeaponComponent::GetLastShotAge() const { return (GetWorld() && LastShotWorldTime >= 0.0f) ? GetWorld()->GetTimeSeconds() - LastShotWorldTime : BIG_NUMBER; }
float UArenaDuelWeaponComponent::GetCurrentSpreadDegrees() const
{
	const AArenaDuelCharacter* Character = Cast<AArenaDuelCharacter>(GetOwner());
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	const float SpeedFactor = FMath::Clamp(Movement ? Movement->Velocity.Size2D() / 600.0f : 0.0f, 0.0f, 1.0f);
	const float AirFactor = Movement && Movement->IsFalling() ? 0.5f : 0.0f;
	return GetCurrentDefinition().BaseSpreadDegrees + GetCurrentDefinition().MovementSpreadDegrees * (SpeedFactor + AirFactor);
}

void UArenaDuelWeaponComponent::StartFire() { bFireHeld = true; ApplyLocalRecoil(); if (GetOwnerRole() == ROLE_Authority) StartAuthoritativeFire(); else ServerRequestStartFire(); }
void UArenaDuelWeaponComponent::StopFire() { bFireHeld = false; if (GetOwnerRole() == ROLE_Authority) StopAuthoritativeFire(); else ServerRequestStopFire(); }
void UArenaDuelWeaponComponent::Reload() { if (GetOwnerRole() == ROLE_Authority) ServerRequestReload_Implementation(); else ServerRequestReload(); }
void UArenaDuelWeaponComponent::EquipWeapon(int32 Index) { if (GetOwnerRole() == ROLE_Authority) ServerRequestEquip_Implementation(Index); else ServerRequestEquip(Index); }
void UArenaDuelWeaponComponent::ServerRequestStartFire_Implementation() { StartAuthoritativeFire(); }
void UArenaDuelWeaponComponent::ServerRequestStopFire_Implementation() { StopAuthoritativeFire(); }
void UArenaDuelWeaponComponent::StartAuthoritativeFire()
{
	if (bServerFireHeld) return;
	bServerFireHeld = true;
	FireAuthoritative();
	if (GetCurrentDefinition().bAutomatic) { const float Interval = 60.0f / FMath::Max(GetCurrentDefinition().RoundsPerMinute, 1.0f); GetWorld()->GetTimerManager().SetTimer(AutomaticFireTimerHandle, this, &UArenaDuelWeaponComponent::FireAuthoritative, Interval, true, Interval); }
}
void UArenaDuelWeaponComponent::StopAuthoritativeFire() { bServerFireHeld = false; if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(AutomaticFireTimerHandle); }
void UArenaDuelWeaponComponent::ServerRequestReload_Implementation()
{
	FArenaDuelWeaponRuntimeState* State = GetMutableCurrentRuntimeState();
	if (!State || bReloading || State->MagazineAmmo >= GetCurrentDefinition().MagazineCapacity || State->ReserveAmmo <= 0) return;
	bReloading = true;
	FTimerDelegate ReloadDelegate; ReloadDelegate.BindUObject(this, &UArenaDuelWeaponComponent::CompleteReload);
	GetWorld()->GetTimerManager().SetTimer(ReloadTimerHandle, ReloadDelegate, GetCurrentDefinition().ReloadDuration, false);
}
void UArenaDuelWeaponComponent::ServerRequestEquip_Implementation(int32 Index)
{
	if (WeaponDefinitions.IsValidIndex(Index) && !bReloading && Index != EquippedWeaponIndex) { StopAuthoritativeFire(); EquippedWeaponIndex = static_cast<uint8>(Index); OnRep_EquippedWeapon(); }
}

void UArenaDuelWeaponComponent::FireAuthoritative()
{
	if (!bServerFireHeld && GetCurrentDefinition().bAutomatic) return;
	FArenaDuelWeaponRuntimeState* State = GetMutableCurrentRuntimeState();
	const FArenaDuelWeaponDefinition& Definition = GetCurrentDefinition();
	if (!State || bReloading || State->MagazineAmmo <= 0 || !GetOwner()) return;
	State->MagazineAmmo--;
	const AArenaDuelCharacter* Character = Cast<AArenaDuelCharacter>(GetOwner());
	const AController* Controller = Character ? Character->GetController() : nullptr;
	if (!Character || !Controller || !GetWorld()) return;
	const FVector Origin = Character->GetPawnViewLocation();
	const FVector Direction = Controller->GetControlRotation().Vector();
	FRandomStream Random(++LastShotSequence);
	int32 BodyPellets = 0, HeadPellets = 0, WorldPellets = 0;
	float ClosestDistance = Definition.Range;
	AActor* LastTarget = nullptr;
	for (int32 Pellet = 0; Pellet < FMath::Max(1, Definition.Pellets); ++Pellet)
	{
		const FVector PelletDirection = Random.VRandCone(Direction, FMath::DegreesToRadians(GetCurrentSpreadDegrees()));
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ArenaDuelWeaponTrace), true, Character);
		if (!GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + PelletDirection * Definition.Range, ECC_Visibility, Params)) continue;
		const EArenaDuelShotResult Result = ClassifyHit(Hit);
		ClosestDistance = FMath::Min(ClosestDistance, FVector::Dist(Origin, Hit.Location));
		LastTarget = Hit.GetActor();
		if (Result == EArenaDuelShotResult::Head) ++HeadPellets;
		else if (Result == EArenaDuelShotResult::Body) ++BodyPellets;
		else ++WorldPellets;
	}
	LastPelletsHit = BodyPellets + HeadPellets;
	LastHeadPellets = HeadPellets;
	const EArenaDuelShotResult Aggregate = HeadPellets > 0 ? EArenaDuelShotResult::Head : BodyPellets > 0 ? EArenaDuelShotResult::Body : WorldPellets > 0 ? EArenaDuelShotResult::World : EArenaDuelShotResult::Miss;
	SetLastShot(Aggregate, ClosestDistance, LastTarget);
}
void UArenaDuelWeaponComponent::CompleteReload()
{
	FArenaDuelWeaponRuntimeState* State = GetMutableCurrentRuntimeState();
	if (!State || !bReloading) return;
	const int32 Loaded = FMath::Min(FMath::Max(0, GetCurrentDefinition().MagazineCapacity - State->MagazineAmmo), State->ReserveAmmo);
	State->MagazineAmmo += Loaded; State->ReserveAmmo -= Loaded; bReloading = false;
}
void UArenaDuelWeaponComponent::ApplyLocalRecoil()
{
	AArenaDuelCharacter* Character = Cast<AArenaDuelCharacter>(GetOwner());
	if (!Character || !Character->IsLocallyControlled()) return;
	if (APlayerController* Controller = Cast<APlayerController>(Character->GetController())) { Controller->AddPitchInput(-GetCurrentDefinition().RecoilVertical); Controller->AddYawInput(GetCurrentDefinition().RecoilHorizontal * ((LastShotSequence & 1) ? 1.0f : -1.0f)); }
}
void UArenaDuelWeaponComponent::RefreshWeaponVisual()
{
	if (!FirstPersonWeaponMesh) return;
	const FVector Scale = EquippedWeaponIndex == 0 ? FVector(0.75f, 0.12f, 0.12f) : EquippedWeaponIndex == 1 ? FVector(0.4f, 0.14f, 0.12f) : EquippedWeaponIndex == 2 ? FVector(1.0f, 0.09f, 0.09f) : FVector(0.55f, 0.22f, 0.16f);
	FirstPersonWeaponMesh->SetRelativeLocation(FVector(35.0f, 18.0f, -18.0f)); FirstPersonWeaponMesh->SetRelativeScale3D(Scale);
}
void UArenaDuelWeaponComponent::SetLastShot(EArenaDuelShotResult Result, float Distance, AActor* Target)
{
	LastShotResult = Result; LastShotDistance = Distance; LastShotTarget = Target; LastShotWorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0f;
	if (GetOwnerRole() == ROLE_Authority) ClientShotConfirmation(LastShotSequence, Result, Distance, LastPelletsHit, LastHeadPellets);
}
void UArenaDuelWeaponComponent::ClientShotConfirmation_Implementation(int32 Sequence, EArenaDuelShotResult Result, float Distance, int32 PelletsHit, int32 HeadPellets)
{
	LastShotSequence = Sequence; LastShotResult = Result; LastShotDistance = Distance; LastPelletsHit = PelletsHit; LastHeadPellets = HeadPellets; LastShotWorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0f;
}
void UArenaDuelWeaponComponent::OnRep_EquippedWeapon() { RefreshWeaponVisual(); }
