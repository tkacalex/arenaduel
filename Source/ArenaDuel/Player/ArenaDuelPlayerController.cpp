// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArenaDuelPlayerController.h"

#include "../Characters/ArenaDuelCharacter.h"
#include "../Characters/ArenaDuelCharacterMovementComponent.h"
#include "../Game/ArenaDuelGameMode.h"
#include "../Game/ArenaDuelGameState.h"
#include "../Game/ArenaDuelMovementDebugHUD.h"
#include "../Player/ArenaDuelPlayerState.h"
#include "../UI/ArenaDuelAdminWidget.h"
#include "../UI/ArenaDuelCharacterSelectWidget.h"
#include "../UI/ArenaDuelPlayerMenuWidget.h"
#include "../Game/ArenaDuelZombieGameMode.h"
#include "../Game/ArenaDuelZombieGameState.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/Engine.h"
#include "AudioDevice.h"
#include "Camera/CameraComponent.h"
#include "../Weapons/ArenaDuelWeaponComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "InputCoreTypes.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"

void AArenaDuelPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::F1, IE_Pressed, this, &AArenaDuelPlayerController::ToggleAdminMenu);
		InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AArenaDuelPlayerController::HandleEscape);
		// Zombie Survival: the shop between waves and the two choices after a run. They do nothing in a duel.
		InputComponent->BindKey(EKeys::Five, IE_Pressed, this, &AArenaDuelPlayerController::SurvivalBuyAmmo);
		InputComponent->BindKey(EKeys::Six, IE_Pressed, this, &AArenaDuelPlayerController::SurvivalBuyHeal);
		InputComponent->BindKey(EKeys::Seven, IE_Pressed, this, &AArenaDuelPlayerController::SurvivalBuyDamage);
		InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &AArenaDuelPlayerController::SurvivalRestartKey);
		InputComponent->BindKey(EKeys::M, IE_Pressed, this, &AArenaDuelPlayerController::SurvivalMenuKey);
	}
}

void AArenaDuelPlayerController::SurvivalBuyAmmo() { if (GetWorld() && GetWorld()->GetGameState<AArenaDuelZombieGameState>()) ServerSurvivalPurchase(static_cast<uint8>(EArenaDuelSurvivalPurchase::Ammo)); }
void AArenaDuelPlayerController::SurvivalBuyHeal() { if (GetWorld() && GetWorld()->GetGameState<AArenaDuelZombieGameState>()) ServerSurvivalPurchase(static_cast<uint8>(EArenaDuelSurvivalPurchase::Heal)); }
void AArenaDuelPlayerController::SurvivalBuyDamage() { if (GetWorld() && GetWorld()->GetGameState<AArenaDuelZombieGameState>()) ServerSurvivalPurchase(static_cast<uint8>(EArenaDuelSurvivalPurchase::Damage)); }

void AArenaDuelPlayerController::SurvivalRestartKey()
{
	const AArenaDuelZombieGameState* Survival = GetWorld() ? GetWorld()->GetGameState<AArenaDuelZombieGameState>() : nullptr;
	if (Survival && Survival->IsGameOver()) ServerSurvivalRestart();
}

void AArenaDuelPlayerController::SurvivalMenuKey()
{
	const AArenaDuelZombieGameState* Survival = GetWorld() ? GetWorld()->GetGameState<AArenaDuelZombieGameState>() : nullptr;
	if (Survival && Survival->IsGameOver()) ReturnToMainMenu();
}

void AArenaDuelPlayerController::ServerSurvivalPurchase_Implementation(uint8 Item)
{
	if (AArenaDuelZombieGameMode* Survival = GetWorld() ? GetWorld()->GetAuthGameMode<AArenaDuelZombieGameMode>() : nullptr; Survival && Item <= static_cast<uint8>(EArenaDuelSurvivalPurchase::Damage))
	{
		Survival->TryPurchase(this, static_cast<EArenaDuelSurvivalPurchase>(Item));
	}
}

void AArenaDuelPlayerController::ServerSurvivalRestart_Implementation()
{
	AArenaDuelZombieGameMode* Survival = GetWorld() ? GetWorld()->GetAuthGameMode<AArenaDuelZombieGameMode>() : nullptr;
	const AArenaDuelZombieGameState* State = GetWorld() ? GetWorld()->GetGameState<AArenaDuelZombieGameState>() : nullptr;
	// Only a finished run can be restarted this way; a running one cannot be reset by a stray key press.
	if (Survival && State && State->IsGameOver()) Survival->RestartSurvival();
}

