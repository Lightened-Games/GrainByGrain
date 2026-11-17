// Pocket Sand - the autoplay bot (-autoplay). It plays through IPSPlayerInput, exactly like the keyboard (so the
// legend lights up), picks a hiding surface by free capacity, distance, guard sight and probe zones, trickles while
// inside crowds, and only pours when no watcher can see it. -PSSkill adds reaction delay and worse choices.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PSAutoplayComponent.generated.h"

UENUM()
enum class EPSBotGoal : uint8
{
	None,
	Load,
	Region,
	Crowd,
	Defeat
};

UCLASS(ClassGroup = (PocketSand), meta = (BlueprintSpawnableComponent))
class POCKETSAND_API UPSAutoplayComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPSAutoplayComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float DecisionInterval = 0.5f;
	/** Stand this far inside a hiding surface so the whole pour footprint lands on it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float RegionInset = 115.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float ArriveRadius = 30.0f;
	/** The arrive radius grows to this many frame steps at low frame rates (no circling the target). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float ArriveStepFactor = 1.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float SightPenaltySeconds = 6.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float CapacityValue = 10.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float StopPourSuspicion = 0.35f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float ResumePourSuspicion = 0.12f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float CapacityMargin = 0.06f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float StickBonus = 14.0f;
	/** Walking direction smoothing (per second) and the radius within which the bot steps around people. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float SteerSmoothing = 5.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float PeopleAvoidRadius = 170.0f;
	/** Parked on a spot but unable to pour (watched, no room, probe nearby) this long -> try somewhere else. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float MaxParkSeconds = 4.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float AvoidSeconds = 8.0f;
	/** QA (-PSDefeat=caught): pour only within this distance of the puddle it heads for. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float CaughtPourRange = 450.0f;
	/** Court sand the bot allows before the late game (a spike only throws what is above the loose line). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float CourtMidGameCap = 1.0f;
	/** With less than this much yard time left the bot uses the court to the brim. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float LateGameSeconds = 22.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float StuckSeconds = 0.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float StuckDistance = 30.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float UnstickSeconds = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float MaxReactionDelay = 0.6f;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	EPSBotGoal Goal = EPSBotGoal::None;
	FVector2D Target = FVector2D::ZeroVector;
	int32 TargetRegion = INDEX_NONE;
	FVector2D SmoothDir = FVector2D::ZeroVector;
	int32 TargetGroup = INDEX_NONE;
	int32 WantCarry = 2;
	float DecisionClock = 0.0f;
	float ParkedClock = 0.0f;
	TMap<int32, float> AvoidUntil;
	float LoadClock = 0.0f;
	float StuckClock = 0.0f;
	float UnstickLeft = 0.0f;
	float UnstickSign = 1.0f;
	FVector2D StuckFrom = FVector2D::ZeroVector;
	bool bHoldPour = false;
	bool bWasActive = false;
	float ReactionDelay = 0.0f;
	float PourIntentClock = 0.0f;
	bool bPourIntent = false;
	float TraceClock = 0.0f;
	FRandomStream Rng;

	class IPSMatchInfo* Match() const;
	void Decide(class IPSMatchInfo* M, class IPSSandCarrier* C, const FVector2D& Me);
	bool SeenAt(class IPSMatchInfo* M, const FVector2D& P, float Exposure) const;
	float SightThreat(class IPSMatchInfo* M, const FVector2D& P, float Exposure) const;
	FVector2D StandPoint(const struct FPSRegionDef& R, const FVector2D& From) const;
	/** The fill level the bot aims for on a region (shared by the picker and the pour check). */
	float RegionCap(class IPSMatchInfo* M, const struct FPSRegionDef& R) const;
	FVector2D Steer(class IPSMatchInfo* M, const FVector2D& Me, const FVector2D& To, float Dt);
};
