#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"
#include "Tests/AutomationEditorCommon.h"

#include "ArenaDuel/Characters/ArenaDuelCharacter.h"
#include "ArenaDuel/Game/ArenaDuelGameMode.h"
#include "ArenaDuel/Player/ArenaDuelPlayerController.h"
#include "ArenaDuelTestRound.h"

#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "GameMapsSettings.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/SoftObjectPath.h"

namespace ArenaDuelPhase3Tests
{
	static constexpr TCHAR TestMap[] = TEXT("/Game/ArenaDuel/Maps/L_Phase3Test");
	static constexpr TCHAR GameModeClass[] = TEXT("/Game/ArenaDuel/Game/BP_ArenaDuelGameMode.BP_ArenaDuelGameMode_C");
	static constexpr TCHAR MappingContext[] = TEXT("/Game/ArenaDuel/Input/IMC_Gameplay.IMC_Gameplay");
	static constexpr TCHAR MoveAction[] = TEXT("/Game/ArenaDuel/Input/IA_Move.IA_Move");
	static constexpr TCHAR LookAction[] = TEXT("/Game/ArenaDuel/Input/IA_Look.IA_Look");
	static constexpr TCHAR JumpAction[] = TEXT("/Game/ArenaDuel/Input/IA_Jump.IA_Jump");

	static AArenaDuelCharacter* FindCharacter(UWorld* World, const bool bLocallyControlled, AArenaDuelCharacter* Exclude = nullptr)
	{
		for (TActorIterator<AArenaDuelCharacter> It(World); It; ++It)
		{
			AArenaDuelCharacter* Character = *It;
			if (Character != Exclude && Character->IsLocallyControlled() == bLocallyControlled)
			{
				return Character;
			}
		}
		return nullptr;
	}

	static UInputAction* LoadAction(const TCHAR* Path)
	{
		return LoadObject<UInputAction>(nullptr, Path);
	}

	static UInputMappingContext* LoadContext()
	{
		return LoadObject<UInputMappingContext>(nullptr, MappingContext);
	}

