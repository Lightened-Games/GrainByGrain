// Pocket Sand - locomotion anim instance (adapted from The Fix's FixHumanAnimInstance).

#include "Animation/PSHumanAnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "AnimationRuntime.h"
#include "BonePose.h"

void UPSHumanAnimInstance::PlayAction(UAnimSequence* Clip, bool bLoop, float Rate, float HoldAt, bool bUpperBody)
{
	bActionUpperBody = bUpperBody;
	if (Clip != ActionAnim || ActionTarget < 1.0f || !bLoop)
	{
		++ActionSerial;
	}
	ActionAnim = Clip;
	bActionLoops = bLoop;
	ActionRate = Rate;
	ActionHoldAt = HoldAt;
	ActionTarget = Clip ? 1.0f : 0.0f;
}

void UPSHumanAnimInstance::StopAction()
{
	ActionTarget = 0.0f;
}

FAnimInstanceProxy* UPSHumanAnimInstance::CreateAnimInstanceProxy()
{
	return new FPSHumanAnimProxy(this);
}

void UPSHumanAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete InProxy;
}

void FPSHumanAnimProxy::CacheRoot(const UAnimSequence* Sequence)
{
	if (!Sequence || Roots.Contains(Sequence))
	{
		return;
	}
	FPSRootInfo Info;
	Info.Length = FMath::Max(Sequence->GetPlayLength(), KINDA_SMALL_NUMBER);
	if (const USkeleton* Skel = Sequence->GetSkeleton())
	{
		const FReferenceSkeleton& Ref = Skel->GetReferenceSkeleton();
		FTransform A, B, Pelvis;
		Sequence->GetBoneTransform(A, FSkeletonPoseBoneIndex(0), FAnimExtractContext(0.0), false);
		Sequence->GetBoneTransform(B, FSkeletonPoseBoneIndex(0), FAnimExtractContext(static_cast<double>(Info.Length)), false);
		Info.Start = A.GetTranslation();
		Info.End = B.GetTranslation();
		const int32 PelvisIndex = Ref.FindBoneIndex(PelvisBone);
		if (PelvisIndex != INDEX_NONE)
		{
			Sequence->GetBoneTransform(Pelvis, FSkeletonPoseBoneIndex(PelvisIndex), FAnimExtractContext(0.0), false);
			Info.bZeroRoot = Pelvis.GetTranslation().Size() > 40.0f;
		}
	}
	Roots.Add(Sequence, Info);
}

void FPSHumanAnimProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
	const UPSHumanAnimInstance* I = CastChecked<UPSHumanAnimInstance>(InAnimInstance);
	Idle = I->IdleAnim;
	Walk = I->WalkAnim;
	Run = I->RunAnim;
	Action = I->ActionAnim;
	WalkAnimSpeed = FMath::Max(I->WalkAnimSpeed, 1.0f);
	RunAnimSpeed = FMath::Max(I->RunAnimSpeed, WalkAnimSpeed * 1.5f);
	RunPhaseOffset = I->RunPhaseOffset;
	MinPlayRate = I->MinPlayRate;
	MaxPlayRate = FMath::Max(I->MaxPlayRate, MinPlayRate);
	GroundSpeed = I->GroundSpeed;
	Waddle = FMath::Clamp(I->Waddle, 0.0f, 1.0f);
	Bend = FMath::Clamp(I->Bend, 0.0f, 1.0f);
	BendDeg = I->BendDeg;
	BendAxis = I->BendAxis.GetSafeNormal();
	WaddleRoll = I->WaddleRoll;
	WaddleSpread = I->WaddleSpread;
	ActionTarget = I->ActionTarget;
	ActionBlendSpeed = I->ActionBlendSpeed;
	ActionRate = I->ActionRate;
	ActionHoldAt = I->ActionHoldAt;
	bActionLoops = I->bActionLoops;
	bActionUpperBody = I->bActionUpperBody;
	ActionSerial = I->ActionSerial;
	PelvisBone = I->PelvisBone;
	SpineBone = I->SpineBone;
	ThighL = I->ThighL;
	ThighR = I->ThighR;
	FootL = I->FootL;
	FootR = I->FootR;
	RollAxis = I->RollAxis.GetSafeNormal();
	bGroundRoot = I->bGroundRoot;
	if (bGroundRoot)
	{
		// Game thread: work out each clip's root handling once (the worker-thread evaluation only reads the cache).
		CacheRoot(Idle);
		CacheRoot(Walk);
		CacheRoot(Run);
		CacheRoot(Action);
	}
}

float FPSHumanAnimProxy::RunWeight() const
{
	if (!Run)
	{
		return 0.0f;
	}
	const float A = WalkAnimSpeed * 1.15f;
	const float B = RunAnimSpeed * 0.85f;
	return FMath::SmoothStep(0.0f, 1.0f, FMath::Clamp((GroundSpeed - A) / FMath::Max(B - A, 1.0f), 0.0f, 1.0f));
}

