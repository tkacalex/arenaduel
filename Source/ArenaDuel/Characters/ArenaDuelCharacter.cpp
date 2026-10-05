// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArenaDuelCharacter.h"

#include "../ArenaDuel.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Engine/LocalPlayer.h"

AArenaDuelCharacter::AArenaDuelCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 350.0f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
}

void AArenaDuelCharacter::BeginPlay()
{
	Super::BeginPlay();

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
		InputSubsystem->AddMappingContext(DefaultMappingContext, 0);
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

	const FVector2D LookAxisVector = Value.Get<FVector2D>();
	AddControllerYawInput(LookAxisVector.X);
	AddControllerPitchInput(LookAxisVector.Y);
}

void AArenaDuelCharacter::JumpStarted()
{
	if (!IsLocallyControlled())
	{
		return;
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
