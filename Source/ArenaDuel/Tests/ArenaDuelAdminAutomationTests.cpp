// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"
#include "Tests/AutomationEditorCommon.h"

#include "ArenaDuel/Characters/ArenaDuelCharacter.h"
#include "ArenaDuel/Characters/ArenaDuelCharacterMovementComponent.h"
#include "ArenaDuel/Game/ArenaDuelGameMode.h"
#include "ArenaDuel/Game/ArenaDuelGameState.h"
#include "ArenaDuel/Player/ArenaDuelPlayerController.h"
#include "ArenaDuel/Player/ArenaDuelPlayerState.h"
#include "ArenaDuel/UI/ArenaDuelAdminWidget.h"
#include "ArenaDuel/Weapons/ArenaDuelWeaponComponent.h"
#include "Components/Overlay.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaDuelAdminWidgetConstructionTest, "ArenaDuel.Admin.NativeWidget", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FArenaDuelAdminWidgetConstructionTest::RunTest(const FString& Parameters)
{
	UArenaDuelAdminWidget* Widget = NewObject<UArenaDuelAdminWidget>(GetTransientPackage());
	TestNotNull(TEXT("Native admin widget is created"), Widget);
	if (!Widget) return false;
	TestTrue(TEXT("Native admin widget initializes"), Widget->Initialize());
	TestTrue(TEXT("Admin widget is focusable"), Widget->IsFocusable());
	TestTrue(TEXT("Widget uses a fullscreen overlay root"), Cast<UOverlay>(Widget->GetRootWidget()) != nullptr);
	TestTrue(TEXT("Player selector has both duel slots"), Widget->HasPlayerSelector());
	TestEqual(TEXT("All five useful admin sections are constructed"), Widget->GetAdminSectionCount(), 5);
	TestFalse(TEXT("Repeated initialization does not rebuild the admin tree"), Widget->Initialize());
	return true;
}

namespace ArenaDuelAdminAutomation
{
	static constexpr TCHAR GameModeClassPath[] = TEXT("/Game/ArenaDuel/Game/BP_ArenaDuelGameMode.BP_ArenaDuelGameMode_C");

	static AArenaDuelPlayerController* FindControllerForSlot(UWorld* World, uint8 Slot)
	{
		if (!World) return nullptr;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			AArenaDuelPlayerController* Controller = Cast<AArenaDuelPlayerController>(It->Get());
			const AArenaDuelPlayerState* State = Controller ? Controller->GetPlayerState<AArenaDuelPlayerState>() : nullptr;
			if (State && State->GetDuelSlot() == Slot) return Controller;
		}
		return nullptr;
	}

	static int32 CountWins(const AArenaDuelGameState* GameState)
	{
		int32 Total = 0;
		if (GameState) for (APlayerState* State : GameState->PlayerArray) if (const AArenaDuelPlayerState* DuelState = Cast<AArenaDuelPlayerState>(State)) Total += DuelState->GetRoundWins();
		return Total;
	}
}

struct FArenaDuelAdminNetworkState : public FBasePIENetworkComponentState
{
	AArenaDuelPlayerController* HostController = nullptr;
	AArenaDuelPlayerController* RemoteController = nullptr;
	uint8 TargetSlot = 1;
	int32 InitialRound = 1;
};