void AArenaDuelPlayerController::ReturnToMainMenu()
{
	if (!IsLocalController()) return;
	CloseAdminMenu();
	ClosePlayerMenu();
	UGameplayStatics::OpenLevel(this, FName(TEXT("/Game/ArenaDuel/Maps/L_MainMenu")));
}

void AArenaDuelPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalController())
	{
		LocalSettings = UArenaDuelLocalSettingsSave::LoadSettings();
		if (!UArenaDuelLocalSettingsSave::HasSavedSettings())
			if (UGameUserSettings* Display=GEngine?GEngine->GetGameUserSettings():nullptr)
			{
				const FIntPoint Resolution=Display->GetScreenResolution(); LocalSettings.ResolutionX=Resolution.X; LocalSettings.ResolutionY=Resolution.Y;
				LocalSettings.WindowMode=static_cast<int32>(Display->GetFullscreenMode()); LocalSettings.bVSync=Display->IsVSyncEnabled(); LocalSettings.FPSLimit=FMath::RoundToInt(Display->GetFrameRateLimit());
			}
		ApplyLocalSettings(LocalSettings, false);
		// A map can be entered from the main menu, which leaves the viewport in menu input with the cursor
		// showing. Every game starts in game input; the lobby and the menus switch away from it themselves.
		bShowMouseCursor = false;
		SetInputMode(FInputModeGameOnly());
		GetWorldTimerManager().SetTimer(MatchPresentationTimer, this, &AArenaDuelPlayerController::RefreshMatchPresentation, 0.2f, true);
		RefreshMatchPresentation();
	}
}

void AArenaDuelPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CloseAdminMenu();
	GetWorldTimerManager().ClearTimer(MatchPresentationTimer);
	if (CharacterSelectWidget) CharacterSelectWidget->RemoveFromParent();
	if (PlayerMenuWidget) PlayerMenuWidget->RemoveFromParent();
	Super::EndPlay(EndPlayReason);
}

void AArenaDuelPlayerController::RequestCharacterSelection(EArenaDuelCharacterArchetype Archetype)
{
	if (IsLocalController()) ServerRequestCharacterSelection(Archetype);
}

void AArenaDuelPlayerController::ToggleCharacterReady()
{
	if (IsLocalController())
		if (const AArenaDuelPlayerState* State = GetPlayerState<AArenaDuelPlayerState>()) ServerSetCharacterReady(!State->IsCharacterReady());
}

void AArenaDuelPlayerController::ServerRequestCharacterSelection_Implementation(EArenaDuelCharacterArchetype Archetype)
{
	if (AArenaDuelGameMode* Mode = GetWorld()->GetAuthGameMode<AArenaDuelGameMode>()) Mode->RequestCharacterSelection(this, Archetype);
}

void AArenaDuelPlayerController::ServerSetCharacterReady_Implementation(bool bReady)
{
	if (AArenaDuelGameMode* Mode = GetWorld()->GetAuthGameMode<AArenaDuelGameMode>()) Mode->RequestCharacterReady(this, bReady);
}

void AArenaDuelPlayerController::RefreshMatchPresentation()
{
	if (!IsLocalController()) return;
	const AArenaDuelGameState* State = GetWorld()->GetGameState<AArenaDuelGameState>();
	if (!State) return;
	const bool bShow = State->IsCharacterSelectVisible();
	if (AArenaDuelMovementDebugHUD* HUD = Cast<AArenaDuelMovementDebugHUD>(GetHUD())) HUD->SetCharacterSelectVisible(bShow);
	if (bShow == bCharacterSelectOpen) return;
	if (bShow)
	{
		CloseAdminMenu();
		if (bPlayerMenuOpen) ClosePlayerMenu();
		if (!CharacterSelectWidget) CharacterSelectWidget = CreateWidget<UArenaDuelCharacterSelectWidget>(this, UArenaDuelCharacterSelectWidget::StaticClass());
		if (!CharacterSelectWidget) return;
		CharacterSelectWidget->AddToViewport(50);
		bShowMouseCursor = true;
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(CharacterSelectWidget->TakeWidget());
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Mode);
		CharacterSelectWidget->SetKeyboardFocus();
	}
	else
	{
		if (CharacterSelectWidget) CharacterSelectWidget->RemoveFromParent();
		bShowMouseCursor = false;
		SetInputMode(FInputModeGameOnly());
	}
	bCharacterSelectOpen = bShow;
}

