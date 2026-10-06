// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "../UI/ArenaDuelAdminTypes.h"
#include "ArenaDuelPlayerController.generated.h"

class UArenaDuelAdminWidget;

UCLASS()
class ARENADUEL_API AArenaDuelPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void SetupInputComponent() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	bool IsAdminMenuOpen() const { return bAdminMenuOpen; }
	bool CanUseDevelopmentAdmin() const;
	void SubmitAdminCommand(EArenaDuelAdminCommand Command, uint8 TargetDuelSlot, float NumericValue);
	void CloseAdminMenu();

	UFUNCTION(Server, Reliable)
	void ServerExecuteAdminCommand(EArenaDuelAdminCommand Command, uint8 TargetDuelSlot, float NumericValue);

protected:
	void ToggleAdminMenu();
	void HandleAdminWidgetAction(EArenaDuelAdminCommand Command, float NumericValue);
	void ExecuteAdminCommandAuthoritatively(EArenaDuelAdminCommand Command, uint8 TargetDuelSlot, float NumericValue);

	UPROPERTY(Transient)
	TObjectPtr<UArenaDuelAdminWidget> AdminWidget;

	bool bAdminMenuOpen = false;
	bool bPreviousMoveInputIgnored = false;
	bool bPreviousLookInputIgnored = false;
};
