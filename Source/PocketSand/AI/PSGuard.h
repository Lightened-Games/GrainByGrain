// Pocket Sand - a yard guard: walks his route glancing side to side, notices yellow, walks over, kneels and rubs it
// between his fingers. The stick guard also pokes the ground wherever sand was found before.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/PSInterfaces.h"
#include "PSGuard.generated.h"

class UPSHumanBodyComponent;
class UPSVisionComponent;
class UPSRoundConfig;

UENUM(BlueprintType)
enum class EPSGuardState : uint8
{
	Patrol,
	Post,
	Watch,
	Investigate,
	Kneel,
	Probe,
	Poke,
	Frozen
};

UCLASS()
class POCKETSAND_API APSGuard : public AActor, public IPSWatcher
{
	GENERATED_BODY()

public:
	APSGuard();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PocketSand") TObjectPtr<UPSHumanBodyComponent> Body;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PocketSand") TObjectPtr<class UCapsuleComponent> Blocker;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PocketSand") TObjectPtr<UPSVisionComponent> Vision;

	/** The stick guard probes old finds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") bool bProbes = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float TurnSpeed = 220.0f;
	/** Turn easing: rate = angle left x TurnEase, at least MinTurnRate (deg/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float TurnEase = 3.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float MinTurnRate = 45.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float ArriveRadius = 70.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float AgentRadius = 58.0f; // body + the baton reach: no clipping chairs or props (2026-11-14; was 40)
	/** Closest a guard walks to the hero (centre to centre, cm): bodies and baton never overlap him. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float HeroClearance = 95.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float ScanInterval = 0.2f;
	/** Prop placement on the UAL rig (head / hand_r bone space). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FTransform CapOffset = FTransform(FRotator::ZeroRotator, FVector(0.0f, 2.0f, 15.0f), FVector(0.215f, 0.235f, 0.075f));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FTransform BrimOffset = FTransform(FRotator::ZeroRotator, FVector(0.0f, 10.0f, 12.0f), FVector(0.17f, 0.14f, 0.012f));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FTransform StickOffset = FTransform(FRotator(0.0f, 0.0f, 90.0f), FVector::ZeroVector, FVector(0.035f, 0.035f, 0.95f));
	/** The probing guard's baton: Poly Haven baseball_bat (CC0), held at the handle. Falls back to the cylinder stick if missing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FSoftObjectPath BatonMesh = FSoftObjectPath(TEXT("/Game/PocketSand/Props/baseball_bat/SM_ps_baseball_bat_1k.SM_ps_baseball_bat_1k"));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FTransform BatonOffset = FTransform(FRotator(0.0f, 0.0f, 90.0f), FVector(0.0f, 0.0f, 0.0f), FVector(0.72f));

	/** Route + optional post (stand at the post until told to patrol). */
	void Setup(const UPSRoundConfig* Config, const TArray<FVector2D>& Route, bool bStartAtPost, const FVector2D& Post, float PostYaw, bool bInProbes);
	void ResetGuard();
	float EaseTurn(float From, float To, float MaxStep) const;
	void SetPatrolling(bool bOn);
	/** Fixed-step brain (called by the GameMode). */
	void Step(float Dt, float SpeedMul, float Now);
	EPSGuardState GetState() const { return State; }
	FVector2D Pos2D() const { return Pos; }
	/** Where he will be in Seconds if he keeps walking his route (the bot reads this). */
	FVector2D PredictPos(float Seconds) const;

	// IPSWatcher
	virtual FVector PS_EyeLocation() const override;
	virtual FVector PS_ViewDir() const override;
	virtual float PS_ViewRange() const override;
	virtual float PS_ViewHalfAngle() const override;
	virtual bool PS_CanSee(const FVector& Point, float ExposureMul) const override;
	virtual float PS_PourSeenGain() const override;
	virtual bool PS_IsTower() const override { return false; }
	virtual void PS_Watch(AActor* Target) override;
	virtual bool PS_IsBusy() const override;
	virtual void PS_Investigate(const FVector& Location) override;
	virtual void PS_ResetWatcher() override { ResetGuard(); }
	virtual void PS_Freeze(bool bFrozen) override;

private:
	UPROPERTY() TObjectPtr<const UPSRoundConfig> Cfg;
	TArray<FVector2D> RoutePts;
	FVector2D PostPos = FVector2D::ZeroVector;
	float PostYawDeg = 0.0f;
	bool bStartPost = false;
	bool bPatrolling = true;
	EPSGuardState State = EPSGuardState::Patrol;
	EPSGuardState ResumeState = EPSGuardState::Patrol;
	FVector2D Pos = FVector2D::ZeroVector;
	float BodyYaw = 0.0f;
	float LookClock = 0.0f;
	/** Posted guards fidget now and then (clipboard check, look around) so they are not statues (reviewer 2026-11-12). */
	float FidgetClock = 5.0f;
	int32 FidgetIndex = 0;
	float StateClock = 0.0f;
	float ScanClock = 0.0f;
	float WatchCool = 0.0f;
	int32 RouteIndex = 0;
	FVector2D Goal = FVector2D::ZeroVector;
	int32 ProbeIndex = INDEX_NONE;
	TWeakObjectPtr<AActor> WatchTarget;
	bool bAlerted = false;
	float LastNow = 0.0f;

	class IPSMatchInfo* Match() const;
	bool MoveTowards(const FVector2D& Target, float Speed, float Dt, bool bRegions);
	void Enter(EPSGuardState NewState);
	void FinishKneel();
	void FinishPoke();
	int32 PickProbe(float Now) const;
	void ApplyPose(float Speed, float Dt);
};
