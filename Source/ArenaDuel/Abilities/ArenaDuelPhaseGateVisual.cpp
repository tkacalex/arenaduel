#include "ArenaDuelPhaseGateVisual.h"
#include "../Player/ArenaDuelPlayerState.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AArenaDuelPhaseGateVisual::AArenaDuelPhaseGateVisual()
{
	bReplicates = true;
	SetReplicateMovement(false);
	InitialLifeSpan = 2.0f;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Engine/BasicShapes/BasicShapeMaterial_Inst.BasicShapeMaterial_Inst"));
	Center = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GateCenter"));
	RootComponent = Center;
	Center->SetStaticMesh(Cube.Object);
	Center->SetMaterial(0, Material.Object);
	Center->SetRelativeScale3D(FVector(0.04f, 0.8f, 1.6f));
	Center->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Center->SetGenerateOverlapEvents(false);
	Center->SetCastShadow(false);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		UStaticMeshComponent* Edge = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("GateRim%d"), Index));
		Edge->SetupAttachment(Center);
		// Absolute scale avoids inheriting the thin center plane's nonuniform scale.
		Edge->SetAbsolute(false, false, true);
		Edge->SetStaticMesh(Cube.Object);
		Edge->SetMaterial(0, Material.Object);
		Edge->SetRelativeLocation(Index < 2 ? FVector(0, Index == 0 ? -52.5f : 52.5f, 0) : FVector(0, 0, Index == 2 ? -53.0f : 53.0f));
		Edge->SetWorldScale3D(Index < 2 ? FVector(0.08f, 0.07f, 1.75f) : FVector(0.08f, 0.9f, 0.07f));
		Edge->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Edge->SetGenerateOverlapEvents(false);
		Edge->SetCastShadow(false);
		Rim.Add(Edge);
	}
}

void AArenaDuelPhaseGateVisual::BeginPlay()
{
	Super::BeginPlay();
	SetLifeSpan(2.0f); // 0.2s cast plus 1.8s residue; never a traversable portal.
	if (auto* MID = Center->CreateAndSetMaterialInstanceDynamic(0)) MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.19f, 0.025f, 0.34f));
	for (UStaticMeshComponent* Edge : Rim) if (auto* MID = Edge->CreateAndSetMaterialInstanceDynamic(0)) MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.2f, 0.8f, 1.0f));
}

void AArenaDuelPhaseGateVisual::DestroyAllForRound(UWorld* World)
{
	if (!World || !World->GetAuthGameMode()) return;
	for (TActorIterator<AArenaDuelPhaseGateVisual> It(World); It; ++It) It->Destroy();
}

void AArenaDuelPhaseGateVisual::DestroyOwnedByPlayerState(UWorld* World, const AArenaDuelPlayerState* State)
{
	if (!World || !World->GetAuthGameMode() || !State) return;
	for (TActorIterator<AArenaDuelPhaseGateVisual> It(World); It; ++It) if (It->GetOwner() == State) It->Destroy();
}
