// Pocket Sand - four inmates keep a rally going; on cue one of them spikes it into the court sand.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PSVolleyballCourt.generated.h"

class UStaticMeshComponent;
class UPSRoundConfig;

UCLASS()
class POCKETSAND_API APSVolleyballCourt : public AActor
{
	GENERATED_BODY()

public:
	APSVolleyballCourt();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PocketSand") TObjectPtr<UStaticMeshComponent> Ball;
	UPROPERTY(EditAnywhere, Category = "PocketSand") TSubclassOf<AActor> PlayerClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float BallScale = 0.22f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float PassApex = 420.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float SpikeApex = 520.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float HandHeight = 200.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FVector2D PlayerOffset = FVector2D(330.0f, 170.0f);

	void Setup(const UPSRoundConfig* Config);
	void ResetCourt();
	/** Starts the spike sequence (telegraph, jump, smash). The impact fires OnSpikeImpact. */
	void TriggerSpike();
	bool IsSpiking() const { return SpikeStage > 0; }
	void Step(float Dt);
	FVector2D Centre() const;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnSpikeImpact, const FVector&);
	FOnSpikeImpact OnSpikeImpact;

private:
	UPROPERTY() TObjectPtr<const UPSRoundConfig> Cfg;
	UPROPERTY() TArray<TObjectPtr<AActor>> Players;
	TArray<FVector2D> Spots;
	int32 From = 0;
	int32 To = 2;
	float Clock = 0.0f;
	float Leg = 1.6f;
	int32 SpikeStage = 0;
	float SpikeClock = 0.0f;
	FVector SpikeFrom = FVector::ZeroVector;
	FVector SpikeTo = FVector::ZeroVector;
	FRandomStream Rng;

	FVector HandPos(int32 Index) const;
	void Hit(int32 Index, bool bSpike);
};
