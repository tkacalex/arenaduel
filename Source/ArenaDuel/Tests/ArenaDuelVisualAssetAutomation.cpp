#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInterface.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "UObject/UObjectGlobals.h"
#include "Engine/Blueprint.h"
#include "MeshDescription.h"
#include "SkeletalMeshAttributes.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/FileManager.h"
#include "../Characters/ArenaDuelCharacter.h"

namespace
{
	bool SaveAssetPackage(UObject* Asset)
	{
		if (!Asset) return false;
		UPackage* Package = Asset->GetOutermost();
		Package->MarkPackageDirty();
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		return UPackage::SavePackage(Package, Asset, *Filename, Args);
	}

	/** The material a slot of a character mesh has to get, by the slot's name; empty for a slot the setup leaves alone. */
	FString RequiredMaterialPath(FString Slot, const FString& Variant)
	{
		// Blender may number a slot (Z_Skin.001); the material is found by the name before the dot.
		int32 Dot = INDEX_NONE;
		if (Slot.FindChar(TEXT('.'), Dot)) Slot.LeftInline(Dot);
		if (Slot.StartsWith(TEXT("FP_"))) return FString::Printf(TEXT("/Game/ArenaDuel/Characters/Common/M_FP%s"), *Slot.RightChop(3));
		if (Slot.StartsWith(TEXT("Z_Skin"))) return FString::Printf(TEXT("/Game/ArenaDuel/Characters/Zombies/M_ZombieSkin_%s"), *Variant);
		if (Slot.StartsWith(TEXT("Z_"))) return FString::Printf(TEXT("/Game/ArenaDuel/Characters/Zombies/M_Zombie%s"), *Slot.RightChop(2));
		return FString();
	}

	/** Everything that is required and missing for one mesh: the mesh itself, a material for one of the given slots. Loads, never saves. */
	TArray<FString> FindMissingSetupAssets(const FString& MeshPath, const FString& Variant, const TArray<FName>* SlotsInsteadOfMesh = nullptr)
	{
		TArray<FString> Missing;
		TArray<FName> Slots;
		if (SlotsInsteadOfMesh) Slots = *SlotsInsteadOfMesh;
		else if (const USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *MeshPath, nullptr, LOAD_NoWarn | LOAD_Quiet)) { for (const FSkeletalMaterial& Material : Mesh->GetMaterials()) Slots.Add(Material.MaterialSlotName); }
		else { Missing.Add(FString::Printf(TEXT("Required mesh missing: %s"), *MeshPath)); return Missing; }
		for (const FName& Slot : Slots)
		{
			const FString Path = RequiredMaterialPath(Slot.ToString(), Variant);
			if (!Path.IsEmpty() && !LoadObject<UMaterialInterface>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet)) Missing.Add(FString::Printf(TEXT("%s: required material missing at %s for slot %s"), *MeshPath, *Path, *Slot.ToString()));
		}
		return Missing;
	}

	bool AssignArmsMeshToCharacterDefaults(USkeletalMesh* Arms)
	{
		if (!Arms) return false;
		AArenaDuelCharacter* NativeDefaults = GetMutableDefault<AArenaDuelCharacter>();
		NativeDefaults->GetFirstPersonArms()->SetSkeletalMesh(Arms);
		UBlueprint* CharacterBlueprint = LoadObject<UBlueprint>(nullptr, TEXT("/Game/ArenaDuel/Characters/BP_ArenaDuelCharacter.BP_ArenaDuelCharacter"));
		if (!CharacterBlueprint || !CharacterBlueprint->GeneratedClass) return false;
		auto* BlueprintDefaults = Cast<AArenaDuelCharacter>(CharacterBlueprint->GeneratedClass->GetDefaultObject());
		if (!BlueprintDefaults) return false;
		BlueprintDefaults->GetFirstPersonArms()->SetSkeletalMesh(Arms);
		return SaveAssetPackage(CharacterBlueprint);
	}
}

