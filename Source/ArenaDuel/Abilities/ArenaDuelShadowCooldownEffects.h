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
