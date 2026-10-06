#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "ArenaDuelLocalSettings.generated.h"

USTRUCT()
struct FArenaDuelLocalSettings
{
	GENERATED_BODY()
	UPROPERTY() float MouseSensitivity = 1.0f;
	UPROPERTY() float ADSMultiplier = 1.0f;
	UPROPERTY() float FOV = 90.0f;
	UPROPERTY() float MasterVolume = 1.0f;
	UPROPERTY() int32 WindowMode = 1;
	UPROPERTY() int32 ResolutionX = 1920;
	UPROPERTY() int32 ResolutionY = 1080;
	UPROPERTY() bool bVSync = false;
	UPROPERTY() int32 FPSLimit = 144;
};

UCLASS()
class ARENADUEL_API UArenaDuelLocalSettingsSave : public USaveGame
{
	GENERATED_BODY()
public:
	UPROPERTY() FArenaDuelLocalSettings Settings;
	static FArenaDuelLocalSettings LoadSettings();
	static bool HasSavedSettings();
	static void SaveSettings(const FArenaDuelLocalSettings& Settings);
};
