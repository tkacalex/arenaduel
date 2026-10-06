// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArenaDuelMovementDebugHUD.h"
#include "../UI/ArenaDuelHUDWidget.h"
#include "../Characters/ArenaDuelCharacter.h"
#include "../Characters/ArenaDuelCharacterMovementComponent.h"
#include "../Weapons/ArenaDuelWeaponComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

void AArenaDuelMovementDebugHUD::BeginPlay()
{
	Super::BeginPlay();
	APlayerController* Controller = GetOwningPlayerController();
	if (Controller && Controller->IsLocalController() && !GameplayWidget)
	{
		GameplayWidget = CreateWidget<UArenaDuelHUDWidget>(Controller, UArenaDuelHUDWidget::StaticClass());
		if (GameplayWidget)
		{
			GameplayWidget->AddToViewport(0);
		}
	}
}

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
	Super::DrawHUD();
	APlayerController* Controller = GetOwningPlayerController();
	#if !UE_BUILD_SHIPPING
	if (Controller && Controller->WasInputKeyJustPressed(EKeys::F3)) bShowDebugOverlay = !bShowDebugOverlay;
	#endif
	AArenaDuelCharacter* Character = Controller ? Cast<AArenaDuelCharacter>(Controller->GetPawn()) : nullptr;
	const UArenaDuelCharacterMovementComponent* Movement = Character ? Character->GetArenaDuelMovementComponent() : nullptr;
	const UArenaDuelWeaponComponent* Weapon = Character ? Character->GetWeaponComponent() : nullptr;
	if (!Canvas) return;

	const FVector2D Center(Canvas->SizeX * 0.5f, Canvas->SizeY * 0.5f);
	const float Spread = Weapon ? Weapon->GetCurrentSpreadDegrees() : 0.0f;
	const float Kick = Weapon ? Weapon->GetCrosshairKick() : 0.0f;
	const float Gap = FMath::Clamp(5.0f + Spread * 3.0f + Kick * 9.0f, 4.0f, 34.0f);
	const FLinearColor Crosshair = Weapon && Weapon->IsAiming() ? FLinearColor(0.75f, 0.95f, 1.0f, 1.0f) : FLinearColor::White;
	const float SX = Canvas->SizeX / 1920.0f;
	const float SY = Canvas->SizeY / 1080.0f;
	const float FrameX = 24.0f * SX;
	const float FrameY = 24.0f * SY;
	const FLinearColor FrameColor(0.47f, 0.91f, 1.0f, 0.22f);
	Canvas->K2_DrawLine(FVector2D(FrameX, FrameY), FVector2D(Canvas->SizeX - FrameX, FrameY), 1.0f, FrameColor);
	Canvas->K2_DrawLine(FVector2D(FrameX, Canvas->SizeY - FrameY), FVector2D(Canvas->SizeX - FrameX, Canvas->SizeY - FrameY), 1.0f, FrameColor);
	Canvas->K2_DrawLine(FVector2D(FrameX, FrameY), FVector2D(FrameX, FrameY + 52.0f * SY), 1.0f, FrameColor);
	Canvas->K2_DrawLine(FVector2D(Canvas->SizeX - FrameX, FrameY), FVector2D(Canvas->SizeX - FrameX, FrameY + 52.0f * SY), 1.0f, FrameColor);
	Canvas->K2_DrawLine(FVector2D(FrameX, Canvas->SizeY - FrameY), FVector2D(FrameX, Canvas->SizeY - FrameY - 52.0f * SY), 1.0f, FrameColor);
	Canvas->K2_DrawLine(FVector2D(Canvas->SizeX - FrameX, Canvas->SizeY - FrameY), FVector2D(Canvas->SizeX - FrameX, Canvas->SizeY - FrameY - 52.0f * SY), 1.0f, FrameColor);
	const float ArcRadius = 250.0f * FMath::Min(SX, SY);
	const FVector2D LeftArcCenter(Canvas->SizeX * 0.03f, Canvas->SizeY * 0.51f);
	const FVector2D RightArcCenter(Canvas->SizeX * 0.97f, Canvas->SizeY * 0.51f);
	for (int32 Segment = 0; Segment < 18; ++Segment)
	{
		const float LeftA0 = FMath::DegreesToRadians(-58.0f + Segment * 3.0f);
		const float LeftA1 = FMath::DegreesToRadians(-55.0f + Segment * 3.0f);
		Canvas->K2_DrawLine(LeftArcCenter + FVector2D(FMath::Cos(LeftA0), FMath::Sin(LeftA0)) * ArcRadius, LeftArcCenter + FVector2D(FMath::Cos(LeftA1), FMath::Sin(LeftA1)) * ArcRadius, 2.0f, FLinearColor(0.73f, 0.44f, 1.0f, 0.28f));
		const float RightA0 = FMath::DegreesToRadians(126.0f + Segment * 3.0f);
		const float RightA1 = FMath::DegreesToRadians(129.0f + Segment * 3.0f);
		Canvas->K2_DrawLine(RightArcCenter + FVector2D(FMath::Cos(RightA0), FMath::Sin(RightA0)) * ArcRadius, RightArcCenter + FVector2D(FMath::Cos(RightA1), FMath::Sin(RightA1)) * ArcRadius, 2.0f, FLinearColor(0.47f, 0.91f, 1.0f, 0.28f));
	}
	Canvas->K2_DrawLine(FVector2D(Canvas->SizeX * 0.5f - 18.0f, Canvas->SizeY - 30.0f), FVector2D(Canvas->SizeX * 0.5f, Canvas->SizeY - 20.0f), 1.0f, FrameColor);
	Canvas->K2_DrawLine(FVector2D(Canvas->SizeX * 0.5f, Canvas->SizeY - 20.0f), FVector2D(Canvas->SizeX * 0.5f + 18.0f, Canvas->SizeY - 30.0f), 1.0f, FrameColor);
	if (Character && Movement && !Character->IsDead())
	{
	Canvas->K2_DrawLine(Center + FVector2D(-Gap - 7.0f, 0.0f), Center + FVector2D(-Gap, 0.0f), 1.5f, Crosshair);
	Canvas->K2_DrawLine(Center + FVector2D(Gap, 0.0f), Center + FVector2D(Gap + 7.0f, 0.0f), 1.5f, Crosshair);
	Canvas->K2_DrawLine(Center + FVector2D(0.0f, -Gap - 7.0f), Center + FVector2D(0.0f, -Gap), 1.5f, Crosshair);
	Canvas->K2_DrawLine(Center + FVector2D(0.0f, Gap), Center + FVector2D(0.0f, Gap + 7.0f), 1.5f, Crosshair);
	if (Weapon && Weapon->GetLastShotAge() < 0.16f && Weapon->GetLastShotResult() != EArenaDuelShotResult::Miss && Weapon->GetLastShotResult() != EArenaDuelShotResult::World)
	{
		const FLinearColor Marker = Weapon->GetLastShotResult() == EArenaDuelShotResult::Head ? FLinearColor(0.89f, 0.78f, 0.49f, 1.0f) : FLinearColor(0.85f, 0.96f, 1.0f, 1.0f);
		Canvas->K2_DrawLine(Center + FVector2D(-14.0f, -14.0f), Center + FVector2D(-5.0f, -5.0f), 2.0f, Marker);
		Canvas->K2_DrawLine(Center + FVector2D(14.0f, -14.0f), Center + FVector2D(5.0f, -5.0f), 2.0f, Marker);
		Canvas->K2_DrawLine(Center + FVector2D(-14.0f, 14.0f), Center + FVector2D(-5.0f, 5.0f), 2.0f, Marker);
		Canvas->K2_DrawLine(Center + FVector2D(14.0f, 14.0f), Center + FVector2D(5.0f, 5.0f), 2.0f, Marker);
	}
	}

	#if !UE_BUILD_SHIPPING
	if (bShowDebugOverlay && Character && Movement)
	{
		const FString Debug = FString::Printf(TEXT("SPEED %03d\nSTATE %s\nSTAMINA %02d / %02d\nSPREAD %.2f\nSEQ %d\nF3 HIDE"), FMath::RoundToInt(Movement->Velocity.Size2D()), *MovementState(Movement), FMath::RoundToInt(Movement->GetStamina()), FMath::RoundToInt(Movement->GetMaxStamina()), Spread, Weapon ? Weapon->GetLastShotSequence() : 0);
		DrawCanvasText(Canvas, Debug, FVector2D(32.0f, 32.0f), FLinearColor(0.8f, 0.9f, 1.0f, 1.0f), GEngine->GetSmallFont());
	}
	#endif
}
