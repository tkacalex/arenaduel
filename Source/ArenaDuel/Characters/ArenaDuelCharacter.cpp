// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArenaDuelCharacter.h"
#include "ArenaDuelCharacterMovementComponent.h"
#include "ArenaDuelVisualAnimInstance.h"
#include "../Weapons/ArenaDuelWeaponComponent.h"

#include "../ArenaDuel.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraTypes.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInterface.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Engine/LocalPlayer.h"
#include "../Player/ArenaDuelPlayerState.h"
#include "../Player/ArenaDuelPlayerController.h"
#include "../Game/ArenaDuelGameMode.h"
#include "../Game/ArenaDuelGameState.h"
#include "../Combat/ArenaDuelAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "InputCoreTypes.h"
#include "GameplayEffect.h"
#include "Net/UnrealNetwork.h"
#include "Components/PointLightComponent.h"
#include "UObject/ConstructorHelpers.h"

AArenaDuelCharacter::AArenaDuelCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UArenaDuelCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0.0f, 0.0f, 64.0f));
	FirstPersonCamera->bUsePawnControlRotation = true;
	FirstPersonViewmodelRoot = CreateDefaultSubobject<USceneComponent>(TEXT("FirstPersonViewmodelRoot"));
	FirstPersonViewmodelRoot->SetupAttachment(FirstPersonCamera);
	FirstPersonArms = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonArms"));
	FirstPersonArms->SetupAttachment(FirstPersonViewmodelRoot);
	FirstPersonArms->SetOnlyOwnerSee(true);
	FirstPersonArms->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	FirstPersonArms->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FirstPersonArms->SetCastShadow(false);
	// Use forearms and hands only, with the elbow cut kept outside the viewmodel frame.
	FirstPersonArms->SetRelativeLocation(FVector(20.0f, 0.0f, -90.0f));
	// Match Manny's measured +Y body basis to the camera's +X forward axis.
	FirstPersonArms->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	// Keep both gloved hands and the forearms prominent in the first-person view.
	FirstPersonArms->SetRelativeScale3D(FVector(0.70f));
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Manny(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"));
	GetMesh()->SetSkeletalMesh(Manny.Object);
	GetMesh()->SetRelativeLocation(FVector(0, 0, -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()));
	GetMesh()->SetRelativeRotation(GetThirdPersonMeshBaseRotation());
	LivingMeshRelativeLocation = GetMesh()->GetRelativeLocation();
	LivingMeshRelativeRotation = GetMesh()->GetRelativeRotation();
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetAnimInstanceClass(UArenaDuelVisualAnimInstance::StaticClass());
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Arms(TEXT("/Game/ArenaDuel/Characters/Common/SKM_ArenaDuelFPSArms"));
	if (Arms.Succeeded()) FirstPersonArms->SetSkeletalMesh(Arms.Object);
	FirstPersonArms->SetAnimInstanceClass(UArenaDuelVisualAnimInstance::StaticClass());
	FirstPersonCamera->bEnableFirstPersonFieldOfView = true;
	FirstPersonCamera->bEnableFirstPersonScale = true;
	FirstPersonCamera->FirstPersonFieldOfView = 94.0f;
	FirstPersonCamera->FirstPersonScale = 0.82f;
	for (const TCHAR* Path : { TEXT("/Game/ArenaDuel/Characters/Common/M_ShadowArmor"), TEXT("/Game/ArenaDuel/Characters/Common/M_WardenArmor"), TEXT("/Game/ArenaDuel/Characters/Common/M_RiftArmor") })
		ArchetypeArmorMaterials.Add(LoadObject<UMaterialInterface>(nullptr, Path));
	CyanVisualMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/ArenaDuel/Characters/Common/M_ArcaneCyan"));
	VioletVisualMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/ArenaDuel/Characters/Common/M_ArcaneViolet"));

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
	WeaponComponent = CreateDefaultSubobject<UArenaDuelWeaponComponent>(TEXT("WeaponComponent"));
	BodyHitZone = CreateDefaultSubobject<UBoxComponent>(TEXT("BodyHitZone"));
	BodyHitZone->SetupAttachment(GetCapsuleComponent());
	BodyHitZone->SetRelativeLocation(FVector(0.0f, 0.0f, -16.0f));
	BodyHitZone->SetBoxExtent(FVector(38.0f, 38.0f, 70.0f));
	// Superseded by per-bone hit zones on the world mesh. Kept as a disabled component so saved assets still load.
	BodyHitZone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyHitZone->SetCollisionResponseToAllChannels(ECR_Ignore);
	BodyHitZone->ComponentTags.Add(TEXT("BodyHitZone"));
	HeadHitZone = CreateDefaultSubobject<UBoxComponent>(TEXT("HeadHitZone"));
	HeadHitZone->SetupAttachment(GetCapsuleComponent());
	HeadHitZone->SetRelativeLocation(FVector(0.0f, 0.0f, 68.0f));
	HeadHitZone->SetBoxExtent(FVector(24.0f, 24.0f, 14.0f));
	HeadHitZone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeadHitZone->SetCollisionResponseToAllChannels(ECR_Ignore);
	HeadHitZone->ComponentTags.Add(TEXT("HeadHitZone"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	LeftShoulderArmor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftShoulderArmor"));
	RightShoulderArmor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightShoulderArmor"));
	LeftShoulderArmor->SetupAttachment(GetMesh(), TEXT("upperarm_l"));
	RightShoulderArmor->SetupAttachment(GetMesh(), TEXT("upperarm_r"));
	for (auto* Plate : {LeftShoulderArmor.Get(), RightShoulderArmor.Get()})
	{
		Plate->SetStaticMesh(CubeMesh.Object);
		Plate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Plate->SetOwnerNoSee(true);
		Plate->SetRelativeScale3D(FVector(0.20f, 0.24f, 0.10f));
		Plate->SetRelativeLocation(FVector(4, 0, 6));
	}
	BodyVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyVisual"));
	BodyVisual->SetupAttachment(GetCapsuleComponent());
	BodyVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyVisual->SetOwnerNoSee(true);
	BodyVisual->SetRelativeLocation(FVector(0.0f, 0.0f, -16.0f));
	BodyVisual->SetRelativeScale3D(FVector(0.76f, 0.76f, 1.4f));
	if (CubeMesh.Succeeded()) BodyVisual->SetStaticMesh(CubeMesh.Object);
	HeadVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadVisual"));
	HeadVisual->SetupAttachment(GetCapsuleComponent());
	HeadVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeadVisual->SetOwnerNoSee(true);
	HeadVisual->SetRelativeLocation(FVector(0.0f, 0.0f, 68.0f));
	HeadVisual->SetRelativeScale3D(FVector(0.48f));
	if (SphereMesh.Succeeded()) HeadVisual->SetStaticMesh(SphereMesh.Object);
	// Retained as deprecated development components for existing asset compatibility, never rendered.
	BodyVisual->SetHiddenInGame(true);
	HeadVisual->SetHiddenInGame(true);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	bReplicates = true;
	bAlwaysRelevant = true;
	// Two pawns cost almost nothing to send, and steady updates keep fast strafes in sync.
	SetNetUpdateFrequency(100.0f);
	SetMinNetUpdateFrequency(60.0f);
}

void AArenaDuelCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (FirstPersonCamera)
	{
		CameraBaseRelativeLocation = FirstPersonCamera->GetRelativeLocation();
	}
	RefreshCharacterVisuals();
	// A duel has two pawns, so each must replicate shots and death to the other from any position.
	if (HasAuthority()) bAlwaysRelevant = true;
}

void AArenaDuelCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	// The capsule shrinks instantly; keep the eye where it was and ease it down instead of snapping.
	if (IsLocallyControlled())
	{
		CrouchEyeOffset += ScaledHalfHeightAdjust;
	}
}

void AArenaDuelCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	if (IsLocallyControlled())
	{
		CrouchEyeOffset -= ScaledHalfHeightAdjust;
	}
}

void AArenaDuelCharacter::UpdateLocalMovementCamera(float DeltaSeconds)
{
	if (!FirstPersonCamera || !IsLocallyControlled() || bDead)
	{
		return;
	}
	const float Dt = FMath::Clamp(DeltaSeconds, 0.0f, 0.1f);
	const UArenaDuelCharacterMovementComponent* Movement = GetArenaDuelMovementComponent();
	const bool bSliding = Movement && Movement->IsSliding();

	// Exponential smoothing is frame-rate independent and never overshoots.
	CrouchEyeOffset *= FMath::Exp(-CrouchCameraInterpSpeed * Dt);
	if (FMath::Abs(CrouchEyeOffset) < 0.01f)
	{
		CrouchEyeOffset = 0.0f;
	}
	const float SlideTarget = bSliding ? 1.0f : 0.0f;
	SlideCameraBlend = SlideTarget + (SlideCameraBlend - SlideTarget) * FMath::Exp(-SlideCameraInterpSpeed * Dt);

	float SteerInput = 0.0f;
	if (bSliding && Controller)
	{
		const FVector InputDirection = Movement->GetCurrentAcceleration().GetSafeNormal2D();
		const FVector ViewRight = FRotationMatrix(FRotator(0.0f, Controller->GetControlRotation().Yaw, 0.0f)).GetUnitAxis(EAxis::Y);
		SteerInput = FVector::DotProduct(InputDirection, ViewRight);
	}
	const float RollTarget = SlideCameraBlend * (-SlideCameraBaseRoll + SteerInput * SlideCameraSteerRoll);
	SlideCameraRoll = RollTarget + (SlideCameraRoll - RollTarget) * FMath::Exp(-SlideCameraInterpSpeed * Dt);

	FVector CameraLocation = CameraBaseRelativeLocation;
	CameraLocation.Z += CrouchEyeOffset - SlideCameraExtraDrop * SlideCameraBlend;
	FirstPersonCamera->SetRelativeLocation(CameraLocation);
}

