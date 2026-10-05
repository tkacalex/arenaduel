// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArenaDuelWeaponComponent.h"

#include "../Characters/ArenaDuelCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
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
		SMG.Id = EArenaDuelWeaponId::ShadeSMG;
		SMG.DisplayName = TEXT("Shade SMG");
		SMG.MagazineCapacity = 32;
		SMG.ReserveCapacity = 128;
		SMG.RoundsPerMinute = 900.0f;
		SMG.BaseSpreadDegrees = 0.65f;
		SMG.MovementSpreadDegrees = 1.8f;

		FArenaDuelWeaponDefinition DMR = Arc;
		DMR.Id = EArenaDuelWeaponId::RuneDMR;
		DMR.DisplayName = TEXT("Rune DMR");
		DMR.MagazineCapacity = 12;
		DMR.ReserveCapacity = 48;
		DMR.RoundsPerMinute = 280.0f;
		DMR.BaseSpreadDegrees = 0.08f;
		DMR.MovementSpreadDegrees = 0.55f;
		DMR.bAutomatic = false;

		FArenaDuelWeaponDefinition Shotgun = Arc;
		Shotgun.Id = EArenaDuelWeaponId::HexShotgun;
		Shotgun.DisplayName = TEXT("Hex Shotgun");
		Shotgun.MagazineCapacity = 6;
		Shotgun.ReserveCapacity = 30;
		Shotgun.RoundsPerMinute = 75.0f;
		Shotgun.BaseSpreadDegrees = 5.0f;
		Shotgun.MovementSpreadDegrees = 2.0f;
		Shotgun.Pellets = 8;
		Shotgun.bAutomatic = false;

		return { Arc, SMG, DMR, Shotgun };
	}
}

UArenaDuelWeaponComponent::UArenaDuelWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
	WeaponDefinitions = MakeDefinitions();
	FirstPersonWeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FirstPersonWeaponMesh"));
	FirstPersonWeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FirstPersonWeaponMesh->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		FirstPersonWeaponMesh->SetStaticMesh(CubeMesh.Object);
	}
}

void UArenaDuelWeaponComponent::BeginPlay()
{
	Super::BeginPlay();
	if (AArenaDuelCharacter* Character = Cast<AArenaDuelCharacter>(GetOwner()))
	{
		FirstPersonWeaponMesh->AttachToComponent(Character->GetFirstPersonCamera(), FAttachmentTransformRules::KeepRelativeTransform);
		FirstPersonWeaponMesh->SetRelativeLocation(FVector(35.0f, 18.0f, -18.0f));
		FirstPersonWeaponMesh->SetRelativeScale3D(FVector(0.55f, 0.12f, 0.12f));
		FirstPersonWeaponMesh->SetVisibility(Character->IsLocallyControlled());
	}
	CurrentMagazineAmmo = GetCurrentDefinition().MagazineCapacity;
	ReserveAmmo = GetCurrentDefinition().ReserveCapacity;
}

void UArenaDuelWeaponComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	FireCooldownRemaining = FMath::Max(0.0f, FireCooldownRemaining - DeltaTime);
	if (bFireHeld && GetCurrentDefinition().bAutomatic && GetOwnerRole() < ROLE_Authority)
	{
		StartFire();
	}
}

void UArenaDuelWeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UArenaDuelWeaponComponent, EquippedWeaponIndex);
	DOREPLIFETIME(UArenaDuelWeaponComponent, CurrentMagazineAmmo);
	DOREPLIFETIME(UArenaDuelWeaponComponent, ReserveAmmo);
	DOREPLIFETIME(UArenaDuelWeaponComponent, bReloading);
}

const FArenaDuelWeaponDefinition& UArenaDuelWeaponComponent::GetCurrentDefinition() const
{
	static const FArenaDuelWeaponDefinition Fallback;
	return WeaponDefinitions.IsValidIndex(EquippedWeaponIndex) ? WeaponDefinitions[EquippedWeaponIndex] : Fallback;
}

EArenaDuelWeaponId UArenaDuelWeaponComponent::GetCurrentWeaponId() const { return GetCurrentDefinition().Id; }
FName UArenaDuelWeaponComponent::GetCurrentWeaponName() const { return GetCurrentDefinition().DisplayName; }

