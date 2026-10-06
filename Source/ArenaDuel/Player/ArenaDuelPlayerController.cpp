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
#include "../Weapons/ArenaDuelWeaponComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "InputCoreTypes.h"
#include "TimerManager.h"

void AArenaDuelPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::F1, IE_Pressed, this, &AArenaDuelPlayerController::ToggleAdminMenu);
		InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AArenaDuelPlayerController::CloseAdminMenu);
	}
}

void AArenaDuelPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalController())
	{
		GetWorldTimerManager().SetTimer(MatchPresentationTimer, this, &AArenaDuelPlayerController::RefreshMatchPresentation, 0.2f, true);
		RefreshMatchPresentation();
	}
}

void AArenaDuelPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CloseAdminMenu();
	GetWorldTimerManager().ClearTimer(MatchPresentationTimer);
	if (CharacterSelectWidget) CharacterSelectWidget->RemoveFromParent();
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
	const ENetMode NetMode = GetNetMode();
	return HasAuthority() && IsLocalController() && (NetMode == NM_Standalone || NetMode == NM_ListenServer);
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
		if (TargetCharacter && TargetCharacter->GetWeaponComponent() && NumericValue >= 0.0f && NumericValue < 4.0f && FMath::IsNearlyEqual(NumericValue, static_cast<float>(FMath::RoundToInt(NumericValue)))) TargetCharacter->GetWeaponComponent()->EquipWeapon(FMath::RoundToInt(NumericValue));
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
	default: break;
	}
#endif
}
