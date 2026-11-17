// Pocket Sand - speed-driven locomotion without an AnimBlueprint (P38): a walk clip (and optionally a run clip, blended
// by speed on one shared phase) scaled to ground speed, blended with idle, an action clip layered over it, and a
// procedural waddle that sways the hips without moving the feet.
//
// Root handling per sampled clip (so blends stay grounded):
//  - Native Rocketbox mocap (RBM_*): the root (Bip01) carries the hip height, bob, sway and forward travel; only the
//    linear forward travel is removed (in place).
//  - UAL clips retargeted onto Rocketbox (RB_*): the hip height sits on the pelvis and the root has no track, so the
//    root's 89.5 cm bind height must be zeroed (it floated everyone by ~90 cm, D16).

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "PSHumanAnimInstance.generated.h"

class UAnimSequence;

struct FPSRootInfo
{
	FVector Start = FVector::ZeroVector;
	FVector End = FVector::ZeroVector;
	float Length = 1.0f;
	/** Pelvis carries the hip height (retargeted clip): zero the root entirely. */
	bool bZeroRoot = false;
};

struct FPSHumanAnimProxy : public FAnimInstanceProxy
{
	FPSHumanAnimProxy() = default;
	explicit FPSHumanAnimProxy(UAnimInstance* InInstance) : FAnimInstanceProxy(InInstance) {}

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual void Update(float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

private:
	void SampleInto(const UAnimSequence* Sequence, double Time, bool bLoop, FPoseContext& Pose) const;
	void ApplyWaddle(FPoseContext& Output) const;
	void ApplyBend(FPoseContext& Output) const;
	void CacheRoot(const UAnimSequence* Sequence);
	float RunWeight() const;

	TObjectPtr<const UAnimSequence> Idle = nullptr;
	TObjectPtr<const UAnimSequence> Walk = nullptr;
	TObjectPtr<const UAnimSequence> Run = nullptr;
	TObjectPtr<const UAnimSequence> Action = nullptr;
	TMap<const UAnimSequence*, FPSRootInfo> Roots;
	float WalkAnimSpeed = 150.0f;
	float RunAnimSpeed = 300.0f;
	float RunPhaseOffset = 0.0f;
	float MinPlayRate = 0.5f;
	float MaxPlayRate = 2.2f;
	float GroundSpeed = 0.0f;
	float Waddle = 0.0f;
	float WaddleRoll = 3.0f;
	float WaddleSpread = 2.5f;
	float Bend = 0.0f;
	float BendDeg = 38.0f;
	FVector BendAxis = FVector(-1.0f, 0.0f, 0.0f);
	float ActionTarget = 0.0f;
	float ActionBlendSpeed = 6.0f;
	float ActionRate = 1.0f;
	float ActionHoldAt = -1.0f;
	bool bActionLoops = true;
	bool bActionUpperBody = false;
	uint32 ActionSerial = 0;
	FName PelvisBone;
	FName SpineBone;
	FName ThighL;
	FName ThighR;
	FName FootL;
	FName FootR;
	FVector RollAxis = FVector(0.0f, 1.0f, 0.0f);
	bool bGroundRoot = false;

	double IdleTime = 0.0;
	double MovePhase = 0.0;
	double ActionTime = 0.0;
	float ActionWeight = 0.0f;
	uint32 SeenSerial = 0;
};

UCLASS(Transient)
class POCKETSAND_API UPSHumanAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

	friend struct FPSHumanAnimProxy;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation") TObjectPtr<UAnimSequence> IdleAnim;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation") TObjectPtr<UAnimSequence> WalkAnim;
	/** Optional faster gait; blended in by speed between WalkAnimSpeed and RunAnimSpeed so the stride grows with speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation") TObjectPtr<UAnimSequence> RunAnim;

	/** Measured root speed of the walk clip at play rate 1 (cm/s); play rate = ground speed / this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float WalkAnimSpeed = 150.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float RunAnimSpeed = 300.0f;
	/** Normalised phase shift applied to the run clip so its foot contacts line up with the walk's. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float RunPhaseOffset = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float MinPlayRate = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float MaxPlayRate = 2.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float ActionBlendSpeed = 6.0f;
	/** Pelvis roll per step at full waddle (degrees). The legs counter-rotate, so the feet stay put. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float WaddleRoll = 3.0f; // 7 read as weird (user 2026-11-09)
	/** Thighs splay outward at full waddle (degrees): the bags between the knees. The feet stay flat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float WaddleSpread = 2.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rig") FName PelvisBone = TEXT("pelvis");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rig") FName SpineBone = TEXT("spine_02");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rig") FName ThighL = TEXT("thigh_l");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rig") FName ThighR = TEXT("thigh_r");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rig") FName FootL = TEXT("foot_l");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rig") FName FootR = TEXT("foot_r");
	/** Component-space axis the pelvis rolls about (the body's forward axis). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rig") FVector RollAxis = FVector(0.0f, 1.0f, 0.0f);
	/** Normalise the root per clip (Rocketbox biped): see the file comment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rig") bool bGroundRoot = false;

	UPROPERTY(Transient, BlueprintReadWrite, Category = "State") float GroundSpeed = 0.0f;
	UPROPERTY(Transient, BlueprintReadWrite, Category = "State") float Waddle = 0.0f;
	/** 0..1: bend forward at the waist (picking a bag up); the hands come from the action clip. */
	UPROPERTY(Transient, BlueprintReadWrite, Category = "State") float Bend = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float BendDeg = 38.0f;
	/** Component-space axis to bend the spine around (the side axis; RollAxis is forward). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rig") FVector BendAxis = FVector(-1.0f, 0.0f, 0.0f); // -X bends forward on the Rocketbox rig (side shot 2026-11-10)

	/** HoldAt >= 0 freezes a non-looping clip at that time (kneel: go down, stay down). */
	/** bUpperBody: the clip drives the spine and up only; the legs keep the native locomotion (PlazaDuty method). */
	void PlayAction(UAnimSequence* Clip, bool bLoop, float Rate = 1.0f, float HoldAt = -1.0f, bool bUpperBody = false);
	void StopAction();
	bool HasAction() const { return ActionAnim != nullptr && ActionTarget > 0.0f; }

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

private:
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> ActionAnim;
	float ActionTarget = 0.0f;
	float ActionRate = 1.0f;
	float ActionHoldAt = -1.0f;
	bool bActionLoops = true;
	bool bActionUpperBody = false;
	uint32 ActionSerial = 0;
};