void AArenaDuelCharacter::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
	UpdateLocalMovementCamera(DeltaTime);
	Super::CalcCamera(DeltaTime, OutResult);
	if (IsLocallyControlled() && !bDead)
	{
		OutResult.Rotation.Roll += SlideCameraRoll;
		OutResult.FOV += SlideCameraFOVKick * SlideCameraBlend;
		// A short, small flinch when hit. View only, the aim direction is untouched.
		const float Flash = GetDamageFlash();
		OutResult.Rotation.Roll += DamagePunchSign * 1.1f * LastDamageStrength * Flash * Flash;
		OutResult.Rotation.Pitch += 0.7f * LastDamageStrength * Flash * Flash;
	}
}

void AArenaDuelCharacter::RefreshCharacterVisuals()
{
	BodyVisual->SetHiddenInGame(true);
	HeadVisual->SetHiddenInGame(true);
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;
	// A corpse keeps the ragdoll collision it was given at death.
	if (!bDead) ConfigureHitZoneCollision();
	UpdateEnemyGlow();
	FirstPersonArms->SetVisibility(IsLocallyControlled() && !bDead);
	// Derived arm-only geometry retains the Epic skeleton, animation and grip socket.
	const auto* State = GetPlayerState<AArenaDuelPlayerState>();
	const EArenaDuelCharacterArchetype Archetype = State ? State->GetCharacterArchetype() : EArenaDuelCharacterArchetype::Shadow;
	const int32 Index = static_cast<int32>(Archetype);
	UMaterialInterface* Accent = Archetype == EArenaDuelCharacterArchetype::Warden ? CyanVisualMaterial.Get() : VioletVisualMaterial.Get();
	LeftShoulderArmor->SetVisibility(Archetype == EArenaDuelCharacterArchetype::Warden);
	RightShoulderArmor->SetVisibility(Archetype != EArenaDuelCharacterArchetype::Shadow);
	for (auto* Plate : {LeftShoulderArmor.Get(), RightShoulderArmor.Get()}) Plate->SetMaterial(0, Accent);
	// Keep the local arms' authored skin/glove materials. Archetype armor belongs
	// to the world character; applying it to every arm section made hands read as
	// a single reflective shoulder mass in first person.
	if (ArchetypeArmorMaterials.IsValidIndex(Index))
		for (int32 MaterialIndex=0; MaterialIndex<GetMesh()->GetNumMaterials(); ++MaterialIndex) GetMesh()->SetMaterial(MaterialIndex,ArchetypeArmorMaterials[Index]);
	if (WeaponComponent) WeaponComponent->RefreshWeaponVisual();
	if (bDead) ApplyDevelopmentDeathPose();
}

FRotator AArenaDuelCharacter::GetThirdPersonMeshBaseRotation() const
{
	// Manny's authored forward axis is +Y while the character/camera forward is +X.
	// Keep actor/controller yaw authoritative and rotate only the visual basis.
	return FRotator(0.0f, -90.0f, 0.0f);
}

void AArenaDuelCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	InitializeAbilityActorInfo();
	RefreshCharacterVisuals();
}

void AArenaDuelCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	InitializeAbilityActorInfo();
	RefreshCharacterVisuals();
}

UAbilitySystemComponent* AArenaDuelCharacter::GetAbilitySystemComponent() const
{
	const AArenaDuelPlayerState* State = GetPlayerState<AArenaDuelPlayerState>();
	return State ? State->GetAbilitySystemComponent() : nullptr;
}

void AArenaDuelCharacter::InitializeAbilityActorInfo()
{
	if (AArenaDuelPlayerState* State = GetPlayerState<AArenaDuelPlayerState>())
	{
		if (UAbilitySystemComponent* ASC = State->GetAbilitySystemComponent())
		{
			ASC->InitAbilityActorInfo(State, this);
			State->GrantCharacterAbilities();
			if (HasAuthority() && State->GetArenaDuelAttributes())
			{
				State->GetArenaDuelAttributes()->SetMaxHealth(100.0f);
				State->GetArenaDuelAttributes()->SetHealth(100.0f);
			}
		}
	}
	if (const AArenaDuelGameState* MatchState = GetWorld() ? GetWorld()->GetGameState<AArenaDuelGameState>() : nullptr) SetRoundInputLocked(!MatchState->IsRoundInProgress());
}

float AArenaDuelCharacter::GetHealth() const
{
	const AArenaDuelPlayerState* State = GetPlayerState<AArenaDuelPlayerState>();
	return State && State->GetArenaDuelAttributes() ? State->GetArenaDuelAttributes()->GetHealth() : 100.0f;
}

float AArenaDuelCharacter::GetMaxHealth() const
{
	const AArenaDuelPlayerState* State = GetPlayerState<AArenaDuelPlayerState>();
	return State && State->GetArenaDuelAttributes() ? State->GetArenaDuelAttributes()->GetMaxHealth() : 100.0f;
}

