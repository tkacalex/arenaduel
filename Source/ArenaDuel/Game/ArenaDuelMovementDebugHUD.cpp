// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArenaDuelMovementDebugHUD.h"
#include "../Characters/ArenaDuelCharacter.h"
#include "../Characters/ArenaDuelCharacterMovementComponent.h"
#include "../Weapons/ArenaDuelWeaponComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

namespace
{
	static FString MovementState(const UArenaDuelCharacterMovementComponent* Movement)
	{
		if (!Movement) return TEXT("UNKNOWN");
		if (Movement->IsWallRunning()) return TEXT("WALLRUN");
		if (Movement->IsMantling()) return TEXT("MANTLE");
		if (Movement->MovementMode == MOVE_Custom && Movement->CustomMovementMode == static_cast<uint8>(EArenaDuelCustomMovementMode::Vault)) return TEXT("VAULT");
		if (Movement->IsSliding()) return TEXT("SLIDE");
		if (Movement->IsCrouching()) return TEXT("CROUCH");
		if (Movement->IsFalling()) return TEXT("AIR");
		if (Movement->IsSprinting()) return TEXT("SPRINT");
		return TEXT("WALK");
	}
	static void DrawCanvasText(UCanvas* Canvas, const FString& Text, const FVector2D& Position, const FLinearColor& Color, UFont* Font)
	{
		FCanvasTextItem Item(Position, FText::FromString(Text), Font, Color);
		Item.EnableShadow(FLinearColor::Black);
		Canvas->DrawItem(Item);
	}
}

