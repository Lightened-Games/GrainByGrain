// Pocket Sand - trouser bags: what you carry and the trickle out of your hems (attach to any pawn).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PSPourComponent.generated.h"

class APSSandGrid;
class UPSRoundConfig;

UCLASS(ClassGroup = (PocketSand), meta = (BlueprintSpawnableComponent))
class POCKETSAND_API UPSPourComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPSPourComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float PourRate = 0.32f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float SpreadRadius = 45.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float PourBehind = 22.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") int32 MaxCarried = 2;

	void InitFromConfig(const UPSRoundConfig* Config);
	void ResetBags();
	void SetWantPour(bool bWant) { bWantPour = bWant; }
	bool WantsPour() const { return bWantPour; }
	bool AddBag();
	/** Pours for Dt seconds at FootLocation into Grid. Returns bags poured this step. */
	float Step(float Dt, APSSandGrid* Grid, const FVector& FootLocation, bool bAllowed);

	int32 Carried() const { return Carried_; }
	float CurrentBag() const { return Carried_ > 0 ? Current : 0.0f; }
	bool IsPouring() const { return bPouring; }
	bool IsFull() const { return Carried_ >= MaxCarried; }
	float LastShownFraction() const { return LastShown; }

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnBagEmptied, const FVector&);
	FOnBagEmptied OnBagEmptied;
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPourChanged, bool, const FVector&);
	FOnPourChanged OnPourChanged;

private:
	int32 Carried_ = 0;
	float Current = 0.0f;
	bool bWantPour = false;
	bool bPouring = false;
	float LastShown = 0.0f;
};
