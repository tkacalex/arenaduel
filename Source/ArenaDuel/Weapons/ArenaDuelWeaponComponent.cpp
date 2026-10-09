// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArenaDuelWeaponComponent.h"
#include "../Abilities/ArenaDuelArcBarrier.h"
#include "../Characters/ArenaDuelCharacter.h"
#include "../Characters/ArenaDuelCharacterMovementComponent.h"
#include "../Game/ArenaDuelGameState.h"
#include "../Player/ArenaDuelPlayerState.h"
#include "../Player/ArenaDuelPlayerController.h"
#include "ArenaDuelWeaponTarget.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "HAL/IConsoleManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// Exact critically damped step. Its response does not depend on the frame rate.
	static void StepSpring(float& Position, float& Velocity, float Target, float Response, float DeltaSeconds)
	{
		const float Decay = FMath::Exp(-FMath::Max(Response, 0.01f) * DeltaSeconds);
		const float Offset = Position - Target;
		const float Step = (Velocity + Response * Offset) * DeltaSeconds;
		Position = Target + (Offset + Step) * Decay;
		Velocity = (Velocity - Response * Step) * Decay;
	}

	// Raise it to keep tracers on screen long enough to inspect where they start.
	static TAutoConsoleVariable<float> CVarTracerSeconds(TEXT("ArenaDuel.TracerSeconds"), 0.11f, TEXT("How long a cosmetic shot tracer stays visible."));

	static TArray<FArenaDuelWeaponDefinition> MakeDefinitions()
	{
		FArenaDuelWeaponDefinition Arc;
		Arc.Id = EArenaDuelWeaponId::ArcRifle;
		Arc.DisplayName = TEXT("Arc Rifle");
		Arc.AimSensitivityMultiplier = 0.75f;
		Arc.BodyDamage = 27.0f; Arc.HeadshotMultiplier = 1.5f;
		FArenaDuelWeaponDefinition SMG = Arc;
		SMG.Id = EArenaDuelWeaponId::ShadeSMG; SMG.DisplayName = TEXT("Shade SMG"); SMG.MagazineCapacity = 32; SMG.ReserveCapacity = 128; SMG.RoundsPerMinute = 900.0f; SMG.BaseSpreadDegrees = 0.65f; SMG.MovementSpreadDegrees = 1.8f;
		SMG.AimFOV = 80.0f; SMG.AimSensitivityMultiplier = 0.80f; SMG.AimSpreadMultiplier = 0.75f; SMG.AimViewmodelLocation = FVector(48.0f, 3.0f, -13.0f);
		SMG.BodyDamage = 20.0f; SMG.HeadshotMultiplier = 1.4f;
		FArenaDuelWeaponDefinition DMR = Arc;
		DMR.Id = EArenaDuelWeaponId::RuneDMR; DMR.DisplayName = TEXT("Rune DMR"); DMR.MagazineCapacity = 12; DMR.ReserveCapacity = 48; DMR.RoundsPerMinute = 280.0f; DMR.BaseSpreadDegrees = 0.08f; DMR.MovementSpreadDegrees = 0.55f; DMR.bAutomatic = false;
		DMR.AimFOV = 68.0f; DMR.AimSensitivityMultiplier = 0.65f; DMR.AimSpreadMultiplier = 0.35f; DMR.AimViewmodelLocation = FVector(58.0f, 1.0f, -11.0f);
		DMR.BodyDamage = 42.0f; DMR.HeadshotMultiplier = 1.6f;
		FArenaDuelWeaponDefinition Shotgun = Arc;
		Shotgun.Id = EArenaDuelWeaponId::HexShotgun; Shotgun.DisplayName = TEXT("Hex Shotgun"); Shotgun.MagazineCapacity = 6; Shotgun.ReserveCapacity = 30; Shotgun.RoundsPerMinute = 75.0f; Shotgun.BaseSpreadDegrees = 5.0f; Shotgun.MovementSpreadDegrees = 2.0f; Shotgun.Pellets = 8; Shotgun.bAutomatic = false;
		Shotgun.AimFOV = 82.0f; Shotgun.AimSensitivityMultiplier = 0.80f; Shotgun.AimSpreadMultiplier = 0.85f; Shotgun.AimViewmodelLocation = FVector(45.0f, 4.0f, -15.0f);
		Shotgun.BodyDamage = 11.0f; Shotgun.HeadshotMultiplier = 1.25f;
		return { Arc, SMG, DMR, Shotgun };
	}

	static EArenaDuelShotResult ClassifyHit(const FHitResult& Hit)
	{
		if (!Hit.GetActor()) return EArenaDuelShotResult::World;
		if (Hit.BoneName == TEXT("head") || (Hit.Component.IsValid() && Hit.Component->ComponentHasTag(TEXT("HeadHitZone")))) return EArenaDuelShotResult::Head;
		if (Hit.Component.IsValid() && Hit.Component->ComponentHasTag(TEXT("BodyHitZone"))) return EArenaDuelShotResult::Body;
		if (Hit.GetActor()->IsA<AArenaDuelWeaponTarget>() || Hit.GetActor()->IsA<APawn>()) return EArenaDuelShotResult::Body;
		return EArenaDuelShotResult::World;
	}
}

EArenaDuelShotResult UArenaDuelWeaponComponent::ClassifyHitBone(FName BoneName)
{
	const FString Bone = BoneName.ToString();
	if (Bone.StartsWith(TEXT("head"))) return EArenaDuelShotResult::Head;
	for (const TCHAR* Limb : { TEXT("upperarm"), TEXT("lowerarm"), TEXT("hand"), TEXT("thigh"), TEXT("calf"), TEXT("foot"), TEXT("ball"),
		TEXT("thumb"), TEXT("index"), TEXT("middle"), TEXT("ring"), TEXT("pinky") })
	{
		if (Bone.StartsWith(Limb)) return EArenaDuelShotResult::Limb;
	}
	// Pelvis, spine, clavicles and neck.
	return EArenaDuelShotResult::Body;
}

