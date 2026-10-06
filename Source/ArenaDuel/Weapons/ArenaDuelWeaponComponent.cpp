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
		SMG.AimFOV = 80.0f; SMG.AimSensitivityMultiplier = 0.85f; SMG.AimSpreadMultiplier = 0.75f; SMG.AimViewmodelLocation = FVector(48.0f, 3.0f, -13.0f);
		FArenaDuelWeaponDefinition DMR = Arc;
		DMR.Id = EArenaDuelWeaponId::RuneDMR; DMR.DisplayName = TEXT("Rune DMR"); DMR.MagazineCapacity = 12; DMR.ReserveCapacity = 48; DMR.RoundsPerMinute = 280.0f; DMR.BaseSpreadDegrees = 0.08f; DMR.MovementSpreadDegrees = 0.55f; DMR.bAutomatic = false;
		DMR.AimFOV = 68.0f; DMR.AimSensitivityMultiplier = 0.7f; DMR.AimSpreadMultiplier = 0.35f; DMR.AimViewmodelLocation = FVector(58.0f, 1.0f, -11.0f);
		FArenaDuelWeaponDefinition Shotgun = Arc;
		Shotgun.Id = EArenaDuelWeaponId::HexShotgun; Shotgun.DisplayName = TEXT("Hex Shotgun"); Shotgun.MagazineCapacity = 6; Shotgun.ReserveCapacity = 30; Shotgun.RoundsPerMinute = 75.0f; Shotgun.BaseSpreadDegrees = 5.0f; Shotgun.MovementSpreadDegrees = 2.0f; Shotgun.Pellets = 8; Shotgun.bAutomatic = false;
		Shotgun.AimFOV = 82.0f; Shotgun.AimSensitivityMultiplier = 0.85f; Shotgun.AimSpreadMultiplier = 0.85f; Shotgun.AimViewmodelLocation = FVector(45.0f, 4.0f, -15.0f);
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
		if (Character->IsLocallyControlled() && Character->GetFirstPersonCamera())
		{
			HipFOV = Character->GetFirstPersonCamera()->FieldOfView;
		}
	}
	if (GetOwnerRole() == ROLE_Authority) { InitializeRuntimeAmmo(); NextAllowedFireServerTimes.SetNum(WeaponDefinitions.Num()); }
	RefreshWeaponVisual();
}

void UArenaDuelWeaponComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		FTimerManager& TimerManager = GetWorld()->GetTimerManager();
		TimerManager.ClearTimer(ReloadTimerHandle);
		TimerManager.ClearTimer(AutomaticFireTimerHandle);
		TimerManager.ClearTimer(LocalCosmeticFireTimerHandle);
		TimerManager.ClearTimer(LocalCosmeticRecoveryTimerHandle);
		TimerManager.ClearTimer(AimVisualTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void UArenaDuelWeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UArenaDuelWeaponComponent, EquippedWeaponIndex);
	DOREPLIFETIME_CONDITION(UArenaDuelWeaponComponent, RuntimeAmmo, COND_OwnerOnly);
	DOREPLIFETIME(UArenaDuelWeaponComponent, bReloading);
	DOREPLIFETIME(UArenaDuelWeaponComponent, bAiming);
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
float UArenaDuelWeaponComponent::GetCrosshairKick() const { return (GetWorld() && LastCosmeticShotWorldTime >= 0.0f) ? FMath::Clamp(1.0f - (GetWorld()->GetTimeSeconds() - LastCosmeticShotWorldTime) / 0.18f, 0.0f, 1.0f) : 0.0f; }
float UArenaDuelWeaponComponent::GetCurrentSpreadDegrees() const
{
	const AArenaDuelCharacter* Character = Cast<AArenaDuelCharacter>(GetOwner());
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	const float SpeedFactor = FMath::Clamp(Movement ? Movement->Velocity.Size2D() / 600.0f : 0.0f, 0.0f, 1.0f);
	const float AirFactor = Movement && Movement->IsFalling() ? 0.5f : 0.0f;
	const float AimMultiplier = bAiming ? GetCurrentDefinition().AimSpreadMultiplier : 1.0f;
	return (GetCurrentDefinition().BaseSpreadDegrees + GetCurrentDefinition().MovementSpreadDegrees * (SpeedFactor + AirFactor)) * AimMultiplier;
}

