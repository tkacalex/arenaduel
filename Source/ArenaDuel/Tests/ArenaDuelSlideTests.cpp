#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "CQTest.h"
#include "Components/PIENetworkComponent.h"
#include "ArenaDuel/Characters/ArenaDuelCharacter.h"
#include "ArenaDuel/Characters/ArenaDuelCharacterMovementComponent.h"
#include "ArenaDuel/Game/ArenaDuelGameState.h"
#include "ArenaDuel/Player/ArenaDuelPlayerState.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/StaticMeshActor.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "Tests/AutomationEditorCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaDuelSlideSustainedStateTest, "ArenaDuel.Slide.SustainedState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FArenaDuelSlideSustainedStateTest::RunTest(const FString& Parameters)
{
	FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/ArenaDuel/Maps/L_Phase4MovementTest"));
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	UClass* CharacterClass = LoadClass<AArenaDuelCharacter>(nullptr, TEXT("/Game/ArenaDuel/Characters/BP_ArenaDuelCharacter.BP_ArenaDuelCharacter_C"));
	if (!TestNotNull(TEXT("Movement world"), World) || !TestNotNull(TEXT("Character class"), CharacterClass))
	{
		return false;
	}

	AArenaDuelCharacter* Character = World->SpawnActor<AArenaDuelCharacter>(CharacterClass, FVector(0.0f, 0.0f, 88.0f), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("Slide character"), Character))
	{
		return false;
	}

	UArenaDuelCharacterMovementComponent* Movement = Character->GetArenaDuelMovementComponent();
	TestTrue(TEXT("Slide duration is 1.5 seconds"), FMath::IsNearlyEqual(Movement->SlideDuration, 1.5f));
	Movement->SetUpdatedComponent(Character->GetCapsuleComponent());
	Movement->SetMovementMode(MOVE_Walking);
	Movement->CurrentFloor.bWalkableFloor = true;
	Movement->Velocity = FVector(800.0f, 0.0f, 0.0f);
	Movement->StartSlide();
	TestTrue(TEXT("Slide enters custom movement"), Movement->IsSliding());
	TestTrue(TEXT("Crouch remains valid in slide mode"), Movement->CanCrouchInCurrentState());
	Movement->UpdateCharacterStateBeforeMovement(0.016f);
	TestTrue(TEXT("Slide survives its next movement update"), Movement->IsSliding());
	Movement->StopSlide();
	TestFalse(TEXT("Releasing slide exits the mode"), Movement->IsSliding());
	TestFalse(TEXT("Slide boost recharges after a slide"), Movement->IsSlideBoostReady());
	Movement->SetMovementMode(MOVE_Walking);
	Movement->CurrentFloor.bWalkableFloor = true;
	Movement->Velocity = FVector(Movement->SprintSpeed, 0.0f, 0.0f);
	Movement->StartSlide();
	TestTrue(TEXT("Immediate re-slide is allowed"), Movement->IsSliding());
	TestTrue(TEXT("Immediate re-slide gets no stacked boost"), Movement->Velocity.Size2D() <= Movement->SprintSpeed + 1.0f);
	Movement->StopSlide();
	Movement->ResetMovementIntentForDevelopment();
	TestTrue(TEXT("Development reset recharges slide boost"), Movement->IsSlideBoostReady());
	Movement->SetMovementMode(MOVE_Walking);
	Movement->CurrentFloor.bWalkableFloor = true;
	Movement->Velocity = FVector(Movement->SprintSpeed, 0.0f, 0.0f);
	Movement->StartSlide();
	TestTrue(TEXT("Boosted slide starts at entry speed"), Movement->Velocity.Size2D() >= Movement->SlideEntrySpeed - 1.0f);
	Movement->ReleaseSlideInput();
	Movement->PhysCustom(0.75f, 0);
	TestTrue(TEXT("A tap keeps the slide active"), Movement->IsSliding());
	Movement->PhysCustom(0.70f, 0);
	TestTrue(TEXT("Slide lasts nearly 1.5 seconds"), Movement->IsSliding());
	TestTrue(TEXT("Slide remains faster than sprint"), Movement->Velocity.Size2D() > Movement->SprintSpeed);
	Movement->PhysCustom(0.06f, 0);
	TestFalse(TEXT("Slide expires after 1.5 seconds"), Movement->IsSliding());

	Character->Destroy();
	return true;
}

struct FArenaDuelSlideNetworkState : FBasePIENetworkComponentState
{
	AStaticMeshActor* FloorActor = nullptr;
	float SlideStartedAt = -1.0f;
	FVector SlideStartLocation = FVector::ZeroVector;
	float SlideDistance = 0.0f;
};