	static UEnhancedInputLocalPlayerSubsystem* GetLocalInputSubsystem(APlayerController* Controller)
	{
		if (!Controller || !Controller->GetLocalPlayer())
		{
			return nullptr;
		}
		return Controller->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaDuelPhase3MapRuntimeTest, "ArenaDuel.Phase3.MapRuntime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FArenaDuelPhase3MapRuntimeTest::RunTest(const FString& Parameters)
{
	FAutomationEditorCommonUtils::LoadMap(ArenaDuelPhase3Tests::TestMap);
	UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	TestNotNull(TEXT("Editor world loaded"), EditorWorld);
	if (!EditorWorld)
	{
		return false;
	}
	TestEqual(TEXT("Loaded map"), EditorWorld->GetOutermost()->GetName(), FString(ArenaDuelPhase3Tests::TestMap));
	TestEqual(TEXT("Configured default GameMode uses the Blueprint class"),
		UGameMapsSettings::GetGlobalDefaultGameMode(), FString(ArenaDuelPhase3Tests::GameModeClass));
	return true;
}

struct FArenaDuelPhase3NetworkState : public FBasePIENetworkComponentState
{
	AArenaDuelCharacter* OwnedPawn = nullptr;
	AArenaDuelCharacter* RemotePawn = nullptr;
	FVector OwnedInitialLocation = FVector::ZeroVector;
	FVector RemoteInitialLocation = FVector::ZeroVector;
	float OwnedInitialYaw = 0.0f;
	float OwnedInitialPitch = 0.0f;
	float OwnedPitchAfterMouseUp = 0.0f;
	float OwnedInitialServerLocationX = 0.0f;
	float OwnedInitialServerYaw = 0.0f;
};

NETWORK_TEST_CLASS(FArenaDuelPhase3NetworkTest, "ArenaDuel.Phase3.Network")
{
	FPIENetworkComponent<FArenaDuelPhase3NetworkState> Network{TestRunner, TestCommandBuilder, bInitializing};

	BEFORE_EACH()
	{
		UClass* GameModeClass = LoadClass<AGameModeBase>(nullptr, ArenaDuelPhase3Tests::GameModeClass);
		FNetworkComponentBuilder<FArenaDuelPhase3NetworkState>()
			.WithClients(1)
			.AsListenServer()
			.WithGameMode(GameModeClass)
			.Build(Network);
	}

	TEST_METHOD(FrameworkAndInputOwnership)
	{
		Network
			.UntilServer(TEXT("Both players are ready and the round is live"), [](FArenaDuelPhase3NetworkState& State) { return ArenaDuelTestRound::EnsureRoundInProgress(State.World); }, FTimespan::FromSeconds(12.0))
			.ThenServer(TEXT("Validate server framework"), [this](FArenaDuelPhase3NetworkState& State)
			{
				TArray<AArenaDuelCharacter*> Characters;
				for (TActorIterator<AArenaDuelCharacter> It(State.World); It; ++It)
				{
					Characters.Add(*It);
				}
				if (Characters.Num() != 2)
				{
					TestRunner->AddError(FString::Printf(TEXT("Expected 2 server Characters, got %d"), Characters.Num()));
					return;
				}
				for (AArenaDuelCharacter* Character : Characters)
				{
					if (!Character->GetController())
					{
						TestRunner->AddError(TEXT("Server Character has no controller"));
					}
					if (!Character->GetIsReplicated())
					{
						TestRunner->AddError(TEXT("Server Character is not replicated"));
					}
					if (!Character->FindComponentByClass<UCameraComponent>())
					{
						TestRunner->AddError(TEXT("Server Character lacks FirstPersonCamera"));
					}
				}
			})
			.UntilClient(TEXT("Wait for replicated Characters"), 0, [](FArenaDuelPhase3NetworkState& State)
			{
				APlayerController* Controller = State.World->GetFirstPlayerController();
				State.OwnedPawn = Controller ? Cast<AArenaDuelCharacter>(Controller->GetPawn()) : nullptr;
				State.RemotePawn = ArenaDuelPhase3Tests::FindCharacter(State.World, false, State.OwnedPawn);
				return State.OwnedPawn != nullptr && State.RemotePawn != nullptr;
			}, FTimespan::FromSeconds(5.0))
			.UntilClient(TEXT("Wait for local Enhanced Input mapping"), 0, [](FArenaDuelPhase3NetworkState& State)
			{
				APlayerController* Controller = State.World->GetFirstPlayerController();
				UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ArenaDuelPhase3Tests::GetLocalInputSubsystem(Controller);
				UInputMappingContext* Context = ArenaDuelPhase3Tests::LoadContext();
				return InputSubsystem && Context && InputSubsystem->HasMappingContext(Context);
			}, FTimespan::FromSeconds(5.0))
			.ThenClient(TEXT("Validate local and remote possession"), 0, [](FArenaDuelPhase3NetworkState& State)
			{
				APlayerController* Controller = State.World->GetFirstPlayerController();
				if (!Controller || !Controller->GetPawn())
				{
					TestRunner->AddError(TEXT("Client has no possessed Pawn"));
					return;
				}
				State.OwnedPawn = Cast<AArenaDuelCharacter>(Controller->GetPawn());
				State.RemotePawn = ArenaDuelPhase3Tests::FindCharacter(State.World, false, State.OwnedPawn);
				if (!State.OwnedPawn || !State.RemotePawn)
				{
					TestRunner->AddError(TEXT("Client did not receive distinct owned and remote ArenaDuel Characters"));
					return;
				}
				UCameraComponent* Camera = State.OwnedPawn->FindComponentByClass<UCameraComponent>();
				if (!Camera || Camera->GetFName() != TEXT("FirstPersonCamera"))
				{
					TestRunner->AddError(TEXT("Owned Character lacks FirstPersonCamera"));
				}
				if (State.OwnedPawn->FindComponentByClass<USpringArmComponent>())
				{
					TestRunner->AddError(TEXT("Owned Character contains an unexpected SpringArm"));
				}
				UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ArenaDuelPhase3Tests::GetLocalInputSubsystem(Controller);
				if (!InputSubsystem || !InputSubsystem->HasMappingContext(ArenaDuelPhase3Tests::LoadContext()))
				{
					TestRunner->AddError(TEXT("Local Enhanced Input Mapping Context is not active"));
				}
				if (State.RemotePawn->IsLocallyControlled() || State.RemotePawn->GetController() == Controller)
				{
					TestRunner->AddError(TEXT("Remote Character incorrectly has local input ownership"));
				}
				State.OwnedInitialLocation = State.OwnedPawn->GetActorLocation();
				State.RemoteInitialLocation = State.RemotePawn->GetActorLocation();
			})
			.ThenClient(TEXT("Inject forward movement"), 0, [](FArenaDuelPhase3NetworkState& State)
			{
				APlayerController* Controller = State.World->GetFirstPlayerController();
				if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ArenaDuelPhase3Tests::GetLocalInputSubsystem(Controller))
				{
					InputSubsystem->StartContinuousInputInjectionForAction(ArenaDuelPhase3Tests::LoadAction(ArenaDuelPhase3Tests::MoveAction), FInputActionValue(FVector2D(0.0f, 1.0f)), {}, {});
				}
			})
			.UntilClient(TEXT("Forward movement occurs"), 0, [](FArenaDuelPhase3NetworkState& State)
			{
				return State.OwnedPawn && FVector::Dist2D(State.OwnedPawn->GetActorLocation(), State.OwnedInitialLocation) > 10.0f;
			}, FTimespan::FromSeconds(5.0))
			.ThenClient(TEXT("Stop movement and capture look state"), 0, [](FArenaDuelPhase3NetworkState& State)
			{
				APlayerController* Controller = State.World->GetFirstPlayerController();
				if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ArenaDuelPhase3Tests::GetLocalInputSubsystem(Controller))
				{
					InputSubsystem->StopContinuousInputInjectionForAction(ArenaDuelPhase3Tests::LoadAction(ArenaDuelPhase3Tests::MoveAction));
				}
				State.OwnedInitialYaw = State.OwnedPawn->GetControlRotation().Yaw;
				State.OwnedInitialPitch = State.OwnedPawn->GetControlRotation().Pitch;
			})
			.ThenClient(TEXT("Inject horizontal look"), 0, [](FArenaDuelPhase3NetworkState& State)
			{
				if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ArenaDuelPhase3Tests::GetLocalInputSubsystem(State.World->GetFirstPlayerController()))
				{
					InputSubsystem->StartContinuousInputInjectionForAction(ArenaDuelPhase3Tests::LoadAction(ArenaDuelPhase3Tests::LookAction), FInputActionValue(FVector2D(2.0f, 0.0f)), {}, {});
				}
			})
			.UntilClient(TEXT("Horizontal look changes yaw"), 0, [](FArenaDuelPhase3NetworkState& State)
			{
				return State.OwnedPawn && !FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(State.OwnedInitialYaw, State.OwnedPawn->GetControlRotation().Yaw), 1.0f);
			}, FTimespan::FromSeconds(5.0))
			.ThenClient(TEXT("Stop horizontal look and inject mouse-up equivalent"), 0, [](FArenaDuelPhase3NetworkState& State)
			{
				if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ArenaDuelPhase3Tests::GetLocalInputSubsystem(State.World->GetFirstPlayerController()))
				{
					InputSubsystem->StopContinuousInputInjectionForAction(ArenaDuelPhase3Tests::LoadAction(ArenaDuelPhase3Tests::LookAction));
					InputSubsystem->StartContinuousInputInjectionForAction(ArenaDuelPhase3Tests::LoadAction(ArenaDuelPhase3Tests::LookAction), FInputActionValue(FVector2D(0.0f, -2.0f)), {}, {});
				}
			})
			.UntilClient(TEXT("Mouse-up looks up without capsule pitch"), 0, [](FArenaDuelPhase3NetworkState& State)
			{
				if (!State.OwnedPawn)
				{
					return false;
				}
				const float PitchDelta = FMath::FindDeltaAngleDegrees(State.OwnedInitialPitch, State.OwnedPawn->GetControlRotation().Pitch);
				State.OwnedPitchAfterMouseUp = State.OwnedPawn->GetControlRotation().Pitch;
				return PitchDelta < -1.0f && FMath::Abs(FMath::FindDeltaAngleDegrees(0.0f, State.OwnedPitchAfterMouseUp)) <= 89.0f && FMath::IsNearlyZero(State.OwnedPawn->GetActorRotation().Pitch, 1.0f);
			}, FTimespan::FromSeconds(5.0))
			.ThenClient(TEXT("Inject mouse-down equivalent"), 0, [](FArenaDuelPhase3NetworkState& State)
			{
				if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ArenaDuelPhase3Tests::GetLocalInputSubsystem(State.World->GetFirstPlayerController()))
				{
					InputSubsystem->StopContinuousInputInjectionForAction(ArenaDuelPhase3Tests::LoadAction(ArenaDuelPhase3Tests::LookAction));
					InputSubsystem->StartContinuousInputInjectionForAction(ArenaDuelPhase3Tests::LoadAction(ArenaDuelPhase3Tests::LookAction), FInputActionValue(FVector2D(0.0f, 2.0f)), {}, {});
				}
			})
			.UntilClient(TEXT("Mouse-down looks down"), 0, [](FArenaDuelPhase3NetworkState& State)
			{
				if (!State.OwnedPawn)
				{
					return false;
				}
				const float CurrentPitch = FRotator::NormalizeAxis(State.OwnedPawn->GetControlRotation().Pitch);
				const float MouseUpPitch = FRotator::NormalizeAxis(State.OwnedPitchAfterMouseUp);
				return CurrentPitch > MouseUpPitch + 1.0f && FMath::IsNearlyZero(State.OwnedPawn->GetActorRotation().Pitch, 1.0f);
			}, FTimespan::FromSeconds(5.0))
			.ThenClient(TEXT("Inject native jump"), 0, [](FArenaDuelPhase3NetworkState& State)
			{
				if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ArenaDuelPhase3Tests::GetLocalInputSubsystem(State.World->GetFirstPlayerController()))
				{
					InputSubsystem->StopContinuousInputInjectionForAction(ArenaDuelPhase3Tests::LoadAction(ArenaDuelPhase3Tests::LookAction));
					InputSubsystem->InjectInputForAction(ArenaDuelPhase3Tests::LoadAction(ArenaDuelPhase3Tests::JumpAction), FInputActionValue(true), {}, {});
				}
			})
			.UntilClient(TEXT("Jump leaves ground"), 0, [](FArenaDuelPhase3NetworkState& State)
			{
				return State.OwnedPawn && (State.OwnedPawn->GetCharacterMovement()->IsFalling() || State.OwnedPawn->GetActorLocation().Z > State.OwnedInitialLocation.Z + 5.0f);
			}, FTimespan::FromSeconds(5.0))
			.UntilServer(TEXT("Movement and jump reach the server"), [](FArenaDuelPhase3NetworkState& State)
			{
				for (TActorIterator<AArenaDuelCharacter> It(State.World); It; ++It)
				{
					if (It->GetCharacterMovement()->Velocity.SizeSquared2D() > 1.0f || It->GetCharacterMovement()->IsFalling())
					{
						return true;
					}
				}
				return false;
			}, FTimespan::FromSeconds(5.0))
			.UntilClient(TEXT("Remote pawn remains independently controlled"), 0, [](FArenaDuelPhase3NetworkState& State)
			{
				return State.RemotePawn && FVector::Dist2D(State.RemotePawn->GetActorLocation(), State.RemoteInitialLocation) < 100.0f;
			}, FTimespan::FromSeconds(1.0));
	}
};

#endif
