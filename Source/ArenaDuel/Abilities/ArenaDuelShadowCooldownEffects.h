// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ArenaDuelShadowCooldownEffects.generated.h"

UCLASS()
class ARENADUEL_API UArenaDuelGE_ShadowStepCooldown : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UArenaDuelGE_ShadowStepCooldown(const FObjectInitializer& ObjectInitializer);
};

UCLASS()
class ARENADUEL_API UArenaDuelGE_VeilWallCooldown : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UArenaDuelGE_VeilWallCooldown(const FObjectInitializer& ObjectInitializer);
};

UCLASS()
class ARENADUEL_API UArenaDuelGE_ArcBarrierCooldown : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UArenaDuelGE_ArcBarrierCooldown(const FObjectInitializer& ObjectInitializer);
};

UCLASS()
class ARENADUEL_API UArenaDuelGE_BurstLeapCooldown : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UArenaDuelGE_BurstLeapCooldown(const FObjectInitializer& ObjectInitializer);
};

UCLASS()
class ARENADUEL_API UArenaDuelGE_RiftGrappleCooldown : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UArenaDuelGE_RiftGrappleCooldown(const FObjectInitializer& ObjectInitializer);
};

UCLASS()
class ARENADUEL_API UArenaDuelGE_PhaseGateCooldown : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UArenaDuelGE_PhaseGateCooldown(const FObjectInitializer& ObjectInitializer);
};