void UArenaDuelWeaponComponent::StartFire()
{
	if (bFireHeld) return;
	if (bReloading || GetCurrentMagazineAmmo() <= 0 || !WeaponDefinitions.IsValidIndex(EquippedWeaponIndex)) return;
	bFireHeld = true;
	if (AArenaDuelCharacter* Character = Cast<AArenaDuelCharacter>(GetOwner()); Character && Character->IsLocallyControlled())
	{
		LocalCosmeticShot();
		if (GetCurrentDefinition().bAutomatic && GetWorld())
		{
			const float Interval = 60.0f / FMath::Max(GetCurrentDefinition().RoundsPerMinute, 1.0f);
			GetWorld()->GetTimerManager().SetTimer(LocalCosmeticFireTimerHandle, this, &UArenaDuelWeaponComponent::LocalCosmeticShot, Interval, true, Interval);
		}
	}
	if (GetOwnerRole() == ROLE_Authority) StartAuthoritativeFire(); else ServerSetFireHeld(true);
}
void UArenaDuelWeaponComponent::StopFire()
{
	CancelLocalAndServerFire();
}
void UArenaDuelWeaponComponent::Reload()
{
	CancelLocalAndServerFire();
	StopAim();
	if (GetOwnerRole() == ROLE_Authority) ServerRequestReload_Implementation(); else ServerRequestReload();
}
void UArenaDuelWeaponComponent::EquipWeapon(int32 Index)
{
	CancelLocalAndServerFire();
	StopAim();
	if (GetOwnerRole() == ROLE_Authority) ServerRequestEquip_Implementation(Index); else ServerRequestEquip(Index);
}
void UArenaDuelWeaponComponent::StartAim()
{
	if (bAiming) return;
	bAiming = true;
	if (GetOwnerRole() == ROLE_Authority) ServerSetAiming_Implementation(true); else ServerSetAiming(true);
	if (GetWorld()) GetWorld()->GetTimerManager().SetTimer(AimVisualTimerHandle, this, &UArenaDuelWeaponComponent::UpdateAimVisual, 0.02f, true);
}
void UArenaDuelWeaponComponent::StopAim()
{
	bAiming = false;
	if (GetOwnerRole() == ROLE_Authority) ServerSetAiming_Implementation(false); else ServerSetAiming(false);
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(AimVisualTimerHandle, this, &UArenaDuelWeaponComponent::UpdateAimVisual, 0.02f, true);
	}
	else
	{
		UpdateAimVisual();
	}
}
void UArenaDuelWeaponComponent::ServerSetAiming_Implementation(bool bAimingState)
{
	bAiming = bAimingState && Cast<AArenaDuelCharacter>(GetOwner()) && Cast<AArenaDuelCharacter>(GetOwner())->GetController() != nullptr;
}
void UArenaDuelWeaponComponent::ServerSetFireHeld_Implementation(bool bHeld) { if (bHeld) StartAuthoritativeFire(); else StopAuthoritativeFire(); }
bool UArenaDuelWeaponComponent::CanBeginAuthoritativeFire() const
{
	const AArenaDuelCharacter* Character = Cast<AArenaDuelCharacter>(GetOwner());
	return Character && Character->GetController() && GetWorld() && WeaponDefinitions.IsValidIndex(EquippedWeaponIndex) && !bReloading && GetCurrentMagazineAmmo() > 0;
}
void UArenaDuelWeaponComponent::StartAuthoritativeFire()
{
	if (bServerFireHeld || !CanBeginAuthoritativeFire()) return;
	bServerFireHeld = true;
	FireAuthoritative();
	if (GetCurrentDefinition().bAutomatic) { const float Interval = 60.0f / FMath::Max(GetCurrentDefinition().RoundsPerMinute, 1.0f); GetWorld()->GetTimerManager().SetTimer(AutomaticFireTimerHandle, this, &UArenaDuelWeaponComponent::FireAuthoritative, Interval, true, Interval); }
}
void UArenaDuelWeaponComponent::StopAuthoritativeFire() { bServerFireHeld = false; if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(AutomaticFireTimerHandle); }
void UArenaDuelWeaponComponent::CancelLocalAndServerFire()
{
	bFireHeld = false;
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(LocalCosmeticFireTimerHandle);
	if (GetOwnerRole() == ROLE_Authority) StopAuthoritativeFire();
	else ServerSetFireHeld(false);
}
void UArenaDuelWeaponComponent::ServerRequestReload_Implementation()
{
	FArenaDuelWeaponRuntimeState* State = GetMutableCurrentRuntimeState();
	if (!State || bReloading || State->MagazineAmmo >= GetCurrentDefinition().MagazineCapacity || State->ReserveAmmo <= 0) return;
	StopAuthoritativeFire();
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
	const AArenaDuelCharacter* Character = Cast<AArenaDuelCharacter>(GetOwner());
	const AController* Controller = Character ? Character->GetController() : nullptr;
	if (!State || bReloading || State->MagazineAmmo <= 0 || !GetOwner() || !Character || !Controller || !GetWorld()) return;
	const int32 DefinitionIndex = static_cast<int32>(EquippedWeaponIndex);
	const double ServerTime = GetWorld()->GetTimeSeconds();
	if (!NextAllowedFireServerTimes.IsValidIndex(DefinitionIndex)) NextAllowedFireServerTimes.SetNum(WeaponDefinitions.Num());
	if (NextAllowedFireServerTimes.IsValidIndex(DefinitionIndex) && ServerTime + KINDA_SMALL_NUMBER < NextAllowedFireServerTimes[DefinitionIndex]) return;
	State->MagazineAmmo--;
	NextAllowedFireServerTimes[DefinitionIndex] = ServerTime + 60.0 / FMath::Max(static_cast<double>(Definition.RoundsPerMinute), 1.0);
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
	if (GetCurrentMagazineAmmo() <= 0 || bReloading || !WeaponDefinitions.IsValidIndex(EquippedWeaponIndex)) return;
	const float HorizontalKick = GetCurrentDefinition().RecoilHorizontal * ((LastShotSequence & 1) ? 1.0f : -1.0f);
	LocalRecoilPitchRemaining += GetCurrentDefinition().RecoilVertical;
	LocalRecoilYawRemaining += HorizontalKick;
	if (APlayerController* Controller = Cast<APlayerController>(Character->GetController())) { Controller->AddPitchInput(-GetCurrentDefinition().RecoilVertical); Controller->AddYawInput(HorizontalKick); }
}
void UArenaDuelWeaponComponent::LocalCosmeticShot()
{
	if (!bFireHeld || bReloading || GetCurrentMagazineAmmo() <= 0 || !WeaponDefinitions.IsValidIndex(EquippedWeaponIndex)) return;
	LastCosmeticShotWorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0f;
	LocalWeaponKick = 1.0f;
	ApplyLocalRecoil();
	if (GetWorld()) GetWorld()->GetTimerManager().SetTimer(LocalCosmeticRecoveryTimerHandle, this, &UArenaDuelWeaponComponent::RecoverCosmeticKick, 0.02f, true);
}
void UArenaDuelWeaponComponent::RecoverCosmeticKick()
{
	if (AArenaDuelCharacter* Character = Cast<AArenaDuelCharacter>(GetOwner()); Character && Character->IsLocallyControlled())
	{
		if (APlayerController* Controller = Cast<APlayerController>(Character->GetController()))
		{
			const float PitchStep = FMath::Min(LocalRecoilPitchRemaining, 0.025f);
			const float YawStep = FMath::Clamp(LocalRecoilYawRemaining, -0.01f, 0.01f);
			Controller->AddPitchInput(PitchStep);
			Controller->AddYawInput(-YawStep);
			LocalRecoilPitchRemaining -= PitchStep;
			LocalRecoilYawRemaining -= YawStep;
		}
	}
	LocalWeaponKick = FMath::FInterpTo(LocalWeaponKick, 0.0f, 0.02f, 12.0f);
	if (FirstPersonWeaponMesh && LocalWeaponKick > KINDA_SMALL_NUMBER) FirstPersonWeaponMesh->SetRelativeLocation(FVector(35.0f - 5.0f * LocalWeaponKick, 18.0f, -18.0f + 2.0f * LocalWeaponKick));
	if (FMath::IsNearlyZero(LocalWeaponKick, KINDA_SMALL_NUMBER) && FMath::IsNearlyZero(LocalRecoilPitchRemaining, KINDA_SMALL_NUMBER) && FMath::IsNearlyZero(LocalRecoilYawRemaining, KINDA_SMALL_NUMBER))
	{
		LocalWeaponKick = 0.0f;
		LocalRecoilPitchRemaining = 0.0f;
		LocalRecoilYawRemaining = 0.0f;
		RefreshWeaponVisual();
		if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(LocalCosmeticRecoveryTimerHandle);
	}
}
void UArenaDuelWeaponComponent::RefreshWeaponVisual()
{
	if (!FirstPersonWeaponMesh) return;
	const FVector Scale = EquippedWeaponIndex == 0 ? FVector(0.75f, 0.12f, 0.12f) : EquippedWeaponIndex == 1 ? FVector(0.4f, 0.14f, 0.12f) : EquippedWeaponIndex == 2 ? FVector(1.0f, 0.09f, 0.09f) : FVector(0.55f, 0.22f, 0.16f);
	const FArenaDuelWeaponDefinition& Definition = GetCurrentDefinition();
	const FVector HipLocation = EquippedWeaponIndex == 2 ? FVector(42.0f, 16.0f, -17.0f) : EquippedWeaponIndex == 3 ? FVector(32.0f, 20.0f, -19.0f) : FVector(35.0f, 18.0f, -18.0f);
	const FRotator HipRotation = EquippedWeaponIndex == 1 ? FRotator(0.0f, 0.0f, -2.0f) : EquippedWeaponIndex == 3 ? FRotator(0.0f, 0.0f, 2.0f) : FRotator::ZeroRotator;
	const FVector Location = bAiming ? Definition.AimViewmodelLocation : HipLocation;
	const FRotator Rotation = bAiming ? Definition.AimViewmodelRotation : HipRotation;
	FirstPersonWeaponMesh->SetRelativeLocation(Location); FirstPersonWeaponMesh->SetRelativeRotation(Rotation); FirstPersonWeaponMesh->SetRelativeScale3D(Scale);
}
void UArenaDuelWeaponComponent::UpdateAimVisual()
{
	AArenaDuelCharacter* Character = Cast<AArenaDuelCharacter>(GetOwner());
	if (!Character || !Character->IsLocallyControlled() || !Character->GetFirstPersonCamera()) return;
	const float TargetFOV = bAiming ? GetCurrentDefinition().AimFOV : HipFOV;
	const float NewFOV = FMath::FInterpTo(Character->GetFirstPersonCamera()->FieldOfView, TargetFOV, 0.02f, 12.0f);
	Character->GetFirstPersonCamera()->SetFieldOfView(NewFOV);
	if (FirstPersonWeaponMesh)
	{
		const FArenaDuelWeaponDefinition& Definition = GetCurrentDefinition();
		const FVector HipLocation = EquippedWeaponIndex == 2 ? FVector(42.0f, 16.0f, -17.0f) : EquippedWeaponIndex == 3 ? FVector(32.0f, 20.0f, -19.0f) : FVector(35.0f, 18.0f, -18.0f);
		const FRotator HipRotation = EquippedWeaponIndex == 1 ? FRotator(0.0f, 0.0f, -2.0f) : EquippedWeaponIndex == 3 ? FRotator(0.0f, 0.0f, 2.0f) : FRotator::ZeroRotator;
		FirstPersonWeaponMesh->SetRelativeLocation(FMath::VInterpTo(FirstPersonWeaponMesh->GetRelativeLocation(), bAiming ? Definition.AimViewmodelLocation : HipLocation, 0.02f, 14.0f));
		FirstPersonWeaponMesh->SetRelativeRotation(FMath::RInterpTo(FirstPersonWeaponMesh->GetRelativeRotation(), bAiming ? Definition.AimViewmodelRotation : HipRotation, 0.02f, 14.0f));
	}
	if (!bAiming && FMath::IsNearlyEqual(NewFOV, HipFOV, 0.1f) && GetWorld()) GetWorld()->GetTimerManager().ClearTimer(AimVisualTimerHandle);
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