void AArenaDuelMovementDebugHUD::DrawHUD()
{
#if !UE_BUILD_SHIPPING
	Super::DrawHUD();
	APlayerController* Controller = GetOwningPlayerController();
	if (Controller && Controller->WasInputKeyJustPressed(EKeys::F3)) bShowDebugOverlay = !bShowDebugOverlay;
	AArenaDuelCharacter* Character = Controller ? Cast<AArenaDuelCharacter>(Controller->GetPawn()) : nullptr;
	const UArenaDuelCharacterMovementComponent* Movement = Character ? Character->GetArenaDuelMovementComponent() : nullptr;
	const UArenaDuelWeaponComponent* Weapon = Character ? Character->GetWeaponComponent() : nullptr;
	if (!Canvas || !Character || !Movement) return;

	const FVector2D Center(Canvas->SizeX * 0.5f, Canvas->SizeY * 0.5f);
	const float Spread = Weapon ? Weapon->GetCurrentSpreadDegrees() : 0.0f;
	const float Kick = Weapon ? Weapon->GetCrosshairKick() : 0.0f;
	const float Gap = FMath::Clamp(5.0f + Spread * 3.0f + Kick * 9.0f, 4.0f, 34.0f);
	const FLinearColor Crosshair = Weapon && Weapon->IsAiming() ? FLinearColor(0.75f, 0.95f, 1.0f, 1.0f) : FLinearColor::White;
	if (!Character->IsDead())
	{
	Canvas->K2_DrawLine(Center + FVector2D(-Gap - 7.0f, 0.0f), Center + FVector2D(-Gap, 0.0f), 1.5f, Crosshair);
	Canvas->K2_DrawLine(Center + FVector2D(Gap, 0.0f), Center + FVector2D(Gap + 7.0f, 0.0f), 1.5f, Crosshair);
	Canvas->K2_DrawLine(Center + FVector2D(0.0f, -Gap - 7.0f), Center + FVector2D(0.0f, -Gap), 1.5f, Crosshair);
	Canvas->K2_DrawLine(Center + FVector2D(0.0f, Gap), Center + FVector2D(0.0f, Gap + 7.0f), 1.5f, Crosshair);
	if (Weapon && Weapon->GetLastShotAge() < 0.16f && Weapon->GetLastShotResult() != EArenaDuelShotResult::Miss && Weapon->GetLastShotResult() != EArenaDuelShotResult::World)
	{
		const FLinearColor Marker = Weapon->GetLastShotResult() == EArenaDuelShotResult::Head ? FLinearColor::Yellow : FLinearColor::Red;
		Canvas->K2_DrawLine(Center + FVector2D(-14.0f, -14.0f), Center + FVector2D(-5.0f, -5.0f), 2.0f, Marker);
		Canvas->K2_DrawLine(Center + FVector2D(14.0f, -14.0f), Center + FVector2D(5.0f, -5.0f), 2.0f, Marker);
		Canvas->K2_DrawLine(Center + FVector2D(-14.0f, 14.0f), Center + FVector2D(-5.0f, 5.0f), 2.0f, Marker);
		Canvas->K2_DrawLine(Center + FVector2D(14.0f, 14.0f), Center + FVector2D(5.0f, 5.0f), 2.0f, Marker);
	}
	}

	if (Weapon)
	{
		const float Right = Canvas->SizeX - 48.0f;
		const float BaseY = Canvas->SizeY - 152.0f;
		DrawCanvasText(Canvas, Weapon->GetCurrentWeaponName().ToString().ToUpper(), FVector2D(Right - 280.0f, BaseY), FLinearColor(0.65f, 0.85f, 1.0f, 1.0f), GEngine->GetSmallFont());
		DrawCanvasText(Canvas, FString::Printf(TEXT("%02d"), Weapon->GetCurrentMagazineAmmo()), FVector2D(Right - 115.0f, BaseY + 18.0f), FLinearColor::White, GEngine->GetLargeFont());
		DrawCanvasText(Canvas, FString::Printf(TEXT("/ %03d"), Weapon->GetReserveAmmo()), FVector2D(Right - 42.0f, BaseY + 47.0f), FLinearColor(0.7f, 0.75f, 0.8f, 1.0f), GEngine->GetSmallFont());
		DrawCanvasText(Canvas, Weapon->IsReloading() ? TEXT("RELOADING") : (Weapon->IsAiming() ? TEXT("AIM") : TEXT("READY")), FVector2D(Right - 280.0f, BaseY + 62.0f), Weapon->IsReloading() ? FLinearColor(1.0f, 0.75f, 0.25f, 1.0f) : FLinearColor(0.75f, 0.8f, 0.85f, 1.0f), GEngine->GetSmallFont());
	}

	const float StaminaRatio = Movement->GetMaxStamina() > 0.0f ? Movement->GetStamina() / Movement->GetMaxStamina() : 0.0f;
	const FVector2D StaminaOrigin(40.0f, Canvas->SizeY - 58.0f);
	const float HealthMax = FMath::Max(Character->GetMaxHealth(), 1.0f);
	const float HealthRatio = FMath::Clamp(Character->GetHealth() / HealthMax, 0.0f, 1.0f);
	const FVector2D HealthOrigin(40.0f, Canvas->SizeY - 118.0f);
	Canvas->K2_DrawLine(HealthOrigin, HealthOrigin + FVector2D(220.0f, 0.0f), 8.0f, FLinearColor(0.08f, 0.1f, 0.12f, 0.8f));
	Canvas->K2_DrawLine(HealthOrigin, HealthOrigin + FVector2D(220.0f * HealthRatio, 0.0f), 6.0f, FLinearColor(0.9f, 0.25f, 0.3f, 1.0f));
	DrawCanvasText(Canvas, FString::Printf(TEXT("%03d HP"), FMath::RoundToInt(Character->GetHealth())), HealthOrigin + FVector2D(0.0f, -30.0f), FLinearColor::White, GEngine->GetMediumFont());
	Canvas->K2_DrawLine(StaminaOrigin, StaminaOrigin + FVector2D(190.0f, 0.0f), 6.0f, FLinearColor(0.08f, 0.1f, 0.12f, 0.8f));
	Canvas->K2_DrawLine(StaminaOrigin, StaminaOrigin + FVector2D(190.0f * FMath::Clamp(StaminaRatio, 0.0f, 1.0f), 0.0f), 4.0f, FLinearColor(0.3f, 0.8f, 0.95f, 1.0f));
	DrawCanvasText(Canvas, TEXT("STAMINA"), StaminaOrigin + FVector2D(0.0f, 12.0f), FLinearColor(0.7f, 0.8f, 0.85f, 1.0f), GEngine->GetSmallFont());

	if (bShowDebugOverlay)
	{
		const FString Debug = FString::Printf(TEXT("SPEED %03d\nSTATE %s\nSTAMINA %02d / %02d\nSPREAD %.2f\nSEQ %d\nF3 HIDE"), FMath::RoundToInt(Movement->Velocity.Size2D()), *MovementState(Movement), FMath::RoundToInt(Movement->GetStamina()), FMath::RoundToInt(Movement->GetMaxStamina()), Spread, Weapon ? Weapon->GetLastShotSequence() : 0);
		DrawCanvasText(Canvas, Debug, FVector2D(32.0f, 32.0f), FLinearColor(0.8f, 0.9f, 1.0f, 1.0f), GEngine->GetSmallFont());
	}
	if (Character->IsDead())
	{
		DrawCanvasText(Canvas, TEXT("DEFEATED"), Center + FVector2D(-75.0f, 54.0f), FLinearColor(1.0f, 0.2f, 0.25f, 1.0f), GEngine->GetMediumFont());
	}
#endif
}