NETWORK_TEST_CLASS(FArenaDuelAdminNetworkTest, "ArenaDuel.Admin.Network")
{
	FPIENetworkComponent<FArenaDuelAdminNetworkState> Network{ TestRunner, TestCommandBuilder, bInitializing };

	BEFORE_EACH()
	{
		UClass* GameModeClass = LoadClass<AGameModeBase>(nullptr, ArenaDuelAdminAutomation::GameModeClassPath);
		FNetworkComponentBuilder<FArenaDuelAdminNetworkState>()
			.WithClients(1)
			.AsListenServer()
			.WithGameMode(GameModeClass)
			.Build(Network);
	}

	TEST_METHOD(HostAdminCommandsAndRemoteAuthorization)
	{
		Network
			.UntilServer(TEXT("Find host and remote player controllers"), [](FArenaDuelAdminNetworkState& State)
			{
				State.HostController = nullptr;
				State.RemoteController = nullptr;
				for (FConstPlayerControllerIterator It = State.World->GetPlayerControllerIterator(); It; ++It)
				{
					AArenaDuelPlayerController* Controller = Cast<AArenaDuelPlayerController>(It->Get());
					if (!Controller || !Controller->GetPawn()) continue;
					if (Controller->CanUseDevelopmentAdmin()) State.HostController = Controller;
					else State.RemoteController = Controller;
				}
				const AArenaDuelGameState* GameState = State.World->GetGameState<AArenaDuelGameState>();
				return State.HostController && State.RemoteController && GameState && GameState->PlayerArray.Num() == 2;
			}, FTimespan::FromSeconds(10.0))
			.ThenServer(TEXT("Exercise authoritative admin actions and deny remote requester"), [this](FArenaDuelAdminNetworkState& State)
			{
				AArenaDuelPlayerController* Host = State.HostController;
				AArenaDuelPlayerController* Remote = State.RemoteController;
				AArenaDuelPlayerState* HostState = Host ? Host->GetPlayerState<AArenaDuelPlayerState>() : nullptr;
				AArenaDuelPlayerState* RemoteState = Remote ? Remote->GetPlayerState<AArenaDuelPlayerState>() : nullptr;
				AArenaDuelCharacter* RemotePawn = Remote ? Cast<AArenaDuelCharacter>(Remote->GetPawn()) : nullptr;
				AArenaDuelGameState* GameState = State.World->GetGameState<AArenaDuelGameState>();
				if (!Host || !Remote || !HostState || !RemoteState || !RemotePawn || !GameState)
				{
					TestRunner->AddError(TEXT("Admin smoke test could not resolve listen host, remote player, or pawn."));
					return;
				}
				State.TargetSlot = RemoteState->GetDuelSlot();
				State.InitialRound = GameState->GetRoundNumber();
				if (!Host->CanUseDevelopmentAdmin()) TestRunner->AddError(TEXT("Listen-server host was not authorized."));
				if (Remote->CanUseDevelopmentAdmin()) TestRunner->AddError(TEXT("Remote client's server PlayerController was incorrectly authorized."));

				const int32 InitialHealth = FMath::RoundToInt(RemotePawn->GetHealth());
				Remote->ServerExecuteAdminCommand(EArenaDuelAdminCommand::SetHealth, State.TargetSlot, 17.0f);
				if (!FMath::IsNearlyEqual(RemotePawn->GetHealth(), static_cast<float>(InitialHealth))) TestRunner->AddError(TEXT("Unauthorized remote admin RPC changed target health."));

				Host->SubmitAdminCommand(EArenaDuelAdminCommand::ToggleGodMode, State.TargetSlot, 0.0f);
				RemotePawn->ApplyServerDamage(25.0f);
				if (!RemoteState->HasAdminGodMode() || !FMath::IsNearlyEqual(RemotePawn->GetHealth(), static_cast<float>(InitialHealth))) TestRunner->AddError(TEXT("God Mode did not block normal server weapon damage."));
				Host->SubmitAdminCommand(EArenaDuelAdminCommand::SetHealth, State.TargetSlot, 42.0f);
				if (!FMath::IsNearlyEqual(RemotePawn->GetHealth(), 42.0f)) TestRunner->AddError(TEXT("Admin Set Health did not bypass God Mode."));
				Host->SubmitAdminCommand(EArenaDuelAdminCommand::FullHeal, State.TargetSlot, 0.0f);
				if (!FMath::IsNearlyEqual(RemotePawn->GetHealth(), RemotePawn->GetMaxHealth())) TestRunner->AddError(TEXT("Full Heal failed."));
				Host->SubmitAdminCommand(EArenaDuelAdminCommand::ResetPlayer, State.TargetSlot, 0.0f);
				if (RemotePawn->IsDead() || !FMath::IsNearlyEqual(RemotePawn->GetHealth(), RemotePawn->GetMaxHealth())) TestRunner->AddError(TEXT("Reset Player did not restore a live target."));

				UArenaDuelWeaponComponent* Weapon = RemotePawn->GetWeaponComponent();
				const EArenaDuelWeaponId ExpectedWeapons[] = { EArenaDuelWeaponId::ArcRifle, EArenaDuelWeaponId::ShadeSMG, EArenaDuelWeaponId::RuneDMR, EArenaDuelWeaponId::HexShotgun };
				for (int32 Index = 0; Index < UE_ARRAY_COUNT(ExpectedWeapons); ++Index)
				{
					Host->SubmitAdminCommand(EArenaDuelAdminCommand::EquipWeapon, State.TargetSlot, static_cast<float>(Index));
					if (!Weapon || Weapon->GetCurrentWeaponId() != ExpectedWeapons[Index]) TestRunner->AddError(TEXT("Server-authoritative equip did not select the requested weapon."));
				}
				Host->SubmitAdminCommand(EArenaDuelAdminCommand::RefillAmmo, State.TargetSlot, 0.0f);
				if (Weapon && (Weapon->GetCurrentMagazineAmmo() != Weapon->GetCurrentDefinition().MagazineCapacity || Weapon->GetReserveAmmo() != Weapon->GetCurrentDefinition().ReserveCapacity)) TestRunner->AddError(TEXT("Refill Ammo did not restore the selected weapon."));
				Host->SubmitAdminCommand(EArenaDuelAdminCommand::ToggleInfiniteAmmo, State.TargetSlot, 0.0f);
				const int32 AmmoBefore = Weapon ? Weapon->GetCurrentMagazineAmmo() : 0;
				if (Weapon) { Weapon->StartFire(); Weapon->StopFire(); }
				if (!RemoteState->HasAdminInfiniteAmmo() || !Weapon || Weapon->GetCurrentMagazineAmmo() != AmmoBefore) TestRunner->AddError(TEXT("Infinite Ammo did not preserve authoritative magazine state."));
				Host->SubmitAdminCommand(EArenaDuelAdminCommand::ToggleInfiniteStamina, State.TargetSlot, 0.0f);
				if (UArenaDuelCharacterMovementComponent* Movement = RemotePawn->GetArenaDuelMovementComponent())
				{
					Movement->ConsumeStamina(35.0f);
					if (!RemoteState->HasAdminInfiniteStamina() || Movement->GetStamina() < Movement->GetMaxStamina() - 0.1f) TestRunner->AddError(TEXT("Infinite Stamina did not hold the authoritative stamina value."));
				}

				Host->SubmitAdminCommand(EArenaDuelAdminCommand::SetPlayer1Wins, 0, 2.0f);
				Host->SubmitAdminCommand(EArenaDuelAdminCommand::SetPlayer2Wins, 0, 3.0f);
				AArenaDuelPlayerController* Player1Controller = ArenaDuelAdminAutomation::FindControllerForSlot(State.World, 0);
				AArenaDuelPlayerController* Player2Controller = ArenaDuelAdminAutomation::FindControllerForSlot(State.World, 1);
				AArenaDuelPlayerState* Player1State = Player1Controller ? Player1Controller->GetPlayerState<AArenaDuelPlayerState>() : nullptr;
				AArenaDuelPlayerState* Player2State = Player2Controller ? Player2Controller->GetPlayerState<AArenaDuelPlayerState>() : nullptr;
				if (!Player1State || !Player2State || Player1State->GetRoundWins() != 2 || Player2State->GetRoundWins() != 3 || !GameState->IsRoundInProgress()) TestRunner->AddError(TEXT("Direct score setting did not clamp/update scores without ending the round."));

				Host->SubmitAdminCommand(EArenaDuelAdminCommand::RestartRound, 0, 0.0f);
				if (!GameState->IsRoundInProgress() || GameState->GetRoundNumber() != State.InitialRound || !Player1State || !Player2State || Player1State->GetRoundWins() != 2 || Player2State->GetRoundWins() != 3) TestRunner->AddError(TEXT("Restart Round incorrectly changed round number or wins."));
				Host->SubmitAdminCommand(EArenaDuelAdminCommand::NextRound, 0, 0.0f);
				if (!GameState->IsRoundInProgress() || GameState->GetRoundNumber() != State.InitialRound + 1) TestRunner->AddError(TEXT("Next Round did not advance the round without ending play."));
				Host->SubmitAdminCommand(EArenaDuelAdminCommand::ResetMatch, 0, 0.0f);
				if (GameState->GetRoundNumber() != 1 || !GameState->IsRoundInProgress() || ArenaDuelAdminAutomation::CountWins(GameState) != 0) TestRunner->AddError(TEXT("Reset Match failed to return to round one and 0:0."));

				Remote = ArenaDuelAdminAutomation::FindControllerForSlot(State.World, State.TargetSlot);
				RemoteState = Remote ? Remote->GetPlayerState<AArenaDuelPlayerState>() : nullptr;
				RemotePawn = Remote ? Cast<AArenaDuelCharacter>(Remote->GetPawn()) : nullptr;
				Host = ArenaDuelAdminAutomation::FindControllerForSlot(State.World, HostState->GetDuelSlot());
				if (Host) Host->SubmitAdminCommand(EArenaDuelAdminCommand::Kill, State.TargetSlot, 0.0f);
				if (!GameState || GameState->IsRoundInProgress() || !RemotePawn || !RemotePawn->IsDead() || !RemoteState || !RemoteState->HasAdminGodMode() || ArenaDuelAdminAutomation::CountWins(GameState) != 1) TestRunner->AddError(TEXT("Admin Kill did not bypass God Mode through the normal round-death path."));
			})
			.UntilServer(TEXT("Normal restart follows admin-awarded death"), [](FArenaDuelAdminNetworkState& State)
			{
				const AArenaDuelGameState* GameState = State.World->GetGameState<AArenaDuelGameState>();
				const AArenaDuelPlayerController* Target = ArenaDuelAdminAutomation::FindControllerForSlot(State.World, State.TargetSlot);
				const AArenaDuelCharacter* Pawn = Target ? Cast<AArenaDuelCharacter>(Target->GetPawn()) : nullptr;
				return GameState && GameState->GetRoundNumber() == 2 && GameState->IsRoundInProgress() && ArenaDuelAdminAutomation::CountWins(GameState) == 1 && Pawn && !Pawn->IsDead() && FMath::IsNearlyEqual(Pawn->GetHealth(), Pawn->GetMaxHealth());
			}, FTimespan::FromSeconds(8.0));
	}
};

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