// Explicit editor setup command. It creates the forearm viewmodel and fixes the
// native and saved Blueprint defaults so HandGrip_R is valid on the next PIE spawn.
// These two are setup commands that write assets, not checks. Their names do not start with "ArenaDuel", so a
// run of the project's tests ("Automation RunTests ArenaDuel") leaves them out; they run only when asked for
// by name ("Automation RunTests ProjectAssetSetup").
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaDuelCreateArmsAsset, "ProjectAssetSetup.Arms", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FArenaDuelCreateArmsAsset::RunTest(const FString& Parameters)
{
	const TCHAR* PackagePath = TEXT("/Game/ArenaDuel/Characters/Common/SKM_ArenaDuelFPSArms");
	// The gloved hands made in Blender (Tools/Editor/SetupFirstPersonHands.py) are the first person mesh.
	// The cut-out mannequin forearms below are only the fallback for a checkout without them.
	USkeletalMesh* Arms = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/ArenaDuel/Characters/Common/SKM_ArenaDuelFPHands"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Arms) Arms = LoadObject<USkeletalMesh>(nullptr, PackagePath, nullptr, LOAD_NoWarn);
	if (!Arms)
	{
		USkeletalMesh* Source = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"));
		if (!Source || !Source->GetMeshDescription(0)) { AddError(TEXT("Epic mesh has no editable source description")); return false; }
		UPackage* Package = CreatePackage(PackagePath);
		Arms = DuplicateObject<USkeletalMesh>(Source, Package, TEXT("SKM_ArenaDuelFPSArms"));
		Arms->SetFlags(RF_Public | RF_Standalone);
		int32 KeptPolygons = 0;
		for (int32 LOD = 0; LOD < Arms->GetLODNum(); ++LOD)
		{
			FMeshDescription* Description = Arms->GetMeshDescription(LOD);
			if (!Description) continue;
			const FSkeletalMeshConstAttributes Attributes(*Description);
			const auto Weights = Attributes.GetVertexSkinWeights();
			const auto BoneNames = Attributes.GetBoneNames();
			TArray<FPolygonID> Remove;
			for (const FPolygonID Polygon : Description->Polygons().GetElementIDs())
			{
				bool bForearmOrHand = true;
				for (const FVertexID Vertex : Description->GetPolygonVertices(Polygon))
				{
					float ForearmWeight = 0.0f;
					for (const auto& Weight : Weights.Get(Vertex))
					{
						const FString Bone = BoneNames[FBoneID(Weight.GetBoneIndex())].ToString();
						if (Bone.StartsWith(TEXT("lowerarm")) || Bone.StartsWith(TEXT("hand"))
							|| Bone.StartsWith(TEXT("thumb")) || Bone.StartsWith(TEXT("index")) || Bone.StartsWith(TEXT("middle"))
							|| Bone.StartsWith(TEXT("ring")) || Bone.StartsWith(TEXT("pinky"))) ForearmWeight += Weight.GetWeight();
					}
					if (ForearmWeight < 0.5f) { bForearmOrHand = false; break; }
				}
				if (!bForearmOrHand) Remove.Add(Polygon); else if (LOD == 0) ++KeptPolygons;
			}
			Description->DeletePolygons(Remove);
			FElementIDRemappings Remappings;
			Description->Compact(Remappings);
			Arms->CommitMeshDescription(LOD);
		}
		if (KeptPolygons < 100) { AddError(FString::Printf(TEXT("Forearm extraction retained too little geometry: %d polygons"), KeptPolygons)); return false; }
		Arms->SetPhysicsAsset(nullptr);
		Arms->Build();
		FAssetRegistryModule::AssetCreated(Arms);
		if (!SaveAssetPackage(Arms)) { AddError(TEXT("Could not save the first-person forearm mesh")); return false; }
		AddInfo(FString::Printf(TEXT("Generated first-person mesh with %d forearm and hand polygons"), KeptPolygons));
	}
	if (!AssignArmsMeshToCharacterDefaults(Arms)) { AddError(TEXT("Could not assign the forearm mesh to native and Blueprint character defaults")); return false; }
	return true;
}