bool AArenaDuelPlayerController::CanUseDevelopmentAdmin() const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	// The admin menu is a development tool, not part of the game: it exists in the editor and in a game
	// started with -ArenaDuelAdmin. Anywhere else the key does nothing and nothing on screen mentions it.
	static const bool bUnlocked = GIsEditor || FParse::Param(FCommandLine::Get(), TEXT("ArenaDuelAdmin"));
	const ENetMode NetMode = GetNetMode();
	return bUnlocked && HasAuthority() && IsLocalController() && (NetMode == NM_Standalone || NetMode == NM_ListenServer);
#endif
}

void AArenaDuelPlayerController::ToggleAdminMenu()
{
#if !UE_BUILD_SHIPPING
	if (!IsLocalController()) return;
	if (const AArenaDuelGameState* State = GetWorld()->GetGameState<AArenaDuelGameState>(); State && State->IsCharacterSelectVisible()) return;
	if (bAdminMenuOpen)
	{
		CloseAdminMenu();
		return;
	}
	if (!CanUseDevelopmentAdmin()) return;
	if (bPlayerMenuOpen) ClosePlayerMenu();
	if (!AdminWidget)
	{
		AdminWidget = CreateWidget<UArenaDuelAdminWidget>(this, UArenaDuelAdminWidget::StaticClass());
		if (!AdminWidget) return;
		FOnArenaDuelAdminAction Action;
		Action.BindUObject(this, &AArenaDuelPlayerController::HandleAdminWidgetAction);
		AdminWidget->SetActionHandler(MoveTemp(Action));
	}
	bPreviousMoveInputIgnored = IsMoveInputIgnored();
	bPreviousLookInputIgnored = IsLookInputIgnored();
	bAdminMenuOpen = true;
	SetIgnoreMoveInput(true);
	SetIgnoreLookInput(true);
	bShowMouseCursor = true;
	if (AArenaDuelCharacter* ControlledCharacter = Cast<AArenaDuelCharacter>(GetPawn()))
	{
		if (UArenaDuelWeaponComponent* Weapon = ControlledCharacter->GetWeaponComponent())
		{
			Weapon->StopFire();
			Weapon->StopAim();
		}
	}
	AdminWidget->AddToViewport(100);
	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(AdminWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
	AdminWidget->SetKeyboardFocus();
#endif
}

void AArenaDuelPlayerController::HandleEscape()
{
	if (bPlayerMenuOpen) { if (PlayerMenuWidget) PlayerMenuWidget->HandleEscape(); return; }
	if (bCharacterSelectOpen) return;
	if (bAdminMenuOpen) { CloseAdminMenu(); return; }
	OpenPlayerMenu(false);
}

void AArenaDuelPlayerController::OpenPlayerMenu(bool bFromCharacterSelect)
{
	if (!IsLocalController()) return;
	CloseAdminMenu();
	if (!PlayerMenuWidget)
	{
		PlayerMenuWidget = CreateWidget<UArenaDuelPlayerMenuWidget>(this, UArenaDuelPlayerMenuWidget::StaticClass());
		if (!PlayerMenuWidget) return;
	}
	bPlayerMenuOpen = true;
	bPlayerMenuFromCharacterSelect = bFromCharacterSelect;
	SetIgnoreMoveInput(true); SetIgnoreLookInput(true); bShowMouseCursor = true;
	if (AArenaDuelCharacter* PlayerCharacter = Cast<AArenaDuelCharacter>(GetPawn()))
		if (UArenaDuelWeaponComponent* Weapon = PlayerCharacter->GetWeaponComponent()) { Weapon->StopFire(); Weapon->StopAim(); }
	PlayerMenuWidget->Configure(bFromCharacterSelect);
	if (bFromCharacterSelect) PlayerMenuWidget->ShowSettings();
	else PlayerMenuWidget->ShowPause();
	PlayerMenuWidget->AddToViewport(200);
	FInputModeGameAndUI Mode; Mode.SetWidgetToFocus(PlayerMenuWidget->TakeWidget()); Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock); Mode.SetHideCursorDuringCapture(false); SetInputMode(Mode); PlayerMenuWidget->SetKeyboardFocus();
}

