// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArenaDuelCharacter.h"
#include "ArenaDuelCharacterMovementComponent.h"
#include "ArenaDuelVisualAnimInstance.h"
#include "../Weapons/ArenaDuelWeaponComponent.h"

#include "../ArenaDuel.h"
#include "Camera/CameraComponent.h"
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
	FirstPersonArms->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FirstPersonArms->SetCastShadow(false);
	FirstPersonArms->SetRelativeLocation(FVector(15.0f, -10.0f, -135.0f));
	FirstPersonArms->SetRelativeRotation(FRotator::ZeroRotator);
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Manny(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"));
	GetMesh()->SetSkeletalMesh(Manny.Object);
	GetMesh()->SetRelativeLocation(FVector(0, 0, -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()));
	GetMesh()->SetRelativeRotation(FRotator::ZeroRotator);
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetAnimInstanceClass(UArenaDuelVisualAnimInstance::StaticClass());
	// The editor setup command can derive the arms on a fresh checkout before they exist.
	USkeletalMesh* ArmsAsset = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/ArenaDuel/Characters/Common/SKM_ArenaDuelArms"), nullptr, LOAD_NoWarn);
	FirstPersonArms->SetSkeletalMesh(ArmsAsset ? ArmsAsset : Manny.Object.Get());
	FirstPersonArms->SetAnimInstanceClass(UArenaDuelVisualAnimInstance::StaticClass());
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
	BodyHitZone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BodyHitZone->SetCollisionResponseToAllChannels(ECR_Ignore);
	BodyHitZone->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	BodyHitZone->ComponentTags.Add(TEXT("BodyHitZone"));
	HeadHitZone = CreateDefaultSubobject<UBoxComponent>(TEXT("HeadHitZone"));
	HeadHitZone->SetupAttachment(GetCapsuleComponent());
	HeadHitZone->SetRelativeLocation(FVector(0.0f, 0.0f, 68.0f));
	HeadHitZone->SetBoxExtent(FVector(24.0f, 24.0f, 14.0f));
	HeadHitZone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HeadHitZone->SetCollisionResponseToAllChannels(ECR_Ignore);
	HeadHitZone->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
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
}

void AArenaDuelCharacter::BeginPlay()
{
	Super::BeginPlay();
	RefreshCharacterVisuals();
}

void AArenaDuelCharacter::RefreshCharacterVisuals()
{
	BodyVisual->SetHiddenInGame(true);
	HeadVisual->SetHiddenInGame(true);
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FirstPersonArms->SetVisibility(IsLocallyControlled() && !bDead);
	// Derived arm-only geometry retains the Epic skeleton, animation and grip socket.
	const auto* State = GetPlayerState<AArenaDuelPlayerState>();
	const EArenaDuelCharacterArchetype Archetype = State ? State->GetCharacterArchetype() : EArenaDuelCharacterArchetype::Shadow;
	const int32 Index = static_cast<int32>(Archetype);
	UMaterialInterface* Accent = Archetype == EArenaDuelCharacterArchetype::Warden ? CyanVisualMaterial.Get() : VioletVisualMaterial.Get();
	LeftShoulderArmor->SetVisibility(Archetype == EArenaDuelCharacterArchetype::Warden);
	RightShoulderArmor->SetVisibility(Archetype != EArenaDuelCharacterArchetype::Shadow);
	for (auto* Plate : {LeftShoulderArmor.Get(), RightShoulderArmor.Get()}) Plate->SetMaterial(0, CyanVisualMaterial);
	for (auto* VisualMesh : {GetMesh(), FirstPersonArms.Get()})
	{
		if (ArchetypeArmorMaterials.IsValidIndex(Index)) VisualMesh->SetMaterial(0, ArchetypeArmorMaterials[Index]);
		VisualMesh->SetMaterial(1, Accent);
	}
	if (WeaponComponent) WeaponComponent->RefreshWeaponVisual();
	if (bDead) ApplyDevelopmentDeathPose();
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
	}
}

bool AArenaDuelCharacter::CanProcessGameplayInput() const
{
	if (!IsLocallyControlled() || bDead) return false;
	if (const AArenaDuelPlayerController* PlayerController = Cast<AArenaDuelPlayerController>(GetController()); PlayerController && PlayerController->IsAdminMenuOpen()) return false;
	const AArenaDuelGameState* GameState = GetWorld() ? GetWorld()->GetGameState<AArenaDuelGameState>() : nullptr;
	return !GameState || GameState->IsRoundInProgress();
}

void AArenaDuelCharacter::SetRoundInputLocked(bool bLocked)
{
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

void AArenaDuelCharacter::ApplyDevelopmentDeathPose()
{
	// Cosmetic lying pose until final death animation assets exist. No ragdoll or hitbox changes.
	GetMesh()->SetRelativeLocation(FVector(0, 0, -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() + 20));
	GetMesh()->SetRelativeRotation(FRotator(0, 0, 90));
	if (GetMesh()->GetAnimInstance()) GetMesh()->GetAnimInstance()->Montage_Stop(0);
	FirstPersonArms->SetVisibility(false);
	if (WeaponComponent) WeaponComponent->RefreshWeaponVisual();
	if (BodyVisual)
	{
		BodyVisual->SetRelativeLocation(FVector(0.0f, 0.0f, -50.0f));
		BodyVisual->SetRelativeRotation(FRotator(0.0f, 0.0f, 90.0f));
	}
	if (HeadVisual)
	{
		HeadVisual->SetRelativeLocation(FVector(0.0f, 55.0f, -65.0f));
		HeadVisual->SetRelativeRotation(FRotator(0.0f, 0.0f, 90.0f));
	}
}

void AArenaDuelCharacter::OnRep_Dead() { if (bDead) SetDeadState(); }

void AArenaDuelCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AArenaDuelCharacter, bDead);
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
	if (WeaponComponent && WeaponComponent->IsAiming())
	{
		LookAxisVector *= WeaponComponent->GetAimSensitivityMultiplier();
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
		MovementComponent->StopSlide();
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