void UArenaDuelWeaponComponent::StartFire()
{
	bFireHeld = true;
	if (GetOwnerRole() == ROLE_Authority)
	{
		FireAuthoritative();
	}
	else
	{
		ServerRequestFire();
	}
}

void UArenaDuelWeaponComponent::StopFire() { bFireHeld = false; }

void UArenaDuelWeaponComponent::Reload()
{
	if (GetOwnerRole() == ROLE_Authority) ServerRequestReload_Implementation();
	else ServerRequestReload();
}

void UArenaDuelWeaponComponent::EquipWeapon(int32 Index)
{
	if (GetOwnerRole() == ROLE_Authority) ServerRequestEquip_Implementation(Index);
	else ServerRequestEquip(Index);
}

void UArenaDuelWeaponComponent::ServerRequestFire_Implementation() { FireAuthoritative(); }
void UArenaDuelWeaponComponent::ServerRequestReload_Implementation()
{
	if (!bReloading && CurrentMagazineAmmo < GetCurrentDefinition().MagazineCapacity && ReserveAmmo > 0)
	{
		bReloading = true;
		FTimerDelegate ReloadDelegate;
		ReloadDelegate.BindUObject(this, &UArenaDuelWeaponComponent::CompleteReload);
		GetWorld()->GetTimerManager().SetTimer(ReloadTimerHandle, ReloadDelegate, GetCurrentDefinition().ReloadDuration, false);
	}
}
void UArenaDuelWeaponComponent::ServerRequestEquip_Implementation(int32 Index)
{
	if (WeaponDefinitions.IsValidIndex(Index) && !bReloading)
	{
		EquippedWeaponIndex = static_cast<uint8>(Index);
		CurrentMagazineAmmo = GetCurrentDefinition().MagazineCapacity;
		ReserveAmmo = GetCurrentDefinition().ReserveCapacity;
		OnRep_EquippedWeapon();
	}
}

void UArenaDuelWeaponComponent::FireAuthoritative()
{
	const FArenaDuelWeaponDefinition& Definition = GetCurrentDefinition();
	if (bReloading || CurrentMagazineAmmo <= 0 || FireCooldownRemaining > 0.0f || !GetOwner()) return;
	CurrentMagazineAmmo--;
	FireCooldownRemaining = 60.0f / FMath::Max(Definition.RoundsPerMinute, 1.0f);

	AArenaDuelCharacter* Character = Cast<AArenaDuelCharacter>(GetOwner());
	APlayerController* Controller = Character ? Cast<APlayerController>(Character->GetController()) : nullptr;
	if (!Controller) return;
	const FVector Origin = Controller->PlayerCameraManager ? Controller->PlayerCameraManager->GetCameraLocation() : Character->GetActorLocation();
	const FVector Direction = Controller->PlayerCameraManager ? Controller->PlayerCameraManager->GetCameraRotation().Vector() : Character->GetActorForwardVector();
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ArenaDuelWeaponTrace), true, Character);
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + Direction * Definition.Range, ECC_Visibility, Params);
	if (!bHit)
	{
		SetLastShot(EArenaDuelShotResult::Miss, Definition.Range, nullptr);
		return;
	}
	const bool bHead = Hit.BoneName == TEXT("head") || (Hit.Component.IsValid() && Hit.Component->ComponentHasTag(TEXT("HeadHitZone")));
	SetLastShot(bHead ? EArenaDuelShotResult::Head : EArenaDuelShotResult::Body, FVector::Dist(Origin, Hit.Location), Hit.GetActor());
}

void UArenaDuelWeaponComponent::CompleteReload()
{
	if (!bReloading) return;
	const int32 Needed = FMath::Max(0, GetCurrentDefinition().MagazineCapacity - CurrentMagazineAmmo);
	const int32 Loaded = FMath::Min(Needed, ReserveAmmo);
	CurrentMagazineAmmo += Loaded;
	ReserveAmmo -= Loaded;
	bReloading = false;
}

void UArenaDuelWeaponComponent::RefreshWeaponVisual() {}
void UArenaDuelWeaponComponent::SetLastShot(EArenaDuelShotResult Result, float Distance, AActor* Target)
{
	LastShotResult = Result;
	LastShotDistance = Distance;
	LastShotTarget = Target;
}
void UArenaDuelWeaponComponent::OnRep_EquippedWeapon() { RefreshWeaponVisual(); }