void AArenaDuelPlayerController::ClosePlayerMenu()
{
	if (!bPlayerMenuOpen) return;
	bPlayerMenuOpen = false;
	if (PlayerMenuWidget) PlayerMenuWidget->RemoveFromParent();
	SetIgnoreMoveInput(false); SetIgnoreLookInput(false);
	if (bPlayerMenuFromCharacterSelect && CharacterSelectWidget && bCharacterSelectOpen)
	{
		bShowMouseCursor=true; FInputModeUIOnly Mode; Mode.SetWidgetToFocus(CharacterSelectWidget->TakeWidget()); Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock); SetInputMode(Mode); CharacterSelectWidget->SetKeyboardFocus();
	}
	else { bShowMouseCursor=false; SetInputMode(FInputModeGameOnly()); }
}

void AArenaDuelPlayerController::ApplyLocalSettings(const FArenaDuelLocalSettings& NewSettings, bool bSave)
{
	if (!IsLocalController()) return;
	LocalSettings=NewSettings; LocalSettings.MouseSensitivity=FMath::Clamp(LocalSettings.MouseSensitivity,0.10f,5.0f); LocalSettings.ADSMultiplier=FMath::Clamp(LocalSettings.ADSMultiplier,0.25f,1.5f); LocalSettings.FOV=FMath::Clamp(LocalSettings.FOV,80.0f,110.0f); LocalSettings.MasterVolume=FMath::Clamp(LocalSettings.MasterVolume,0.0f,1.0f);
	if (bSave || UArenaDuelLocalSettingsSave::HasSavedSettings())
	if (UGameUserSettings* Display=GEngine?GEngine->GetGameUserSettings():nullptr)
	{
		Display->SetFullscreenMode(static_cast<EWindowMode::Type>(FMath::Clamp(LocalSettings.WindowMode,0,2))); Display->SetScreenResolution(FIntPoint(LocalSettings.ResolutionX,LocalSettings.ResolutionY)); Display->SetVSyncEnabled(LocalSettings.bVSync); Display->SetFrameRateLimit(static_cast<float>(FMath::Max(0,LocalSettings.FPSLimit))); Display->ApplySettings(false); if(bSave)Display->SaveSettings();
	}
	if (GEngine) { FAudioDeviceHandle Device=GEngine->GetMainAudioDevice(); if(Device.IsValid())Device->SetTransientPrimaryVolume(LocalSettings.MasterVolume); }
	ApplySettingsToPawn(); if(bSave)UArenaDuelLocalSettingsSave::SaveSettings(LocalSettings);
}

void AArenaDuelPlayerController::ApplySettingsToPawn()
{
	if (AArenaDuelCharacter* CurrentCharacter=Cast<AArenaDuelCharacter>(GetPawn()))
	{
		if(UArenaDuelWeaponComponent* Weapon=CurrentCharacter->GetWeaponComponent()) Weapon->SetUserHipFOV(LocalSettings.FOV);
		else if(CurrentCharacter->GetFirstPersonCamera()) CurrentCharacter->GetFirstPersonCamera()->SetFieldOfView(LocalSettings.FOV);
	}
}

void AArenaDuelPlayerController::CloseAdminMenu()
{
	if (!bAdminMenuOpen) return;
	bAdminMenuOpen = false;
	if (AdminWidget) AdminWidget->RemoveFromParent();
	bShowMouseCursor = false;
	SetIgnoreMoveInput(bPreviousMoveInputIgnored);
	SetIgnoreLookInput(bPreviousLookInputIgnored);
	SetInputMode(FInputModeGameOnly());
}