UArenaDuelWeaponComponent::UArenaDuelWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(true);
	WeaponDefinitions = MakeDefinitions();
	WeaponBodyMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/ArenaDuel/Characters/Common/M_ArcaneMetal"));
	WeaponAccentCyan=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/ArenaDuel/Characters/Common/M_ArcaneCyan"));
	WeaponAccentViolet=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/ArenaDuel/Characters/Common/M_ArcaneViolet"));
	FireSound=LoadObject<USoundBase>(nullptr,TEXT("/Game/Weapons/GrenadeLauncher/Audio/FirstPersonTemplateWeaponFire02"));
	FirstPersonWeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FirstPersonWeaponMesh"));
	FirstPersonWeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FirstPersonWeaponMesh->SetCastShadow(false);
	FirstPersonWeaponMesh->SetOnlyOwnerSee(true);
	FirstPersonWeaponMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	ThirdPersonWeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ThirdPersonWeaponMesh"));
	ThirdPersonWeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ThirdPersonWeaponMesh->SetOwnerNoSee(true);
	ThirdPersonWeaponMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;
	for (const TCHAR* Name : {TEXT("ArcRifle"), TEXT("ShadeSMG"), TEXT("RuneDMR"), TEXT("HexShotgun")})
	{
		FArenaDuelWeaponVisualDefinition Visual;
		Visual.Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(FString::Printf(TEXT("/Game/ArenaDuel/Weapons/%s/SM_%s.SM_%s"), Name, Name, Name)));
		// Generated firearm meshes use +X for muzzle forward. The measured rifle
		// idle pose rotates HandGrip_R to -78.4 degrees after Manny's mesh basis
		// correction, so this local yaw brings the muzzle back onto actor/camera +X.
		// Keep FP and TP values independent because they use different parents.
		Visual.FirstPersonGripRotation = FRotator(0.0f, 78.4f, 0.0f);
		Visual.ThirdPersonGripRotation = FRotator(0.0f, 78.4f, 0.0f);
		WeaponVisualDefinitions.Add(Visual);
	}
	WeaponVisualDefinitions[0].HipViewmodelLocation=FVector(28,16,-40);
	WeaponVisualDefinitions[1].HipViewmodelLocation=FVector(34,17,-39);
	WeaponVisualDefinitions[1].HipViewmodelRotation = FRotator(0, 0, -2);
	WeaponVisualDefinitions[2].HipViewmodelLocation = FVector(44,16,-39);
	WeaponVisualDefinitions[3].HipViewmodelLocation = FVector(30,19,-42);
	WeaponVisualDefinitions[3].HipViewmodelRotation = FRotator(0, 0, 2);
	WeaponVisualDefinitions[0].FirstPersonScale=FVector(0.40f);
	WeaponVisualDefinitions[1].FirstPersonScale=FVector(0.38f);
	WeaponVisualDefinitions[2].FirstPersonScale=FVector(0.36f);
	WeaponVisualDefinitions[3].FirstPersonScale=FVector(0.38f);
	// Local positions on the gun meshes. These are the support-hand targets for
	// the current generated weapons and can be tuned per weapon in the component.
	WeaponVisualDefinitions[0].LeftHandGripLocation=FVector(20,-5,-3);
	WeaponVisualDefinitions[1].LeftHandGripLocation=FVector(11,-5,-3);
	WeaponVisualDefinitions[2].LeftHandGripLocation=FVector(27,-5,-3);
	WeaponVisualDefinitions[3].LeftHandGripLocation=FVector(17,-6,-3);
	for (FArenaDuelWeaponVisualDefinition& Visual : WeaponVisualDefinitions) Visual.ThirdPersonScale = FVector(0.75f);
	// Visual alignment only; approved FOV, sensitivity, spread and recoil are unchanged.
	for (auto& Definition : WeaponDefinitions) Definition.AimViewmodelLocation.Z -= 19.0f;
}

void UArenaDuelWeaponComponent::BeginPlay()
{
	Super::BeginPlay();
	if (AArenaDuelCharacter* Character = Cast<AArenaDuelCharacter>(GetOwner()))
	{
		if (Character->IsLocallyControlled() && Character->GetFirstPersonCamera())
		{
			HipFOV = Character->GetFirstPersonCamera()->FieldOfView;
		}
	}
	if (GetOwnerRole() == ROLE_Authority) { InitializeRuntimeAmmo(); NextAllowedFireServerTimes.SetNum(WeaponDefinitions.Num()); }
	RefreshWeaponVisual();
}

void UArenaDuelWeaponComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	TickLocalPresentation(DeltaTime);
}