void AArenaDuelCharacter::ApplyServerDamage(float DamageAmount)
{
	if (!HasAuthority() || bDead || DamageAmount <= 0.0f) return;
	if (const AArenaDuelGameState* State = GetWorld() ? GetWorld()->GetGameState<AArenaDuelGameState>() : nullptr; State && !State->IsRoundInProgress()) return;
	const AArenaDuelPlayerState* ArenaPlayerState = GetPlayerState<AArenaDuelPlayerState>();
	if (ArenaPlayerState && ArenaPlayerState->HasAdminGodMode()) return;
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC) return;
	UGameplayEffect* DamageEffect = NewObject<UGameplayEffect>(GetTransientPackage(), TEXT("ArenaDuelDamageEffect"));
	DamageEffect->DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UArenaDuelAttributeSet::GetHealthAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(-DamageAmount));
	DamageEffect->Modifiers.Add(Modifier);
	ASC->ApplyGameplayEffectToSelf(DamageEffect, 1.0f, ASC->MakeEffectContext());
	ClientNotifyDamaged(DamageAmount);
	if (GetHealth() <= 0.0f) HandleDeath();
}

void AArenaDuelCharacter::AdminSetHealth(float NewHealth)
{
	if (!HasAuthority() || bDead) return;
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		const float ClampedHealth = FMath::Clamp(NewHealth, 0.0f, GetMaxHealth());
		ASC->SetNumericAttributeBase(UArenaDuelAttributeSet::GetHealthAttribute(), ClampedHealth);
		if (ClampedHealth <= 0.0f) HandleDeath();
	}
}

void AArenaDuelCharacter::AdminKill()
{
	if (!HasAuthority() || bDead) return;
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		ASC->SetNumericAttributeBase(UArenaDuelAttributeSet::GetHealthAttribute(), 0.0f);
	}
	HandleDeath();
}

void AArenaDuelCharacter::AdminResetPlayer()
{
	if (!HasAuthority() || bDead) return;
	AdminSetHealth(GetMaxHealth());
	if (WeaponComponent)
	{
		WeaponComponent->CancelCombatActions();
		WeaponComponent->RefillAllAmmoForDevelopment();
	}
	if (UArenaDuelCharacterMovementComponent* Movement = GetArenaDuelMovementComponent())
	{
		Movement->ResetMovementIntentForDevelopment();
		Movement->RefillStaminaForDevelopment();
	}
}

void AArenaDuelCharacter::PlayShadowStepCameraImpulse()
{
	if (!IsLocallyControlled() || !FirstPersonCamera || !GetWorld() || bDead) return;
	GetWorldTimerManager().ClearTimer(ShadowStepCameraTimer);
	ShadowStepCameraBaseRotation = FirstPersonCamera->GetRelativeRotation();
	ShadowStepCameraStartTime = GetWorld()->GetTimeSeconds();
	GetWorldTimerManager().SetTimer(ShadowStepCameraTimer, this, &AArenaDuelCharacter::UpdateShadowStepCameraImpulse, 0.02f, true);
	UpdateShadowStepCameraImpulse();
}

void AArenaDuelCharacter::PlayRiftCameraImpulse()
{
	// Cosmetic roll only: reuse the bounded impulse without touching ADS FOV or recoil.
	if (FirstPersonCamera && GetWorld() && GetWorldTimerManager().IsTimerActive(ShadowStepCameraTimer))
		FirstPersonCamera->SetRelativeRotation(ShadowStepCameraBaseRotation);
	PlayShadowStepCameraImpulse();
}

void AArenaDuelCharacter::UpdateShadowStepCameraImpulse()
{
	if (!FirstPersonCamera || !GetWorld()) return;
	constexpr float ImpulseDuration = 0.16f;
	const float Alpha = FMath::Clamp((GetWorld()->GetTimeSeconds() - ShadowStepCameraStartTime) / ImpulseDuration, 0.0f, 1.0f);
	const float Recovery = 1.0f - FMath::SmoothStep(0.0f, 1.0f, Alpha);
	FirstPersonCamera->SetRelativeRotation(ShadowStepCameraBaseRotation + FRotator(0.0f, 0.0f, -2.0f * Recovery));
	if (Alpha >= 1.0f)
	{
		FirstPersonCamera->SetRelativeRotation(ShadowStepCameraBaseRotation);
		GetWorldTimerManager().ClearTimer(ShadowStepCameraTimer);
	}
}

void AArenaDuelCharacter::HandleDeath()
{
	if (bDead) return;
	bDead = true;
	SetDeadState();
	if (HasAuthority() && GetWorld())
	{
		if (AArenaDuelGameMode* ArenaGameMode = GetWorld()->GetAuthGameMode<AArenaDuelGameMode>())
		{
			ArenaGameMode->HandlePlayerDeath(this);
		}
	}
}

void AArenaDuelCharacter::SetDeadState()
{
	if (WeaponComponent) WeaponComponent->CancelCombatActions();
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent()) ASC->CancelAllAbilities();
	ApplyDevelopmentDeathPose();
	StartLocalDeathCamera();
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->StopMovementImmediately();
		GetCharacterMovement()->DisableMovement();
		// Network smoothing rewrites the mesh offset on proxies and on the listen server, which
		// stood corpses back up. The pawn is destroyed on respawn, so movement can stay off.
		GetCharacterMovement()->NetworkSmoothingMode = ENetworkSmoothingMode::Disabled;
		GetCharacterMovement()->SetComponentTickEnabled(false);
	}
}

