#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"
#include "Misc/AutomationTest.h"
#include "ArenaDuel/Abilities/ArenaDuelGA_RiftGrapple.h"
#include "ArenaDuel/Abilities/ArenaDuelGA_PhaseGate.h"
#include "ArenaDuel/Abilities/ArenaDuelGA_ShadowStep.h"
#include "ArenaDuel/Abilities/ArenaDuelGA_ArcBarrier.h"
#include "ArenaDuel/Abilities/ArenaDuelArcBarrier.h"
#include "ArenaDuel/Abilities/ArenaDuelRiftTargeting.h"
#include "ArenaDuel/Abilities/ArenaDuelPhaseGateVisual.h"
#include "ArenaDuel/Abilities/ArenaDuelGameplayTags.h"
#include "ArenaDuel/Abilities/ArenaDuelShadowCooldownEffects.h"
#include "ArenaDuel/Characters/ArenaDuelCharacter.h"
#include "ArenaDuel/Player/ArenaDuelPlayerController.h"
#include "ArenaDuel/Game/ArenaDuelGameMode.h"
#include "ArenaDuel/Game/ArenaDuelGameState.h"
#include "ArenaDuel/UI/ArenaDuelCharacterSelectWidget.h"
#include "ArenaDuel/UI/ArenaDuelAdminWidget.h"
#include "AbilitySystemComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaDuelRiftContractTest, "ArenaDuel.Rift.Contract", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FArenaDuelRiftContractTest::RunTest(const FString& Parameters)
{
	const auto* Grapple = GetDefault<UArenaDuelGA_RiftGrapple>();
	const auto* Gate = GetDefault<UArenaDuelGA_PhaseGate>();
	TestTrue(TEXT("Grapple is local predicted GAS with native tags"), Grapple->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalPredicted && Grapple->GetAssetTags().HasTagExact(TAG_Ability_Rift_RiftGrapple) && Grapple->GetCooldownTags()->HasTagExact(TAG_Cooldown_Rift_RiftGrapple));
	TestTrue(TEXT("Gate has native ability and cooldown tags"), Gate->GetAssetTags().HasTagExact(TAG_Ability_Rift_PhaseGate) && Gate->GetCooldownTags()->HasTagExact(TAG_Cooldown_Rift_PhaseGate));
	float Seconds = 0;
	TestTrue(TEXT("Grapple has real six-second cooldown"), GetDefault<UArenaDuelGE_RiftGrappleCooldown>()->DurationMagnitude.GetStaticMagnitudeIfPossible(1, Seconds) && FMath::IsNearlyEqual(Seconds, 6.0f));
	TestTrue(TEXT("Gate has real fourteen-second cooldown"), GetDefault<UArenaDuelGE_PhaseGateCooldown>()->DurationMagnitude.GetStaticMagnitudeIfPossible(1, Seconds) && FMath::IsNearlyEqual(Seconds, 14.0f));
	const auto& Presentation = UArenaDuelCharacterSelectWidget::GetPresentation(EArenaDuelCharacterArchetype::Rift);
	TestEqual(TEXT("Rift roster name"), Presentation.Name, FString(TEXT("RIFT")));
	TestEqual(TEXT("Rift secondary slot"), Presentation.SecondaryName, FString(TEXT("PHASE GATE")));
	auto* Admin = NewObject<UArenaDuelAdminWidget>();
	TestTrue(TEXT("Admin selector contains three implemented kits"), Admin->Initialize() && Admin->HasImplementedArchetypeSelector());
	const auto* Visual = GetDefault<AArenaDuelPhaseGateVisual>();
	TestTrue(TEXT("Gate visuals replicate without Tick"), Visual->GetIsReplicated() && !Visual->PrimaryActorTick.bCanEverTick);
	TArray<UPrimitiveComponent*> Primitives;
	Visual->GetComponents(Primitives);
	for (const auto* Primitive : Primitives) TestTrue(TEXT("Every gate primitive is NoCollision"), Primitive->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
	return true;
}

namespace RiftSmoke
{
	AArenaDuelPlayerController* PC(UWorld* World, bool bLocal)
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			if (auto* Controller = Cast<AArenaDuelPlayerController>(It->Get()); Controller && Controller->IsLocalController() == bLocal) return Controller;
		return nullptr;
	}
	AArenaDuelCharacter* Pawn(UWorld* World, bool bLocal) { const auto* Controller = PC(World, bLocal); return Controller ? Cast<AArenaDuelCharacter>(Controller->GetPawn()) : nullptr; }
	bool CorrectKit(AArenaDuelPlayerState* PS)
	{
		if (!PS) return false;
		const auto* ASC = PS->GetAbilitySystemComponent();
		return PS->GetCharacterArchetype() == EArenaDuelCharacterArchetype::Rift && ASC->GetActivatableAbilities().Num() == 2
			&& ASC->FindAbilitySpecFromClass(UArenaDuelGA_RiftGrapple::StaticClass()) && ASC->FindAbilitySpecFromClass(UArenaDuelGA_PhaseGate::StaticClass());
	}
	bool AbilitiesBlocked(AArenaDuelPlayerState* PS)
	{
		const auto* ASC = PS->GetAbilitySystemComponent();
		for (const auto& Spec : ASC->GetActivatableAbilities())
			if (Spec.Ability && Spec.Ability->CanActivateAbility(Spec.Handle, ASC->AbilityActorInfo.Get())) return false;
		return true;
	}
	void Fixtures(UWorld* World)
	{
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		for (int32 Index = 0; Index < 2; ++Index)
		{
			auto* Shape = World->SpawnActor<AStaticMeshActor>(Index == 0 ? FVector(0, 20000, -50) : FVector(1000, 20000, 450), FRotator::ZeroRotator);
			Shape->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
			Shape->GetStaticMeshComponent()->SetStaticMesh(Cube);
			Shape->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
			Shape->SetActorScale3D(Index == 0 ? FVector(100, 100, 1) : FVector(1, 40, 10));
			Shape->GetStaticMeshComponent()->SetMobility(EComponentMobility::Static);
		}
	}
	bool ClearCapsule(const AArenaDuelCharacter* Character)
	{
		const auto* Capsule = Character->GetCapsuleComponent();
		FCollisionQueryParams Params(SCENE_QUERY_STAT(RiftSmokeOverlap), false, Character);
		return !Character->GetWorld()->OverlapBlockingTestByProfile(Character->GetActorLocation(), FQuat::Identity, Capsule->GetCollisionProfileName(),
			FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius() - 1, Capsule->GetScaledCapsuleHalfHeight() - 1), Params);
	}
}

