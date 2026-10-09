#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "ArenaDuelVisualAnimInstance.generated.h"

class UAnimSequence;

// Small native locomotion player. Cosmetic only; never supplies root motion or hit collision.
UCLASS(Transient)
class ARENADUEL_API UArenaDuelVisualAnimInstance : public UAnimSingleNodeInstance
{
	GENERATED_BODY()
public:
	UArenaDuelVisualAnimInstance();
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	const UAnimSequence* GetReloadClip() const { return Reload; }
protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
private:
	UPROPERTY() TObjectPtr<UAnimSequence> Idle;
	UPROPERTY() TObjectPtr<UAnimSequence> Walk;
	UPROPERTY() TObjectPtr<UAnimSequence> Run;
	UPROPERTY() TObjectPtr<UAnimSequence> Fall;
	UPROPERTY() TObjectPtr<UAnimSequence> Reload;
	// Directional locomotion: forward, backward, left, right.
	UPROPERTY() TObjectPtr<UAnimSequence> WalkClips[4];
	UPROPERTY() TObjectPtr<UAnimSequence> RunClips[4];
	int32 MoveDirection = 0;
};