bool AArenaDuelCharacter::CanProcessGameplayInput() const
{
	if (!IsLocallyControlled() || bDead) return false;
	if (const AArenaDuelPlayerController* PlayerController = Cast<AArenaDuelPlayerController>(GetController()); PlayerController && (PlayerController->IsAdminMenuOpen() || PlayerController->IsPlayerMenuOpen())) return false;
	const AArenaDuelGameState* GameState = GetWorld() ? GetWorld()->GetGameState<AArenaDuelGameState>() : nullptr;
	return !GameState || GameState->CanLivingCharacterMove(this);
}

void AArenaDuelCharacter::SetRoundInputLocked(bool bLocked)
{
	if (bLocked)
	{
		if (const AArenaDuelGameState* State = GetWorld() ? GetWorld()->GetGameState<AArenaDuelGameState>() : nullptr;
			State && State->CanLivingCharacterMove(this)) bLocked = false;
	}
	if (!bLocked)
	{
		// GameState and the fresh pawn may arrive in either replication order.
		if (!bDead && GetCharacterMovement() && GetCharacterMovement()->MovementMode == MOVE_None) GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		return;
	}
	if (WeaponComponent) WeaponComponent->CancelCombatActions();
	// End root-motion and wind-up tasks before disabling movement during a round break.
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent()) ASC->CancelAllAbilities();
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}
}

void AArenaDuelCharacter::StartLocalDeathCamera()
{
	if (!IsLocallyControlled() || !FirstPersonCamera || !GetWorld())
	{
		return;
	}

	FirstPersonCamera->bUsePawnControlRotation = false;
	GetWorldTimerManager().ClearTimer(ShadowStepCameraTimer);
	LocalDeathCameraStartLocation = FirstPersonCamera->GetRelativeLocation();
	LocalDeathCameraStartRotation = FirstPersonCamera->GetRelativeRotation();
	LocalDeathCameraStartTime = GetWorld()->GetTimeSeconds();
	GetWorldTimerManager().SetTimer(LocalDeathCameraTimer, this, &AArenaDuelCharacter::UpdateLocalDeathCamera, 0.02f, true);
	UpdateLocalDeathCamera();
}

void AArenaDuelCharacter::UpdateLocalDeathCamera()
{
	if (!FirstPersonCamera || !GetWorld())
	{
		return;
	}

	constexpr float DeathCameraDuration = 0.38f;
	const float Alpha = FMath::Clamp((GetWorld()->GetTimeSeconds() - LocalDeathCameraStartTime) / DeathCameraDuration, 0.0f, 1.0f);
	const float SmoothAlpha = FMath::SmoothStep(0.0f, 1.0f, Alpha);
	const FVector DeathLocation(0.0f, 15.0f, -52.0f);
	const FRotator DeathRotation(-10.0f, 0.0f, 80.0f);

	FirstPersonCamera->SetRelativeLocation(FMath::Lerp(LocalDeathCameraStartLocation, DeathLocation, SmoothAlpha));
	FirstPersonCamera->SetRelativeRotation(FMath::Lerp(LocalDeathCameraStartRotation, DeathRotation, SmoothAlpha));

	if (Alpha >= 1.0f)
	{
		FirstPersonCamera->SetRelativeLocation(DeathLocation);
		FirstPersonCamera->SetRelativeRotation(DeathRotation);
		GetWorldTimerManager().ClearTimer(LocalDeathCameraTimer);
	}
}

void AArenaDuelCharacter::ClientNotifyDamaged_Implementation(float DamageAmount)
{
	LastDamageWorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	LastDamageStrength = FMath::Clamp(DamageAmount / 40.0f, 0.35f, 1.0f);
	DamagePunchSign = -DamagePunchSign;
}

float AArenaDuelCharacter::GetDamageFlash() const
{
	constexpr float FlashSeconds = 0.3f;
	const float Age = GetWorld() ? GetWorld()->GetTimeSeconds() - LastDamageWorldTime : FlashSeconds;
	return FMath::Clamp(1.0f - Age / FlashSeconds, 0.0f, 1.0f);
}

void AArenaDuelCharacter::UpdateEnemyGlow()
{
	// Only a living opponent glows, and only on the machine that looks at them. The lights and the
	// body share lighting channel 1, which nothing else uses, so walls and floor stay untouched and
	// the glow cannot leak around cover.
	const bool bGlow = !bDead && !IsLocallyControlled() && GetNetMode() != NM_DedicatedServer && GetWorld() && GetWorld()->IsGameWorld();
	if (bGlow && EnemyGlowLights.Num() == 0)
	{
		GetMesh()->SetLightingChannels(true, true, false);
		for (const FVector& Offset : { FVector(70.0f, 0.0f, 30.0f), FVector(-70.0f, 0.0f, 30.0f), FVector(0.0f, 70.0f, 30.0f), FVector(0.0f, -70.0f, 30.0f) })
		{
			UPointLightComponent* Light = NewObject<UPointLightComponent>(this);
			Light->SetupAttachment(GetCapsuleComponent());
			Light->SetRelativeLocation(Offset);
			Light->SetLightingChannels(false, true, false);
			Light->SetLightColor(FLinearColor(1.0f, 0.08f, 0.05f));
			Light->SetIntensity(EnemyGlowIntensity);
			Light->SetAttenuationRadius(220.0f);
			Light->SetCastShadows(false);
			Light->RegisterComponent();
			EnemyGlowLights.Add(Light);
		}
	}
	for (UPointLightComponent* Light : EnemyGlowLights)
	{
		if (Light) Light->SetVisibility(bGlow);
	}
}

