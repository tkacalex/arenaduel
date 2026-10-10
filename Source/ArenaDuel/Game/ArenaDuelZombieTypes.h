#pragma once

#include "CoreMinimal.h"
#include "ArenaDuelZombieTypes.generated.h"

UENUM(BlueprintType)
enum class EArenaDuelZombieType : uint8
{
	Normal,
	Fast,
	Armored,
	MiniBoss,
	Boss
};

/** Everything that makes one zombie type different from another. Balanced for survival only; duel values are untouched. */
USTRUCT(BlueprintType)
struct FArenaDuelZombieTypeConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FString DisplayName = TEXT("ZOMBIE");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "1")) float Health = 100.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0")) float MoveSpeed = 330.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0")) float Damage = 12.0f;
	/** Seconds between two melee hits. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.1")) float AttackInterval = 1.1f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "10")) float AttackRange = 150.0f;
	/** Seconds between the start of a swing and the hit. Moving out of reach in that time avoids it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0")) float AttackWindup = 0.35f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.2")) float Scale = 1.0f;
	/** Extra factor on a weapon's own head shot multiplier. Above 1 makes the head a weak spot. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0")) float HeadDamageFactor = 1.0f;
	/** Factor on torso and limb hits. Below 1 is armour. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0")) float BodyDamageFactor = 1.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0")) int32 Points = 100;
	/** Asset path of the material that marks the type. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FString MaterialPath;
	/** Asset path of the type's own body. It brings its own materials; without it the mannequin is used and tinted with MaterialPath. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FString MeshPath;
};

/** How many of each type one wave brings in total, not how many are alive at once. */
USTRUCT(BlueprintType)
struct FArenaDuelWaveDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0")) int32 Normal = 0;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0")) int32 Fast = 0;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0")) int32 Armored = 0;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0")) int32 MiniBoss = 0;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0")) int32 Boss = 0;

	int32 Total() const { return Normal + Fast + Armored + MiniBoss + Boss; }
	int32 Count(EArenaDuelZombieType Type) const
	{
		switch (Type)
		{
		case EArenaDuelZombieType::Fast: return Fast;
		case EArenaDuelZombieType::Armored: return Armored;
		case EArenaDuelZombieType::MiniBoss: return MiniBoss;
		case EArenaDuelZombieType::Boss: return Boss;
		default: return Normal;
		}
	}
};

UENUM(BlueprintType)
enum class EArenaDuelSurvivalPurchase : uint8
{
	Ammo,
	Heal,
	Damage
};