void FPSHumanAnimProxy::Update(float DeltaSeconds)
{
	FAnimInstanceProxy::Update(DeltaSeconds);
	if (Idle)
	{
		IdleTime = FMath::Fmod(IdleTime + DeltaSeconds, (double)FMath::Max(Idle->GetPlayLength(), KINDA_SMALL_NUMBER));
	}
	if (Walk && GroundSpeed > 1.0f)
	{
		// One shared phase for walk and run: the gait's native speed and cycle length blend with the run weight, so the
		// stride (not just the cadence) grows with speed and the planted foot does not slide.
		const float R = RunWeight();
		const float NativeSpeed = FMath::Lerp(WalkAnimSpeed, RunAnimSpeed, R);
		const float CycleLen = FMath::Lerp(Walk->GetPlayLength(), Run ? Run->GetPlayLength() : Walk->GetPlayLength(), R);
		const float PlayRate = FMath::Clamp(GroundSpeed / NativeSpeed, MinPlayRate, MaxPlayRate);
		MovePhase = FMath::Fmod(MovePhase + DeltaSeconds * PlayRate / FMath::Max(CycleLen, KINDA_SMALL_NUMBER), 1.0);
	}
	if (SeenSerial != ActionSerial)
	{
		SeenSerial = ActionSerial;
		ActionTime = 0.0;
	}
	if (Action)
	{
		const double Len = FMath::Max(Action->GetPlayLength(), KINDA_SMALL_NUMBER);
		ActionTime += DeltaSeconds * ActionRate;
		if (bActionLoops)
		{
			ActionTime = FMath::Fmod(ActionTime, Len);
		}
		else
		{
			const double Cap = ActionHoldAt >= 0.0f ? FMath::Min((double)ActionHoldAt, Len - 0.001) : Len - 0.001;
			ActionTime = FMath::Min(ActionTime, Cap);
		}
	}
	ActionWeight = FMath::FInterpConstantTo(ActionWeight, Action ? ActionTarget : 0.0f, DeltaSeconds, ActionBlendSpeed);
}

void FPSHumanAnimProxy::SampleInto(const UAnimSequence* Sequence, double Time, bool bLoop, FPoseContext& Pose) const
{
	FAnimationPoseData PoseData(Pose);
	Sequence->GetAnimationPose(PoseData, FAnimExtractContext(Time, false, FDeltaTimeRecord(), bLoop));
	if (!bGroundRoot || Pose.Pose.GetNumBones() == 0)
	{
		return;
	}
	const FPSRootInfo* Info = Roots.Find(Sequence);
	FTransform& Root = Pose.Pose[FCompactPoseBoneIndex(0)];
	if (!Info || Info->bZeroRoot)
	{
		Root.SetTranslation(FVector::ZeroVector);
		return;
	}
	// Mocap: keep the hip height, bob and sway; remove only the steady forward travel (the capsule does the moving).
	const float U = FMath::Clamp(static_cast<float>(Time) / Info->Length, 0.0f, 1.0f);
	const FVector Drift = Info->Start + (Info->End - Info->Start) * U;
	FVector T = Root.GetTranslation();
	T.X -= Drift.X;
	T.Y -= Drift.Y;
	Root.SetTranslation(T);
}

void FPSHumanAnimProxy::ApplyWaddle(FPoseContext& Output) const
{
	if (Waddle <= 0.01f)
	{
		return;
	}
	// Two steps per walk cycle: the hips rock onto each foot and the spine counters. The legs keep their clip
	// orientation (plus a small outward splay for the bags), and the feet keep theirs, so knees track and soles stay flat.
	const float MoveW = FMath::Clamp(GroundSpeed / 60.0f, 0.0f, 1.0f);
	const float Rock = FMath::Sin(static_cast<float>(MovePhase) * UE_TWO_PI * 2.0f) * WaddleRoll * Waddle * MoveW;
	const float Spread = WaddleSpread * Waddle;
	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	auto Find = [&Bones](FName Name) -> FCompactPoseBoneIndex
	{
		const int32 MeshIndex = Bones.GetPoseBoneIndexForBoneName(Name);
		return MeshIndex == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
	};
	FCSPose<FCompactPose> CS;
	CS.InitPose(Output.Pose);
	TArray<FBoneTransform> Changes;
	auto SetRotation = [&CS, &Changes](FCompactPoseBoneIndex Index, const FQuat& Rotation)
	{
		if (Index.GetInt() == INDEX_NONE)
		{
			return;
		}
		FTransform T = CS.GetComponentSpaceTransform(Index);
		T.SetRotation(Rotation);
		Changes.Add(FBoneTransform(Index, T));
		CS.SafeSetCSBoneTransforms(Changes);
		Changes.Reset();
	};
	auto Rotation = [&CS](FCompactPoseBoneIndex Index)
	{
		return Index.GetInt() == INDEX_NONE ? FQuat::Identity : CS.GetComponentSpaceTransform(Index).GetRotation();
	};
	const FCompactPoseBoneIndex Pelvis = Find(PelvisBone), Spine = Find(SpineBone), TL = Find(ThighL), TR = Find(ThighR), FL = Find(FootL), FR = Find(FootR);
	const FQuat ThighLRot = Rotation(TL), ThighRRot = Rotation(TR), FootLRot = Rotation(FL), FootRRot = Rotation(FR);
	const float Rad = UE_PI / 180.0f;
	SetRotation(Pelvis, FQuat(RollAxis, Rock * Rad) * Rotation(Pelvis));
	SetRotation(Spine, FQuat(RollAxis, -Rock * 0.8f * Rad) * Rotation(Spine));
	SetRotation(TL, FQuat(RollAxis, -Spread * Rad) * ThighLRot);
	SetRotation(TR, FQuat(RollAxis, Spread * Rad) * ThighRRot);
	SetRotation(FL, FootLRot);
	SetRotation(FR, FootRRot);
	FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(CS), Output.Pose);
}

