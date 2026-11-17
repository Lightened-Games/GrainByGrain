// Pocket Sand - tower guard.

#include "AI/PSTowerGuard.h"
#include "Components/PSHumanBodyComponent.h"
#include "Components/PSVisionComponent.h"
#include "Components/SpotLightComponent.h"
#include "Data/PSRoundConfig.h"
#include "World/PSSandGrid.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"

APSTowerGuard::APSTowerGuard()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Tags.Add(TEXT("PS.Tower")); // FactoryKit menu shot anchor
	Body = CreateDefaultSubobject<UPSHumanBodyComponent>(TEXT("Body"));
	Body->SetupAttachment(RootComponent);
	Vision = CreateDefaultSubobject<UPSVisionComponent>(TEXT("Vision"));
	Vision->SetupAttachment(RootComponent);
	Vision->ConeOpacity = 0.2f;
	Beam = CreateDefaultSubobject<USpotLightComponent>(TEXT("Beam"));
	Beam->SetupAttachment(RootComponent);
	Beam->SetUsingAbsoluteRotation(true);
	Beam->SetInnerConeAngle(4.0f);
	Beam->SetOuterConeAngle(9.0f);
	Beam->SetAttenuationRadius(3500.0f);
	Beam->SetCastShadows(false);
	Beam->SetLightColor(FLinearColor(1.0f, 0.93f, 0.8f));
	Beam->bUseInverseSquaredFalloff = false;
	Beam->SetLightFalloffExponent(2.0f);
}

void APSTowerGuard::Setup(const UPSRoundConfig* Config)
{
	Cfg = Config;
	if (!Cfg)
	{
		return;
	}
	SetActorLocation(FVector(FVector2D(Cfg->TowerLocation), Cfg->TowerHeight));
	FPSOutfit O;
	O.Top = FLinearColor(0.035f, 0.05f, 0.09f);
	O.Bottom = FLinearColor(0.02f, 0.022f, 0.028f);
	O.Shoes = FLinearColor(0.01f, 0.01f, 0.01f);
	O.SkinTone = 0.45f;
	O.SleeveEnd = 80.0f;
	O.Avatar = Cfg->GuardAvatar;
	Body->WalkClip = TEXT("RBM_Walk");
	Body->IdleClip = TEXT("RBM_Idle");
	Body->WalkAnimSpeed = 101.0f;
	Body->Dress(Cfg, O);
	Body->SetIdleClip(TEXT("RBM_Idle")); // native mocap (the retargeted torch idle turned the right foot out 43 deg)
	Vision->InitCone(Cfg->TowerRange, Cfg->TowerHalfAngle);
	Beam->SetRelativeLocation(FVector(0.0f, 0.0f, 150.0f));
	Beam->SetIntensity(BeamIntensity);
	ResetTower();
}

void APSTowerGuard::ResetTower()
{
	Clock = 0.0f;
	Phase = 0.0f;
	TrackLeft = 0.0f;
	WatchCool = 0.0f;
	ReportCool = 0.0f;
	Track.Reset();
	bFrozenState = false;
	Yaw = Cfg ? Cfg->TowerYawMin : 0.0f;
	Vision->ResetScan();
}

float APSTowerGuard::SweepYawAt(float Time, float SweepSeconds) const
{
	if (!Cfg)
	{
		return 0.0f;
	}
	// Smooth ping-pong between the two limits (Time is the accumulated sweep phase in seconds).
	const float T = 0.5f - 0.5f * FMath::Cos(UE_TWO_PI * Time / FMath::Max(SweepSeconds, 1.0f));
	return FMath::Lerp(Cfg->TowerYawMin, Cfg->TowerYawMax, T);
}

void APSTowerGuard::Step(float Dt, float SweepSeconds)
{
	if (!Cfg || bFrozenState)
	{
		return;
	}
	Clock += Dt;
	WatchCool = FMath::Max(0.0f, WatchCool - Dt);
	ReportCool = FMath::Max(0.0f, ReportCool - Dt);
	const FVector Base(FVector2D(Cfg->TowerLocation), Cfg->TowerHeight);
	if (TrackLeft > 0.0f && Track.IsValid())
	{
		TrackLeft -= Dt;
		const FVector2D D = FVector2D(Track->GetActorLocation()) - FVector2D(Base);
		Yaw = FMath::FixedTurn(Yaw, FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X)), TrackSpeed * Dt);
		if (TrackLeft <= 0.0f)
		{
			WatchCool = Cfg->WatchCooldown;
		}
	}
	else
	{
		Phase += Dt;
		Yaw = SweepYawAt(Phase, SweepSeconds);
	}
	Vision->SetGaze(Base, Yaw);
	SetActorRotation(FRotator(0.0f, Yaw, 0.0f));
	// The beam points at the ground at about two thirds of the cone's reach.
	const FVector Aim = FVector(FVector2D(Base) + FVector2D(FRotator(0.0f, Yaw, 0.0f).Vector()) * Cfg->TowerRange * 0.62f, 0.0f);
	Beam->SetWorldRotation((Aim - (Base + FVector(0.0f, 0.0f, 150.0f))).Rotation());
	Vision->SetConeLook(TrackLeft > 0.0f ? 1.0f : 0.0f, true);

	ScanClock += Dt;
	if (ScanClock >= ScanInterval)
	{
		const float S = ScanClock;
		ScanClock = 0.0f;
		IPSMatchInfo* M = GetWorld() ? Cast<IPSMatchInfo>(GetWorld()->GetAuthGameMode()) : nullptr;
		FVector Spot;
		if (M && ReportCool <= 0.0f && Vision->ScanPatch(M->PS_Grid(), Cfg->PatchNoticeMass, Cfg->PatchNoticeSeconds, S, Spot))
		{
			ReportCool = ReportCooldown;
			M->PS_RequestInvestigate(Spot);
		}
	}
}

FVector APSTowerGuard::PS_EyeLocation() const { return Vision->Eye(); }
FVector APSTowerGuard::PS_ViewDir() const { return Vision->Dir(); }
float APSTowerGuard::PS_ViewRange() const { return Vision->Range; }
float APSTowerGuard::PS_ViewHalfAngle() const { return Vision->HalfAngle; }

bool APSTowerGuard::PS_CanSee(const FVector& Point, float ExposureMul) const
{
	return !bFrozenState && Vision->CanSee(Point, ExposureMul);
}

float APSTowerGuard::PS_PourSeenGain() const
{
	return Cfg ? Cfg->TowerPourSeenGain : 0.9f;
}

void APSTowerGuard::PS_Watch(AActor* Target)
{
	if (WatchCool > 0.0f || TrackLeft > 0.0f || !Target || !Cfg)
	{
		return;
	}
	Track = Target;
	TrackLeft = Cfg->WatchSeconds;
}
