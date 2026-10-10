// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArenaDuelMovementDebugHUD.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "../UI/ArenaDuelHUDWidget.h"
#include "ArenaDuelGameState.h"
#include "ArenaDuelZombieGameMode.h"
#include "ArenaDuelZombieGameState.h"
#include "../Player/ArenaDuelPlayerState.h"
#include "../Characters/ArenaDuelCharacter.h"
#include "../Characters/ArenaDuelCharacterMovementComponent.h"
#include "../Weapons/ArenaDuelWeaponComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/BodySetup.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"

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
			const AArenaDuelGameState* State = GetWorld()->GetGameState<AArenaDuelGameState>();
			SetCharacterSelectVisible(!State || State->IsCharacterSelectVisible());
		}
	}
}

void AArenaDuelMovementDebugHUD::SetCharacterSelectVisible(bool bVisible)
{
	if (GameplayWidget) GameplayWidget->SetVisibility(bVisible ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
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

namespace
{
	/**
	 * Telescopic sight picture: everything outside a round field of view is black, the edge of the lens is
	 * shaded, and a fine black reticle with three heavier posts crosses it. Sizes follow the viewport
	 * height, so it looks the same at every resolution and aspect ratio.
	 */
	void DrawScopePicture(UCanvas* Canvas, float Alpha)
	{
		const float Width = Canvas->SizeX, Height = Canvas->SizeY;
		const FVector2D Center(Width * 0.5f, Height * 0.5f);
		const float Radius = 0.47f * FMath::Min(Width, Height);
		const float Far = FVector2D(Width, Height).Size();
		const FLinearColor Solid(0.0f, 0.0f, 0.0f, Alpha), Clear(0.0f, 0.0f, 0.0f, 0.0f), Shade(0.0f, 0.0f, 0.0f, 0.82f * Alpha);
		const int32 Segments = 128;
		TArray<FCanvasUVTri> Triangles;
		Triangles.Reserve(Segments * 4);
		const auto AddTriangle = [&Triangles](const FVector2D& A, const FVector2D& B, const FVector2D& C, const FLinearColor& ColorA, const FLinearColor& ColorB, const FLinearColor& ColorC)
		{
			FCanvasUVTri Triangle;
			Triangle.V0_Pos = A; Triangle.V1_Pos = B; Triangle.V2_Pos = C;
			Triangle.V0_Color = ColorA; Triangle.V1_Color = ColorB; Triangle.V2_Color = ColorC;
			Triangles.Add(Triangle);
		};
		for (int32 Index = 0; Index < Segments; ++Index)
		{
			const float A0 = UE_TWO_PI * Index / Segments, A1 = UE_TWO_PI * (Index + 1) / Segments;
			const FVector2D D0(FMath::Cos(A0), FMath::Sin(A0)), D1(FMath::Cos(A1), FMath::Sin(A1));
			// Black from the lens edge to beyond the screen corners.
			AddTriangle(Center + D0 * Radius, Center + D0 * Far, Center + D1 * Far, Solid, Solid, Solid);
			AddTriangle(Center + D0 * Radius, Center + D1 * Far, Center + D1 * Radius, Solid, Solid, Solid);
			// Shaded rim inside the edge, fading to clear.
			AddTriangle(Center + D0 * Radius * 0.88f, Center + D0 * Radius, Center + D1 * Radius, Clear, Shade, Shade);
			AddTriangle(Center + D0 * Radius * 0.88f, Center + D1 * Radius, Center + D1 * Radius * 0.88f, Clear, Shade, Clear);
		}
		Canvas->K2_DrawTriangle(nullptr, Triangles);

		const float Unit = FMath::Max(Height / 1080.0f, 0.5f);
		const FLinearColor Line(0.0f, 0.0f, 0.0f, Alpha);
		const float Fine = FMath::Max(1.0f, 1.2f * Unit), Post = 4.0f * Unit, PostStart = 0.36f * Radius;
		// The reticle is black. A faint pale edge is drawn under it first, or it would vanish against a dark scene.
		const FLinearColor Edge(0.75f, 0.8f, 0.85f, 0.10f * Alpha);
		const float EdgeExtra = FMath::Max(1.0f, 1.2f * Unit);
		for (const bool bEdgePass : { true, false })
		{
			const FLinearColor& Color = bEdgePass ? Edge : Line;
			const float Extra = bEdgePass ? EdgeExtra : 0.0f;
			// Fine cross through the centre.
			Canvas->K2_DrawLine(Center - FVector2D(Radius, 0.0f), Center + FVector2D(Radius, 0.0f), Fine + Extra, Color);
			Canvas->K2_DrawLine(Center - FVector2D(0.0f, Radius), Center + FVector2D(0.0f, Radius), Fine + Extra, Color);
			// Heavy posts left, right and below; the upper half stays open.
			Canvas->K2_DrawLine(Center - FVector2D(Radius, 0.0f), Center - FVector2D(PostStart, 0.0f), Post + Extra, Color);
			Canvas->K2_DrawLine(Center + FVector2D(PostStart, 0.0f), Center + FVector2D(Radius, 0.0f), Post + Extra, Color);
			Canvas->K2_DrawLine(Center + FVector2D(0.0f, PostStart), Center + FVector2D(0.0f, Radius), Post + Extra, Color);
		}
	}

	/** Zombie Survival: wave, enemies left, points, countdown and shop, announcements, boss bar and the game over panel. */
	void DrawSurvival(UCanvas* Canvas, const AArenaDuelZombieGameState* State, const APlayerController* Controller)
	{
		const UFont* Font = GEngine ? GEngine->GetLargeFont() : nullptr;
		if (!Font) return;
		const float Width = Canvas->SizeX, Height = Canvas->SizeY, Unit = FMath::Max(Height / 1080.0f, 0.5f);
		const float Now = State->GetServerWorldTimeSeconds();
		const AArenaDuelPlayerState* Player = Controller ? Controller->GetPlayerState<AArenaDuelPlayerState>() : nullptr;
		const auto Text = [&](const FString& String, float X, float Y, float Scale, const FLinearColor& Color, bool bCentre)
		{
			FCanvasTextItem Item(FVector2D(X, Y), FText::FromString(String), Font, Color);
			Item.Scale = FVector2D(Scale * Unit, Scale * Unit);
			Item.bCentreX = bCentre;
			Item.EnableShadow(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f));
			Canvas->DrawItem(Item);
		};
		const FLinearColor Pale(0.86f, 0.92f, 1.0f, 1.0f), Green(0.45f, 1.0f, 0.55f, 1.0f), Amber(1.0f, 0.78f, 0.35f, 1.0f), Red(1.0f, 0.3f, 0.25f, 1.0f);

		Text(State->GetWave() > 0 ? FString::Printf(TEXT("WAVE %d"), State->GetWave()) : FString(TEXT("GET READY")), Width * 0.5f, 26.0f * Unit, 2.0f, Pale, true);
		if (!State->IsIntermission() && !State->IsGameOver()) Text(FString::Printf(TEXT("ZOMBIES LEFT  %d"), State->GetZombiesRemaining()), Width * 0.5f, 68.0f * Unit, 1.2f, Green, true);
		Text(FString::Printf(TEXT("POINTS  %d"), Player ? Player->GetSurvivalPoints() : 0), Width - 300.0f * Unit, 30.0f * Unit, 1.5f, Amber, false);
		Text(FString::Printf(TEXT("KILLS  %d"), Player ? Player->GetSurvivalKills() : 0), Width - 300.0f * Unit, 64.0f * Unit, 1.1f, Pale, false);

		if (State->IsIntermission() && !State->IsGameOver())
		{
			const int32 Seconds = FMath::Max(0, FMath::CeilToInt(State->GetNextWaveServerTime() - Now));
			Text(FString::Printf(TEXT("NEXT WAVE IN  %d"), Seconds), Width * 0.5f, 68.0f * Unit, 1.3f, Amber, true);
			// The last seconds are counted down large, so the start of a wave never comes as a surprise.
			if (Seconds >= 1 && Seconds <= 5 && State->GetWave() > 0) Text(FString::FromInt(Seconds), Width * 0.5f, Height * 0.36f, 4.5f, FLinearColor(1.0f, 0.78f, 0.35f, 0.85f), true);
			if (const AArenaDuelZombieGameMode* Rules = GetDefault<AArenaDuelZombieGameMode>(); Rules && State->GetWave() > 0)
			{
				Text(FString::Printf(TEXT("[5] AMMO  %d      [6] HEAL  %d      [7] DAMAGE +25%%  %d  (LEVEL %d)"),
					Rules->GetPurchaseCost(Controller, EArenaDuelSurvivalPurchase::Ammo), Rules->GetPurchaseCost(Controller, EArenaDuelSurvivalPurchase::Heal),
					Rules->GetPurchaseCost(Controller, EArenaDuelSurvivalPurchase::Damage), Player ? Player->GetSurvivalDamageLevel() : 0), Width * 0.5f, Height - 150.0f * Unit, 1.1f, Pale, true);
			}
		}

		// Badly hurt: the screen edges pulse red until the player has recovered.
		if (const AArenaDuelCharacter* Hurt = Controller ? Cast<AArenaDuelCharacter>(Controller->GetPawn()) : nullptr; Hurt && !Hurt->IsDead() && !State->IsGameOver() && Hurt->GetMaxHealth() > 0.0f && Hurt->GetHealth() / Hurt->GetMaxHealth() < 0.35f)
		{
			const float Danger = 1.0f - FMath::Clamp(Hurt->GetHealth() / (Hurt->GetMaxHealth() * 0.35f), 0.0f, 1.0f);
			const float Pulse = 0.55f + 0.45f * FMath::Sin(Now * 5.0f);
			const float Band = 0.07f * Height;
			const FLinearColor Edge(0.8f, 0.03f, 0.02f, (0.10f + 0.22f * Danger) * Pulse);
			for (const FVector4f& Rect : { FVector4f(0.0f, 0.0f, Width, Band), FVector4f(0.0f, Height - Band, Width, Band), FVector4f(0.0f, Band, Band, Height - 2.0f * Band), FVector4f(Width - Band, Band, Band, Height - 2.0f * Band) })
			{
				FCanvasTileItem Tile(FVector2D(Rect.X, Rect.Y), FVector2D(Rect.Z, Rect.W), Edge);
				Tile.BlendMode = SE_BLEND_Translucent;
				Canvas->DrawItem(Tile);
			}
		}

		// Boss bar under the wave line.
		if (const float Boss = State->GetBossHealthFraction(); Boss >= 0.0f && !State->IsGameOver())
		{
			const float BarWidth = 620.0f * Unit, BarHeight = 14.0f * Unit, X = (Width - BarWidth) * 0.5f, Y = 132.0f * Unit;
			Text(State->GetBossName(), Width * 0.5f, 100.0f * Unit, 1.2f, Red, true);
			FCanvasTileItem Back(FVector2D(X - 2.0f, Y - 2.0f), FVector2D(BarWidth + 4.0f, BarHeight + 4.0f), FLinearColor(0.0f, 0.0f, 0.0f, 0.75f));
			Back.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(Back);
			FCanvasTileItem Fill(FVector2D(X, Y), FVector2D(BarWidth * FMath::Clamp(Boss, 0.0f, 1.0f), BarHeight), Red);
			Fill.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(Fill);
		}

		// A horn with every wave start, once per announcement.
		static float HeardAnnouncementTime = -1000.0f;
		if (State->GetAnnouncementServerTime() > HeardAnnouncementTime + 0.01f || State->GetAnnouncementServerTime() < HeardAnnouncementTime - 1.0f)
		{
			HeardAnnouncementTime = State->GetAnnouncementServerTime();
			static TWeakObjectPtr<USoundBase> Horn;
			if (!Horn.IsValid()) Horn = LoadObject<USoundBase>(nullptr, TEXT("/Game/ArenaDuel/Audio/S_SurvivalWaveStart.S_SurvivalWaveStart"), nullptr, LOAD_NoWarn | LOAD_Quiet);
			if (Horn.IsValid() && Controller && State->GetAnnouncement().StartsWith(TEXT("WAVE")) && !State->GetAnnouncement().EndsWith(TEXT("CLEARED")) && Now - HeardAnnouncementTime < 2.0f) UGameplayStatics::PlaySound2D(Controller, Horn.Get(), 0.6f);
		}

		// Announcement: large, centred, fading over three seconds.
		if (const float Age = Now - State->GetAnnouncementServerTime(); Age >= 0.0f && Age < 3.0f && !State->GetAnnouncement().IsEmpty() && !State->IsGameOver())
		{
			const float Alpha = FMath::Clamp(1.5f - Age * 0.5f, 0.0f, 1.0f);
			Text(State->GetAnnouncement(), Width * 0.5f, Height * 0.3f, 3.0f, FLinearColor(1.0f, 0.95f, 0.85f, Alpha), true);
		}

		if (State->IsGameOver())
		{
			FCanvasTileItem Dim(FVector2D(0.0f, 0.0f), FVector2D(Width, Height), FLinearColor(0.0f, 0.0f, 0.0f, 0.72f));
			Dim.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(Dim);
			// The wave the player died in was not survived.
			const int32 Survived = FMath::Max(0, State->GetWave() - (State->IsIntermission() ? 0 : 1));
			Text(TEXT("GAME OVER"), Width * 0.5f, Height * 0.26f, 4.0f, Red, true);
			Text(FString::Printf(TEXT("WAVES SURVIVED   %d"), Survived), Width * 0.5f, Height * 0.42f, 1.8f, Pale, true);
			Text(FString::Printf(TEXT("ZOMBIES KILLED   %d"), Player ? Player->GetSurvivalKills() : State->GetTotalKills()), Width * 0.5f, Height * 0.48f, 1.8f, Pale, true);
			Text(FString::Printf(TEXT("POINTS   %d"), Player ? Player->GetSurvivalPoints() : 0), Width * 0.5f, Height * 0.54f, 1.8f, Amber, true);
			if (State->GetRunEndServerTime() >= 0.0f)
			{
				const int32 RunSeconds = FMath::Max(0, FMath::RoundToInt(State->GetRunEndServerTime() - State->GetRunStartServerTime()));
				Text(FString::Printf(TEXT("TIME   %d:%02d"), RunSeconds / 60, RunSeconds % 60), Width * 0.5f, Height * 0.60f, 1.4f, Pale, true);
			}
			Text(TEXT("[ENTER]  PLAY AGAIN          [M]  MAIN MENU"), Width * 0.5f, Height * 0.66f, 1.4f, Green, true);
		}
	}
}

void AArenaDuelMovementDebugHUD::DrawHUD()
{
	Super::DrawHUD();
	if (const AArenaDuelGameState* State = GetWorld()->GetGameState<AArenaDuelGameState>(); !State || State->IsCharacterSelectVisible()) return;
	APlayerController* Controller = GetOwningPlayerController();
	#if !UE_BUILD_SHIPPING
	if (Controller && Controller->WasInputKeyJustPressed(EKeys::F3)) bShowDebugOverlay = !bShowDebugOverlay;
	#endif
	AArenaDuelCharacter* Character = Controller ? Cast<AArenaDuelCharacter>(Controller->GetPawn()) : nullptr;
	const UArenaDuelCharacterMovementComponent* Movement = Character ? Character->GetArenaDuelMovementComponent() : nullptr;
	const UArenaDuelWeaponComponent* Weapon = Character ? Character->GetWeaponComponent() : nullptr;
	if (!Canvas) return;
	if (const AArenaDuelZombieGameState* Survival = GetWorld()->GetGameState<AArenaDuelZombieGameState>())
	{
		// Survival has its own header; the duel scoreboard has nothing to show here.
		if (GameplayWidget) GameplayWidget->SetMatchHeaderVisible(false);
		DrawSurvival(Canvas, Survival, Controller);
	}

	const FVector2D Center(Canvas->SizeX * 0.5f, Canvas->SizeY * 0.5f);
	const float Spread = Weapon ? Weapon->GetCurrentSpreadDegrees() : 0.0f;
	// Flashbang: the whole view washes out to white and recovers. The HUD widgets stay readable on top.
	if (const float Blind = Character ? Character->GetFlashBlindness() : 0.0f; Blind > 0.0f && Canvas)
	{
		DrawRect(FLinearColor(1.0f, 1.0f, 1.0f, FMath::Clamp(Blind, 0.0f, 1.0f)), 0.0f, 0.0f, Canvas->SizeX, Canvas->SizeY);
	}
	// Screen edge flash when the local player takes a hit: four thin red bands that fade within a third of a second.
	if (const float Flash = Character ? Character->GetDamageFlash() : 0.0f; Flash > 0.0f && Canvas)
	{
		const float Width = Canvas->SizeX, Height = Canvas->SizeY, Band = 0.045f * Height;
		const FLinearColor Edge(0.95f, 0.08f, 0.06f, 0.32f * Flash * Flash);
		DrawRect(Edge, 0.0f, 0.0f, Width, Band);
		DrawRect(Edge, 0.0f, Height - Band, Width, Band);
		DrawRect(Edge, 0.0f, Band, Band, Height - 2.0f * Band);
		DrawRect(Edge, Width - Band, Band, Band, Height - 2.0f * Band);
	}
	const float ScopePicture = Weapon && Character && !Character->IsDead() ? Weapon->GetScopeOverlayAlpha() : 0.0f;
	if (ScopePicture > 0.0f) DrawScopePicture(Canvas, ScopePicture);
	const float Kick = Weapon ? Weapon->GetCrosshairKick() : 0.0f;
	const float Gap = FMath::Clamp(5.0f + Spread * 3.0f + Kick * 9.0f, 4.0f, 34.0f);
	const FLinearColor Crosshair = Weapon && Weapon->IsAiming() ? FLinearColor(0.75f, 0.95f, 1.0f, 1.0f) : FLinearColor::White;
	if (Character && Movement && !Character->IsDead())
	{
	// The scope has its own reticle; the hip crosshair gives way to it.
	if (ScopePicture < 0.3f)
	{
	Canvas->K2_DrawLine(Center + FVector2D(-Gap - 7.0f, 0.0f), Center + FVector2D(-Gap, 0.0f), 1.5f, Crosshair);
	Canvas->K2_DrawLine(Center + FVector2D(Gap, 0.0f), Center + FVector2D(Gap + 7.0f, 0.0f), 1.5f, Crosshair);
	Canvas->K2_DrawLine(Center + FVector2D(0.0f, -Gap - 7.0f), Center + FVector2D(0.0f, -Gap), 1.5f, Crosshair);
	Canvas->K2_DrawLine(Center + FVector2D(0.0f, Gap), Center + FVector2D(0.0f, Gap + 7.0f), 1.5f, Crosshair);
	}
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
	if (bShowHitZones && GetWorld())
	{
		for (TActorIterator<AArenaDuelCharacter> It(GetWorld()); It; ++It)
		{
			// One box per physics body: gold head, cyan torso, violet limbs.
			const USkeletalMeshComponent* Mesh = It->GetMesh();
			if (!Mesh || It->IsDead()) continue;
			for (const FBodyInstance* BodyInstance : Mesh->Bodies)
			{
				if (!BodyInstance || !BodyInstance->IsValidBodyInstance() || !BodyInstance->BodySetup.IsValid()) continue;
				const EArenaDuelShotResult Zone = UArenaDuelWeaponComponent::ClassifyHitBone(BodyInstance->BodySetup->BoneName);
				const FColor Color = Zone == EArenaDuelShotResult::Head ? FColor(227, 199, 125) : Zone == EArenaDuelShotResult::Limb ? FColor(220, 100, 255) : FColor(90, 210, 255);
				const FBox Bounds = BodyInstance->GetBodyBounds();
				DrawDebugBox(GetWorld(), Bounds.GetCenter(), Bounds.GetExtent(), Color, false, 0.0f, 0, 1.0f);
			}
		}
	}	if (bShowDebugOverlay && Character && Movement)
	{
		const FString Debug = FString::Printf(TEXT("SPEED %03d\nSTATE %s\nSTAMINA %02d / %02d\nSPREAD %.2f\nSEQ %d\nF3 HIDE"), FMath::RoundToInt(Movement->Velocity.Size2D()), *MovementState(Movement), FMath::RoundToInt(Movement->GetStamina()), FMath::RoundToInt(Movement->GetMaxStamina()), Spread, Weapon ? Weapon->GetLastShotSequence() : 0);
		DrawCanvasText(Canvas, Debug, FVector2D(32.0f, 32.0f), FLinearColor(0.8f, 0.9f, 1.0f, 1.0f), GEngine->GetSmallFont());
	}
	#endif
}
