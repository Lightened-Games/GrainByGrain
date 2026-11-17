// Pocket Sand - the yard crowd: groups of inmates drift between points of interest. Their feet trample shown sand into
// the gravel (moving cover), they part for the hero and the guards, and they steer around beds, court and benches.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PSCrowdDirectorComponent.generated.h"

class APSSandGrid;
class UPSRoundConfig;

USTRUCT(BlueprintType)
struct FPSCrowdGroup
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Crowd") FVector2D Pos = FVector2D::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Crowd") FVector2D Target = FVector2D::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Crowd") FVector2D Vel = FVector2D::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Crowd") int32 Poi = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Crowd") float DwellLeft = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Crowd") bool bMoving = false;
	UPROPERTY(BlueprintReadOnly, Category = "Crowd") int32 Size = 0;
	/** Eased group speed (cm/s): groups ease into a walk and slow down before they arrive. */
	float CurSpeed = 0.0f;
};

USTRUCT()
struct FPSCrowdAgentState
{
	GENERATED_BODY()

	UPROPERTY() TWeakObjectPtr<AActor> Actor;
	FVector2D Pos = FVector2D::ZeroVector;
	FVector2D Vel = FVector2D::ZeroVector;
	FVector2D Slot = FVector2D::ZeroVector;
	/** Low-passed steering intent: heading follows this, so slot-keeping and separation don't make the walk wobble. */
	FVector2D Desired = FVector2D::ZeroVector;
	int32 Group = 0;
	float Yaw = 0.0f;
	/** Per-person variety: walking speed multiplier, how quickly they follow the group off, where they look while chatting. */
	float SpeedMul = 1.0f;
	float Follow = 0.0f;
	float FollowRate = 1.0f;
	float ChatYawOffset = 0.0f;
	float GestureClock = 0.0f;
	/** Reacting to the hero walking past: seconds left looking at him, and the cooldown before the next look. */
	float NoticeLeft = 0.0f;
	float NoticeCool = 0.0f;
};

UCLASS(ClassGroup = (PocketSand), meta = (BlueprintSpawnableComponent))
class POCKETSAND_API UPSCrowdDirectorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPSCrowdDirectorComponent();

	UPROPERTY(EditAnywhere, Category = "PocketSand") TSubclassOf<AActor> AgentClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float SlotSteer = 3.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float SeparationStrength = 2.2f;
	/** Agents step aside for the hero and guards within this radius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float GiveWayRadius = 62.0f;
	/** Steering below this speed (cm/s) means standing: no step, no foot slide. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float WalkThreshold = 25.0f;
	/** Hard spacing: extra gap (cm) kept between bodies, and the capsule radius of the hero and guards. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float HardGap = 6.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float OtherBodyRadius = 40.0f;
	/** How fast an inmate turns toward where it wants to walk (deg/s); it only ever steps along its facing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float AgentTurnRate = 200.0f;
	/** Rate (1/s) at which an inmate's steering intent follows the raw slot/separation push. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float IntentSmoothing = 3.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float RegionMargin = 45.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float MaxSpeedMul = 1.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float TalkFraction = 0.55f;
	/** Natural motion (user 2026-11-09: "movement is very robotic"). Provisional (C8). */
	/** Groups start spread out and wander to random yard spots (not only the points of interest). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Natural") bool bRandomSpots = true;
	/** Start screen: inmates keep this far from the hero (cm; 0 = off) so nobody walks through the hero shot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Natural") float MenuHeroClearRadius = 600.0f;
	bool bMenuClear = false;
	FVector2D HeroPos = FVector2D::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Natural") float SpotMargin = 350.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Natural") float SpotSeparation = 900.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Natural") float GroupAccelSeconds = 1.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Natural") float GroupSlowRadius = 220.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Natural") FVector2D AgentSpeedRange = FVector2D(0.88f, 1.1f);
	/** How fast each person picks up the group's walk (per second; random in range): a staggered start. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Natural") FVector2D FollowRateRange = FVector2D(0.8f, 2.2f);
	/** Turns ease: rate = angle left x TurnEase, clamped between MinTurnRate and AgentTurnRate (deg/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Natural") float TurnEase = 3.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Natural") float MinTurnRate = 40.0f;
	/** Standing people turn toward the group slowly, and only past this angle; each looks a little off-centre. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Natural") float ChatTurnRate = 70.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Natural") float ChatTurnThreshold = 25.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Natural") float ChatYawSpread = 22.0f;
	/** Standing inmates now and then do a small gesture (Rocketbox mocap): interval range (s) and the clips. */
	/** The yard reacts (user 2026-11-13: "make the environment better and interactive"): a standing inmate the hero passes
	 *  within NoticeRadius turns to watch him for NoticeSeconds and makes a small gesture, then goes back to chatting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Natural") float NoticeRadius = 330.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Natural") float NoticeSeconds = 2.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Natural") float NoticeCooldown = 14.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Natural") float NoticeTurnRate = 90.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Natural") TArray<FString> NoticeClips = { TEXT("RBM_LookAround_03"), TEXT("RBM_ScratchHead_02"), TEXT("RBM_Shrug_02") };
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Natural") FVector2D GestureInterval = FVector2D(4.0f, 10.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Natural") TArray<FString> GestureClips = { TEXT("RBM_LookAround"), TEXT("RBM_LookAround_02"), TEXT("RBM_LookAround_03"), TEXT("RBM_ScratchHead"), TEXT("RBM_ScratchHead_02"), TEXT("RBM_Shrug"), TEXT("RBM_Shrug_02"), TEXT("RBM_Stretch"), TEXT("RBM_Clap") };

	void Init(const UPSRoundConfig* Config, APSSandGrid* InGrid);
	void ResetCrowd(int32 Seed);
	/** Others = hero and guards (agents give way). */
	void Step(float Dt, float SpeedMul, const TArray<FVector2D>& Others);
	void SetCheer(bool bOn);

	int32 CountNear(const FVector& Location, float Radius) const;
	const TArray<FPSCrowdGroup>& Groups() const { return GroupStates; }
	const TArray<FPSCrowdAgentState>& Agents() const { return AgentStates; }
	/** Group centre this many seconds ahead along its current leg (bot planning). */
	FVector2D PredictGroup(int32 Group, float Seconds) const;

private:
	UPROPERTY() TObjectPtr<const UPSRoundConfig> Cfg;
	UPROPERTY() TObjectPtr<APSSandGrid> Grid;
	UPROPERTY() TArray<FPSCrowdGroup> GroupStates;
	UPROPERTY() TArray<FPSCrowdAgentState> AgentStates;
	FRandomStream Rng;

	void PickNextPoi(FPSCrowdGroup& G, int32 Index);
	FVector2D PushOut(const FVector2D& P, float Radius) const;
	FVector2D Avoid(const FVector2D& From, const FVector2D& Dir, float Radius) const;
};