bool UArenaDuelWeaponComponent::GetLeftHandGripWorldLocation(FVector& OutLocation) const
{
	if (!WeaponVisualDefinitions.IsValidIndex(EquippedWeaponIndex) || !FirstPersonWeaponMesh || !FirstPersonWeaponMesh->GetStaticMesh()) return false;
	if (FirstPersonWeaponMesh->DoesSocketExist(TEXT("LeftHandGrip")))
	{
		OutLocation = FirstPersonWeaponMesh->GetSocketLocation(TEXT("LeftHandGrip"));
	}
	else
	{
		OutLocation = FirstPersonWeaponMesh->GetComponentTransform().TransformPosition(WeaponVisualDefinitions[EquippedWeaponIndex].LeftHandGripLocation);
	}
	return !OutLocation.ContainsNaN();
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
	if (const AArenaDuelCharacter* Character = Cast<AArenaDuelCharacter>(GetOwner()); !Character || Character->IsDead() || !IsRoundInProgress() || IsLocalAdminMenuOpen()) return;
	if (bFireHeld) return;
	if (bReloading || (GetCurrentMagazineAmmo() <= 0 && !HasInfiniteAmmoForDevelopment()) || !WeaponDefinitions.IsValidIndex(EquippedWeaponIndex)) return;
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
	if (const AArenaDuelCharacter* Character = Cast<AArenaDuelCharacter>(GetOwner()); !Character || Character->IsDead() || !IsRoundInProgress() || IsLocalAdminMenuOpen()) return;
	CancelLocalAndServerFire();
	StopAim();
	if (GetOwnerRole() == ROLE_Authority) ServerRequestReload_Implementation(); else ServerRequestReload();
}
void UArenaDuelWeaponComponent::EquipWeapon(int32 Index)
{
	if (const AArenaDuelCharacter* Character = Cast<AArenaDuelCharacter>(GetOwner()); !Character || Character->IsDead() || !IsRoundInProgress() || IsLocalAdminMenuOpen()) return;
	CancelLocalAndServerFire(true);
	StopAim();
	if (GetOwnerRole() == ROLE_Authority) ServerRequestEquip_Implementation(Index); else ServerRequestEquip(Index);
}
void UArenaDuelWeaponComponent::StartAim()
{
	if (const AArenaDuelCharacter* Character = Cast<AArenaDuelCharacter>(GetOwner()); !Character || Character->IsDead() || !IsRoundInProgress() || IsLocalAdminMenuOpen()) return;
	if (const AArenaDuelCharacter* Character = Cast<AArenaDuelCharacter>(GetOwner()); Character && Character->GetArenaDuelMovementComponent() && Character->GetArenaDuelMovementComponent()->IsSprinting()) return;
	if (bAiming) return;
	bAiming = true;
	if (GetOwnerRole() == ROLE_Authority) ServerSetAiming_Implementation(true); else ServerSetAiming(true);
	SetComponentTickEnabled(true);
}
void UArenaDuelWeaponComponent::StopAim()
{
	bAiming = false;
	if (GetOwnerRole() == ROLE_Authority) ServerSetAiming_Implementation(false); else ServerSetAiming(false);
	SetComponentTickEnabled(true);
}
void UArenaDuelWeaponComponent::CancelCombatActions()
{
	AArenaDuelCharacter* Character = Cast<AArenaDuelCharacter>(GetOwner());
	const bool bAuthority = GetOwnerRole() == ROLE_Authority;
	const bool bLocalOwner = Character && Character->IsLocallyControlled();
	if (bAuthority)
	{
		StopAuthoritativeFire();
		bAiming = false;
		bReloading = false;
		if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(ReloadTimerHandle);
	}
	if (bLocalOwner)
	{
		CancelLocalAndServerFire(true);
		StopAim();
		if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(LocalCosmeticRecoveryTimerHandle);
	}
	if (!bAuthority && !bLocalOwner)
	{
		bFireHeld = false;
		bAiming = false;
		if (GetWorld())
		{
			GetWorld()->GetTimerManager().ClearTimer(LocalCosmeticFireTimerHandle);
			GetWorld()->GetTimerManager().ClearTimer(LocalCosmeticRecoveryTimerHandle);
			GetWorld()->GetTimerManager().ClearTimer(AimVisualTimerHandle);
		}
	}
}
void UArenaDuelWeaponComponent::RefillAllAmmoForDevelopment()
{
	if (GetOwnerRole() != ROLE_Authority) return;
	if (RuntimeAmmo.Num() != WeaponDefinitions.Num()) RuntimeAmmo.SetNum(WeaponDefinitions.Num());
	for (int32 Index = 0; Index < WeaponDefinitions.Num(); ++Index)
	{
		RuntimeAmmo[Index].MagazineAmmo = WeaponDefinitions[Index].MagazineCapacity;
		RuntimeAmmo[Index].ReserveAmmo = WeaponDefinitions[Index].ReserveCapacity;
	}
	if (AActor* Owner = GetOwner()) Owner->ForceNetUpdate();
}

void UArenaDuelWeaponComponent::ServerSetAiming_Implementation(bool bAimingState)
{
	AArenaDuelCharacter* Character = Cast<AArenaDuelCharacter>(GetOwner());
	if (!Character) return;
	if (!bAimingState) { bAiming = false; return; }
	if (!Character->IsDead() && Character->GetController() && IsRoundInProgress()
		&& (!Character->GetArenaDuelMovementComponent() || !Character->GetArenaDuelMovementComponent()->IsSprinting())) bAiming = true;
}
void UArenaDuelWeaponComponent::ServerSetFireHeld_Implementation(bool bHeld) { if (bHeld && IsRoundInProgress()) StartAuthoritativeFire(); else StopAuthoritativeFire(); }
bool UArenaDuelWeaponComponent::IsRoundInProgress() const
{
	const UWorld* World = GetWorld();
	const AArenaDuelGameState* GameState = World ? World->GetGameState<AArenaDuelGameState>() : nullptr;
	return !GameState || GameState->IsRoundInProgress();
}
bool UArenaDuelWeaponComponent::HasInfiniteAmmoForDevelopment() const
{
	const AArenaDuelCharacter* Character = Cast<AArenaDuelCharacter>(GetOwner());
	const AArenaDuelPlayerState* PlayerState = Character ? Character->GetPlayerState<AArenaDuelPlayerState>() : nullptr;
	return PlayerState && PlayerState->HasAdminInfiniteAmmo();
}
bool UArenaDuelWeaponComponent::IsLocalAdminMenuOpen() const
{
	const AArenaDuelCharacter* Character = Cast<AArenaDuelCharacter>(GetOwner());
	const AArenaDuelPlayerController* PlayerController = Character && Character->IsLocallyControlled() ? Cast<AArenaDuelPlayerController>(Character->GetController()) : nullptr;
	return PlayerController && PlayerController->IsAdminMenuOpen();
}
bool UArenaDuelWeaponComponent::CanBeginAuthoritativeFire() const
{
	const AArenaDuelCharacter* Character = Cast<AArenaDuelCharacter>(GetOwner());
	return Character && !Character->IsDead() && IsRoundInProgress() && Character->GetController() && GetWorld() && WeaponDefinitions.IsValidIndex(EquippedWeaponIndex) && !bReloading && (GetCurrentMagazineAmmo() > 0 || HasInfiniteAmmoForDevelopment());
}
void UArenaDuelWeaponComponent::StartAuthoritativeFire()
{
	if (bServerFireHeld || !CanBeginAuthoritativeFire()) return;
	bServerFireHeld = true;
	FireAuthoritative();
	if (GetCurrentDefinition().bAutomatic) { const float Interval = 60.0f / FMath::Max(GetCurrentDefinition().RoundsPerMinute, 1.0f); GetWorld()->GetTimerManager().SetTimer(AutomaticFireTimerHandle, this, &UArenaDuelWeaponComponent::FireAuthoritative, Interval, true, Interval); }
}
void UArenaDuelWeaponComponent::StopAuthoritativeFire() { bServerFireHeld = false; if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(AutomaticFireTimerHandle); }
void UArenaDuelWeaponComponent::CancelLocalAndServerFire(bool bClearLocalRecoil)
{
	const bool bWasFireHeld = bFireHeld;
	bFireHeld = false;
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(LocalCosmeticFireTimerHandle);
	if (bClearLocalRecoil)
	{
		LocalWeaponKick = 0.0f;
		RecoilVelocity = 0.0f;
		VisualYawKick = 0.0f;
		VisualYawVelocity = 0.0f;
		LocalRecoilPitchRemaining = 0.0f;
		LocalRecoilYawRemaining = 0.0f;
		LocalRecoilRecoveryTimeRemaining = 0.0f;
		if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(LocalCosmeticRecoveryTimerHandle);
		RefreshWeaponVisual();
	}
	else if (bWasFireHeld && (FMath::Abs(LocalRecoilPitchRemaining) > KINDA_SMALL_NUMBER || FMath::Abs(LocalRecoilYawRemaining) > KINDA_SMALL_NUMBER))
	{
		const float AccumulatedKick = FMath::Max(FMath::Abs(LocalRecoilPitchRemaining), FMath::Abs(LocalRecoilYawRemaining));
		LocalRecoilRecoveryTimeRemaining = FMath::Lerp(0.25f, 0.55f, FMath::Clamp(AccumulatedKick / 10.0f, 0.0f, 1.0f));
		if (GetWorld()) GetWorld()->GetTimerManager().SetTimer(LocalCosmeticRecoveryTimerHandle, this, &UArenaDuelWeaponComponent::RecoverCosmeticKick, 0.02f, true);
	}
	if (GetOwnerRole() == ROLE_Authority) StopAuthoritativeFire();
	else ServerSetFireHeld(false);
}
void UArenaDuelWeaponComponent::ServerRequestReload_Implementation()
{
	const AArenaDuelCharacter* Character = Cast<AArenaDuelCharacter>(GetOwner());
	if (!Character || Character->IsDead() || !IsRoundInProgress()) return;
	FArenaDuelWeaponRuntimeState* State = GetMutableCurrentRuntimeState();
	if (!State || bReloading || State->MagazineAmmo >= GetCurrentDefinition().MagazineCapacity || (State->ReserveAmmo <= 0 && !HasInfiniteAmmoForDevelopment())) return;
	StopAuthoritativeFire();
	bReloading = true;
	FTimerDelegate ReloadDelegate; ReloadDelegate.BindUObject(this, &UArenaDuelWeaponComponent::CompleteReload);
	GetWorld()->GetTimerManager().SetTimer(ReloadTimerHandle, ReloadDelegate, GetCurrentDefinition().ReloadDuration, false);
}
void UArenaDuelWeaponComponent::ServerRequestEquip_Implementation(int32 Index)
{
	const AArenaDuelCharacter* Character = Cast<AArenaDuelCharacter>(GetOwner());
	if (!Character || Character->IsDead() || !IsRoundInProgress()) return;
	if (WeaponDefinitions.IsValidIndex(Index) && !bReloading && Index != EquippedWeaponIndex) { StopAuthoritativeFire(); EquippedWeaponIndex = static_cast<uint8>(Index); OnRep_EquippedWeapon(); }
}

