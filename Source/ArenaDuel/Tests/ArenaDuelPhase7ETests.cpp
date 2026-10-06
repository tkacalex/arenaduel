#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../Player/ArenaDuelLocalSettings.h"
#include "../UI/ArenaDuelPlayerMenuWidget.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/PointLight.h"
#include "FileHelpers.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaDuelLocalSettingsPersistenceTest, "ArenaDuel.Phase7E.Settings.Persistence", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FArenaDuelLocalSettingsPersistenceTest::RunTest(const FString& Parameters)
{
	UArenaDuelLocalSettingsSave* Save = NewObject<UArenaDuelLocalSettingsSave>();
	Save->Settings.MouseSensitivity=2.35f; Save->Settings.ADSMultiplier=0.72f; Save->Settings.FOV=104.0f; Save->Settings.MasterVolume=0.63f;
	Save->Settings.WindowMode=2; Save->Settings.ResolutionX=1600; Save->Settings.ResolutionY=900; Save->Settings.bVSync=true; Save->Settings.FPSLimit=165;
	TArray<uint8> Bytes;
	TestTrue(TEXT("Settings serialize through Unreal SaveGame archive"), UGameplayStatics::SaveGameToMemory(Save,Bytes));
	const UArenaDuelLocalSettingsSave* Loaded=Cast<UArenaDuelLocalSettingsSave>(UGameplayStatics::LoadGameFromMemory(Bytes));
	TestNotNull(TEXT("Settings deserialize"),Loaded);
	if(Loaded)
	{
		TestEqual(TEXT("Mouse sensitivity persists locally"),Loaded->Settings.MouseSensitivity,2.35f);
		TestEqual(TEXT("ADS multiplier persists locally"),Loaded->Settings.ADSMultiplier,0.72f);
		TestEqual(TEXT("FOV persists locally"),Loaded->Settings.FOV,104.0f);
		TestEqual(TEXT("Master volume persists locally"),Loaded->Settings.MasterVolume,0.63f);
		TestTrue(TEXT("Display settings persist locally"),Loaded->Settings.WindowMode==2 && Loaded->Settings.ResolutionX==1600 && Loaded->Settings.ResolutionY==900 && Loaded->Settings.bVSync && Loaded->Settings.FPSLimit==165);
	}
	UArenaDuelPlayerMenuWidget* Widget=NewObject<UArenaDuelPlayerMenuWidget>();
	TestTrue(TEXT("Native settings menu initializes"),Widget->Initialize());
	TestTrue(TEXT("Pause, settings sliders and display controls exist once"),Widget->HasExpectedTree());
	const UWidget* Root=Widget->GetRootWidget();
	TestFalse(TEXT("Repeated initialize keeps the same tree"),Widget->Initialize());
	TestTrue(TEXT("No duplicate menu root"),Widget->GetRootWidget()==Root);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaDuelArenaCoreStructureTest, "ArenaDuel.Phase7E.Arena.Structure", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FArenaDuelArenaCoreStructureTest::RunTest(const FString& Parameters)
{
	UWorld* World=UEditorLoadingAndSavingUtils::LoadMap(TEXT("/Game/ArenaDuel/Maps/L_ArenaCore"));
	TestNotNull(TEXT("L_ArenaCore loads"),World);
	if(!World)return false;
	int32 StartCount=0,WallCount=0,FloorCount=0,CeilingCount=0,LightCount=0;
	for(TActorIterator<AActor> It(World);It;++It)
	{
		const FString Label=It->GetActorLabel();
		if(It->IsA<APlayerStart>())++StartCount;
		if(const APointLight* Light=Cast<APointLight>(*It))
		{
			++LightCount;
			const UPointLightComponent* Component=Cast<UPointLightComponent>(Light->GetLightComponent());
			TestTrue(TEXT("Arena fill lights are movable and shadowless"),Component && Component->Mobility==EComponentMobility::Movable && !Component->CastShadows && Component->Intensity>0.0f);
		}
		if(Label.StartsWith(TEXT("ArenaCore_Wall_")))++WallCount;
		if(Label==TEXT("ArenaCore_Floor"))++FloorCount;
		if(Label==TEXT("ArenaCore_Ceiling"))++CeilingCount;
		if(Label.StartsWith(TEXT("ArenaCore_Floor"))||Label.StartsWith(TEXT("ArenaCore_Ceiling"))||Label.StartsWith(TEXT("ArenaCore_Wall_")))
		{
			const AStaticMeshActor* MeshActor=Cast<AStaticMeshActor>(*It);
			TestNotNull(TEXT("Arena shell member is static geometry"),MeshActor);
			if(MeshActor)
			{
				const UStaticMeshComponent* Component=MeshActor->GetStaticMeshComponent();
				TestTrue(TEXT("Arena shell has collision and static mobility"),Component && Component->GetCollisionEnabled()!=ECollisionEnabled::NoCollision && Component->Mobility==EComponentMobility::Static);
			}
		}
	}
	TestEqual(TEXT("Exactly two duel starts"),StartCount,2);
	TestEqual(TEXT("Four closed walls"),WallCount,4);
	TestEqual(TEXT("Floor exists"),FloorCount,1);
	TestEqual(TEXT("Ceiling closes the shell"),CeilingCount,1);
	TestEqual(TEXT("Five lightweight development fill lights"),LightCount,5);
	return true;
}

#endif
