// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArenaDuelCharacter.h"
#include "ArenaDuelCharacterMovementComponent.h"
#include "../Weapons/ArenaDuelWeaponComponent.h"

#include "../ArenaDuel.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Engine/LocalPlayer.h"

AArenaDuelCharacter::AArenaDuelCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UArenaDuelCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0.0f, 0.0f, 64.0f));
	FirstPersonCamera->bUsePawnControlRotation = true;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
	WeaponComponent = CreateDefaultSubobject<UArenaDuelWeaponComponent>(TEXT("WeaponComponent"));
}

UArenaDuelCharacterMovementComponent* AArenaDuelCharacter::GetArenaDuelMovementComponent() const
{
	return Cast<UArenaDuelCharacterMovementComponent>(GetCharacterMovement());
}

void AArenaDuelCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

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
}

void AArenaDuelCharacter::Move(const FInputActionValue& Value)
{
	if (!IsLocallyControlled() || !Controller)
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
	if (!IsLocallyControlled())
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
	if (!IsLocallyControlled())
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
	if (IsLocallyControlled())
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
	if (IsLocallyControlled())
	{
		bCrouchInputHeld = true;
		Crouch();
	}
}

void AArenaDuelCharacter::CrouchCompleted()
{
	if (IsLocallyControlled())
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
	if (IsLocallyControlled())
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
