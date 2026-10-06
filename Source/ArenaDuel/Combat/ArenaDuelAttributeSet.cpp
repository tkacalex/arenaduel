#include "ArenaDuelAttributeSet.h"
#include "Net/UnrealNetwork.h"

UArenaDuelAttributeSet::UArenaDuelAttributeSet()
{
	InitHealth(100.0f);
	InitMaxHealth(100.0f);
}

void UArenaDuelAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UArenaDuelAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArenaDuelAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
}

void UArenaDuelAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
	MaxHealth.SetBaseValue(FMath::Max(1.0f, MaxHealth.GetBaseValue()));
	Health.SetBaseValue(FMath::Clamp(Health.GetBaseValue(), 0.0f, MaxHealth.GetBaseValue()));
}

void UArenaDuelAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth) { GAMEPLAYATTRIBUTE_REPNOTIFY(UArenaDuelAttributeSet, Health, OldHealth); }
void UArenaDuelAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth) { GAMEPLAYATTRIBUTE_REPNOTIFY(UArenaDuelAttributeSet, MaxHealth, OldMaxHealth); }