void AArenaDuelPlayerController::HandleAdminWidgetAction(EArenaDuelAdminCommand Command, float NumericValue)
{
	if (Command == EArenaDuelAdminCommand::ToggleDebugOverlay || Command == EArenaDuelAdminCommand::ToggleHitZones)
	{
		if (!CanUseDevelopmentAdmin()) return;
		if (AArenaDuelMovementDebugHUD* DebugHUD = Cast<AArenaDuelMovementDebugHUD>(GetHUD()))
		{
			if (Command == EArenaDuelAdminCommand::ToggleDebugOverlay) DebugHUD->SetShowDebugOverlay(!DebugHUD->IsShowingDebugOverlay());
			else DebugHUD->SetShowHitZones(!DebugHUD->IsShowingHitZones());
		}
		return;
	}
	SubmitAdminCommand(Command, AdminWidget ? AdminWidget->GetSelectedDuelSlot() : 0, NumericValue);
}

void AArenaDuelPlayerController::SubmitAdminCommand(EArenaDuelAdminCommand Command, uint8 TargetDuelSlot, float NumericValue)
{
	if (!CanUseDevelopmentAdmin()) return;
	if (HasAuthority()) ExecuteAdminCommandAuthoritatively(Command, TargetDuelSlot, NumericValue);
	else ServerExecuteAdminCommand(Command, TargetDuelSlot, NumericValue);
}

void AArenaDuelPlayerController::ServerExecuteAdminCommand_Implementation(EArenaDuelAdminCommand Command, uint8 TargetDuelSlot, float NumericValue)
{
	if (!CanUseDevelopmentAdmin()) return;
	ExecuteAdminCommandAuthoritatively(Command, TargetDuelSlot, NumericValue);
}