void AArenaDuelCharacter::ConfigureHitZoneCollision()
{
	// The physics asset bodies of the world mesh are the hit zones. They answer no collision channel,
	// so nothing blocks or overlaps them; weapons query them directly through TraceHitZones. The pose
	// must keep updating where nobody renders the body, or the server would test a frozen pose.
	USkeletalMeshComponent* Body = GetMesh();
	BodyHitZone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeadHitZone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetCollisionObjectType(ECC_Pawn);
	Body->SetCollisionResponseToAllChannels(ECR_Ignore);
	Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Body->SetGenerateOverlapEvents(false);
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Body->bEnableUpdateRateOptimizations = false;
}

bool AArenaDuelCharacter::TraceHitZones(const FVector& Start, const FVector& End, FHitResult& OutHit) const
{
	USkeletalMeshComponent* Body = GetMesh();
	if (bDead || !Body || !Body->GetPhysicsAsset()) return false;
	return Body->LineTraceComponent(OutHit, Start, End, FCollisionQueryParams(SCENE_QUERY_STAT(ArenaDuelHitZones), false));
}

void AArenaDuelCharacter::RecordServerHit(const FVector& WorldLocation, const FVector& Direction)
{
	if (!HasAuthority() || bDead) return;
	DeathHitLocation = WorldLocation;
	DeathHitDirection = Direction.GetSafeNormal();
}

void AArenaDuelCharacter::ApplyDevelopmentDeathPose()
{
	FirstPersonArms->SetVisibility(false);
	if (bDeathPresentationLatched) return;
	bDeathPresentationLatched = true;
	if (WeaponComponent) WeaponComponent->RefreshWeaponVisual();
	UpdateEnemyGlow();
	StartDeathRagdoll();
}

void AArenaDuelCharacter::StartDeathRagdoll()
{
	// The upright capsule and hit boxes would block the survivor and keep taking hits for a body on the floor.
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyHitZone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeadHitZone->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Cosmetic and simulated locally on every machine that draws the body. The pawn is destroyed and
	// respawned at the next round, which is the only reset the corpse needs.
	USkeletalMeshComponent* Body = GetMesh();
	if (!Body || !Body->GetPhysicsAsset() || GetNetMode() == NM_DedicatedServer) return;
	const FVector CarriedVelocity = GetVelocity().GetClampedToMaxSize(600.0f);
	Body->bPauseAnims = false;
	Body->SetCollisionProfileName(TEXT("Ragdoll"));
	Body->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	Body->SetAllBodiesSimulatePhysics(true);
	Body->SetSimulatePhysics(true);
	Body->WakeAllRigidBodies();
	Body->bBlendPhysics = true;
	Body->SetAllPhysicsLinearVelocity(CarriedVelocity);
	// One push at the wound in the shot direction. Kills without a recorded hit simply collapse.
	constexpr float HitImpulse = 3500.0f;
	if (!FVector(DeathHitDirection).IsNearlyZero()) Body->AddImpulseAtLocation(FVector(DeathHitDirection) * HitImpulse, FVector(DeathHitLocation));
	if (GetWorld()) GetWorldTimerManager().SetTimer(DeathPoseTimer, this, &AArenaDuelCharacter::SettleDeathRagdoll, 4.0f, false);
}

void AArenaDuelCharacter::SettleDeathRagdoll()
{
	// Whatever is still twitching after the fall is put to rest so the body lies still.
	if (USkeletalMeshComponent* Body = GetMesh()) Body->PutAllRigidBodiesToSleep();
}
void AArenaDuelCharacter::OnRep_Dead() { if (bDead) SetDeadState(); }

void AArenaDuelCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AArenaDuelCharacter, bDead);
	DOREPLIFETIME(AArenaDuelCharacter, DeathHitLocation);
	DOREPLIFETIME(AArenaDuelCharacter, DeathHitDirection);
}

UArenaDuelCharacterMovementComponent* AArenaDuelCharacter::GetArenaDuelMovementComponent() const
{
	return Cast<UArenaDuelCharacterMovementComponent>(GetCharacterMovement());
}

void AArenaDuelCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	RefreshCharacterVisuals();

	if (!IsLocallyControlled())
	{
		return;
	}
	if (AArenaDuelPlayerController* ArenaController = Cast<AArenaDuelPlayerController>(GetController())) ArenaController->ApplyCurrentUserSettingsToPawn();

	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController)
	{
		UE_LOG(LogArenaDuel, Warning, TEXT("ArenaDuelCharacter has no PlayerController for local input setup."));
		return;
	}

	ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();
	if (!LocalPlayer)
	{
		UE_LOG(LogArenaDuel, Warning, TEXT("ArenaDuelCharacter could not resolve a LocalPlayer for input setup."));
		return;
	}

	if (!DefaultMappingContext)
	{
		UE_LOG(LogArenaDuel, Warning, TEXT("ArenaDuelCharacter has no DefaultMappingContext. Configure IMC_Gameplay in the Character defaults."));
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
	{
		if (!InputSubsystem->HasMappingContext(DefaultMappingContext))
		{
			InputSubsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

void AArenaDuelCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInputComponent)
	{
		UE_LOG(LogArenaDuel, Error, TEXT("ArenaDuelCharacter requires an EnhancedInputComponent."));
		return;
	}

	if (MoveAction)
	{
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AArenaDuelCharacter::Move);
	}
	else
	{
		UE_LOG(LogArenaDuel, Warning, TEXT("ArenaDuelCharacter has no MoveAction configured."));
	}

	if (LookAction)
	{
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AArenaDuelCharacter::Look);
	}
	else
	{
		UE_LOG(LogArenaDuel, Warning, TEXT("ArenaDuelCharacter has no LookAction configured."));
	}

	if (JumpAction)
	{
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AArenaDuelCharacter::JumpStarted);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AArenaDuelCharacter::JumpCompleted);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Canceled, this, &AArenaDuelCharacter::JumpCompleted);
	}
	else
	{
		UE_LOG(LogArenaDuel, Warning, TEXT("ArenaDuelCharacter has no JumpAction configured."));
	}

	if (SprintAction)
	{
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &AArenaDuelCharacter::SprintStarted);
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &AArenaDuelCharacter::SprintCompleted);
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Canceled, this, &AArenaDuelCharacter::SprintCompleted);
	}
	else
	{
		UE_LOG(LogArenaDuel, Warning, TEXT("ArenaDuelCharacter has no SprintAction configured."));
	}

	if (CrouchAction)
	{
		EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Started, this, &AArenaDuelCharacter::CrouchStarted);
		EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Completed, this, &AArenaDuelCharacter::CrouchCompleted);
		EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Canceled, this, &AArenaDuelCharacter::CrouchCompleted);
	}
	else
	{
		UE_LOG(LogArenaDuel, Warning, TEXT("ArenaDuelCharacter has no CrouchAction configured."));
	}

	if (SlideAction)
	{
		EnhancedInputComponent->BindAction(SlideAction, ETriggerEvent::Started, this, &AArenaDuelCharacter::SlideStarted);
		EnhancedInputComponent->BindAction(SlideAction, ETriggerEvent::Completed, this, &AArenaDuelCharacter::SlideCompleted);
		EnhancedInputComponent->BindAction(SlideAction, ETriggerEvent::Canceled, this, &AArenaDuelCharacter::SlideCompleted);
	}
	else
	{
		UE_LOG(LogArenaDuel, Warning, TEXT("ArenaDuelCharacter has no SlideAction configured."));
	}

	if (FireAction)
	{
		EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Started, this, &AArenaDuelCharacter::WeaponFireStarted);
		EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Completed, this, &AArenaDuelCharacter::WeaponFireCompleted);
		EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Canceled, this, &AArenaDuelCharacter::WeaponFireCompleted);
	}
	if (AimAction)
	{
		EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Started, this, &AArenaDuelCharacter::AimStarted);
		EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Completed, this, &AArenaDuelCharacter::AimCompleted);
		EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Canceled, this, &AArenaDuelCharacter::AimCompleted);
	}
	if (ReloadAction) EnhancedInputComponent->BindAction(ReloadAction, ETriggerEvent::Started, this, &AArenaDuelCharacter::WeaponReloadStarted);
	if (Weapon1Action) EnhancedInputComponent->BindAction(Weapon1Action, ETriggerEvent::Started, this, &AArenaDuelCharacter::Weapon1Started);
	if (Weapon2Action) EnhancedInputComponent->BindAction(Weapon2Action, ETriggerEvent::Started, this, &AArenaDuelCharacter::Weapon2Started);
	if (Weapon3Action) EnhancedInputComponent->BindAction(Weapon3Action, ETriggerEvent::Started, this, &AArenaDuelCharacter::Weapon3Started);
	if (Weapon4Action) EnhancedInputComponent->BindAction(Weapon4Action, ETriggerEvent::Started, this, &AArenaDuelCharacter::Weapon4Started);
	PlayerInputComponent->BindKey(EKeys::Q, IE_Pressed, this, &AArenaDuelCharacter::PrimaryAbilityStarted);
	PlayerInputComponent->BindKey(EKeys::E, IE_Pressed, this, &AArenaDuelCharacter::SecondaryAbilityStarted);
}

void AArenaDuelCharacter::PrimaryAbilityStarted()
{
	if (!CanProcessGameplayInput()) return;
	if (AArenaDuelPlayerState* State = GetPlayerState<AArenaDuelPlayerState>()) State->TryActivatePrimaryAbility();
}

void AArenaDuelCharacter::SecondaryAbilityStarted()
{
	if (!CanProcessGameplayInput()) return;
	if (AArenaDuelPlayerState* State = GetPlayerState<AArenaDuelPlayerState>()) State->TryActivateSecondaryAbility();
}