namespace ArenaDuelSlideTests
{
	void ConfigureFloor(AStaticMeshActor& Actor)
	{
		if (Actor.HasAuthority()) Actor.SetReplicates(true);
		UStaticMeshComponent* Mesh = Actor.GetStaticMeshComponent();
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
		Mesh->SetCollisionProfileName(TEXT("BlockAll"));
		Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		Actor.SetActorEnableCollision(true);
		Mesh->RecreatePhysicsState();
		Actor.SetActorLocation(FVector(0.0f, 0.0f, -50.0f));
		Actor.SetActorScale3D(FVector(80.0f, 8.0f, 1.0f));
	}

	AArenaDuelCharacter* OwnedPawn(UWorld* World)
	{
		APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
		return Controller ? Cast<AArenaDuelCharacter>(Controller->GetPawn()) : nullptr;
	}

	AArenaDuelCharacter* ServerClientPawn(UWorld* World)
	{
		if (!World) return nullptr;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* Controller = It->Get();
			const AArenaDuelPlayerState* State = Controller ? Controller->GetPlayerState<AArenaDuelPlayerState>() : nullptr;
			if (State && State->GetDuelSlot() == 1) return Cast<AArenaDuelCharacter>(Controller->GetPawn());
		}
		return nullptr;
	}

	UEnhancedInputLocalPlayerSubsystem* Input(UWorld* World)
	{
		APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
		return Controller && Controller->GetLocalPlayer() ? Controller->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	}

	UInputAction* Action(const TCHAR* Path)
	{
		return LoadObject<UInputAction>(nullptr, Path);
	}
}

