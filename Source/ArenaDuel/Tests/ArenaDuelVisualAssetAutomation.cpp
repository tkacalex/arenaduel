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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaDuelCreateArmsAsset, "ArenaDuel.VisualAssetSetup.Arms", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaDuelAssignCharacterMaterials, "ArenaDuel.VisualAssetSetup.CharacterMaterials", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FArenaDuelAssignCharacterMaterials::RunTest(const FString& Parameters)
{
	const auto Assign = [this](const FString& MeshPath, const FString& Variant)
	{
		USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *MeshPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Mesh) return;
		TArray<FSkeletalMaterial> Materials = Mesh->GetMaterials();
		int32 Assigned = 0;
		for (FSkeletalMaterial& Material : Materials)
		{
			// Blender may number a slot (Z_Skin.001); the material is found by the name before the dot.
			FString Slot = Material.MaterialSlotName.ToString();
			int32 Dot = INDEX_NONE;
			if (Slot.FindChar(TEXT('.'), Dot)) Slot.LeftInline(Dot);
			FString Path;
			if (Slot.StartsWith(TEXT("FP_"))) Path = FString::Printf(TEXT("/Game/ArenaDuel/Characters/Common/M_FP%s"), *Slot.RightChop(3));
			else if (Slot.StartsWith(TEXT("Z_Skin"))) Path = FString::Printf(TEXT("/Game/ArenaDuel/Characters/Zombies/M_ZombieSkin_%s"), *Variant);
			else if (Slot.StartsWith(TEXT("Z_"))) Path = FString::Printf(TEXT("/Game/ArenaDuel/Characters/Zombies/M_Zombie%s"), *Slot.RightChop(2));
			if (Path.IsEmpty()) continue;
			if (UMaterialInterface* Found = LoadObject<UMaterialInterface>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet)) { Material.MaterialInterface = Found; ++Assigned; }
			else AddWarning(FString::Printf(TEXT("%s: no material at %s for slot %s"), *Mesh->GetName(), *Path, *Slot));
		}
		Mesh->Modify();
		Mesh->SetMaterials(Materials);
		// Zombie bodies take their hit zones from the mannequin's physics asset.
		if (!Variant.IsEmpty() && !Mesh->GetPhysicsAsset())
		{
			if (UPhysicsAsset* Zones = LoadObject<UPhysicsAsset>(nullptr, TEXT("/Game/Characters/Mannequins/Rigs/PA_Mannequin.PA_Mannequin"))) Mesh->SetPhysicsAsset(Zones);
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
	return true;
}

#endif
