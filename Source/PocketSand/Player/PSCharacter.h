// Pocket Sand - the hero: an inmate with two bags of tunnel sand down his trousers.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Interfaces/PSInterfaces.h"
#include "PSCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UPSHumanBodyComponent;
class UPSPourComponent;
class UPSSuspicionComponent;

UCLASS()
class POCKETSAND_API APSCharacter : public ACharacter, public IPSSandCarrier
{
	GENERATED_BODY()

public:
	APSCharacter();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PocketSand") TObjectPtr<USpringArmComponent> Boom;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PocketSand") TObjectPtr<UCameraComponent> Camera;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PocketSand") TObjectPtr<UPSHumanBodyComponent> Body;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PocketSand") TObjectPtr<UPSPourComponent> Pour;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PocketSand") TObjectPtr<UPSSuspicionComponent> Suspicion;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PocketSand") TObjectPtr<UStaticMeshComponent> Ring;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float RingRadius = 62.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float StepDistance = 70.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float TrickleFxInterval = 0.16f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float TrickleFxScale = 0.07f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float RotationRate = 450.0f;
	/** Hand animations: brushing the trousers while pouring, dusting off on victory, a shrug on defeat (Rocketbox mocap). */
	/** Pouring keeps the hands natural (the walk/idle arm swing); set a clip name to add a gesture. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Animation") FString PourClip;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Animation") FString LoadClip = TEXT("RBM_TakeBag");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Animation") float LoadClipRate = 2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Animation") float PourClipRate = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Animation") FString VictoryClip = TEXT("RBM_Dust_02");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Animation") FString DefeatClip = TEXT("RBM_Shrug");
	UFUNCTION() void HandleMatchState(EPSMatchState State, EPSDefeat Reason);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float BagScale = 0.17f;
	/** Hero outfit: a lighter shirt than the yard so he reads in a crowd. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FPSOutfit Outfit;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void FellOutOfWorld(const UDamageType& DmgType) override;

	// IPSSandCarrier
	virtual int32 PS_Carried() const override;
	virtual float PS_CurrentBag() const override;
	virtual bool PS_IsPouring() const override;
	virtual bool PS_IsWaddling() const override;
	virtual bool PS_IsLoading() const override { return LoadLeft > 0.0f; }
	virtual float PS_Suspicion() const override;
	virtual void PS_AddBag() override;
	virtual void PS_SetWantPour(bool bWant) override;
	virtual void PS_SetRun(bool bInRun) override;
	bool bRun = false;
	float Bend = 0.0f;
	/** Sand work is done crouched with the hands low (user 2026-11-12: "sit when doing sand work hands shouldn't be air"). */
	bool bCrouching = false;
	float StillTime = 0.0f;
	float CrouchLinger = 0.0f;
	/** Facing held while crouched (the door while loading): he never swivels on his heels while squatting. */
	float CrouchYaw = 0.0f;
	FVector LoadFacing = FVector::ZeroVector;
	bool bHasLoadFacing = false;
	/** Turn toward the bag store while loading (deg/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Animation") float LoadTurnRate = 540.0f;
	/** Crouched hands-low work clip (Rocketbox m_crouch_gestic) for loading bags and pouring on the spot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Animation") FString CrouchClip = TEXT("RBM_CrouchWork");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Animation") float CrouchBlendSpeed = 3.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Animation") float ActionBlendSpeedDefault = 6.0f;
	/** Pouring this slow (cm/s) for StillToCrouch seconds crouches; moving faster than StandSpeed stands straight up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Animation") float StillSpeed = 25.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Animation") float StillToCrouch = 0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Animation") float StandSpeed = 15.0f;
	/** Stay down this long between two bags so the hero doesn't bob up and down. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Animation") float CrouchLingerSeconds = 0.4f;
	virtual bool PS_SandWouldShow() const override { return bShowsHere; }
	virtual void PS_ResetCarrier(const FVector& Location, float Yaw) override;
	virtual void PS_LockMovement(bool bLocked) override;

	void SetCameraYawOffset(float Offset);
	/** Where the trickle lands (trouser hems, just behind the feet). */
	FVector FootLocation() const;
	/** Would sand poured where you stand show right now (ring turns red)? */
	bool SandWouldShow() const { return bShowsHere; }
	bool InCrowdCover() const { return bCrowdCover; }

private:
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> RingMID;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Bags;
	UPROPERTY() TObjectPtr<const class UPSRoundConfig> Cfg;
	bool bLocked = true;
	bool bShowsHere = false;
	bool bCrowdCover = false;
	float LoadLeft = 0.0f;
	float StepAccum = 0.0f;
	float FxClock = 0.0f;
	float YawOffset = 0.0f;
	float BagPop = 0.0f;
	FVector LastLoc = FVector::ZeroVector;
	FVector StartLoc = FVector::ZeroVector;
	float StartYaw = 0.0f;

	class IPSMatchInfo* Match() const;
	void Setup();
	void UpdateSpeed();
	void UpdateRing(float Dt);
	void UpdateBags(float Dt);
	float ComputeSuspicionGain(float Dt);
	void HandleBagEmptied(const FVector& Where);
	void HandlePourChanged(bool bOn, const FVector& Where);
	void HandleCaught();
};