NETWORK_TEST_CLASS(FArenaDuelSlideNetworkTest, "ArenaDuel.Slide.Network")
{
	FPIENetworkComponent<FArenaDuelSlideNetworkState> Network{TestRunner, TestCommandBuilder, bInitializing};
	BEFORE_EACH()
	{
		UClass* Mode = LoadClass<AGameModeBase>(nullptr, TEXT("/Game/ArenaDuel/Game/BP_ArenaDuelGameMode.BP_ArenaDuelGameMode_C"));
		FNetworkComponentBuilder<FArenaDuelSlideNetworkState>().WithClients(1).AsListenServer().WithGameMode(Mode).Build(Network);
		Network.SpawnAndReplicate<AStaticMeshActor, &FArenaDuelSlideNetworkState::FloorActor>([](AStaticMeshActor& Actor) { ArenaDuelSlideTests::ConfigureFloor(Actor); });
	}
	TEST_METHOD(SustainedDuringLiveRound)
	{
		using namespace ArenaDuelSlideTests;
		Network
			.UntilServer(TEXT("Wait for auto ready and live round"), [](FArenaDuelSlideNetworkState& State)
			{
				const AArenaDuelGameState* GameState = State.World ? State.World->GetGameState<AArenaDuelGameState>() : nullptr;
				return GameState && GameState->IsRoundInProgress() && ServerClientPawn(State.World) && State.FloorActor;
			}, FTimespan::FromSeconds(22.0))
			.ThenServer(TEXT("Place client pawn on slide lane"), [](FArenaDuelSlideNetworkState& State)
			{
				AArenaDuelCharacter* Pawn = ServerClientPawn(State.World);
				Pawn->SetActorLocation(FVector(-2500.0f, 0.0f, 100.0f), false, nullptr, ETeleportType::TeleportPhysics);
				Pawn->SetActorRotation(FRotator::ZeroRotator);
				Pawn->GetArenaDuelMovementComponent()->Velocity = FVector::ZeroVector;
				Pawn->GetArenaDuelMovementComponent()->SetMovementMode(MOVE_Walking);
				Pawn->ForceNetUpdate();
			})
			.UntilClient(TEXT("Client has movement input"), 0, [](FArenaDuelSlideNetworkState& State)
			{
				AArenaDuelCharacter* Pawn = OwnedPawn(State.World);
				if (State.FloorActor) ConfigureFloor(*State.FloorActor);
				return Pawn && Input(State.World) && State.FloorActor && FVector::Dist2D(Pawn->GetActorLocation(), FVector(-2500.0f, 0.0f, 100.0f)) < 150.0f && Pawn->GetArenaDuelMovementComponent()->IsMovingOnGround();
			}, FTimespan::FromSeconds(5.0))
			.ThenClient(TEXT("Sprint forward through Enhanced Input"), 0, [](FArenaDuelSlideNetworkState& State)
			{
				UEnhancedInputLocalPlayerSubsystem* Subsystem = Input(State.World);
				UInputAction* Move = Action(TEXT("/Game/ArenaDuel/Input/IA_Move.IA_Move"));
				UInputAction* Sprint = Action(TEXT("/Game/ArenaDuel/Input/IA_Sprint.IA_Sprint"));
				Subsystem->StartContinuousInputInjectionForAction(Move, FInputActionValue(FVector2D(0.0f, 1.0f)), {}, {});
				Subsystem->StartContinuousInputInjectionForAction(Sprint, FInputActionValue(true), {}, {});
				Subsystem->InjectInputForAction(Move, FInputActionValue(FVector2D(0.0f, 1.0f)), {}, {});
				Subsystem->InjectInputForAction(Sprint, FInputActionValue(true), {}, {});
			})
			.UntilClient(TEXT("Sprint reaches slide entry speed"), 0, [](FArenaDuelSlideNetworkState& State)
			{
				AArenaDuelCharacter* Pawn = OwnedPawn(State.World);
				return Pawn && Pawn->GetArenaDuelMovementComponent()->IsMovingOnGround() && Pawn->GetVelocity().Size2D() >= Pawn->GetArenaDuelMovementComponent()->SlideMinSpeed;
			}, FTimespan::FromSeconds(5.0))
			.ThenClient(TEXT("Hold slide input"), 0, [](FArenaDuelSlideNetworkState& State)
			{
				UEnhancedInputLocalPlayerSubsystem* Subsystem = Input(State.World);
				UInputAction* Slide = Action(TEXT("/Game/ArenaDuel/Input/IA_Slide.IA_Slide"));
				Subsystem->StartContinuousInputInjectionForAction(Slide, FInputActionValue(true), {}, {});
				Subsystem->InjectInputForAction(Slide, FInputActionValue(true), {}, {});
			})
			.UntilClient(TEXT("Slide travels forward while remaining active"), 0, [](FArenaDuelSlideNetworkState& State)
			{
				AArenaDuelCharacter* Pawn = OwnedPawn(State.World);
				const bool bSliding = Pawn && Pawn->GetArenaDuelMovementComponent()->IsSliding();
				if (!bSliding) { State.SlideStartedAt = -1.0f; return false; }
				if (State.SlideStartedAt < 0.0f)
				{
					State.SlideStartedAt = State.World->GetTimeSeconds();
					State.SlideStartLocation = Pawn->GetActorLocation();
				}
				State.SlideDistance = FVector::Dist2D(State.SlideStartLocation, Pawn->GetActorLocation());
				return State.World->GetTimeSeconds() - State.SlideStartedAt >= 0.15f && State.SlideDistance >= 80.0f && Pawn->IsCrouched();
			}, FTimespan::FromSeconds(5.0))
			.UntilServer(TEXT("Server reconstructs sustained slide"), [](FArenaDuelSlideNetworkState& State)
			{
				AArenaDuelCharacter* Pawn = ServerClientPawn(State.World);
				return Pawn && Pawn->GetArenaDuelMovementComponent()->IsSliding() && Pawn->IsCrouched();
			}, FTimespan::FromSeconds(3.0))
			.ThenClient(TEXT("Release movement input"), 0, [](FArenaDuelSlideNetworkState& State)
			{
				UEnhancedInputLocalPlayerSubsystem* Subsystem = Input(State.World);
				for (const TCHAR* Path : {TEXT("/Game/ArenaDuel/Input/IA_Move.IA_Move"), TEXT("/Game/ArenaDuel/Input/IA_Sprint.IA_Sprint"), TEXT("/Game/ArenaDuel/Input/IA_Slide.IA_Slide")})
				{
					Subsystem->StopContinuousInputInjectionForAction(Action(Path));
				}
			})
			.UntilClient(TEXT("Slide continues at crouch height after releasing the key"), 0, [](FArenaDuelSlideNetworkState& State)
			{
				AArenaDuelCharacter* Pawn = OwnedPawn(State.World);
				return Pawn && State.World->GetTimeSeconds() - State.SlideStartedAt >= 1.25f && Pawn->GetArenaDuelMovementComponent()->IsSliding() && Pawn->IsCrouched() && Pawn->GetVelocity().Size2D() > Pawn->GetArenaDuelMovementComponent()->SprintSpeed;
			}, FTimespan::FromSeconds(2.0))
			.UntilServer(TEXT("Server exits slide automatically after 1.5 seconds"), [](FArenaDuelSlideNetworkState& State)
			{
				AArenaDuelCharacter* Pawn = ServerClientPawn(State.World);
				return Pawn && !Pawn->GetArenaDuelMovementComponent()->IsSliding();
			}, FTimespan::FromSeconds(3.0));
	}
};

#endif
