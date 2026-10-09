#include "ArenaDuelHealPad.h"
#include "../Characters/ArenaDuelCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// Development helper: ArenaDuel.Health <value> sets the health of the local player, to try the pad without a fight.
	FAutoConsoleCommandWithWorldAndArgs CmdSetHealth(TEXT("ArenaDuel.Health"), TEXT("Set the health of the local player (server or listen host only)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
			AArenaDuelCharacter* Pawn = Controller ? Cast<AArenaDuelCharacter>(Controller->GetPawn()) : nullptr;
			if (Pawn && Args.Num() > 0) Pawn->AdminSetHealth(FCString::Atof(*Args[0]));
		}));
}

AArenaDuelHealPad::AArenaDuelHealPad()
{
	PrimaryActorTick.bCanEverTick = false;
	// Placed in the level, so every machine has it. Only the server heals.
	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	RootComponent = Visual;
	// A thin plate: it must not block movement or bullets, the player just stands on the floor above it.
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetCastShadow(false);
	Visual->SetMobility(EComponentMobility::Static);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded()) Visual->SetStaticMesh(Cube.Object);
	Visual->SetRelativeScale3D(FVector(PadHalfSize * 0.02f, PadHalfSize * 0.02f, 0.03f));
}

void AArenaDuelHealPad::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority()) GetWorldTimerManager().SetTimer(HealTimer, this, &AArenaDuelHealPad::HealStandingPlayers, GetStepInterval(), true);
}

bool AArenaDuelHealPad::IsOnPad(const FVector& FeetLocation) const
{
	const FVector Local = FeetLocation - GetActorLocation();
	return FMath::Abs(Local.X) <= PadHalfSize && FMath::Abs(Local.Y) <= PadHalfSize && Local.Z > -30.0 && Local.Z < 60.0;
}

void AArenaDuelHealPad::HealStandingPlayers()
{
	for (TActorIterator<AArenaDuelCharacter> It(GetWorld()); It; ++It)
	{
		AArenaDuelCharacter* Character = *It;
		if (!Character || Character->IsDead() || !Character->GetCapsuleComponent()) continue;
		// Feet, not the capsule centre, so crouching on the pad counts and jumping over it does not.
		const FVector Feet = Character->GetActorLocation() - FVector(0.0, 0.0, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		if (IsOnPad(Feet)) Character->ApplyServerHeal(HealStep);
	}
}