void UArenaDuelWeaponComponent::FireAuthoritative()
{
	if (!IsRoundInProgress()) { StopAuthoritativeFire(); return; }
	if (!bServerFireHeld && GetCurrentDefinition().bAutomatic) return;
	FArenaDuelWeaponRuntimeState* State = GetMutableCurrentRuntimeState();
	const FArenaDuelWeaponDefinition& Definition = GetCurrentDefinition();
	const AArenaDuelCharacter* Character = Cast<AArenaDuelCharacter>(GetOwner());
	const AController* Controller = Character ? Character->GetController() : nullptr;
	if (!State || !GetOwner() || !Character || !Controller || !GetWorld() || Character->IsDead() || bReloading || (State->MagazineAmmo <= 0 && !HasInfiniteAmmoForDevelopment())) return;
	const int32 DefinitionIndex = static_cast<int32>(EquippedWeaponIndex);
	const double ServerTime = GetWorld()->GetTimeSeconds();
	if (!NextAllowedFireServerTimes.IsValidIndex(DefinitionIndex)) NextAllowedFireServerTimes.SetNum(WeaponDefinitions.Num());
	if (NextAllowedFireServerTimes.IsValidIndex(DefinitionIndex) && ServerTime + KINDA_SMALL_NUMBER < NextAllowedFireServerTimes[DefinitionIndex]) return;
	if (!HasInfiniteAmmoForDevelopment()) State->MagazineAmmo--;
	NextAllowedFireServerTimes[DefinitionIndex] = ServerTime + 60.0 / FMath::Max(static_cast<double>(Definition.RoundsPerMinute), 1.0);
	const FVector Origin = Character->GetPawnViewLocation();
	const FVector Direction = Controller->GetControlRotation().Vector();
	FRandomStream Random(++LastShotSequence);
	int32 BodyPellets = 0, HeadPellets = 0, LimbPellets = 0, WorldPellets = 0;
	TArray<FVector_NetQuantize> TraceEnds;
	float ClosestDistance = Definition.Range;
	AActor* LastTarget = nullptr;
	for (int32 Pellet = 0; Pellet < FMath::Max(1, Definition.Pellets); ++Pellet)
	{
		const FVector PelletDirection = Random.VRandCone(Direction, FMath::DegreesToRadians(GetCurrentSpreadDegrees()));
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ArenaDuelWeaponTrace), true, Character);
		const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + PelletDirection * Definition.Range, ECC_Visibility, Params);
		const FVector WorldEnd = bHit ? FVector(Hit.ImpactPoint) : Origin + PelletDirection * Definition.Range;
		// Characters are hit on their animated physics bodies, never on the world trace. The body
		// trace stops at the world impact, so cover and barriers protect whatever is behind them.
		FHitResult BodyHit;
		AArenaDuelCharacter* Victim = nullptr;
		for (TActorIterator<AArenaDuelCharacter> It(GetWorld()); It; ++It)
		{
			FHitResult Candidate;
			if (*It != Character && !It->IsDead() && It->TraceHitZones(Origin, WorldEnd, Candidate) && (!Victim || Candidate.Distance < BodyHit.Distance))
			{
				BodyHit = Candidate;
				Victim = *It;
			}
		}
		if (Victim)
		{
			const EArenaDuelShotResult Zone = ClassifyHitBone(BodyHit.BoneName);
			const float Multiplier = Zone == EArenaDuelShotResult::Head ? Definition.HeadshotMultiplier : Zone == EArenaDuelShotResult::Limb ? Definition.LimbDamageMultiplier : 1.0f;
			Victim->RecordServerHit(BodyHit.ImpactPoint, PelletDirection);
			Victim->ApplyServerDamage(Definition.BodyDamage * Multiplier);
			TraceEnds.Add(BodyHit.ImpactPoint);
			ClosestDistance = FMath::Min(ClosestDistance, BodyHit.Distance);
			LastTarget = Victim;
			if (Zone == EArenaDuelShotResult::Head) ++HeadPellets;
			else if (Zone == EArenaDuelShotResult::Limb) ++LimbPellets;
			else ++BodyPellets;
			continue;
		}
		TraceEnds.Add(WorldEnd);
		if (!bHit) continue;
		if (AArenaDuelArcBarrier* Barrier = Cast<AArenaDuelArcBarrier>(Hit.GetActor()))
		{
			Barrier->ApplyBarrierDamage(Definition.BodyDamage);
			ClosestDistance = FMath::Min(ClosestDistance, FVector::Dist(Origin, Hit.Location));
			LastTarget = Barrier;
			++WorldPellets;
			continue;
		}
		const EArenaDuelShotResult Result = ClassifyHit(Hit);
		if ((Result == EArenaDuelShotResult::Body || Result == EArenaDuelShotResult::Head) && Hit.GetActor() != Character)
		{
			if (AArenaDuelCharacter* Victim = Cast<AArenaDuelCharacter>(Hit.GetActor()))
			{
				const float Damage = Definition.BodyDamage * (Result == EArenaDuelShotResult::Head ? Definition.HeadshotMultiplier : 1.0f);
				Victim->RecordServerHit(Hit.ImpactPoint, PelletDirection);
				Victim->ApplyServerDamage(Damage);
			}
		}
		ClosestDistance = FMath::Min(ClosestDistance, FVector::Dist(Origin, Hit.Location));
		LastTarget = Hit.GetActor();
		if (Result == EArenaDuelShotResult::Head) ++HeadPellets;
		else if (Result == EArenaDuelShotResult::Body) ++BodyPellets;
		else ++WorldPellets;
	}
	LastPelletsHit = BodyPellets + HeadPellets + LimbPellets;
	LastHeadPellets = HeadPellets;
	const EArenaDuelShotResult Aggregate = HeadPellets > 0 ? EArenaDuelShotResult::Head : BodyPellets > 0 ? EArenaDuelShotResult::Body : LimbPellets > 0 ? EArenaDuelShotResult::Limb : WorldPellets > 0 ? EArenaDuelShotResult::World : EArenaDuelShotResult::Miss;
	SetLastShot(Aggregate, ClosestDistance, LastTarget);
	MulticastShotFired(TraceEnds);
}
void UArenaDuelWeaponComponent::MulticastShotFired_Implementation(const TArray<FVector_NetQuantize>& TraceEnds)
{
	UWorld* World = GetWorld();
	const AArenaDuelCharacter* Character = Cast<AArenaDuelCharacter>(GetOwner());
	if (!World || !Character || World->GetNetMode() == NM_DedicatedServer) return;
	// The shooter sees the viewmodel gun, everyone else the world gun.
	const bool bLocalView = Character->IsLocallyControlled();
	const UStaticMeshComponent* Gun = bLocalView ? FirstPersonWeaponMesh : ThirdPersonWeaponMesh;
	FVector Muzzle = Character->GetPawnViewLocation();
	if (Gun && Gun->GetStaticMesh())
	{
		if (Gun->DoesSocketExist(TEXT("Muzzle"))) Muzzle = Gun->GetSocketLocation(TEXT("Muzzle"));
		else
		{
			// Meshes without the socket point +X at the muzzle.
			const FBoxSphereBounds Bounds = Gun->GetStaticMesh()->GetBounds();
			Muzzle = Gun->GetComponentTransform().TransformPosition(Bounds.Origin + FVector(Bounds.BoxExtent.X, 0.0f, 0.0f));
		}
	}
	// The viewmodel is drawn through its own lens and scaled toward the eye, while tracers are world
	// geometry. Map the muzzle to the world point that lands on the same pixel as the drawn barrel tip.
	if (const UCameraComponent* Camera = bLocalView ? Character->GetFirstPersonCamera() : nullptr)
	{
		const FTransform View = Camera->GetComponentTransform();
		const FVector InView = View.InverseTransformPositionNoScale(Muzzle);
		const float Scale = Camera->bEnableFirstPersonScale ? Camera->FirstPersonScale : 1.0f;
		const float Lens = Camera->bEnableFirstPersonFieldOfView
			? FMath::Tan(FMath::DegreesToRadians(Camera->FieldOfView * 0.5f)) / FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(Camera->FirstPersonFieldOfView, 1.0f, 170.0f) * 0.5f))
			: 1.0f;
		Muzzle = View.TransformPositionNoScale(FVector(InView.X * Scale, InView.Y * Scale * Lens, InView.Z * Scale * Lens));
	}	const AArenaDuelPlayerState* PlayerState = Character->GetPlayerState<AArenaDuelPlayerState>();
	UMaterialInterface* Material = PlayerState && PlayerState->GetDuelSlot() == 1 ? WeaponAccentViolet.Get() : WeaponAccentCyan.Get();
	auto ExpireAfter = [World](USceneComponent* Component, float Seconds)
	{
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(Component, [Component]() { Component->DestroyComponent(); }), Seconds, false);
	};
	UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	for (const FVector_NetQuantize& End : TraceEnds)
	{
		const FVector Delta = FVector(End) - Muzzle;
		const double Length = Delta.Size();
		if (!Cylinder || Length < 40.0) continue;
		UStaticMeshComponent* Tracer = NewObject<UStaticMeshComponent>(GetOwner());
		Tracer->SetStaticMesh(Cylinder);
		if (Material) Tracer->SetMaterial(0, Material);
		Tracer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Tracer->SetCastShadow(false);
		Tracer->SetWorldTransform(FTransform(FRotationMatrix::MakeFromZ(Delta).ToQuat(), Muzzle + Delta * 0.5, FVector(0.005, 0.005, Length / 100.0)));
		Tracer->RegisterComponentWithWorld(World);
		ExpireAfter(Tracer, CVarTracerSeconds.GetValueOnGameThread());
	}
	UPointLightComponent* Flash = NewObject<UPointLightComponent>(GetOwner());
	Flash->SetWorldLocation(Muzzle);
	Flash->SetIntensity(6000.0f);
	Flash->SetAttenuationRadius(300.0f);
	Flash->SetLightColor(FLinearColor(1.0f, 0.82f, 0.55f));
	Flash->SetCastShadows(false);
	Flash->RegisterComponentWithWorld(World);
	ExpireAfter(Flash, 0.07f);
	if (FireSound) UGameplayStatics::PlaySoundAtLocation(this, FireSound, Muzzle, Character->IsLocallyControlled() ? 0.55f : 0.9f);
}
void UArenaDuelWeaponComponent::CompleteReload(){
	FArenaDuelWeaponRuntimeState* State = GetMutableCurrentRuntimeState();
	if (const AArenaDuelCharacter* Character = Cast<AArenaDuelCharacter>(GetOwner()); !Character || Character->IsDead() || !IsRoundInProgress()) { bReloading = false; return; }
	if (!State || !bReloading) return;
	const int32 MissingAmmo = FMath::Max(0, GetCurrentDefinition().MagazineCapacity - State->MagazineAmmo);
	const int32 Loaded = HasInfiniteAmmoForDevelopment() ? MissingAmmo : FMath::Min(MissingAmmo, State->ReserveAmmo);
	State->MagazineAmmo += Loaded;
	if (!HasInfiniteAmmoForDevelopment()) State->ReserveAmmo -= Loaded;
	bReloading = false;
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
	if (!bFireHeld || bReloading || (GetCurrentMagazineAmmo() <= 0 && !HasInfiniteAmmoForDevelopment()) || !WeaponDefinitions.IsValidIndex(EquippedWeaponIndex)) return;
	LastCosmeticShotWorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0f;
	const float Kick = WeaponVisualDefinitions.IsValidIndex(EquippedWeaponIndex) ? WeaponVisualDefinitions[EquippedWeaponIndex].VisualRecoilKick : 1.0f;
	LocalWeaponKick = FMath::Min(LocalWeaponKick + Kick, 3.0f);
	VisualYawKick = FMath::Clamp(VisualYawKick + ((LastShotSequence & 1) ? 0.35f : -0.35f) * Kick, -1.5f, 1.5f);
	ApplyLocalRecoil();
	LocalRecoilRecoveryTimeRemaining = 0.0f;
	if (GetWorld()) GetWorld()->GetTimerManager().SetTimer(LocalCosmeticRecoveryTimerHandle, this, &UArenaDuelWeaponComponent::RecoverCosmeticKick, 0.02f, true);
}
void UArenaDuelWeaponComponent::RecoverCosmeticKick()
{
	AArenaDuelCharacter* Character = Cast<AArenaDuelCharacter>(GetOwner());
	const bool bCanRecoverCamera = Character && Character->IsLocallyControlled() && !bFireHeld;
	const float DeltaSeconds = GetWorld() ? FMath::Max(GetWorld()->GetDeltaSeconds(), 0.001f) : 0.02f;
	if (bCanRecoverCamera)
	{
		if (APlayerController* Controller = Cast<APlayerController>(Character->GetController()))
		{
			if (LocalRecoilRecoveryTimeRemaining <= 0.0f
				&& (!FMath::IsNearlyZero(LocalRecoilPitchRemaining) || !FMath::IsNearlyZero(LocalRecoilYawRemaining)))
			{
				const float AccumulatedKick = FMath::Max(FMath::Abs(LocalRecoilPitchRemaining), FMath::Abs(LocalRecoilYawRemaining));
				LocalRecoilRecoveryTimeRemaining = FMath::Lerp(0.25f, 0.55f, FMath::Clamp(AccumulatedKick / 10.0f, 0.0f, 1.0f));
			}
			const float RecoveryAlpha = LocalRecoilRecoveryTimeRemaining > 0.0f
				? FMath::Clamp(DeltaSeconds / LocalRecoilRecoveryTimeRemaining, 0.0f, 1.0f)
				: 1.0f;
			const float PitchStep = LocalRecoilPitchRemaining * RecoveryAlpha;
			const float YawStep = LocalRecoilYawRemaining * RecoveryAlpha;
			Controller->AddPitchInput(PitchStep);
			Controller->AddYawInput(-YawStep);
			LocalRecoilPitchRemaining *= 1.0f - RecoveryAlpha;
			LocalRecoilYawRemaining *= 1.0f - RecoveryAlpha;
			LocalRecoilRecoveryTimeRemaining = FMath::Max(0.0f, LocalRecoilRecoveryTimeRemaining - DeltaSeconds);
		}
	}
	if (!bFireHeld && FMath::IsNearlyZero(LocalWeaponKick, KINDA_SMALL_NUMBER) && FMath::IsNearlyZero(LocalRecoilPitchRemaining, 0.005f) && FMath::IsNearlyZero(LocalRecoilYawRemaining, 0.005f))
	{
		LocalWeaponKick = 0.0f;
		LocalRecoilPitchRemaining = 0.0f;
		LocalRecoilYawRemaining = 0.0f;
		LocalRecoilRecoveryTimeRemaining = 0.0f;
		if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(LocalCosmeticRecoveryTimerHandle);
	}
}
void UArenaDuelWeaponComponent::RefreshWeaponVisual()
{
	auto* Character = Cast<AArenaDuelCharacter>(GetOwner());
	if (!Character || !WeaponVisualDefinitions.IsValidIndex(EquippedWeaponIndex)) return;
	const auto& Visual = WeaponVisualDefinitions[EquippedWeaponIndex];
	UStaticMesh* Mesh = Visual.Mesh.LoadSynchronous();
	FirstPersonWeaponMesh->AttachToComponent(Character->GetFirstPersonArms(), FAttachmentTransformRules::KeepRelativeTransform, TEXT("HandGrip_R"));
	// Arm geometry is intentionally scaled independently from the weapon model.
	// Preserve the per-weapon first-person scale instead of inheriting the arm scale.
	FirstPersonWeaponMesh->SetAbsolute(false, false, true);
	ThirdPersonWeaponMesh->AttachToComponent(Character->GetMesh(), FAttachmentTransformRules::KeepRelativeTransform, TEXT("HandGrip_R"));
	for (auto* WeaponMesh : { FirstPersonWeaponMesh.Get(), ThirdPersonWeaponMesh.Get() })
	{
		// These components are owned by the weapon component, not direct actor defaults.
		// Explicit registration also covers host/client possession arriving after BeginPlay.
		if (!WeaponMesh->IsRegistered() && GetWorld() && !Character->HasAnyFlags(RF_ClassDefaultObject)) WeaponMesh->RegisterComponent();
		WeaponMesh->SetStaticMesh(Mesh);
		const bool bFirstPerson = WeaponMesh == FirstPersonWeaponMesh.Get();
		WeaponMesh->SetRelativeLocation(bFirstPerson ? Visual.FirstPersonGripLocation : Visual.ThirdPersonGripLocation);
		WeaponMesh->SetRelativeRotation(bFirstPerson ? Visual.FirstPersonGripRotation : Visual.ThirdPersonGripRotation);
		WeaponMesh->SetRelativeScale3D(bFirstPerson ? Visual.FirstPersonScale : Visual.ThirdPersonScale);
		if (WeaponBodyMaterial) for (int32 I=0;I<WeaponMesh->GetNumMaterials();++I) WeaponMesh->SetMaterial(I,WeaponBodyMaterial);
	}
	const AArenaDuelPlayerState* Player=Character->GetPlayerState<AArenaDuelPlayerState>();
	UMaterialInterface* Accent=Player && Player->GetCharacterArchetype()==EArenaDuelCharacterArchetype::Warden?WeaponAccentCyan.Get():WeaponAccentViolet.Get();
	if(Accent)
	{
		if(FirstPersonWeaponMesh->GetNumMaterials()>1)FirstPersonWeaponMesh->SetMaterial(1,Accent);
		if(ThirdPersonWeaponMesh->GetNumMaterials()>1)ThirdPersonWeaponMesh->SetMaterial(1,Accent);
	}
	FirstPersonWeaponMesh->SetRelativeScale3D(Visual.FirstPersonScale);
	ThirdPersonWeaponMesh->SetRelativeScale3D(Visual.ThirdPersonScale);
	FirstPersonWeaponMesh->SetVisibility(Character->IsLocallyControlled() && !Character->IsDead());
	ThirdPersonWeaponMesh->SetVisibility(true);
	SetComponentTickEnabled(Character->IsLocallyControlled() && !Character->IsDead());
	// Initialize once. The local presentation tick owns this root after possession.
	if (!bPresentationInitialized && Character->IsLocallyControlled() && Character->GetFirstPersonViewmodelRoot())
	{
		Character->GetFirstPersonViewmodelRoot()->SetRelativeLocation(Visual.HipViewmodelLocation);
		Character->GetFirstPersonViewmodelRoot()->SetRelativeRotation(Visual.HipViewmodelRotation);
		bPresentationInitialized = true;
	}
}

