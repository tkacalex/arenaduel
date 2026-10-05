// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArenaDuelMovementDebugHUD.h"

#include "../Characters/ArenaDuelCharacter.h"
#include "../Characters/ArenaDuelCharacterMovementComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"

namespace
{
	static FString MovementState(const UArenaDuelCharacterMovementComponent* Movement)
	{
		if (!Movement)
		{
			return TEXT("UNKNOWN");
		}
		if (Movement->IsWallRunning()) return TEXT("WALLRUN");
		if (Movement->IsMantling()) return TEXT("MANTLE");
		if (Movement->MovementMode == MOVE_Custom && Movement->CustomMovementMode == static_cast<uint8>(EArenaDuelCustomMovementMode::Vault)) return TEXT("VAULT");
		if (Movement->IsSliding()) return TEXT("SLIDE");
		if (Movement->IsCrouching()) return TEXT("CROUCH");
		if (Movement->IsFalling()) return TEXT("AIR");
		if (Movement->IsSprinting()) return TEXT("SPRINT");
		return TEXT("WALK");
	}
}

void AArenaDuelMovementDebugHUD::DrawHUD()
{
#if !UE_BUILD_SHIPPING
	Super::DrawHUD();
	APlayerController* Controller = GetOwningPlayerController();
	AArenaDuelCharacter* Character = Controller ? Cast<AArenaDuelCharacter>(Controller->GetPawn()) : nullptr;
	const UArenaDuelCharacterMovementComponent* Movement = Character ? Character->GetArenaDuelMovementComponent() : nullptr;
	if (!Canvas || !Movement)
	{
		return;
	}

	const float Speed = Movement->Velocity.Size2D();
	const FString Text = FString::Printf(
		TEXT("SPEED: %03d\nSTATE: %s\nSTAMINA: %02d / %02d\n\nWASD  MOVE\nMOUSE  LOOK\nSHIFT  SPRINT\nCTRL  SLIDE\nC  CROUCH\nSPACE  JUMP / SLIDE JUMP / WALL JUMP"),
		FMath::RoundToInt(Speed),
		*MovementState(Movement),
		FMath::RoundToInt(Movement->GetStamina()),
		FMath::RoundToInt(Movement->GetMaxStamina()));

	FCanvasTextItem Item(FVector2D(32.0f, 32.0f), FText::FromString(Text), GEngine->GetSmallFont(), FLinearColor::White);
	Item.EnableShadow(FLinearColor::Black);
	Canvas->DrawItem(Item);
#endif
}