void AArenaDuelPlayerController::ExecuteAdminCommandAuthoritatively(EArenaDuelAdminCommand Command, uint8 TargetDuelSlot, float NumericValue)
{
#if UE_BUILD_SHIPPING
	return;
#else
	if (!HasAuthority() || !CanUseDevelopmentAdmin() || !GetWorld() || TargetDuelSlot > 1 || !FMath::IsFinite(NumericValue) || Command >= EArenaDuelAdminCommand::ToggleDebugOverlay) return;
	AArenaDuelGameState* GameState = GetWorld()->GetGameState<AArenaDuelGameState>();
	AArenaDuelGameMode* GameMode = GetWorld()->GetAuthGameMode<AArenaDuelGameMode>();
	AArenaDuelPlayerState* TargetState = nullptr;
	AArenaDuelCharacter* TargetCharacter = nullptr;
	if (TargetDuelSlot <= 1 && GameState)
	{
		for (APlayerState* State : GameState->PlayerArray)
		{
			AArenaDuelPlayerState* DuelState = Cast<AArenaDuelPlayerState>(State);
			if (DuelState && DuelState->GetDuelSlot() == TargetDuelSlot)
			{
				TargetState = DuelState;
				for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
				{
					APlayerController* Controller = It->Get();
					if (Controller && Controller->PlayerState == TargetState) { TargetCharacter = Cast<AArenaDuelCharacter>(Controller->GetPawn()); break; }
				}
				break;
			}
		}
	}
	const int32 BoundedValue = FMath::RoundToInt(FMath::Clamp(NumericValue, 0.0f, 5.0f));
	switch (Command)
	{
	case EArenaDuelAdminCommand::FullHeal:
		if (TargetCharacter && !TargetCharacter->IsDead()) TargetCharacter->AdminSetHealth(TargetCharacter->GetMaxHealth());
		break;
	case EArenaDuelAdminCommand::SetHealth:
		if (TargetCharacter && !TargetCharacter->IsDead()) TargetCharacter->AdminSetHealth(NumericValue);
		break;
	case EArenaDuelAdminCommand::Kill:
		if (TargetCharacter) TargetCharacter->AdminKill();
		break;
	case EArenaDuelAdminCommand::ToggleGodMode:
		if (TargetState) TargetState->ToggleAdminGodMode();
		break;
	case EArenaDuelAdminCommand::ResetPlayer:
		if (TargetCharacter && !TargetCharacter->IsDead()) TargetCharacter->AdminResetPlayer();
		break;
	case EArenaDuelAdminCommand::EquipWeapon:
		if (TargetCharacter && TargetCharacter->GetWeaponComponent() && NumericValue >= 0.0f && NumericValue < 4.0f && FMath::IsNearlyEqual(NumericValue, static_cast<float>(FMath::RoundToInt(NumericValue)))) TargetCharacter->GetWeaponComponent()->GrantAllWeaponsForDevelopment(), TargetCharacter->GetWeaponComponent()->EquipWeapon(FMath::RoundToInt(NumericValue));
		break;
	case EArenaDuelAdminCommand::RefillAmmo:
		if (TargetCharacter && TargetCharacter->GetWeaponComponent()) TargetCharacter->GetWeaponComponent()->RefillAllAmmoForDevelopment();
		break;
	case EArenaDuelAdminCommand::ToggleInfiniteAmmo:
		if (TargetState) TargetState->ToggleAdminInfiniteAmmo();
		break;
	case EArenaDuelAdminCommand::RestartRound:
		if (GameMode) GameMode->AdminRestartRound();
		break;
	case EArenaDuelAdminCommand::NextRound:
		if (GameMode) GameMode->AdminAdvanceRound();
		break;
	case EArenaDuelAdminCommand::AwardRound:
		if (GameMode && NumericValue >= 0.0f && NumericValue <= 1.0f && FMath::IsNearlyEqual(NumericValue, static_cast<float>(FMath::RoundToInt(NumericValue)))) GameMode->AdminAwardRound(static_cast<uint8>(FMath::RoundToInt(NumericValue)));
		break;
	case EArenaDuelAdminCommand::SetPlayer1Wins:
		if (GameMode) if (AArenaDuelPlayerState* State = GameMode->FindPlayerStateByDuelSlot(0)) State->SetRoundWinsForDevelopment(BoundedValue);
		break;
	case EArenaDuelAdminCommand::SetPlayer2Wins:
		if (GameMode) if (AArenaDuelPlayerState* State = GameMode->FindPlayerStateByDuelSlot(1)) State->SetRoundWinsForDevelopment(BoundedValue);
		break;
	case EArenaDuelAdminCommand::ResetMatch:
		if (GameMode) GameMode->AdminResetMatch();
		break;
	case EArenaDuelAdminCommand::RefillStamina:
		if (TargetCharacter && TargetCharacter->GetArenaDuelMovementComponent()) TargetCharacter->GetArenaDuelMovementComponent()->RefillStaminaForDevelopment();
		break;
	case EArenaDuelAdminCommand::ToggleInfiniteStamina:
		if (TargetState) TargetState->ToggleAdminInfiniteStamina();
		break;
	case EArenaDuelAdminCommand::SetArchetypeShadow:
		if (TargetState) TargetState->SetCharacterArchetypeForDevelopment(EArenaDuelCharacterArchetype::Shadow);
		break;
	case EArenaDuelAdminCommand::SetArchetypeWarden:
		if (TargetState) TargetState->SetCharacterArchetypeForDevelopment(EArenaDuelCharacterArchetype::Warden);
		break;
	case EArenaDuelAdminCommand::SetArchetypeRift:
		if (TargetState) TargetState->SetCharacterArchetypeForDevelopment(EArenaDuelCharacterArchetype::Rift);
		break;
	case EArenaDuelAdminCommand::SurvivalKillAll:
		// Everything of the wave, also what has not spawned yet; killing only the living leaves the queue to follow.
		if (AArenaDuelZombieGameMode* Survival = Cast<AArenaDuelZombieGameMode>(GameMode)) Survival->DevFinishWave();
		break;
	case EArenaDuelAdminCommand::SurvivalFinishWave:
		if (AArenaDuelZombieGameMode* Survival = Cast<AArenaDuelZombieGameMode>(GameMode)) Survival->DevNextWaveNow();
		break;
	case EArenaDuelAdminCommand::SurvivalAddPoints:
		if (TargetState && Cast<AArenaDuelZombieGameMode>(GameMode)) TargetState->AddSurvivalPoints(5000);
		break;
	case EArenaDuelAdminCommand::SurvivalToggleHarmless:
		if (AArenaDuelZombieGameMode* Survival = Cast<AArenaDuelZombieGameMode>(GameMode)) Survival->DevSetZombieDamageScale(Survival->GetZombieDamageScale() > 0.0f ? 0.0f : 1.0f);
		break;
	case EArenaDuelAdminCommand::SurvivalRestart:
		if (AArenaDuelZombieGameMode* Survival = Cast<AArenaDuelZombieGameMode>(GameMode)) Survival->RestartSurvival();
		break;
	default: break;
	}
#endif
}