struct FArenaDuelRiftNetworkState : public FBasePIENetworkComponentState
{
	FVector Start = FVector::ZeroVector;
	float SampleTime = 0;
};

NETWORK_TEST_CLASS(FArenaDuelRiftNetworkSmoke, "ArenaDuel.Rift.Network")
{
	FPIENetworkComponent<FArenaDuelRiftNetworkState> Network{TestRunner, TestCommandBuilder, bInitializing};
	BEFORE_EACH()
	{
		UClass* Mode = LoadClass<AGameModeBase>(nullptr, TEXT("/Game/ArenaDuel/Game/BP_ArenaDuelGameMode.BP_ArenaDuelGameMode_C"));
		FNetworkComponentBuilder<FArenaDuelRiftNetworkState>().WithClients(1).AsListenServer().WithGameMode(Mode).Build(Network);
	}
	TEST_METHOD(PublicRiftKitTraversalAndLifecycle)
	{
		using namespace RiftSmoke;
		Network
		.UntilServer(TEXT("Both players join selection"), [](FArenaDuelRiftNetworkState& S) { return Pawn(S.World, true) && Pawn(S.World, false); }, FTimespan::FromSeconds(10))
		.ThenClient(TEXT("Remote public player selects Rift with normal RPC"), 0, [](FArenaDuelRiftNetworkState& S) { PC(S.World, true)->RequestCharacterSelection(EArenaDuelCharacterArchetype::Rift); })
		.UntilServer(TEXT("Server accepted Rift and replaced Shadow kit"), [](FArenaDuelRiftNetworkState& S) { return CorrectKit(PC(S.World, false)->GetPlayerState<AArenaDuelPlayerState>()); }, FTimespan::FromSeconds(5))
		.ThenServer(TEXT("Selection gate and host ready"), [this](FArenaDuelRiftNetworkState& S)
		{
			auto* PS = PC(S.World, false)->GetPlayerState<AArenaDuelPlayerState>();
			if (!AbilitiesBlocked(PS)) TestRunner->AddError(TEXT("Selection allowed Rift ability"));
			PC(S.World, true)->ServerSetCharacterReady(true);
			Fixtures(S.World);
		})
		.UntilClient(TEXT("Owner receives replicated Rift kit"), 0, [](FArenaDuelRiftNetworkState& S) { return PC(S.World, true) && CorrectKit(PC(S.World, true)->GetPlayerState<AArenaDuelPlayerState>()); }, FTimespan::FromSeconds(5))
		.ThenClient(TEXT("Owner ready begins countdown"), 0, [](FArenaDuelRiftNetworkState& S) { Fixtures(S.World); PC(S.World, true)->ServerSetCharacterReady(true); })
		.UntilServer(TEXT("Rift countdown started"), [](FArenaDuelRiftNetworkState& S) { return S.World->GetGameState<AArenaDuelGameState>()->GetMatchPhase() == EArenaDuelMatchPhase::Countdown; }, FTimespan::FromSeconds(5))
		.ThenServer(TEXT("Countdown denies abilities"), [this](FArenaDuelRiftNetworkState& S)
		{
			auto* PS = PC(S.World, false)->GetPlayerState<AArenaDuelPlayerState>();
			if (!AbilitiesBlocked(PS)) TestRunner->AddError(TEXT("Countdown allowed Rift ability"));
		})
		.UntilServer(TEXT("Rift match starts"), [](FArenaDuelRiftNetworkState& S) { return S.World->GetGameState<AArenaDuelGameState>()->IsRoundInProgress(); }, FTimespan::FromSeconds(8))
		.ThenServer(TEXT("Place test pawn on real isolated collision floor"), [](FArenaDuelRiftNetworkState& S)
		{
			auto* Character = Pawn(S.World, false);
			Character->GetCharacterMovement()->StopMovementImmediately();
			Character->TeleportTo(FVector(0, 20000, 100), FRotator::ZeroRotator);
			PC(S.World, false)->SetControlRotation(FRotator::ZeroRotator);
			S.Start = Character->GetActorLocation();
		})
		.UntilClient(TEXT("Owner sees relocated pawn and active round"), 0, [](FArenaDuelRiftNetworkState& S)
		{
			auto* Character = Pawn(S.World, true);
			return Character && S.World->GetGameState<AArenaDuelGameState>()->IsRoundInProgress() && FVector::Dist2D(Character->GetActorLocation(), FVector(0, 20000, 0)) < 30;
		}, FTimespan::FromSeconds(5))
		.ThenClient(TEXT("Predicted grapple activates toward fixture wall"), 0, [this](FArenaDuelRiftNetworkState& S)
		{
			PC(S.World, true)->SetControlRotation(FRotator::ZeroRotator);
			auto* PS = PC(S.World, true)->GetPlayerState<AArenaDuelPlayerState>();
			FVector Destination, Anchor;
			if (!ArenaDuelRiftTargeting::FindGrappleDestination(*Pawn(S.World, true), Destination, Anchor)) TestRunner->AddError(TEXT("Real world grapple fixture not targetable"));
			PS->TryActivatePrimaryAbility();
			if (PS->TryActivatePrimaryAbility()) TestRunner->AddError(TEXT("Immediate grapple repeat accepted"));
		})
		.UntilServer(TEXT("Authoritative grapple cooldown and multiframe movement"), [](FArenaDuelRiftNetworkState& S)
		{
			auto* Character = Pawn(S.World, false);
			const auto* PS = Character->GetPlayerState<AArenaDuelPlayerState>();
			return PS->GetPrimaryAbilityCooldownRemaining() > 4 && Character->GetActorLocation().X > S.Start.X + 400 && Character->GetHealth() == 100;
		}, FTimespan::FromSeconds(4))
		.UntilClient(TEXT("Client observes pull, real cooldown and no damage"), 0, [](FArenaDuelRiftNetworkState& S)
		{
			auto* Character = Pawn(S.World, true);
			return Character && Character->GetActorLocation().X > 400 && Character->GetHealth() == 100 && Character->GetPlayerState<AArenaDuelPlayerState>()->GetPrimaryAbilityCooldownRemaining() > 3;
		}, FTimespan::FromSeconds(3))
		.UntilServer(TEXT("Root motion finishes and capsule stays outside wall"), [](FArenaDuelRiftNetworkState& S)
		{
			auto* Character = Pawn(S.World, false);
			const auto* Spec = Character->GetAbilitySystemComponent()->FindAbilitySpecFromClass(UArenaDuelGA_RiftGrapple::StaticClass());
			return !Spec->IsActive() && Character->GetActorLocation().X < 950 && ClearCapsule(Character);
		}, FTimespan::FromSeconds(4))
		.ThenServer(TEXT("Reset fixture position only, keep actual cooldown state"), [](FArenaDuelRiftNetworkState& S)
		{
			auto* Character = Pawn(S.World, false);
			Character->GetCharacterMovement()->StopMovementImmediately();
			Character->TeleportTo(FVector(0, 20000, 100), FRotator::ZeroRotator);
			S.Start = Character->GetActorLocation();
		})
		.UntilClient(TEXT("Owner returns to gate source"), 0, [](FArenaDuelRiftNetworkState& S) { return Pawn(S.World, true)->GetActorLocation().X < 30; }, FTimespan::FromSeconds(5))
		.ThenClient(TEXT("Gate starts predicted cast, not client teleport"), 0, [this](FArenaDuelRiftNetworkState& S)
		{
			auto* Character = Pawn(S.World, true);
			const FVector Before = Character->GetActorLocation();
			PC(S.World, true)->SetControlRotation(FRotator::ZeroRotator);
			Character->GetPlayerState<AArenaDuelPlayerState>()->TryActivateSecondaryAbility();
			if (FVector::Dist(Before, Character->GetActorLocation()) > 1) TestRunner->AddError(TEXT("Client teleported during cast"));
		})
		.UntilServer(TEXT("Server performs bounded gate teleport before wall"), [](FArenaDuelRiftNetworkState& S)
		{
			auto* Character = Pawn(S.World, false);
			return Character->GetPlayerState<AArenaDuelPlayerState>()->GetSecondaryAbilityCooldownRemaining() > 12 && Character->GetActorLocation().X > 700
				&& Character->GetActorLocation().X < 950 && FVector::Dist(Character->GetActorLocation(), S.Start) <= 1400 && ClearCapsule(Character) && Character->GetHealth() == 100;
		}, FTimespan::FromSeconds(4))
		.UntilClient(TEXT("Owner reconciles safe teleport and replicated gates"), 0, [](FArenaDuelRiftNetworkState& S)
		{
			int32 Visuals = 0;
			for (TActorIterator<AArenaDuelPhaseGateVisual> It(S.World); It; ++It) ++Visuals;
			auto* Character = Pawn(S.World, true);
			return Character->GetActorLocation().X > 700 && Character->GetActorLocation().X < 950 && ClearCapsule(Character)
				&& Character->GetPlayerState<AArenaDuelPlayerState>()->GetSecondaryAbilityCooldownRemaining() > 12 && Visuals == 2;
		}, FTimespan::FromSeconds(4))
		.ThenClient(TEXT("Gate cooldown rejects repeat"), 0, [this](FArenaDuelRiftNetworkState& S)
		{
			if (PC(S.World, true)->GetPlayerState<AArenaDuelPlayerState>()->TryActivateSecondaryAbility()) TestRunner->AddError(TEXT("Gate cooldown accepted repeat"));
		})
		.ThenServer(TEXT("Death cancels abilities, cleans gates and rejects dead activation"), [this](FArenaDuelRiftNetworkState& S)
		{
			auto* Character = Pawn(S.World, false);
			Character->AdminKill();
			auto* PS = Character->GetPlayerState<AArenaDuelPlayerState>();
			PS->ResetAbilitiesForNewRound();
			if (!Character->IsDead() || !AbilitiesBlocked(PS)) TestRunner->AddError(TEXT("Dead Rift activation accepted"));
			if (!AbilitiesBlocked(PC(S.World, true)->GetPlayerState<AArenaDuelPlayerState>())) TestRunner->AddError(TEXT("Living player could activate during RoundBreak"));
			for (TActorIterator<AArenaDuelPhaseGateVisual> It(S.World); It; ++It) if (!It->IsActorBeingDestroyed()) TestRunner->AddError(TEXT("Round end left gate visual"));
		})
		.UntilServer(TEXT("Normal restart preserves unique Rift kit with ready cooldowns"), [](FArenaDuelRiftNetworkState& S)
		{
			auto* PS = PC(S.World, false)->GetPlayerState<AArenaDuelPlayerState>();
			return S.World->GetGameState<AArenaDuelGameState>()->IsRoundInProgress() && !Pawn(S.World, false)->IsDead()
				&& CorrectKit(PS) && PS->GetPrimaryAbilityCooldownRemaining() == 0 && PS->GetSecondaryAbilityCooldownRemaining() == 0;
		}, FTimespan::FromSeconds(8))
		.ThenServer(TEXT("Invalid anchors reject safely, kit swaps remove old specs, match ends"), [this](FArenaDuelRiftNetworkState& S)
		{
			auto* Character = Pawn(S.World, false);
			auto* PS = Character->GetPlayerState<AArenaDuelPlayerState>();
			FVector Destination, Anchor;
			PC(S.World, false)->SetControlRotation(FRotator(90, 0, 0));
			if (ArenaDuelRiftTargeting::FindGrappleDestination(*Character, Destination, Anchor)) TestRunner->AddError(TEXT("Empty sky was a grapple target"));
			PS->TryActivatePrimaryAbility();
			if (PS->GetPrimaryAbilityCooldownRemaining() > 0) TestRunner->AddError(TEXT("Invalid grapple consumed cooldown"));
			Character->GetCharacterMovement()->StopMovementImmediately();
			Character->TeleportTo(FVector(0, 20000, 100), FRotator::ZeroRotator);
			auto* Other = Pawn(S.World, true);
			Other->GetCharacterMovement()->StopMovementImmediately();
			Other->TeleportTo(FVector(600, 20000, 100), FRotator::ZeroRotator);
			PC(S.World, false)->SetControlRotation((Other->GetPawnViewLocation() - Character->GetPawnViewLocation()).Rotation());
			FHitResult PlayerHit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(RiftPlayerAnchorTest), false, Character);
			const bool bHitPlayer = S.World->LineTraceSingleByChannel(PlayerHit, Character->GetPawnViewLocation(), Character->GetPawnViewLocation() + Character->GetControlRotation().Vector() * 2200, ECC_Visibility, Params) && PlayerHit.GetActor() == Other;
			if (!bHitPlayer || ArenaDuelRiftTargeting::FindGrappleDestination(*Character, Destination, Anchor)) TestRunner->AddError(TEXT("Player hit-zone anchor rejection failed"));
			PC(S.World, false)->SetControlRotation(FRotator::ZeroRotator);
			FActorSpawnParameters BarrierParams;
			BarrierParams.Owner = PC(S.World, true)->GetPlayerState<AArenaDuelPlayerState>();
			auto* Barrier = S.World->SpawnActor<AArenaDuelArcBarrier>(FVector(500, 20000, 150), FRotator::ZeroRotator, BarrierParams);
			if (!Barrier || ArenaDuelRiftTargeting::FindGrappleDestination(*Character, Destination, Anchor)) TestRunner->AddError(TEXT("Temporary barrier accepted as grapple anchor"));
			if (!ArenaDuelRiftTargeting::FindGateDestination(*Character, Destination) || Destination.X >= 480) TestRunner->AddError(TEXT("Gate failed to stop before physical barrier"));
			if (Barrier) Barrier->Destroy();
			Other->TeleportTo(FVector(150, 20000, 100), FRotator::ZeroRotator);
			if (ArenaDuelRiftTargeting::FindGateDestination(*Character, Destination)) TestRunner->AddError(TEXT("Gate accepted capsule path through close player"));
			if (Other->GetHealth() != 100) TestRunner->AddError(TEXT("Rift abilities damaged the opponent"));
			PS->SetCharacterArchetypeAuthoritatively(EArenaDuelCharacterArchetype::Shadow);
			if (PS->GetAbilitySystemComponent()->GetActivatableAbilities().Num() != 2 || !PS->GetAbilitySystemComponent()->FindAbilitySpecFromClass(UArenaDuelGA_ShadowStep::StaticClass())) TestRunner->AddError(TEXT("Rift to Shadow replacement failed"));
			PS->SetCharacterArchetypeAuthoritatively(EArenaDuelCharacterArchetype::Warden);
			if (PS->GetAbilitySystemComponent()->GetActivatableAbilities().Num() != 2 || !PS->GetAbilitySystemComponent()->FindAbilitySpecFromClass(UArenaDuelGA_ArcBarrier::StaticClass())) TestRunner->AddError(TEXT("Rift to Warden replacement failed"));
			PS->SetCharacterArchetypeAuthoritatively(EArenaDuelCharacterArchetype::Rift);
			S.World->GetAuthGameMode<AArenaDuelGameMode>()->FindPlayerStateByDuelSlot(0)->SetRoundWinsForDevelopment(4);
			S.World->GetAuthGameMode<AArenaDuelGameMode>()->AdminAwardRound(0);
			if (!AbilitiesBlocked(PS)) TestRunner->AddError(TEXT("Match result allowed Rift activation"));
		})
		.UntilServer(TEXT("First-to-five returns selected Rift, not ready, to lobby"), [](FArenaDuelRiftNetworkState& S)
		{
			auto* PS = PC(S.World, false)->GetPlayerState<AArenaDuelPlayerState>();
			return S.World->GetGameState<AArenaDuelGameState>()->GetMatchPhase() == EArenaDuelMatchPhase::CharacterSelect && CorrectKit(PS) && !PS->IsCharacterReady();
		}, FTimespan::FromSeconds(10));
	}
};

#endif
