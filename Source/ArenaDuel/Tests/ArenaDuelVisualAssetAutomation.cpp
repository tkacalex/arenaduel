#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/SkeletalMesh.h"
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

#endif