void AArenaDuelCharacter::Move(const FInputActionValue& Value)
{
	if (!CanProcessGameplayInput() || !Controller)
	{
		return;
	}

	const FVector2D MovementVector = Value.Get<FVector2D>();
	const FRotator ControlRotation = Controller->GetControlRotation();
	const FRotator YawRotation(0.0f, ControlRotation.Yaw, 0.0f);

	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), MovementVector.Y);
	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), MovementVector.X);
}

void AArenaDuelCharacter::Look(const FInputActionValue& Value)
{
	if (!CanProcessGameplayInput())
	{
		return;
	}

	FVector2D LookAxisVector = Value.Get<FVector2D>();
	if (const AArenaDuelPlayerController* PC = Cast<AArenaDuelPlayerController>(GetController())) LookAxisVector *= PC->GetLocalSettings().MouseSensitivity;
	if (WeaponComponent && WeaponComponent->IsAiming())
	{
		const AArenaDuelPlayerController* PC = Cast<AArenaDuelPlayerController>(GetController());
		LookAxisVector *= WeaponComponent->GetAimSensitivityMultiplier() * (PC ? PC->GetLocalSettings().ADSMultiplier : 1.0f);
	}
	AddControllerYawInput(LookAxisVector.X);
	// Unreal's mouse Y convention is positive while moving down. Negate once here
	// so the default ArenaDuel camera follows normal FPS behavior: mouse up looks up.
	AddControllerPitchInput(-LookAxisVector.Y);
	if (Controller)
	{
		FRotator ControlRotation = Controller->GetControlRotation();
		ControlRotation.Pitch = FMath::Clamp(FRotator::NormalizeAxis(ControlRotation.Pitch), -88.0f, 88.0f);
		Controller->SetControlRotation(ControlRotation);
	}
}

void AArenaDuelCharacter::JumpStarted()
{
	if (!CanProcessGameplayInput())
	{
		return;
	}

	if (UArenaDuelCharacterMovementComponent* MovementComponent = GetArenaDuelMovementComponent())
	{
		if (MovementComponent->IsSliding())
		{
			MovementComponent->QueueAdvancedJump(false);
			if (MovementComponent->TrySlideJump())
			{
				return;
			}
		}
		else if (MovementComponent->IsWallRunning())
		{
			MovementComponent->QueueAdvancedJump(true);
			if (MovementComponent->TryWallJump())
			{
				return;
			}
		}
		else
		{
			MovementComponent->ClearAdvancedJumpIntent();
		}
	}
	Jump();
}

void AArenaDuelCharacter::JumpCompleted()
{
	if (!IsLocallyControlled())
	{
		return;
	}

	StopJumping();
}

void AArenaDuelCharacter::SprintStarted()
{
	if (CanProcessGameplayInput())
	{
		if (UArenaDuelCharacterMovementComponent* MovementComponent = GetArenaDuelMovementComponent())
		{
			MovementComponent->StartSprint();
			if (WeaponComponent) WeaponComponent->StopAim();
		}
	}
}

void AArenaDuelCharacter::SprintCompleted()
{
	if (UArenaDuelCharacterMovementComponent* MovementComponent = GetArenaDuelMovementComponent())
	{
		MovementComponent->StopSprint();
	}
}

void AArenaDuelCharacter::CrouchStarted()
{
	if (CanProcessGameplayInput())
	{
		bCrouchInputHeld = true;
		Crouch();
	}
}

void AArenaDuelCharacter::CrouchCompleted()
{
	if (CanProcessGameplayInput())
	{
		bCrouchInputHeld = false;
		if (!GetArenaDuelMovementComponent() || !GetArenaDuelMovementComponent()->IsSliding())
		{
			UnCrouch();
		}
	}
}

void AArenaDuelCharacter::SlideStarted()
{
	if (CanProcessGameplayInput())
	{
		if (UArenaDuelCharacterMovementComponent* MovementComponent = GetArenaDuelMovementComponent())
		{
			MovementComponent->StartSlide();
		}
	}
}

void AArenaDuelCharacter::SlideCompleted()
{
	if (UArenaDuelCharacterMovementComponent* MovementComponent = GetArenaDuelMovementComponent())
	{
		MovementComponent->ReleaseSlideInput();
	}
}

void AArenaDuelCharacter::WeaponFireStarted() { if (WeaponComponent) WeaponComponent->StartFire(); }
void AArenaDuelCharacter::WeaponFireCompleted() { if (WeaponComponent) WeaponComponent->StopFire(); }
void AArenaDuelCharacter::AimStarted() { if (WeaponComponent) WeaponComponent->StartAim(); }
void AArenaDuelCharacter::AimCompleted() { if (WeaponComponent) WeaponComponent->StopAim(); }
void AArenaDuelCharacter::WeaponReloadStarted() { if (WeaponComponent) WeaponComponent->Reload(); }
void AArenaDuelCharacter::Weapon1Started() { if (WeaponComponent) WeaponComponent->EquipWeapon(0); }
void AArenaDuelCharacter::Weapon2Started() { if (WeaponComponent) WeaponComponent->EquipWeapon(1); }
void AArenaDuelCharacter::Weapon3Started() { if (WeaponComponent) WeaponComponent->EquipWeapon(2); }
void AArenaDuelCharacter::Weapon4Started() { if (WeaponComponent) WeaponComponent->EquipWeapon(3); }
