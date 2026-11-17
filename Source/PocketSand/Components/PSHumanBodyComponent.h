// Pocket Sand - a person: the CC0 UAL rig drives (invisible) and the CC0 base character, eyes, brows and hair follow
// it through leader pose, dressed by our kit-band material. Attach to any actor (hero, inmates, guards).

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Data/PSTypes.h"
#include "PSHumanBodyComponent.generated.h"

class USkeletalMeshComponent;
class USkeletalMesh;
class UStaticMesh;
class UStaticMeshComponent;
class UAnimSequence;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UPSHumanAnimInstance;
class UPSRoundConfig;

UCLASS(ClassGroup = (PocketSand), meta = (BlueprintSpawnableComponent))
class POCKETSAND_API UPSHumanBodyComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UPSHumanBodyComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float MeshYaw = -90.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float WalkAnimSpeed = 93.0f;
	/** Optional faster gait blended in by speed (empty = walk clip only). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float RunAnimSpeed = 300.0f;
	/** Run-clip phase shift so its left heel contact lands with the walk's (probe_contact_phase.py: walk 0.685, slow run 0.725). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float RunPhaseOffset = 0.04f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float BodyScale = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float SkinToneDark = 0.35f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") bool bCastShadow = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") bool bUpdateRateOptimizations = true;
	/** NPCs are moved on the fixed simulation step (30 Hz); glide the visible body between steps so walking does not judder. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") bool bSmoothSimSteps = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float SmoothStepSeconds = 1.0f / 30.0f;
	/** Jumps longer than this (respawns, teleports) snap instead of gliding. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float SmoothSnapDistance = 250.0f;

	UPROPERTY(EditAnywhere, Category = "Assets") FString RigPath = TEXT("/UAL/UAL1_Standard/SkeletalMeshes/UAL1_Standard");
	UPROPERTY(EditAnywhere, Category = "Assets") FString ClipPrefix = TEXT("/UAL/UAL1_Standard/SkeletalMeshes/UAL1_Standard");
	UPROPERTY(EditAnywhere, Category = "Assets") FString IdleClip = TEXT("Idle_Loop");
	UPROPERTY(EditAnywhere, Category = "Assets") FString WalkClip = TEXT("Walk_Loop");
	UPROPERTY(EditAnywhere, Category = "Assets") FString RunClip;
	/** Standing actions that play on the upper body only (retargeted clips keep their legs out of the gait). */
	UPROPERTY(EditAnywhere, Category = "Assets") TArray<FString> UpperBodyActions = { TEXT("Interact"), TEXT("Punch_Cross"), TEXT("Sword_Attack"), TEXT("PickUp_Table"),
		TEXT("RBM_Dust_01"), TEXT("RBM_Dust_02"), TEXT("RBM_ScratchHead"), TEXT("RBM_Shrug"), TEXT("RBM_LookAround"), TEXT("RBM_TakeBag"),
		TEXT("RBM_LookAround_02"), TEXT("RBM_LookAround_03"), TEXT("RBM_ScratchHead_02"), TEXT("RBM_Shrug_02"), TEXT("RBM_Stretch"), TEXT("RBM_Clap"), TEXT("RBM_DocCheck") };
	UPROPERTY(EditAnywhere, Category = "Assets") FString BodyPath = TEXT("/UBC/Superhero_Male_FullBody/SkeletalMeshes/SuperHero_Male");
	UPROPERTY(EditAnywhere, Category = "Assets") FString EyePath = TEXT("/UBC/Superhero_Male_FullBody/SkeletalMeshes/Eyes");
	UPROPERTY(EditAnywhere, Category = "Assets") FString BrowPath = TEXT("/UBC/Superhero_Male_FullBody/SkeletalMeshes/Eyebrows");
	UPROPERTY(EditAnywhere, Category = "Assets") TArray<FString> HairPaths = { TEXT("/Hair/Hair_Buzzed/SkeletalMeshes/Hair_Buzzed"), TEXT("/Hair/Hair_SimpleParted/SkeletalMeshes/Hair_SimpleParted"), TEXT("") };
	UPROPERTY(EditAnywhere, Category = "Assets") FString KitMaterial = TEXT("MI_PS_KitMale");
	UPROPERTY(EditAnywhere, Category = "Assets") FString HairMaterial = TEXT("MI_PS_Hair");
	UPROPERTY(EditAnywhere, Category = "Assets") FString EyeMaterial = TEXT("MI_PS_Eyes");
	UPROPERTY(EditAnywhere, Category = "Assets") FString InvisibleMaterial = TEXT("MI_PS_Invisible");
	/** Rocketbox mode: <CharacterRoot>/Rocketbox/<Avatar>/SK_<Avatar>, clips retargeted from UAL as RB_<Clip>. */
	UPROPERTY(EditAnywhere, Category = "Assets") FString RocketboxFolder = TEXT("/Rocketbox");
	UPROPERTY(EditAnywhere, Category = "Assets") FString RocketboxClipPrefix = TEXT("/Rocketbox/Anims/RB_");
	UPROPERTY(EditAnywhere, Category = "Rig") FName RBPelvis = TEXT("Bip01-Pelvis");
	UPROPERTY(EditAnywhere, Category = "Rig") FName RBSpine = TEXT("Bip01-Spine1");
	UPROPERTY(EditAnywhere, Category = "Rig") FName RBThighL = TEXT("Bip01-L-Thigh");
	UPROPERTY(EditAnywhere, Category = "Rig") FName RBThighR = TEXT("Bip01-R-Thigh");
	UPROPERTY(EditAnywhere, Category = "Rig") FName RBFootL = TEXT("Bip01-L-Foot");
	UPROPERTY(EditAnywhere, Category = "Rig") FName RBFootR = TEXT("Bip01-R-Foot");
	/** Native Rocketbox mocap clips (no retarget) are named RBM_* and live beside the retargeted RB_* clips. */
	UPROPERTY(EditAnywhere, Category = "Assets") FString RocketboxMocapFolder = TEXT("/Rocketbox/Anims/");
	UPROPERTY(EditAnywhere, Category = "Rig") FName RBHead = TEXT("Bip01-Head");
	UPROPERTY(EditAnywhere, Category = "Rig") FName RBHandR = TEXT("Bip01-R-Hand");
	/** Component-space forward axis of the Rocketbox skeleton (waddle roll and thigh splay rotate about it). Measured in close-ups. */
	UPROPERTY(EditAnywhere, Category = "Rig") FVector RBRollAxis = FVector(0.0f, 1.0f, 0.0f);

	/** Build the body from the config's asset roots. Safe to call again (re-dress). */
	void Dress(const UPSRoundConfig* Config, const FPSOutfit& Outfit);
	void SetLocomotion(float GroundSpeed, float Waddle01);
	/** 0..1 forward bend at the waist (bag pickup). */
	void SetBend(float Bend01);
	/** How fast actions blend in and out (1/s): slower for crouching down and standing up. */
	void SetActionBlendSpeed(float PerSecond);
	/** Plays a UAL clip by name (e.g. "Fixing_Kneeling") over the locomotion. */
	void PlayAction(const FString& ClipName, bool bLoop, float Rate = 1.0f, float HoldAt = -1.0f);
	void StopAction();
	void SetIdleClip(const FString& ClipName);
	/** Attach a prop mesh to a rig bone (cap on the head, stick in the hand, bags on the thighs). */
	UStaticMeshComponent* AttachProp(UStaticMesh* Mesh, UMaterialInterface* Material, FName Bone, const FTransform& Offset);
	USkeletalMeshComponent* Driver() const { return Rig; }
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	bool IsDressed() const { return Rig != nullptr && Anim != nullptr; }
	FVector BoneLocation(FName Bone) const;
	bool IsRocketbox() const { return !Avatar.IsEmpty(); }
	FName HeadBone() const { return IsRocketbox() ? RBHead : FName(TEXT("head")); }
	FName RightHandBone() const { return IsRocketbox() ? RBHandR : FName(TEXT("hand_r")); }
	FName ToeBone(bool bLeft) const { return IsRocketbox() ? FName(bLeft ? TEXT("Bip01-L-Toe0") : TEXT("Bip01-R-Toe0")) : FName(bLeft ? TEXT("ball_l") : TEXT("ball_r")); }
	FName ThighBone(bool bLeft) const { return IsRocketbox() ? (bLeft ? RBThighL : RBThighR) : FName(bLeft ? TEXT("thigh_l") : TEXT("thigh_r")); }

private:
	UPROPERTY() TObjectPtr<USkeletalMeshComponent> Rig;
	FTransform SmoothOffset = FTransform::Identity;
	FTransform SmoothFrom = FTransform::Identity;
	FTransform SmoothTarget = FTransform::Identity;
	float SmoothAlpha = 1.0f;
	bool bSmoothInit = false;
	UPROPERTY() TObjectPtr<UPSHumanAnimInstance> Anim;
	UPROPERTY() TArray<TObjectPtr<USkeletalMeshComponent>> Followers;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> Dynamics;
	UPROPERTY() TMap<FString, TObjectPtr<UAnimSequence>> ClipCache;
	FString CharRoot;
	FString Avatar;

	UAnimSequence* Clip(const FString& Name);
	USkeletalMeshComponent* AddFollower(USkeletalMesh* Mesh, UMaterialInterface* Material);
};
