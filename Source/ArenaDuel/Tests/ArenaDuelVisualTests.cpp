#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"
#include "Misc/AutomationTest.h"
#include "ArenaDuel/Characters/ArenaDuelCharacter.h"
#include "ArenaDuel/Characters/ArenaDuelVisualAnimInstance.h"
#include "ArenaDuel/Weapons/ArenaDuelWeaponComponent.h"
#include "ArenaDuel/Player/ArenaDuelPlayerController.h"
#include "ArenaDuel/Game/ArenaDuelGameMode.h"
#include "ArenaDuel/Game/ArenaDuelGameState.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInterface.h"
#include "EngineUtils.h"
#include "UObject/UnrealType.h"

namespace VisualSmoke
{
	// The corpse is a physics ragdoll on the world body.
	bool PlaysDeath(const AArenaDuelCharacter* Character)
	{
		return Character && Character->GetMesh()->IsSimulatingPhysics() && Character->GetMesh()->GetCollisionEnabled() != ECollisionEnabled::NoCollision;
	}	bool Flag(const UPrimitiveComponent* Component, const TCHAR* Name)
	{
		const auto* Property = FindFProperty<FBoolProperty>(UPrimitiveComponent::StaticClass(), Name);
		return Property && Property->GetPropertyValue_InContainer(Component);
	}
	AArenaDuelCharacter* Pawn(UWorld* World, bool bLocal)
	{
		for (TActorIterator<AArenaDuelCharacter> It(World); It; ++It)
			if (!It->IsActorBeingDestroyed() && It->IsLocallyControlled() == bLocal) return *It;
		return nullptr;
	}
	AArenaDuelPlayerController* PC(UWorld* World)
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			if (auto* Controller = Cast<AArenaDuelPlayerController>(It->Get()); Controller && Controller->IsLocalController()) return Controller;
		return nullptr;
	}
	bool Valid(AArenaDuelCharacter* Character)
	{
		if (!Character) return false;
		// NullRHI never renders a mesh. Evaluate bones explicitly in this headless test,
		// otherwise socket measurements would describe the reference pose, not gameplay.
		Character->GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		Character->GetFirstPersonArms()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		const auto* Arms = Character->GetFirstPersonArms();
		const auto* Weapon = Character->GetWeaponComponent();
		const auto* FP = Weapon->GetFirstPersonWeaponMesh();
		const auto* TP = Weapon->GetThirdPersonWeaponMesh();
		return Character->GetMesh()->GetSkeletalMeshAsset() && Arms->GetSkeletalMeshAsset()
			&& Character->BodyVisual->bHiddenInGame && Character->HeadVisual->bHiddenInGame
			&& Flag(Character->GetMesh(), TEXT("bOwnerNoSee")) && Flag(Arms, TEXT("bOnlyOwnerSee"))
			&& FP->IsRegistered() && TP->IsRegistered() && FP->GetStaticMesh() && TP->GetStaticMesh() == FP->GetStaticMesh()
			&& Flag(FP, TEXT("bOnlyOwnerSee")) && Flag(TP, TEXT("bOwnerNoSee"))
			&& Arms->IsVisible() == Character->IsLocallyControlled() && FP->IsVisible() == Character->IsLocallyControlled()
			&& TP->IsVisible() && FP->GetAttachParent() == Arms && TP->GetAttachParent() == Character->GetMesh()
			&& Arms->GetCollisionEnabled() == ECollisionEnabled::NoCollision && TP->GetCollisionEnabled() == ECollisionEnabled::NoCollision
			&& Cast<UArenaDuelVisualAnimInstance>(Arms->GetAnimInstance()) && Cast<UArenaDuelVisualAnimInstance>(Character->GetMesh()->GetAnimInstance());
	}
	bool Equipped(AArenaDuelCharacter* Character, int32 Index)
	{
		const TCHAR* Names[] = {TEXT("SM_ArcRifle"), TEXT("SM_ShadeSMG"), TEXT("SM_RuneDMR"), TEXT("SM_HexShotgun")};
		return Valid(Character) && static_cast<int32>(Character->GetWeaponComponent()->GetCurrentWeaponId()) == Index
			&& Character->GetWeaponComponent()->GetThirdPersonWeaponMesh()->GetStaticMesh()->GetName() == Names[Index];
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaDuelVisualContract, "ArenaDuel.Visuals.Contract", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FArenaDuelVisualContract::RunTest(const FString& Parameters)
{
	const auto* Character = GetDefault<AArenaDuelCharacter>();
	TestNotNull(TEXT("Real humanoid body"), Character->GetMesh()->GetSkeletalMeshAsset());
	const auto* BlueprintClass = LoadClass<AArenaDuelCharacter>(nullptr, TEXT("/Game/ArenaDuel/Characters/BP_ArenaDuelCharacter.BP_ArenaDuelCharacter_C"));
	const auto* BlueprintDefaults = BlueprintClass ? BlueprintClass->GetDefaultObject<AArenaDuelCharacter>() : nullptr;
	TestTrue(TEXT("Saved Blueprint inherits body and first-person mesh"), BlueprintDefaults && BlueprintDefaults->GetMesh()->GetSkeletalMeshAsset() && BlueprintDefaults->GetFirstPersonArms()->GetSkeletalMeshAsset());
	TestNotNull(TEXT("Compatible skeletal first-person mesh"), Character->GetFirstPersonArms()->GetSkeletalMeshAsset());
	TestTrue(TEXT("First person uses the dedicated forearm and hand mesh"), Character->GetFirstPersonArms()->GetSkeletalMeshAsset() && Character->GetFirstPersonArms()->GetSkeletalMeshAsset()->GetName() == TEXT("SKM_ArenaDuelFPSArms"));
	TestTrue(TEXT("Primitives no longer render"), Character->BodyVisual->bHiddenInGame && Character->HeadVisual->bHiddenInGame);
	TestTrue(TEXT("Body/arms visibility separated"), VisualSmoke::Flag(Character->GetMesh(), TEXT("bOwnerNoSee")) && VisualSmoke::Flag(Character->GetFirstPersonArms(), TEXT("bOnlyOwnerSee")));
	TestEqual(TEXT("Body hit extent unchanged"), Character->BodyHitZone->GetUnscaledBoxExtent(), FVector(38,38,70));
	TestEqual(TEXT("Head hit extent unchanged"), Character->HeadHitZone->GetUnscaledBoxExtent(), FVector(24,24,14));
	TestEqual(TEXT("Body hit position unchanged"), Character->BodyHitZone->GetRelativeLocation(), FVector(0,0,-16));
	TestEqual(TEXT("Head hit position unchanged"), Character->HeadHitZone->GetRelativeLocation(), FVector(0,0,68));
	for (const TCHAR* Name : {TEXT("ArcRifle"), TEXT("ShadeSMG"), TEXT("RuneDMR"), TEXT("HexShotgun")})
	{
		const FString Path = FString::Printf(TEXT("/Game/ArenaDuel/Weapons/%s/SM_%s"), Name, Name);
		const auto* MeshAsset = LoadObject<UStaticMesh>(nullptr, *Path);
		const UStaticMeshSocket* Muzzle = MeshAsset ? MeshAsset->FindSocket(TEXT("Muzzle")) : nullptr;
		TestTrue(TEXT("Generated weapon muzzle is ahead of grip on +X"), Muzzle && Muzzle->RelativeLocation.X > 0.0f
			&& FVector::DotProduct(Muzzle->RelativeRotation.RotateVector(FVector::ForwardVector), FVector::ForwardVector) > 0.99f);
	}
	return true;
}

struct FArenaDuelVisualNetworkState : FBasePIENetworkComponentState
{
	TWeakObjectPtr<AArenaDuelCharacter> PreviousPawn;
	FTransform DeathMeshRelativeTransform = FTransform::Identity;
	float DeathObservedAt = -1.0f;
	bool bDeathTransformCaptured = false;
	FTransform HipViewmodelTransform = FTransform::Identity;
	float HipFieldOfView = 90.0f;
};
NETWORK_TEST_CLASS(FArenaDuelVisualNetworkSmoke, "ArenaDuel.Visuals.Network")
{
	FPIENetworkComponent<FArenaDuelVisualNetworkState> Network{TestRunner, TestCommandBuilder, bInitializing};
	BEFORE_EACH()
	{
		auto* Mode = LoadClass<AGameModeBase>(nullptr, TEXT("/Game/ArenaDuel/Game/BP_ArenaDuelGameMode.BP_ArenaDuelGameMode_C"));
		FNetworkComponentBuilder<FArenaDuelVisualNetworkState>().WithClients(1).AsListenServer().WithGameMode(Mode).Build(Network);
	}
	TEST_METHOD(HostClientWeaponArchetypeAndRespawn)
	{
		using namespace VisualSmoke;
		Network.UntilServer(TEXT("Both pawns exist"), [](auto& S) { return Pawn(S.World,true) && Pawn(S.World,false); }, FTimespan::FromSeconds(10))
		.ThenServer(TEXT("Host readies"), [](auto& S) { PC(S.World)->ServerSetCharacterReady(true); })
		.UntilClient(TEXT("Local and opponent visuals initialized"), 0, [](auto& S) { return Valid(Pawn(S.World,true)) && Valid(Pawn(S.World,false)); }, FTimespan::FromSeconds(10))
		.ThenClient(TEXT("Client readies"), 0, [](auto& S) { PC(S.World)->ServerSetCharacterReady(true); })
		.UntilServer(TEXT("Active round and host viewmodel"), [](auto& S) { return S.World->template GetGameState<AArenaDuelGameState>()->IsRoundInProgress() && Valid(Pawn(S.World,true)) && Valid(Pawn(S.World,false)); }, FTimespan::FromSeconds(10))
		.UntilClient(TEXT("Client observes round start"),0,[](auto& S){ return S.World->template GetGameState<AArenaDuelGameState>()->IsRoundInProgress() && Valid(Pawn(S.World,true)); },FTimespan::FromSeconds(5));
		for (int32 Index=0; Index<4; ++Index)
		{
			Network.ThenServer(TEXT("Host switches visual weapon"), [Index](auto& S){ for (bool Local : {true,false}) Pawn(S.World,Local)->GetWeaponComponent()->GrantAllWeaponsForDevelopment(); Pawn(S.World,true)->GetWeaponComponent()->EquipWeapon(Index); })
			.ThenClient(TEXT("Client switches through normal weapon command"),0,[Index](auto& S){ Pawn(S.World,true)->GetWeaponComponent()->EquipWeapon(Index); })
			.UntilServer(TEXT("Server has both weapon choices"),[Index](auto& S){ return Equipped(Pawn(S.World,true),Index) && Equipped(Pawn(S.World,false),Index); },FTimespan::FromSeconds(5))
			.UntilClient(TEXT("Owner and remote held weapons converge"),0,[Index](auto& S){ return Equipped(Pawn(S.World,true),Index) && Equipped(Pawn(S.World,false),Index); },FTimespan::FromSeconds(5))
			.UntilClient(TEXT("Support hand reaches this weapon grip without invalid pose"),0,[](auto& S){
				const auto* C=Pawn(S.World,true);
				if (!C || !C->GetFirstPersonArms() || !C->GetFirstPersonArms()->GetSkeletalMeshAsset()) return false;
				FVector GripWorld;
				if (!C->GetWeaponComponent()->GetLeftHandGripWorldLocation(GripWorld)) return false;
				const FVector HandWorld=C->GetFirstPersonArms()->GetBoneLocation(TEXT("hand_l"));
				return !GripWorld.ContainsNaN() && !HandWorld.ContainsNaN() && FVector::Dist(GripWorld,HandWorld)<6.0f;
			},FTimespan::FromSeconds(4))
			.ThenClient(TEXT("Record weapon hip transform and enter ADS"),0,[](auto& S){
				auto* C=Pawn(S.World,true);
				S.HipViewmodelTransform=C->GetFirstPersonViewmodelRoot()->GetRelativeTransform();
				S.HipFieldOfView=C->GetFirstPersonCamera()->FieldOfView;
				C->GetWeaponComponent()->StartAim();
			})
			.UntilClient(TEXT("Each weapon ADS keeps muzzle in front and aligned"),0,[](auto& S){
				const auto* C=Pawn(S.World,true);
				const auto* Gun=C ? C->GetWeaponComponent()->GetFirstPersonWeaponMesh() : nullptr;
				if(!C || !Gun || !Gun->GetStaticMesh() || !Gun->GetStaticMesh()->FindSocket(TEXT("Muzzle"))) return false;
				const FTransform CameraTransform=C->GetFirstPersonCamera()->GetComponentTransform();
				const FVector GripView=CameraTransform.InverseTransformPosition(Gun->GetComponentLocation());
				const FVector MuzzleView=CameraTransform.InverseTransformPosition(Gun->GetSocketLocation(TEXT("Muzzle")));
				return C->GetWeaponComponent()->IsAiming()
					&& FMath::IsNearlyEqual(C->GetFirstPersonCamera()->FieldOfView,C->GetWeaponComponent()->GetActiveAimFOV(),0.1f)
					&& FVector::DotProduct(Gun->GetForwardVector(),CameraTransform.GetUnitAxis(EAxis::X))>0.98f
					&& MuzzleView.X>GripView.X+10.0f;
			},FTimespan::FromSeconds(4))
			.ThenClient(TEXT("Return this weapon from ADS to its exact hip transform"),0,[](auto& S){ Pawn(S.World,true)->GetWeaponComponent()->StopAim(); })
			.UntilClient(TEXT("Hip transform and FOV restore without drift"),0,[](auto& S){
				const auto* C=Pawn(S.World,true);
				if (!C) return false;
				FVector HipLocation;
				FRotator HipRotation;
				C->GetWeaponComponent()->GetCurrentViewmodelBaseTransform(HipLocation, HipRotation);
				const FTransform Actual=C->GetFirstPersonViewmodelRoot()->GetRelativeTransform();
				return C && !C->GetWeaponComponent()->IsAiming()
					&& FMath::IsNearlyEqual(C->GetFirstPersonCamera()->FieldOfView,S.HipFieldOfView,0.1f)
					&& FVector::Dist(Actual.GetLocation(),HipLocation)<2.0f
					&& Actual.GetRotation().AngularDistance(HipRotation.Quaternion())<FMath::DegreesToRadians(2.0f);
			},FTimespan::FromSeconds(4));
		}
		for (auto Archetype : {EArenaDuelCharacterArchetype::Warden, EArenaDuelCharacterArchetype::Rift, EArenaDuelCharacterArchetype::Shadow})
		{
			Network.ThenServer(TEXT("Switch both archetype appearances"),[Archetype](auto& S){ for(bool Local:{true,false}) Pawn(S.World,Local)->template GetPlayerState<AArenaDuelPlayerState>()->SetCharacterArchetypeForDevelopment(Archetype); })
			.UntilClient(TEXT("Body armor updates while first-person skin stays intact"),0,[Archetype](auto& S){
				const TCHAR* Names[] = {TEXT("M_ShadowArmor"),TEXT("M_WardenArmor"),TEXT("M_RiftArmor")};
				for(bool Local:{true,false}) {
					auto* C=Pawn(S.World,Local); if(!C || !C->GetMesh()->GetMaterial(0) || C->GetMesh()->GetMaterial(0)->GetName()!=Names[static_cast<int32>(Archetype)]) return false;
					if(!C->GetFirstPersonArms()->GetMaterial(0) || C->GetFirstPersonArms()->GetMaterial(0)->GetName()==Names[static_cast<int32>(Archetype)]) return false;
				} return true;
			},FTimespan::FromSeconds(5));
		}
		Network.ThenClient(TEXT("Record owner before respawn"),0,[](auto& S){ S.PreviousPawn=Pawn(S.World,true); })
		.ThenServer(TEXT("Normal authoritative round reset"),[](auto& S){ S.PreviousPawn=Pawn(S.World,true); S.World->template GetAuthGameMode<AArenaDuelGameMode>()->AdminRestartRound(); })
		.UntilServer(TEXT("Fresh host and world representation"),[](auto& S){return Pawn(S.World,true)!=S.PreviousPawn.Get() && Valid(Pawn(S.World,true)) && Valid(Pawn(S.World,false));},FTimespan::FromSeconds(8))
		.UntilClient(TEXT("Fresh client arms and opponent weapon without manual switch"),0,[](auto& S){auto HoldsLoadoutDefault=[](AArenaDuelCharacter* C){ return C && C->GetWeaponComponent()->GetLoadoutFirearms().Num()>0 && Equipped(C,C->GetWeaponComponent()->GetLoadoutFirearms()[0]) && C->GetWeaponComponent()->GetActiveSlot()==EArenaDuelLoadoutSlot::Primary; }; return Pawn(S.World,true)!=S.PreviousPawn.Get() && HoldsLoadoutDefault(Pawn(S.World,true)) && HoldsLoadoutDefault(Pawn(S.World,false));},FTimespan::FromSeconds(8))
		.ThenClient(TEXT("Report evaluated pose and attachment"),0,[this](auto& S){
			const auto* C=Pawn(S.World,true);
			const auto* Anim=Cast<UArenaDuelVisualAnimInstance>(C->GetFirstPersonArms()->GetAnimInstance());
			if (!Anim || !Anim->GetAnimationAsset()) TestRunner->AddError(TEXT("First person pose did not evaluate"));
			const auto* Gun=C->GetWeaponComponent()->GetFirstPersonWeaponMesh();
			const FVector ViewLocation = C->GetFirstPersonCamera()->GetComponentTransform().InverseTransformPosition(Gun->GetComponentLocation());
			const float ForwardDot = FVector::DotProduct(Gun->GetForwardVector(),C->GetFirstPersonCamera()->GetForwardVector());
			if (ForwardDot < 0.98f || ViewLocation.X < 10 || ViewLocation.X > 100 || FMath::Abs(ViewLocation.Y) > 40)
				TestRunner->AddError(FString::Printf(TEXT("Evaluated viewmodel is not in front of camera: forwardDot=%.3f viewLocation=(%.1f, %.1f, %.1f) gunYaw=%.1f cameraYaw=%.1f"), ForwardDot, ViewLocation.X, ViewLocation.Y, ViewLocation.Z, Gun->GetComponentRotation().Yaw, C->GetFirstPersonCamera()->GetComponentRotation().Yaw));
		})
		.ThenClient(TEXT("ADS keeps connected arm and gun hierarchy"),0,[](auto& S){ Pawn(S.World,true)->GetWeaponComponent()->StartAim(); })
		.UntilClient(TEXT("ADS FOV and visual alignment settle"),0,[](auto& S){
			const auto* C=Pawn(S.World,true); const auto* W=C->GetWeaponComponent();
			return W->IsAiming() && FMath::IsNearlyEqual(C->GetFirstPersonCamera()->FieldOfView,W->GetActiveAimFOV(),0.1f)
				&& C->GetFirstPersonViewmodelRoot()->GetRelativeLocation().Equals(W->GetCurrentDefinition().AimViewmodelLocation,0.1f);
		},FTimespan::FromSeconds(4))
		.ThenClient(TEXT("Leave ADS"),0,[](auto& S){ Pawn(S.World,true)->GetWeaponComponent()->StopAim(); })
		.UntilClient(TEXT("User-selected hip FOV restored"),0,[](auto& S){const auto* C=Pawn(S.World,true);const auto* Controller=VisualSmoke::PC(S.World);return C&&Controller&&FMath::IsNearlyEqual(C->GetFirstPersonCamera()->FieldOfView,Controller->GetLocalSettings().FOV,0.1f);},FTimespan::FromSeconds(4))
		.ThenServer(TEXT("Death keeps skeletal presentation cosmetic"),[](auto& S){ S.DeathObservedAt=-1.0f; S.bDeathTransformCaptured=false; Pawn(S.World,false)->AdminKill(); })
		.UntilClient(TEXT("Death hides own arms and ragdolls the world body"),0,[](auto& S){
			const auto* C=Pawn(S.World,true);
			if(C && C->IsDead() && S.DeathObservedAt<0.0f)
				S.DeathObservedAt=S.World->GetTimeSeconds();
			const bool bFallen=C && C->IsDead() && VisualSmoke::PlaysDeath(C);
			if(bFallen && !S.bDeathTransformCaptured) { S.DeathMeshRelativeTransform=C->GetMesh()->GetRelativeTransform(); S.bDeathTransformCaptured=true; }
			return C && C->IsDead() && !C->GetFirstPersonArms()->IsVisible() && !C->GetWeaponComponent()->GetFirstPersonWeaponMesh()->IsVisible()
				&& VisualSmoke::PlaysDeath(C) && bFallen;
		},FTimespan::FromSeconds(2))
		.UntilClient(TEXT("Client corpse remains a ragdoll for 2.8 seconds"),0,[](auto& S){
			const auto* C=Pawn(S.World,true);
			return C && C->IsDead() && VisualSmoke::PlaysDeath(C) && S.DeathObservedAt>=0.0f && S.bDeathTransformCaptured
				&& S.World->GetTimeSeconds()-S.DeathObservedAt>=2.8f;
		},FTimespan::FromSeconds(3.0))
		.UntilClient(TEXT("Automatic next round restores own and opponent visuals"),0,[](auto& S){return Valid(Pawn(S.World,true)) && !Pawn(S.World,true)->IsDead() && Valid(Pawn(S.World,false));},FTimespan::FromSeconds(6))
		.ThenServer(TEXT("Reverse the death direction by killing the listen-server pawn"),[](auto& S){ S.DeathObservedAt=-1.0f; S.bDeathTransformCaptured=false; Pawn(S.World,true)->AdminKill(); })
		.UntilClient(TEXT("Remote host corpse reaches the fallen state"),0,[](auto& S){
			const auto* C=Pawn(S.World,false);
			if(C && C->IsDead() && S.DeathObservedAt<0.0f)
				S.DeathObservedAt=S.World->GetTimeSeconds();
			const bool bFallen=C && C->IsDead() && VisualSmoke::PlaysDeath(C);
			if(bFallen && !S.bDeathTransformCaptured) { S.DeathMeshRelativeTransform=C->GetMesh()->GetRelativeTransform(); S.bDeathTransformCaptured=true; }
			return C && C->IsDead() && VisualSmoke::PlaysDeath(C) && bFallen;
		},FTimespan::FromSeconds(2))
		.UntilClient(TEXT("Remote host corpse remains a ragdoll for 2.8 seconds"),0,[](auto& S){
			const auto* C=Pawn(S.World,false);
			return C && C->IsDead() && VisualSmoke::PlaysDeath(C) && S.DeathObservedAt>=0.0f && S.bDeathTransformCaptured
				&& S.World->GetTimeSeconds()-S.DeathObservedAt>=2.8f;
		},FTimespan::FromSeconds(3.0))
		.UntilClient(TEXT("Final round restart replaces the reverse-direction corpse"),0,[](auto& S){return Valid(Pawn(S.World,true)) && !Pawn(S.World,true)->IsDead() && Valid(Pawn(S.World,false)) && !Pawn(S.World,false)->IsDead();},FTimespan::FromSeconds(6));
	}
};
#endif
