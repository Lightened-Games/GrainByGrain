// Pocket Sand - inmate body.

#include "AI/PSInmate.h"
#include "Components/PSHumanBodyComponent.h"
#include "Components/CapsuleComponent.h"

APSInmate::APSInmate()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Body = CreateDefaultSubobject<UPSHumanBodyComponent>(TEXT("Body"));
	Body->SetupAttachment(RootComponent);
	Body->bSmoothSimSteps = true;
	// Solid body (user 2026-11-09: the hero walked through an NPC): blocks pawns, ignored by cameras, no navigation.
	Blocker = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Blocker"));
	Blocker->SetupAttachment(RootComponent);
	Blocker->InitCapsuleSize(30.0f, 88.0f);
	Blocker->SetRelativeLocation(FVector(0.0f, 0.0f, 90.0f));
	Blocker->SetCollisionProfileName(TEXT("Pawn"));
	Blocker->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Blocker->SetCanEverAffectNavigation(false);
	Blocker->SetHiddenInGame(true);
	Tags.Add(TEXT("PS.Body"));
	Tags.Add(TEXT("FactoryQA.Gait")); // QA only: FactoryQA probes tagged non-character walkers
	// Native Rocketbox mocap (no retarget), speeds measured by Tools/Editor/probe/probe_mocap.py.
	Body->IdleClip = TEXT("RBM_Idle");
	Body->WalkClip = TEXT("RBM_Walk");
	Body->WalkAnimSpeed = 101.0f;
	// No run clip: the crowd never goes faster than ~140 cm/s, and walk/run blends are what fold knees mid-stride.
}

void APSInmate::PS_PlayAction(const FString& Clip, bool bLoop, float Rate)
{
	if (Clip.IsEmpty())
	{
		Body->StopAction();
	}
	else
	{
		Body->PlayAction(Clip, bLoop, Rate);
	}
}

void APSInmate::PS_Dress(const UPSRoundConfig* Config, const FPSOutfit& Outfit)
{
	Body->Dress(Config, Outfit);
	ShownYaw = GetActorRotation().Yaw;
	WantYaw = ShownYaw;
}

void APSInmate::PS_SetPose(const FVector& Location, float Yaw, float Speed)
{
	SetActorLocation(Location);
	WantYaw = Yaw;
	Body->SetLocomotion(Speed, 0.0f);
}

void APSInmate::PS_SetIdleStyle(int32 Style)
{
	if (Style == IdleStyle)
	{
		return;
	}
	IdleStyle = Style;
	// Native mocap idles only: the retargeted UAL idles stand with the right foot turned out 43 deg (probe_idle_feet.py).
	Body->SetIdleClip(Style == 1 || Style == 2 ? TEXT("RBM_Idle_02") : TEXT("RBM_Idle"));
}

void APSInmate::PS_Cheer(bool bOn)
{
	if (bOn)
	{
		Body->PlayAction(TEXT("Dance_Loop"), true, 1.0f);
	}
	else
	{
		Body->StopAction();
	}
}

void APSInmate::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ShownYaw = FMath::FixedTurn(ShownYaw, WantYaw, TurnSpeed * 60.0f * DeltaSeconds);
	SetActorRotation(FRotator(0.0f, ShownYaw, 0.0f));
}