bool FPSHumanAnimProxy::Evaluate(FPoseContext& Output)
{
	if (!Idle || !Walk)
	{
		return false;
	}
	FPoseContext Ground(Output);
	const float W = FMath::Clamp(GroundSpeed / FMath::Max(WalkAnimSpeed * 0.6f, 1.0f), 0.0f, 1.0f);
	const float R = RunWeight();
	auto SampleGait = [this, R](FPoseContext& Out)
	{
		SampleInto(Walk, MovePhase * Walk->GetPlayLength(), true, Out);
		if (Run && R > 0.001f)
		{
			FPoseContext RunPose(Out);
			const double RunPhase = FMath::Fmod(MovePhase + RunPhaseOffset + 1.0, 1.0);
			SampleInto(Run, RunPhase * Run->GetPlayLength(), true, RunPose);
			if (R >= 0.999f)
			{
				Out = RunPose;
			}
			else
			{
				FPoseContext WalkPose(Out);
				WalkPose = Out;
				FAnimationPoseData Blend(Out);
				FAnimationRuntime::BlendTwoPosesTogether(FAnimationPoseData(WalkPose), FAnimationPoseData(RunPose), 1.0f - R, Blend);
			}
		}
	};
	if (W <= 0.001f)
	{
		SampleInto(Idle, IdleTime, true, Ground);
	}
	else if (W >= 0.999f)
	{
		SampleGait(Ground);
	}
	else
	{
		FPoseContext A(Output), B(Output);
		SampleInto(Idle, IdleTime, true, A);
		SampleGait(B);
		FAnimationPoseData Out(Ground);
		FAnimationRuntime::BlendTwoPosesTogether(FAnimationPoseData(A), FAnimationPoseData(B), 1.0f - W, Out);
	}
	if (Action && ActionWeight > KINDA_SMALL_NUMBER)
	{
		FPoseContext ActionPose(Output);
		SampleInto(Action, ActionTime, bActionLoops, ActionPose);
		if (bActionUpperBody)
		{
			// Spine and everything under it from the action; pelvis, legs and root stay on the locomotion.
			Output = Ground;
			const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
			const int32 SpineMesh = Bones.GetPoseBoneIndexForBoneName(SpineBone);
			if (SpineMesh != INDEX_NONE)
			{
				const FCompactPoseBoneIndex Spine = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(SpineMesh));
				for (FCompactPoseBoneIndex I : Output.Pose.ForEachBoneIndex())
				{
					FCompactPoseBoneIndex A = I;
					while (A.GetInt() != INDEX_NONE && A != Spine)
					{
						A = Bones.GetParentBoneIndex(A);
					}
					if (A == Spine)
					{
						FTransform T = Ground.Pose[I];
						T.BlendWith(ActionPose.Pose[I], ActionWeight);
						Output.Pose[I] = T;
					}
				}
			}
		}
		else
		{
			FAnimationPoseData Out(Output);
			FAnimationRuntime::BlendTwoPosesTogether(FAnimationPoseData(Ground), FAnimationPoseData(ActionPose), 1.0f - ActionWeight, Out);
		}
	}
	else
	{
		Output = Ground;
	}
	ApplyWaddle(Output);
	ApplyBend(Output);
	return true;
}

void FPSHumanAnimProxy::ApplyBend(FPoseContext& Output) const
{
	// Bend forward from the waist (only the spine: the legs and feet keep the clip, so nothing slides).
	if (Bend <= 0.01f)
	{
		return;
	}
	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	const int32 MeshIndex = Bones.GetPoseBoneIndexForBoneName(SpineBone);
	if (MeshIndex == INDEX_NONE)
	{
		return;
	}
	const FCompactPoseBoneIndex Spine = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
	FCSPose<FCompactPose> CS;
	CS.InitPose(Output.Pose);
	FTransform T = CS.GetComponentSpaceTransform(Spine);
	T.SetRotation(FQuat(BendAxis, BendDeg * Bend * UE_PI / 180.0f) * T.GetRotation());
	TArray<FBoneTransform> Changes;
	Changes.Add(FBoneTransform(Spine, T));
	CS.SafeSetCSBoneTransforms(Changes);
	FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(CS), Output.Pose);
}