// Pocket Sand - human body component.

#include "Components/PSHumanBodyComponent.h"
#include "Animation/PSHumanAnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Data/PSRoundConfig.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "PocketSand.h"

namespace
{
	template <typename T>
	T* LoadPath(const FString& Path)
	{
		if (Path.IsEmpty())
		{
			return nullptr;
		}
		const FString Name = FPaths::GetBaseFilename(Path);
		return LoadObject<T>(nullptr, *FString::Printf(TEXT("%s.%s"), *Path, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
}

UPSHumanBodyComponent::UPSHumanBodyComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UPSHumanBodyComponent::BeginPlay()
{
	Super::BeginPlay();
	if (bSmoothSimSteps && GetOwner())
	{
		SmoothOffset = GetRelativeTransform();
		SetUsingAbsoluteLocation(true);
		SetUsingAbsoluteRotation(true);
		SetComponentTickEnabled(true);
	}
}

void UPSHumanBodyComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bSmoothSimSteps || !GetOwner())
	{
		return;
	}
	// Each new simulation pose becomes the target; the body glides from where it is shown now to it over one step.
	const FTransform Target = SmoothOffset * GetOwner()->GetActorTransform();
	if (!bSmoothInit || FVector::Dist(Target.GetLocation(), SmoothTarget.GetLocation()) > SmoothSnapDistance)
	{
		bSmoothInit = true;
		SmoothFrom = SmoothTarget = Target;
		SmoothAlpha = 1.0f;
	}
	else if (!Target.GetLocation().Equals(SmoothTarget.GetLocation(), 0.01) || !Target.GetRotation().Equals(SmoothTarget.GetRotation(), 1.0e-4))
	{
		SmoothFrom = GetComponentTransform();
		SmoothTarget = Target;
		SmoothAlpha = 0.0f;
	}
	SmoothAlpha = FMath::Min(1.0f, SmoothAlpha + DeltaTime / FMath::Max(SmoothStepSeconds, KINDA_SMALL_NUMBER));
	SetWorldLocationAndRotation(FMath::Lerp(SmoothFrom.GetLocation(), SmoothTarget.GetLocation(), SmoothAlpha),
		FQuat::Slerp(SmoothFrom.GetRotation(), SmoothTarget.GetRotation(), SmoothAlpha));
}

UAnimSequence* UPSHumanBodyComponent::Clip(const FString& Name)
{
	if (TObjectPtr<UAnimSequence>* Found = ClipCache.Find(Name))
	{
		return *Found;
	}
	const FString Path = !Avatar.IsEmpty() && Name.StartsWith(TEXT("RBM_")) ? CharRoot + RocketboxMocapFolder + Name
		: CharRoot + (Avatar.IsEmpty() ? ClipPrefix : RocketboxClipPrefix) + Name;
	UAnimSequence* S = LoadPath<UAnimSequence>(Path);
	if (!S)
	{
		UE_LOG(LogPocketSand, Warning, TEXT("[PocketSand] Missing clip %s"), *Path);
	}
	ClipCache.Add(Name, S);
	return S;
}

USkeletalMeshComponent* UPSHumanBodyComponent::AddFollower(USkeletalMesh* Mesh, UMaterialInterface* Material)
{
	if (!Mesh || !Rig)
	{
		return nullptr;
	}
	USkeletalMeshComponent* C = NewObject<USkeletalMeshComponent>(GetOwner());
	C->SetSkeletalMesh(Mesh);
	if (Material)
	{
		for (int32 S = 0; S < C->GetNumMaterials(); ++S)
		{
			C->SetMaterial(S, Material);
		}
	}
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetupAttachment(Rig);
	C->RegisterComponent();
	C->SetLeaderPoseComponent(Rig);
	C->SetCastShadow(bCastShadow);
	C->bEnableUpdateRateOptimizations = bUpdateRateOptimizations;
	GetOwner()->AddInstanceComponent(C);
	Followers.Add(C);
	return C;
}

void UPSHumanBodyComponent::Dress(const UPSRoundConfig* Config, const FPSOutfit& Outfit)
{
	if (!Config || !GetOwner())
	{
		return;
	}
	CharRoot = Config->CharacterRoot;
	SmoothStepSeconds = 1.0f / FMath::Max(Config->SimHz, 1.0f);
	if (Avatar != Outfit.Avatar)
	{
		ClipCache.Reset();
	}
	Avatar = Outfit.Avatar;
	const FString MatRoot = Config->MaterialRoot;
	for (USkeletalMeshComponent* F : Followers)
	{
		if (F)
		{
			F->DestroyComponent();
		}
	}
	Followers.Reset();
	Dynamics.Reset();
	if (!Rig)
	{
		Rig = NewObject<USkeletalMeshComponent>(GetOwner(), TEXT("Rig"));
		Rig->SetupAttachment(this);
		Rig->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Rig->SetCastShadow(false);
		Rig->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		Rig->RegisterComponent();
		GetOwner()->AddInstanceComponent(Rig);
	}
	Rig->SetRelativeRotation(FRotator(0.0f, MeshYaw, 0.0f));
	Rig->SetRelativeScale3D(FVector(BodyScale));
	if (!Avatar.IsEmpty())
	{
		// Rocketbox avatar: one visible skinned mesh with its own PBR materials, driven by retargeted UAL clips.
		USkeletalMesh* AvatarMesh = LoadPath<USkeletalMesh>(CharRoot + RocketboxFolder + TEXT("/") + Avatar + TEXT("/SK_") + Avatar);
		if (!AvatarMesh)
		{
			UE_LOG(LogPocketSand, Error, TEXT("[PocketSand] Missing Rocketbox avatar %s - run Tools/build_content.ps1 -Stages humans"), *Avatar);
			return;
		}
		Rig->SetSkeletalMesh(AvatarMesh);
		Rig->SetCastShadow(bCastShadow);
		Rig->bEnableUpdateRateOptimizations = bUpdateRateOptimizations;
		Rig->EmptyOverrideMaterials();
		Rig->SetAnimInstanceClass(UPSHumanAnimInstance::StaticClass());
		Anim = Cast<UPSHumanAnimInstance>(Rig->GetAnimInstance());
		if (Anim)
		{
			Anim->IdleAnim = Clip(IdleClip);
			Anim->WalkAnim = Clip(WalkClip);
			Anim->WalkAnimSpeed = WalkAnimSpeed;
			Anim->RunAnim = RunClip.IsEmpty() ? nullptr : Clip(RunClip);
			Anim->RunAnimSpeed = RunAnimSpeed;
			Anim->RunPhaseOffset = RunPhaseOffset;
			Anim->PelvisBone = RBPelvis;
			Anim->FootL = RBFootL;
			Anim->FootR = RBFootR;
			Anim->SpineBone = RBSpine;
			Anim->ThighL = RBThighL;
			Anim->ThighR = RBThighR;
			Anim->RollAxis = RBRollAxis;
			Anim->bGroundRoot = true;
		}
		return;
	}
	USkeletalMesh* RigMesh = LoadPath<USkeletalMesh>(CharRoot + RigPath);
	if (!RigMesh)
	{
		UE_LOG(LogPocketSand, Error, TEXT("[PocketSand] Missing rig %s%s - run Tools/build_content.ps1 -Stages character"), *CharRoot, *RigPath);
		return;
	}
	Rig->SetSkeletalMesh(RigMesh);
	if (UMaterialInterface* Invisible = LoadPath<UMaterialInterface>(MatRoot / InvisibleMaterial))
	{
		for (int32 S = 0; S < Rig->GetNumMaterials(); ++S)
		{
			Rig->SetMaterial(S, Invisible);
		}
	}
	Rig->SetAnimInstanceClass(UPSHumanAnimInstance::StaticClass());
	Anim = Cast<UPSHumanAnimInstance>(Rig->GetAnimInstance());
	if (Anim)
	{
		Anim->IdleAnim = Clip(IdleClip);
		Anim->WalkAnim = Clip(WalkClip);
		Anim->WalkAnimSpeed = WalkAnimSpeed;
	}

	UMaterialInterface* KitBase = LoadPath<UMaterialInterface>(MatRoot / KitMaterial);
	UMaterialInstanceDynamic* Kit = KitBase ? UMaterialInstanceDynamic::Create(KitBase, this) : nullptr;
	if (Kit)
	{
		Kit->SetVectorParameterValue(TEXT("ShirtColor"), Outfit.Top);
		Kit->SetVectorParameterValue(TEXT("TrouserColor"), Outfit.Bottom);
		Kit->SetVectorParameterValue(TEXT("ShoeColor"), Outfit.Shoes);
		Kit->SetScalarParameterValue(TEXT("SkinTone"), FMath::Lerp(1.0f, SkinToneDark, Outfit.SkinTone));
		Kit->SetScalarParameterValue(TEXT("SleeveEnd"), Outfit.SleeveEnd);
		Dynamics.Add(Kit);
	}
	AddFollower(LoadPath<USkeletalMesh>(CharRoot + BodyPath), Kit);
	AddFollower(LoadPath<USkeletalMesh>(CharRoot + EyePath), LoadPath<UMaterialInterface>(MatRoot / EyeMaterial));
	UMaterialInterface* HairBase = LoadPath<UMaterialInterface>(MatRoot / HairMaterial);
	UMaterialInstanceDynamic* Hair = HairBase ? UMaterialInstanceDynamic::Create(HairBase, this) : nullptr;
	if (Hair)
	{
		Hair->SetVectorParameterValue(TEXT("Tint"), FLinearColor::LerpUsingHSV(FLinearColor(0.018f, 0.014f, 0.01f), FLinearColor(0.3f, 0.18f, 0.07f), Outfit.HairTone));
		Dynamics.Add(Hair);
	}
	AddFollower(LoadPath<USkeletalMesh>(CharRoot + BrowPath), Hair);
	if (HairPaths.IsValidIndex(Outfit.Hair) && !HairPaths[Outfit.Hair].IsEmpty())
	{
		AddFollower(LoadPath<USkeletalMesh>(CharRoot + HairPaths[Outfit.Hair]), Hair);
	}
}

void UPSHumanBodyComponent::SetActionBlendSpeed(float PerSecond)
{
	if (Anim)
	{
		Anim->ActionBlendSpeed = PerSecond;
	}
}

void UPSHumanBodyComponent::SetBend(float Bend01)
{
	if (Anim)
	{
		Anim->Bend = Bend01;
	}
}

void UPSHumanBodyComponent::SetLocomotion(float GroundSpeed, float Waddle01)
{
	if (Anim)
	{
		Anim->GroundSpeed = GroundSpeed;
		Anim->Waddle = Waddle01;
	}
}

void UPSHumanBodyComponent::PlayAction(const FString& ClipName, bool bLoop, float Rate, float HoldAt)
{
	if (Anim)
	{
		if (UAnimSequence* S = Clip(ClipName))
		{
			Anim->PlayAction(S, bLoop, Rate, HoldAt, UpperBodyActions.Contains(ClipName));
		}
	}
}

void UPSHumanBodyComponent::StopAction()
{
	if (Anim)
	{
		Anim->StopAction();
	}
}

void UPSHumanBodyComponent::SetIdleClip(const FString& ClipName)
{
	if (Anim)
	{
		if (UAnimSequence* S = Clip(ClipName))
		{
			Anim->IdleAnim = S;
		}
	}
}

UStaticMeshComponent* UPSHumanBodyComponent::AttachProp(UStaticMesh* Mesh, UMaterialInterface* Material, FName Bone, const FTransform& Offset)
{
	if (!Mesh || !Rig || !GetOwner())
	{
		return nullptr;
	}
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(GetOwner());
	C->SetStaticMesh(Mesh);
	if (Material)
	{
		C->SetMaterial(0, Material);
	}
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetupAttachment(Rig, Bone);
	C->SetRelativeTransform(Offset);
	C->ComponentTags.Add(TEXT("NoCollide")); // carried items (cap, stick, trouser bags) move with the body
	C->SetCastShadow(bCastShadow);
	C->RegisterComponent();
	GetOwner()->AddInstanceComponent(C);
	return C;
}

FVector UPSHumanBodyComponent::BoneLocation(FName Bone) const
{
	if (Rig && Rig->GetBoneIndex(Bone) != INDEX_NONE)
	{
		return Rig->GetBoneLocation(Bone);
	}
	return GetComponentLocation();
}
