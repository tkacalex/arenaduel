#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/SkeletalMesh.h"
#include "MeshDescription.h"
#include "SkeletalMeshAttributes.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/FileManager.h"

// Explicit editor setup command, never a runtime path or part of regression test discovery.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaDuelCreateArmsAsset, "ArenaDuel.VisualAssetSetup.Arms", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FArenaDuelCreateArmsAsset::RunTest(const FString& Parameters)
{
	const TCHAR* PackagePath = TEXT("/Game/ArenaDuel/Characters/Common/SKM_ArenaDuelArms");
	if (LoadObject<USkeletalMesh>(nullptr, PackagePath, nullptr, LOAD_NoWarn)) return true;
	USkeletalMesh* Source = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"));
	if (!Source || !Source->GetMeshDescription(0)) { AddError(TEXT("Epic mesh has no editable source description")); return false; }
	UPackage* Package = CreatePackage(PackagePath);
	USkeletalMesh* Arms = DuplicateObject<USkeletalMesh>(Source, Package, TEXT("SKM_ArenaDuelArms"));
	Arms->SetFlags(RF_Public | RF_Standalone);
	int32 KeptPolygons = 0;
	for (int32 LOD = 0; LOD < Arms->GetLODNum(); ++LOD)
	{
		FMeshDescription* Description = Arms->GetMeshDescription(LOD);
		if (!Description) continue; // Generated reductions rebuild from the edited source.
		const FSkeletalMeshConstAttributes Attributes(*Description);
		const auto Weights = Attributes.GetVertexSkinWeights();
		const auto BoneNames = Attributes.GetBoneNames();
		TArray<FPolygonID> Remove;
		for (const FPolygonID Polygon : Description->Polygons().GetElementIDs())
		{
			bool bArmPolygon = true;
			for (const FVertexID Vertex : Description->GetPolygonVertices(Polygon))
			{
				float ArmWeight = 0;
				for (const auto& Weight : Weights.Get(Vertex))
				{
					const FString Bone = BoneNames[FBoneID(Weight.GetBoneIndex())].ToString();
					if (Bone.StartsWith(TEXT("upperarm")) || Bone.StartsWith(TEXT("lowerarm")) || Bone.StartsWith(TEXT("hand"))
						|| Bone.StartsWith(TEXT("thumb")) || Bone.StartsWith(TEXT("index")) || Bone.StartsWith(TEXT("middle"))
						|| Bone.StartsWith(TEXT("ring")) || Bone.StartsWith(TEXT("pinky"))) ArmWeight += Weight.GetWeight();
				}
				if (ArmWeight < 0.5f) { bArmPolygon = false; break; }
			}
			if (!bArmPolygon) Remove.Add(Polygon); else if (LOD == 0) ++KeptPolygons;
		}
		Description->DeletePolygons(Remove);
		FElementIDRemappings Remappings;
		Description->Compact(Remappings);
		Arms->CommitMeshDescription(LOD);
	}
	if (KeptPolygons < 100) { AddError(TEXT("Arm extraction retained too little geometry")); return false; }
	Arms->SetPhysicsAsset(nullptr);
	Arms->Build();
	FAssetRegistryModule::AssetCreated(Arms);
	Package->MarkPackageDirty();
	const FString Filename = FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension());
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
	FSavePackageArgs Args;
	Args.TopLevelFlags = RF_Public | RF_Standalone;
	if (!UPackage::SavePackage(Package, Arms, *Filename, Args)) { AddError(TEXT("Unreal could not serialize arms")); return false; }
	AddInfo(FString::Printf(TEXT("Unreal-derived arms: %d source polygons, compatible skeleton and sockets preserved"), KeptPolygons));
	return true;
}
#endif