void UArenaDuelWeaponComponent::SetUserHipFOV(float NewFOV)
{
	HipFOV=FMath::Clamp(NewFOV,80.0f,110.0f);
	if(!bAiming)
		if(const AArenaDuelCharacter* Character=Cast<AArenaDuelCharacter>(GetOwner()); Character && Character->IsLocallyControlled() && Character->GetFirstPersonCamera())Character->GetFirstPersonCamera()->SetFieldOfView(HipFOV);
}
void UArenaDuelWeaponComponent::GetCurrentViewmodelBaseTransform(FVector& OutLocation, FRotator& OutRotation) const
{
	const FArenaDuelWeaponDefinition& Definition = GetCurrentDefinition();
	const FArenaDuelWeaponVisualDefinition Visual = WeaponVisualDefinitions.IsValidIndex(EquippedWeaponIndex) ? WeaponVisualDefinitions[EquippedWeaponIndex] : FArenaDuelWeaponVisualDefinition();
	const FVector HipLocation = Visual.HipViewmodelLocation;
	const FRotator HipRotation = Visual.HipViewmodelRotation;
	OutLocation = bAiming ? Definition.AimViewmodelLocation : HipLocation;
	OutRotation = bAiming ? Definition.AimViewmodelRotation : HipRotation;
}
void UArenaDuelWeaponComponent::UpdateAimVisual()
{
	TickLocalPresentation(GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f);
}

