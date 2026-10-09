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
protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
private:
	UPROPERTY() TObjectPtr<UAnimSequence> Idle;
	UPROPERTY() TObjectPtr<UAnimSequence> Walk;
	UPROPERTY() TObjectPtr<UAnimSequence> Run;
	UPROPERTY() TObjectPtr<UAnimSequence> Fall;
	UPROPERTY() TObjectPtr<UAnimSequence> Reload;
};