// Explicit editor setup command, run after the import scripts under Tools/Editor: gives the first person
// hands and the zombie bodies their materials by slot name and saves them. A Python script cannot do this
// part: the package of a freshly loaded skeletal mesh stays open for reading and the save fails.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaDuelAssignCharacterMaterials, "ProjectAssetSetup.CharacterMaterials", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FArenaDuelAssignCharacterMaterials::RunTest(const FString& Parameters)
{
	const auto Assign = [this](const FString& MeshPath, const FString& Variant)
	{
		// Every mesh named below and every material its slots ask for is required: anything missing fails the
		// setup for that mesh before a single asset is touched.
		const TArray<FString> Missing = FindMissingSetupAssets(MeshPath, Variant);
		for (const FString& Entry : Missing) AddError(Entry);
		if (Missing.Num() > 0) return;
		USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *MeshPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		TArray<FSkeletalMaterial> Materials = Mesh->GetMaterials();
		int32 Assigned = 0;
		for (FSkeletalMaterial& Material : Materials)
		{
			const FString Path = RequiredMaterialPath(Material.MaterialSlotName.ToString(), Variant);
			if (Path.IsEmpty()) continue;
			Material.MaterialInterface = LoadObject<UMaterialInterface>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
			++Assigned;
		}
		Mesh->Modify();
		Mesh->SetMaterials(Materials);
		// Zombie bodies take their hit zones from the mannequin's physics asset.
		if (!Variant.IsEmpty() && !Mesh->GetPhysicsAsset())
		{
			if (UPhysicsAsset* Zones = LoadObject<UPhysicsAsset>(nullptr, TEXT("/Game/Characters/Mannequins/Rigs/PA_Mannequin.PA_Mannequin"))) Mesh->SetPhysicsAsset(Zones);
			else AddError(FString::Printf(TEXT("%s: the hit zone physics asset PA_Mannequin is missing"), *Mesh->GetName()));
		}
		ResetLoaders(Mesh->GetOutermost());
		if (!SaveAssetPackage(Mesh)) AddError(FString::Printf(TEXT("Could not save %s"), *Mesh->GetName()));
		else AddInfo(FString::Printf(TEXT("%s: %d of %d slots assigned"), *Mesh->GetName(), Assigned, Materials.Num()));
	};
	Assign(TEXT("/Game/ArenaDuel/Characters/Common/SKM_ArenaDuelFPHands"), FString());
	for (const TCHAR* Variant : { TEXT("Normal"), TEXT("Runner"), TEXT("Armoured"), TEXT("Brute"), TEXT("Abomination") })
	{
		Assign(FString::Printf(TEXT("/Game/ArenaDuel/Characters/Zombies/SKM_Zombie_%s"), Variant), Variant);
	}
	return !HasAnyErrors();
}

// Phase 9, E15: the setup's own check, run against assets that do not exist. It loads and never saves, so it is a
// regular test and touches nothing.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaDuelPhase9AssetSetupMissingTest, "ArenaDuel.Phase9.AssetSetup.MissingAssetsAreErrors", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FArenaDuelPhase9AssetSetupMissingTest::RunTest(const FString& Parameters)
{
	const TArray<FString> NoMesh = FindMissingSetupAssets(TEXT("/Game/ArenaDuel/Characters/Zombies/SKM_Zombie_DoesNotExist"), TEXT("Normal"));
	TestEqual(TEXT("A missing mesh is one error"), NoMesh.Num(), 1);
	TestTrue(TEXT("The error names the mesh"), NoMesh.Num() == 1 && NoMesh[0].Contains(TEXT("SKM_Zombie_DoesNotExist")) && NoMesh[0].Contains(TEXT("Required mesh missing")));
	// A mesh whose slots ask for materials that are not there: two of them missing, one slot the setup leaves alone.
	const TArray<FName> Slots = { FName(TEXT("Z_NoSuchPart")), FName(TEXT("Z_Skin.001")), FName(TEXT("SomethingElse")) };
	const TArray<FString> NoMaterial = FindMissingSetupAssets(TEXT("/Game/ArenaDuel/Characters/Zombies/SKM_Pretend"), TEXT("NoSuchVariant"), &Slots);
	TestEqual(TEXT("Every missing material is an error, an unrelated slot is not"), NoMaterial.Num(), 2);
	TestTrue(TEXT("The errors name the material paths"), NoMaterial.Num() == 2 && NoMaterial[0].Contains(TEXT("/Game/ArenaDuel/Characters/Zombies/M_ZombieNoSuchPart")) && NoMaterial[1].Contains(TEXT("M_ZombieSkin_NoSuchVariant")));
	TestEqual(TEXT("A numbered slot resolves like its plain name"), RequiredMaterialPath(TEXT("Z_Skin.001"), TEXT("Runner")), FString(TEXT("/Game/ArenaDuel/Characters/Zombies/M_ZombieSkin_Runner")));
	// The real meshes have everything they need, so the setup would not fail on the project as it is.
	TestEqual(TEXT("The hands have all their materials"), FindMissingSetupAssets(TEXT("/Game/ArenaDuel/Characters/Common/SKM_ArenaDuelFPHands"), FString()).Num(), 0);
	for (const TCHAR* Variant : { TEXT("Normal"), TEXT("Runner"), TEXT("Armoured"), TEXT("Brute"), TEXT("Abomination") })
	{
		const TArray<FString> Missing = FindMissingSetupAssets(FString::Printf(TEXT("/Game/ArenaDuel/Characters/Zombies/SKM_Zombie_%s"), Variant), Variant);
		for (const FString& Entry : Missing) AddError(Entry);
	}
	return !HasAnyErrors();
}

#endif
