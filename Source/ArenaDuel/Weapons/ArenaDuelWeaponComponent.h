// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "ArenaDuelWeaponComponent.generated.h"

class UInputAction;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EArenaDuelWeaponId : uint8
{
	ArcRifle,
	ShadeSMG,
	RuneDMR,
	HexShotgun
};

UENUM(BlueprintType)
enum class EArenaDuelShotResult : uint8
{
	Miss,
	Body,
	Head
};

USTRUCT(BlueprintType)
struct ARENADUEL_API FArenaDuelWeaponDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	EArenaDuelWeaponId Id = EArenaDuelWeaponId::ArcRifle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FName DisplayName = TEXT("Arc Rifle");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	int32 MagazineCapacity = 30;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	int32 ReserveCapacity = 120;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float RoundsPerMinute = 650.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float Range = 10000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float BaseSpreadDegrees = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float MovementSpreadDegrees = 1.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float RecoilVertical = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float RecoilHorizontal = 0.08f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float ReloadDuration = 1.8f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	int32 Pellets = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bAutomatic = true;
};

UCLASS(ClassGroup=(Weapons), meta=(BlueprintSpawnableComponent))
class ARENADUEL_API UArenaDuelWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UArenaDuelWeaponComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void StartFire();
	void StopFire();
	void Reload();
	void EquipWeapon(int32 Index);

	const FArenaDuelWeaponDefinition& GetCurrentDefinition() const;
	int32 GetWeaponDefinitionCount() const { return WeaponDefinitions.Num(); }
	const FArenaDuelWeaponDefinition* GetWeaponDefinition(int32 Index) const { return WeaponDefinitions.IsValidIndex(Index) ? &WeaponDefinitions[Index] : nullptr; }
	EArenaDuelWeaponId GetCurrentWeaponId() const;
	FName GetCurrentWeaponName() const;
	int32 GetCurrentMagazineAmmo() const { return CurrentMagazineAmmo; }
	int32 GetReserveAmmo() const { return ReserveAmmo; }
	bool IsReloading() const { return bReloading; }
	bool IsFireHeld() const { return bFireHeld; }
	EArenaDuelShotResult GetLastShotResult() const { return LastShotResult; }
	float GetLastShotDistance() const { return LastShotDistance; }

protected:
	UFUNCTION(Server, Unreliable)
	void ServerRequestFire();

	UFUNCTION(Server, Reliable)
	void ServerRequestReload();

	UFUNCTION(Server, Reliable)
	void ServerRequestEquip(int32 Index);

	void FireAuthoritative();
	void CompleteReload();
	void RefreshWeaponVisual();
	void SetLastShot(EArenaDuelShotResult Result, float Distance, AActor* Target);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapons")
	TArray<FArenaDuelWeaponDefinition> WeaponDefinitions;

	UPROPERTY(ReplicatedUsing=OnRep_EquippedWeapon, VisibleInstanceOnly, BlueprintReadOnly, Category="Weapons")
	uint8 EquippedWeaponIndex = 0;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Weapons")
	int32 CurrentMagazineAmmo = 30;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Weapons")
	int32 ReserveAmmo = 120;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Weapons")
	bool bReloading = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapons")
	TObjectPtr<UStaticMeshComponent> FirstPersonWeaponMesh;

	FTimerHandle ReloadTimerHandle;

	EArenaDuelShotResult LastShotResult = EArenaDuelShotResult::Miss;
	float LastShotDistance = 0.0f;
	TWeakObjectPtr<AActor> LastShotTarget;
	bool bFireHeld = false;
	float FireCooldownRemaining = 0.0f;

	UFUNCTION()
	void OnRep_EquippedWeapon();
};
