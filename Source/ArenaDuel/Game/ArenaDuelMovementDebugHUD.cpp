// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArenaDuelMovementDebugHUD.h"

#include "../Characters/ArenaDuelCharacter.h"
#include "../Characters/ArenaDuelCharacterMovementComponent.h"
#include "../Weapons/ArenaDuelWeaponComponent.h"
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
	const UArenaDuelWeaponComponent* Weapon = Character ? Character->GetWeaponComponent() : nullptr;
	if (!Canvas || !Movement)
	{
		return;
	}

	const float Speed = Movement->Velocity.Size2D();
	const FString WeaponName = Weapon ? Weapon->GetCurrentWeaponName().ToString().ToUpper() : TEXT("NONE");
	const int32 Magazine = Weapon ? Weapon->GetCurrentMagazineAmmo() : 0;
	const int32 Reserve = Weapon ? Weapon->GetReserveAmmo() : 0;
	const FString LastShot = Weapon ? (Weapon->GetLastShotResult() == EArenaDuelShotResult::Head ? TEXT("HEAD") : Weapon->GetLastShotResult() == EArenaDuelShotResult::Body ? TEXT("BODY") : TEXT("MISS")) : TEXT("MISS");
	const FString Text = FString::Printf(
		TEXT("WEAPON: %s\nAMMO: %d / %d\nRELOADING: %s\nLAST SHOT: %s\nDISTANCE: %03d m\n\nSPEED: %03d\nSTATE: %s\nSTAMINA: %02d / %02d\n\nWASD  MOVE\nMOUSE  LOOK\nSHIFT  SPRINT\nCTRL  SLIDE\nC  CROUCH\nSPACE  JUMP / SLIDE JUMP / WALL JUMP\nLMB FIRE   R RELOAD   1-4 SWITCH"),
		*WeaponName,
		Magazine,
		Reserve,
		Weapon && Weapon->IsReloading() ? TEXT("YES") : TEXT("NO"),
		*LastShot,
		Weapon ? FMath::RoundToInt(Weapon->GetLastShotDistance() / 100.0f) : 0,
		FMath::RoundToInt(Speed),
		*MovementState(Movement),
		FMath::RoundToInt(Movement->GetStamina()),
		FMath::RoundToInt(Movement->GetMaxStamina()));

	FCanvasTextItem Item(FVector2D(32.0f, 32.0f), FText::FromString(Text), GEngine->GetSmallFont(), FLinearColor::White);
	Item.EnableShadow(FLinearColor::Black);
	Canvas->DrawItem(Item);
#endif
}