void UArenaDuelWeaponComponent::TickLocalPresentation(float DeltaSeconds)
{
	AArenaDuelCharacter* Character = Cast<AArenaDuelCharacter>(GetOwner());
	if (!Character || !Character->IsLocallyControlled() || Character->IsDead() || !GetWorld()
		|| !Character->GetFirstPersonViewmodelRoot() || !Character->GetFirstPersonCamera()
		|| !WeaponVisualDefinitions.IsValidIndex(EquippedWeaponIndex)) return;
	DeltaSeconds = FMath::Clamp(DeltaSeconds, 0.0f, 0.25f);
	if (DeltaSeconds <= 0.0f) return;
	const FArenaDuelWeaponVisualDefinition& Visual = WeaponVisualDefinitions[EquippedWeaponIndex];
	const UArenaDuelCharacterMovementComponent* Movement = Character->GetArenaDuelMovementComponent();
	const bool bSprint = Movement && Movement->IsSprinting();
	const bool bFalling = Movement && Movement->IsFalling();
	const float AimTarget = bAiming && !bSprint ? 1.0f : 0.0f;
	const float AimDecay = FMath::Exp(-FMath::Max(ViewmodelFeel.AimResponse, 0.01f) * DeltaSeconds);
	const float MotionDecay = FMath::Exp(-FMath::Max(ViewmodelFeel.MotionResponse, 0.01f) * DeltaSeconds);
	AimBlend = AimTarget + (AimBlend - AimTarget) * AimDecay;
	SprintBlend = (bSprint ? 1.0f : 0.0f) + (SprintBlend - (bSprint ? 1.0f : 0.0f)) * MotionDecay;
	Character->GetFirstPersonCamera()->SetFieldOfView(FMath::Lerp(HipFOV, GetCurrentDefinition().AimFOV, AimBlend));

	const FRotator ControlRotation = Character->GetControlRotation();
	const float YawRate = bHadControlRotation ? FMath::Clamp(FRotator::NormalizeAxis(ControlRotation.Yaw - PreviousControlRotation.Yaw) / DeltaSeconds, -720.0f, 720.0f) : 0.0f;
	const float PitchRate = bHadControlRotation ? FMath::Clamp(FRotator::NormalizeAxis(ControlRotation.Pitch - PreviousControlRotation.Pitch) / DeltaSeconds, -720.0f, 720.0f) : 0.0f;
	PreviousControlRotation = ControlRotation;
	bHadControlRotation = true;
	const float MotionWeight = FMath::Lerp(1.0f, ViewmodelFeel.AdsMotionMultiplier, AimBlend);
	const float SwayGain = FMath::Max(ViewmodelFeel.SwayDegreesPerDegreePerSecond, 0.0f) * MotionWeight;
	const float MaxSway = FMath::Max(ViewmodelFeel.MaxSwayDegrees, 0.0f);
	StepSpring(SwayYaw, SwayYawVelocity, FMath::Clamp(-YawRate * SwayGain, -MaxSway, MaxSway), ViewmodelFeel.MotionResponse, DeltaSeconds);
	StepSpring(SwayPitch, SwayPitchVelocity, FMath::Clamp(-PitchRate * SwayGain, -MaxSway, MaxSway), ViewmodelFeel.MotionResponse, DeltaSeconds);
	StepSpring(LocalWeaponKick, RecoilVelocity, 0.0f, ViewmodelFeel.RecoilResponse, DeltaSeconds);
	StepSpring(VisualYawKick, VisualYawVelocity, 0.0f, ViewmodelFeel.RecoilResponse, DeltaSeconds);

	const FVector LocalVelocity = Character->GetActorRotation().UnrotateVector(Character->GetVelocity());
	const float Speed = FVector2D(LocalVelocity.X, LocalVelocity.Y).Size();
	const float MoveTarget = FMath::Clamp(Speed / (bSprint ? 900.0f : 600.0f), 0.0f, 1.0f);
	BobBlend = MoveTarget + (BobBlend - MoveTarget) * MotionDecay;
	BobPhase = FMath::Fmod(BobPhase + Speed * DeltaSeconds * 0.025f, 2.0f * PI);
	IdlePhase = FMath::Fmod(IdlePhase + DeltaSeconds * 1.6f, 2.0f * PI);
	if (bWasFalling && !bFalling)
	{
		LandingVelocity -= FMath::Clamp(-PreviousVerticalVelocity / 650.0f, 0.0f, 1.0f) * ViewmodelFeel.LandingKick * 18.0f;
	}
	else if (!bWasFalling && bFalling)
	{
		LandingVelocity += ViewmodelFeel.LandingKick * 4.0f;
	}
	bWasFalling = bFalling;
	PreviousVerticalVelocity = Character->GetVelocity().Z;
	StepSpring(LandingOffset, LandingVelocity, 0.0f, ViewmodelFeel.MotionResponse, DeltaSeconds);

	// One short camera trace at most every 50 ms. It only moves the cosmetic root.
	WallTraceTime -= DeltaSeconds;
	if (WallTraceTime <= 0.0f)
	{
		WallTraceTime = 0.05f;
		const FVector Eye = Character->GetFirstPersonCamera()->GetComponentLocation();
		const FVector End = Eye + Character->GetFirstPersonCamera()->GetForwardVector() * 110.0f;
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ArenaDuelViewmodelWall), false, Character);
		WallTarget = GetWorld()->LineTraceSingleByChannel(Hit, Eye, End, ECC_Visibility, Params)
			? FMath::Clamp(1.0f - Hit.Distance / 110.0f, 0.0f, 1.0f) : 0.0f;
	}
	WallBlend = WallTarget + (WallBlend - WallTarget) * MotionDecay;
	EquipDrop *= MotionDecay;
	const float BobAmount = BobBlend * FMath::Lerp(ViewmodelFeel.MovementBob, ViewmodelFeel.SprintBob, SprintBlend) * MotionWeight;
	const float Breath = ViewmodelFeel.IdleBreathing * (1.0f - BobBlend) * MotionWeight;
	const FVector Bob(0.0f, FMath::Sin(BobPhase) * BobAmount * 0.5f,
		FMath::Abs(FMath::Cos(BobPhase)) * BobAmount + FMath::Sin(IdlePhase) * Breath);
	const FVector MotionLocation(-5.0f * LocalWeaponKick - ViewmodelFeel.WallPushback * WallBlend,
		-SwayYaw * 0.3f + FMath::Clamp(LocalVelocity.Y / 900.0f, -1.0f, 1.0f) * MotionWeight,
		Bob.Z + LandingOffset - ViewmodelFeel.SprintLowering * SprintBlend - 10.0f * EquipDrop - 8.0f * WallBlend);
	const FVector BaseLocation = FMath::Lerp(Visual.HipViewmodelLocation, GetCurrentDefinition().AimViewmodelLocation, AimBlend);
	const FRotator BaseRotation = FMath::Lerp(Visual.HipViewmodelRotation, GetCurrentDefinition().AimViewmodelRotation, AimBlend);
	const FRotator MotionRotation(SwayPitch - 1.5f * LocalWeaponKick + 5.0f * SprintBlend,
		SwayYaw + VisualYawKick,
		-SwayYaw * 0.35f + FMath::Sin(BobPhase) * BobAmount * 0.3f);
	const FVector FinalLocation = BaseLocation + MotionLocation + FVector(0.0f, Bob.Y, 0.0f);
	const FRotator FinalRotation = BaseRotation + MotionRotation;
	if (!FinalLocation.ContainsNaN() && !FinalRotation.ContainsNaN())
	{
		Character->GetFirstPersonViewmodelRoot()->SetRelativeLocation(FinalLocation);
		Character->GetFirstPersonViewmodelRoot()->SetRelativeRotation(FinalRotation);
	}
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
void UArenaDuelWeaponComponent::OnRep_EquippedWeapon()
{
	if (const AArenaDuelCharacter* Character = Cast<AArenaDuelCharacter>(GetOwner()); Character && Character->IsLocallyControlled()) EquipDrop = 1.0f;
	RefreshWeaponVisual();
}
