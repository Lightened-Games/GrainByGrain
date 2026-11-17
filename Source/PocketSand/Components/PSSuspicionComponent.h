// Pocket Sand - how close the guards are to catching you in the act (0..1). Fills while a watcher sees you pour
// (much slower inside a crowd) or sees you waddle; drains otherwise. Full = caught.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PSSuspicionComponent.generated.h"

class UPSRoundConfig;

UCLASS(ClassGroup = (PocketSand), meta = (BlueprintSpawnableComponent))
class POCKETSAND_API UPSSuspicionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPSSuspicionComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float Decay = 0.22f;
	/** Broadcast changes once they move this much. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float BroadcastStep = 0.01f;

	void InitFromConfig(const UPSRoundConfig* Config);
	void ResetValue();
	/** Gain is the strongest per-second rise from any watcher this step (0 = nobody sees anything). */
	void Step(float Dt, float Gain);

	float Value() const { return Current; }
	float Peak() const { return PeakValue; }
	bool IsRising() const { return bRising; }

	DECLARE_MULTICAST_DELEGATE(FOnFull);
	FOnFull OnFull;

private:
	float Current = 0.0f;
	float PeakValue = 0.0f;
	float LastBroadcast = 0.0f;
	bool bRising = false;
	bool bFullSent = false;
};
