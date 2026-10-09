#include "ArenaDuelRiftTargeting.h"
#include "EngineUtils.h"
#include "ArenaDuelArcBarrier.h"
#include "../Characters/ArenaDuelCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"

namespace
{
	FCollisionShape RiftCapsule(const AArenaDuelCharacter& Character)
	{
		return FCollisionShape::MakeCapsule(Character.GetCapsuleComponent()->GetScaledCapsuleRadius(), Character.GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	}
	bool Fits(const AArenaDuelCharacter& Character, const FVector& Point)
	{
		const FCollisionQueryParams Params(SCENE_QUERY_STAT(RiftClearance), false, &Character);
		return !Character.GetWorld()->OverlapBlockingTestByProfile(Point, FQuat::Identity, Character.GetCapsuleComponent()->GetCollisionProfileName(), RiftCapsule(Character), Params);
	}
}

bool ArenaDuelRiftTargeting::IsTravelClear(const AArenaDuelCharacter& Character, const FVector& Destination, float MaxRange)
{
	if (!Character.GetWorld() || Destination.ContainsNaN()) return false;
	const float Distance = FVector::Dist(Character.GetActorLocation(), Destination);
	if (Distance < MinimumTravel || Distance > MaxRange || !Fits(Character, Destination)) return false;
	FHitResult Hit;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(RiftPath), false, &Character);
	return !Character.GetWorld()->SweepSingleByProfile(Hit, Character.GetActorLocation(), Destination, FQuat::Identity,
		Character.GetCapsuleComponent()->GetCollisionProfileName(), RiftCapsule(Character), Params);
}

bool ArenaDuelRiftTargeting::FindGrappleDestination(const AArenaDuelCharacter& Character, FVector& Destination, FVector& Anchor)
{
	if (!Character.GetWorld() || !Character.GetController()) return false;
	const FVector Eye = Character.GetPawnViewLocation();
	FHitResult Hit;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(RiftAnchor), false, &Character);
	const FVector Reach = Eye + Character.GetControlRotation().Vector() * GrappleRange;
	const bool bWorldHit = Character.GetWorld()->LineTraceSingleByChannel(Hit, Eye, Reach, ECC_Visibility, Params);
	// A player standing in the line still blocks the grapple, now tested on their per-bone hit zones.
	for (TActorIterator<AArenaDuelCharacter> It(Character.GetWorld()); It; ++It)
	{
		FHitResult BodyHit;
		if (*It != &Character && It->TraceHitZones(Eye, bWorldHit ? FVector(Hit.ImpactPoint) : Reach, BodyHit)) return false;
	}
	if (!bWorldHit) return false;
	const UPrimitiveComponent* Component = Hit.GetComponent();
	if (!Component || !Hit.GetActor() || Hit.GetActor()->IsA<ACharacter>() || Hit.GetActor()->IsA<AArenaDuelArcBarrier>()
		|| Component->Mobility != EComponentMobility::Static || Hit.bStartPenetrating) return false;
	Anchor = Hit.ImpactPoint;
	const FVector Normal = Hit.ImpactNormal.GetSafeNormal();
	const float Offset = FMath::Abs(Normal.Z) * Character.GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
		+ Normal.Size2D() * Character.GetCapsuleComponent()->GetScaledCapsuleRadius() + 12.0f;
	Destination = Anchor + Normal * Offset;
	return IsTravelClear(Character, Destination, GrappleRange);
}

bool ArenaDuelRiftTargeting::FindGateDestination(const AArenaDuelCharacter& Character, FVector& Destination)
{
	if (!Character.GetWorld() || !Character.GetController()) return false;
	FRotator Facing = Character.GetControlRotation();
	Facing.Pitch = FMath::Clamp(FRotator::NormalizeAxis(Facing.Pitch), -15.0f, 15.0f);
	Facing.Roll = 0;
	const FVector Direction = Facing.Vector();
	const FVector Start = Character.GetActorLocation();
	Destination = Start + Direction * (GateRange - 8.0f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RiftGate), false, &Character);
	FHitResult PathHit;
	if (Character.GetWorld()->SweepSingleByProfile(PathHit, Start, Destination, FQuat::Identity,
		Character.GetCapsuleComponent()->GetCollisionProfileName(), RiftCapsule(Character), Params))
	{
		if (PathHit.bStartPenetrating) return false;
		Destination = PathHit.Location - Direction * 8.0f;
	}
	// Require a supported destination, not an arbitrary point in empty air beyond the map.
	FHitResult Floor;
	if (!Character.GetWorld()->LineTraceSingleByChannel(Floor, Destination + FVector(0, 0, 100), Destination - FVector(0, 0, 600), ECC_Visibility, Params)
		|| Floor.ImpactNormal.Z < 0.7f || !Floor.GetComponent() || Floor.GetComponent()->Mobility != EComponentMobility::Static) return false;
	Destination.Z = Floor.ImpactPoint.Z + Character.GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 4.0f;
	if (FMath::Abs(Destination.Z - Start.Z) > 350.0f) return false;
	return IsGateDestinationSafe(Character, Destination);
}

bool ArenaDuelRiftTargeting::IsGateDestinationSafe(const AArenaDuelCharacter& Character, const FVector& Destination)
{
	if (!IsTravelClear(Character, Destination, GateRange)) return false;
	const FVector Bottom = Destination - FVector(0, 0, Character.GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	FHitResult Support;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(RiftGateSupport), false, &Character);
	return Character.GetWorld()->LineTraceSingleByChannel(Support, Bottom + FVector(0, 0, 8), Bottom - FVector(0, 0, 20), ECC_Visibility, Params)
		&& Support.ImpactNormal.Z >= 0.7f && Support.GetComponent() && Support.GetComponent()->Mobility == EComponentMobility::Static;
}
