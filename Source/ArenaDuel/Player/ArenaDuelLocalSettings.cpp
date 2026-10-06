#include "ArenaDuelLocalSettings.h"
#include "Kismet/GameplayStatics.h"

namespace { constexpr const TCHAR* SlotName = TEXT("ArenaDuelLocalSettings"); }

FArenaDuelLocalSettings UArenaDuelLocalSettingsSave::LoadSettings()
{
	if (const auto* Save = Cast<UArenaDuelLocalSettingsSave>(UGameplayStatics::LoadGameFromSlot(SlotName, 0)))
	{
		FArenaDuelLocalSettings Value = Save->Settings;
		Value.MouseSensitivity = FMath::Clamp(Value.MouseSensitivity, 0.10f, 5.0f);
		Value.ADSMultiplier = FMath::Clamp(Value.ADSMultiplier, 0.25f, 1.5f);
		Value.FOV = FMath::Clamp(Value.FOV, 80.0f, 110.0f);
		Value.MasterVolume = FMath::Clamp(Value.MasterVolume, 0.0f, 1.0f);
		return Value;
	}
	return {};
}

bool UArenaDuelLocalSettingsSave::HasSavedSettings()
{
	return UGameplayStatics::DoesSaveGameExist(SlotName,0);
}

void UArenaDuelLocalSettingsSave::SaveSettings(const FArenaDuelLocalSettings& Settings)
{
	UArenaDuelLocalSettingsSave* Save = Cast<UArenaDuelLocalSettingsSave>(UGameplayStatics::CreateSaveGameObject(StaticClass()));
	if (!Save) return;
	Save->Settings = Settings;
	UGameplayStatics::SaveGameToSlot(Save, SlotName, 0);
}
