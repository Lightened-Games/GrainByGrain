// Pocket Sand - yard guard.

#include "AI/PSGuard.h"
#include "Kismet/GameplayStatics.h"
#include "AI/PSNav.h"
#include "Components/PSHumanBodyComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PSVisionComponent.h"
#include "Data/PSRoundConfig.h"
#include "Framework/PSEventSubsystem.h"
#include "World/PSArt.h"
#include "World/PSSandGrid.h"
#include "PocketSand.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/GameModeBase.h"

#define LOCTEXT_NAMESPACE "PocketSand"

APSGuard::APSGuard()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Body = CreateDefaultSubobject<UPSHumanBodyComponent>(TEXT("Body"));
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
	Body->SetupAttachment(RootComponent);
	Body->bSmoothSimSteps = true;
	Tags.Add(TEXT("FactoryQA.Gait")); // QA only: FactoryQA probes tagged non-character walkers
	// Native Rocketbox mocap (no retarget): patrol walk, slow run when sent to investigate (probe_mocap.py speeds).
	Body->WalkClip = TEXT("RBM_Walk");
	Body->IdleClip = TEXT("RBM_Idle");
	Body->WalkAnimSpeed = 101.0f;
	Body->RunClip = TEXT("RBM_Run_Slow");
	Body->RunAnimSpeed = 241.0f;
	Vision = CreateDefaultSubobject<UPSVisionComponent>(TEXT("Vision"));
	Vision->SetupAttachment(RootComponent);
}

IPSMatchInfo* APSGuard::Match() const
{
	return GetWorld() ? Cast<IPSMatchInfo>(GetWorld()->GetAuthGameMode()) : nullptr;
}

void APSGuard::Setup(const UPSRoundConfig* Config, const TArray<FVector2D>& Route, bool bStartAtPost, const FVector2D& Post, float PostYaw, bool bInProbes)
{
	Cfg = Config;
	RoutePts = Route;
	bStartPost = bStartAtPost;
	PostPos = Post;
	PostYawDeg = PostYaw;
	bProbes = bInProbes;
	if (!Cfg)
	{
		return;
	}
	// Navy uniform, long sleeves, black boots; a cap and (stick guard) a baton-length stick.
	FPSOutfit O;
	O.Top = FLinearColor(0.035f, 0.05f, 0.09f);
	O.Bottom = FLinearColor(0.02f, 0.022f, 0.028f);
	O.Shoes = FLinearColor(0.01f, 0.01f, 0.01f);
	O.SkinTone = bProbes ? 0.15f : 0.7f;
	O.Hair = 0;
	O.HairTone = 0.1f;
	O.SleeveEnd = 80.0f;
	O.Avatar = Cfg->GuardAvatar;
	Body->Dress(Cfg, O);
	if (!Body->IsRocketbox())
	{
		// The Rocketbox officer has his own helmet; the base character gets a cap.
		Body->AttachProp(PSArt::CylinderMesh(), PSArt::Mat(EPSMat::Cap), Body->HeadBone(), CapOffset);
		Body->AttachProp(PSArt::CylinderMesh(), PSArt::Mat(EPSMat::Cap), Body->HeadBone(), BrimOffset);
	}
	if (bProbes)
	{
		if (UStaticMesh* Baton = Cast<UStaticMesh>(BatonMesh.TryLoad()))
		{
			Body->AttachProp(Baton, nullptr, Body->RightHandBone(), BatonOffset);
		}
		else
		{
			Body->AttachProp(PSArt::CylinderMesh(), PSArt::Mat(EPSMat::Stick), Body->RightHandBone(), StickOffset);
		}
	}
	Vision->InitCone(Cfg->GuardSightRange, Cfg->GuardSightHalfAngle);
	ResetGuard();
}

float APSGuard::EaseTurn(float From, float To, float MaxStep) const
{
	// Ease in and out of turns (no constant-rate robot turns): MaxStep (deg this frame) for big turns, slowing as the
	// angle closes, never below MinTurnRate.
	const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;
	const float Left = FMath::Abs(FMath::FindDeltaAngleDegrees(From, To));
	return FMath::FixedTurn(From, To, FMath::Clamp(Left * TurnEase * Dt, FMath::Min(MinTurnRate * Dt, MaxStep), MaxStep));
}

void APSGuard::ResetGuard()
{
	bPatrolling = !bStartPost;
	RouteIndex = 0;
	Pos = bStartPost ? PostPos : (RoutePts.Num() > 0 ? RoutePts[0] : FVector2D(GetActorLocation()));
	BodyYaw = bStartPost ? PostYawDeg : 0.0f;
	if (!bStartPost && RoutePts.Num() > 1)
	{
		const FVector2D D = RoutePts[1] - RoutePts[0];
		BodyYaw = FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X));
		RouteIndex = 1;
	}
	LookClock = 0.0f;
	StateClock = 0.0f;
	WatchCool = 0.0f;
	ProbeIndex = INDEX_NONE;
	WatchTarget.Reset();
	bAlerted = false;
	Vision->ResetScan();
	Body->StopAction();
	Enter(bStartPost ? EPSGuardState::Post : EPSGuardState::Patrol);
	ApplyPose(0.0f, 0.0f);
}

void APSGuard::SetPatrolling(bool bOn)
{
	if (bOn == bPatrolling)
	{
		return;
	}
	bPatrolling = bOn;
	if (bOn && State == EPSGuardState::Post)
	{
		// Join the route at its nearest point.
		float Best = BIG_NUMBER;
		for (int32 I = 0; I < RoutePts.Num(); ++I)
		{
			const float D = FVector2D::DistSquared(RoutePts[I], Pos);
			if (D < Best)
			{
				Best = D;
				RouteIndex = I;
			}
		}
		Enter(EPSGuardState::Patrol);
	}
}

void APSGuard::Enter(EPSGuardState NewState)
{
	State = NewState;
	StateClock = 0.0f;
	switch (NewState)
	{
	case EPSGuardState::Kneel:
		Body->PlayAction(TEXT("Fixing_Kneeling"), true, 1.0f);
		break;
	case EPSGuardState::Poke:
		Body->PlayAction(TEXT("Sword_Attack"), false, 0.8f);
		break;
	case EPSGuardState::Frozen:
		break;
	default:
		Body->StopAction();
		break;
	}
}

bool APSGuard::MoveTowards(const FVector2D& Target, float Speed, float Dt, bool bRegions)
{
	const FVector2D To = Target - Pos;
	const float L = To.Size();
	if (L <= ArriveRadius)
	{
		return true;
	}
	const FVector2D Dir = PSNav::Avoid(Cfg, Pos, To / L, AgentRadius, bRegions, 180.0f);
	Pos = PSNav::PushOut(Cfg, Pos + Dir * FMath::Min(Speed * Dt, L), AgentRadius, bRegions, 30.0f);
	// Never walk into the hero (user 2026-11-14: "characters are still going into objects"): keep a body's width apart.
	if (const APawn* Hero = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		const FVector2D D = Pos - FVector2D(Hero->GetActorLocation());
		const float Dist = D.Size();
		if (Dist < HeroClearance && Dist > 1.0f)
		{
			Pos = PSNav::PushOut(Cfg, FVector2D(Hero->GetActorLocation()) + D / Dist * HeroClearance, AgentRadius, bRegions, 30.0f);
		}
	}
	const float WantYaw = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
	BodyYaw = EaseTurn(BodyYaw, WantYaw, TurnSpeed * Dt);
	return false;
}

int32 APSGuard::PickProbe(float Now) const
{
	const IPSMatchInfo* M = Match();
	if (!bProbes || !M || !Cfg)
	{
		return INDEX_NONE;
	}
	const TArray<FPSProbeZone>& Zones = M->PS_ProbeZones();
	int32 Best = INDEX_NONE;
	float BestD = Cfg->ProbeDetour * Cfg->ProbeDetour;
	for (int32 I = 0; I < Zones.Num(); ++I)
	{
		if (Now - Zones[I].LastPoke < Cfg->ProbeCooldown)
		{
			continue;
		}
		const float D = FVector2D::DistSquared(Zones[I].Center, Pos);
		if (D < BestD)
		{
			BestD = D;
			Best = I;
		}
	}
	return Best;
}

void APSGuard::Step(float Dt, float SpeedMul, float Now)
{
	if (!Cfg)
	{
		return;
	}
	LastNow = Now;
	IPSMatchInfo* M = Match();
	StateClock += Dt;
	LookClock += Dt;
	WatchCool = FMath::Max(0.0f, WatchCool - Dt);
	float Speed = 0.0f;
	float GazeOffset = 0.0f;
	const float Walk = Cfg->GuardWalkSpeed * SpeedMul;

	switch (State)
	{
	case EPSGuardState::Patrol:
	{
		if (RoutePts.Num() == 0)
		{
			break;
		}
		if (MoveTowards(RoutePts[RouteIndex], Walk, Dt, true))
		{
			RouteIndex = (RouteIndex + 1) % RoutePts.Num();
		}
		Speed = Walk;
		GazeOffset = FMath::Sin(LookClock * UE_TWO_PI / FMath::Max(Cfg->GuardLookPeriod, 0.5f)) * Cfg->GuardLookSweep;
		const int32 P = PickProbe(Now);
		if (P != INDEX_NONE)
		{
			ProbeIndex = P;
			Goal = M->PS_ProbeZones()[P].Center;
			Enter(EPSGuardState::Probe);
		}
		break;
	}
	case EPSGuardState::Post:
		BodyYaw = EaseTurn(BodyYaw, PostYawDeg, TurnSpeed * Dt);
		GazeOffset = FMath::Sin(LookClock * UE_TWO_PI / FMath::Max(Cfg->GuardLookPeriod * 1.3f, 0.5f)) * Cfg->GuardLookSweep * 1.2f;
		FidgetClock -= Dt;
		if (FidgetClock <= 0.0f)
		{
			static const TCHAR* Fidgets[] = { TEXT("RBM_DocCheck"), TEXT("RBM_LookAround_02"), TEXT("RBM_ScratchHead_02") };
			Body->PlayAction(Fidgets[FidgetIndex++ % UE_ARRAY_COUNT(Fidgets)], false, 1.0f);
			FidgetClock = 9.0f + FMath::Fmod(Now * 7.3f + PostPos.X * 0.01f, 6.0f);
		}
		if (bPatrolling)
		{
			SetPatrolling(true);
		}
		break;
	case EPSGuardState::Watch:
	{
		if (AActor* T = WatchTarget.Get())
		{
			const FVector2D D = FVector2D(T->GetActorLocation()) - Pos;
			BodyYaw = EaseTurn(BodyYaw, FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X)), TurnSpeed * 1.5f * Dt);
		}
		if (StateClock >= Cfg->WatchSeconds)
		{
			WatchCool = Cfg->WatchCooldown;
			Enter(ResumeState);
		}
		break;
	}
	case EPSGuardState::Investigate:
		Speed = Cfg->GuardInvestigateSpeed * SpeedMul;
		if (MoveTowards(Goal, Speed, Dt, false))
		{
			Enter(EPSGuardState::Kneel);
		}
		break;
	case EPSGuardState::Kneel:
	{
		const FVector2D D = Goal - Pos;
		if (D.SizeSquared() > 100.0f)
		{
			BodyYaw = EaseTurn(BodyYaw, FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X)), TurnSpeed * Dt);
		}
		if (StateClock >= Cfg->KneelSeconds)
		{
			FinishKneel();
		}
		break;
	}
	case EPSGuardState::Probe:
		Speed = Walk;
		if (MoveTowards(Goal, Walk, Dt, false))
		{
			Enter(EPSGuardState::Poke);
		}
		break;
	case EPSGuardState::Poke:
		if (StateClock >= Cfg->PokeSeconds)
		{
			FinishPoke();
		}
		break;
	case EPSGuardState::Frozen:
	default:
		break;
	}

	// Gaze: glances while walking; locked on target while watching, investigating or kneeling.
	const bool bGlance = State == EPSGuardState::Patrol || State == EPSGuardState::Post;
	Vision->SetGaze(FVector(Pos, Vision->EyeHeight), BodyYaw + (bGlance ? GazeOffset : 0.0f));
	ScanClock += Dt;
	if ((State == EPSGuardState::Patrol || State == EPSGuardState::Post || State == EPSGuardState::Watch || State == EPSGuardState::Probe) && ScanClock >= ScanInterval)
	{
		const float ScanDt = ScanClock;
		ScanClock = 0.0f;
		FVector Spot;
		if (M && Vision->ScanPatch(M->PS_Grid(), Cfg->PatchNoticeMass, Cfg->PatchNoticeSeconds, ScanDt, Spot))
		{
			PS_Investigate(Spot);
		}
	}
	const float Alert = State == EPSGuardState::Investigate || State == EPSGuardState::Kneel || State == EPSGuardState::Watch ? 1.0f : Vision->StareFraction(Cfg->PatchNoticeSeconds);
	Vision->SetConeLook(Alert, State != EPSGuardState::Frozen);
	ApplyPose(Speed, Dt);
}

void APSGuard::FinishKneel()
{
	IPSMatchInfo* M = Match();
	APSSandGrid* Grid = M ? M->PS_Grid() : nullptr;
	const FVector2D G(Goal);
	const float Mass = Grid ? Grid->Field().ShownMass(G, Cfg->PatchRadius * 1.4f) : 0.0f;
	UPSEventSubsystem* Bus = UPSEventSubsystem::Get(this);
	if (Grid && Mass >= Cfg->PatchNoticeMass * Cfg->PatchConfirmFraction)
	{
		Grid->Field().Clear(G, Cfg->ClearRadius, false);
		M->PS_ReportFind(FVector(G, 0.0f), false);
	}
	else if (Bus)
	{
		// The crowd got there first: he shrugs and walks on.
		Bus->OnPatchMissed.Broadcast(FVector(G, 0.0f), this);
	}
	Enter(bPatrolling ? EPSGuardState::Patrol : EPSGuardState::Post);
}

void APSGuard::FinishPoke()
{
	IPSMatchInfo* M = Match();
	APSSandGrid* Grid = M ? M->PS_Grid() : nullptr;
	if (M && Grid && M->PS_ProbeZones().IsValidIndex(ProbeIndex))
	{
		const FPSProbeZone Zone = M->PS_ProbeZones()[ProbeIndex];
		M->PS_TouchProbeZone(ProbeIndex, LastNow);
		if (UPSEventSubsystem* Bus = UPSEventSubsystem::Get(this))
		{
			Bus->OnPoke.Broadcast(FVector(Pos, 0.0f), this);
		}
		if (Grid->Field().AnyMass(Zone.Center, Zone.Radius) >= Cfg->ProbeFindMass)
		{
			Grid->Field().Clear(Zone.Center, Zone.Radius, true);
			M->PS_ReportFind(FVector(Zone.Center, 0.0f), true);
		}
	}
	ProbeIndex = INDEX_NONE;
	Enter(bPatrolling ? EPSGuardState::Patrol : EPSGuardState::Post);
}

void APSGuard::ApplyPose(float Speed, float Dt)
{
	const float Z = 0.0f;
	SetActorLocationAndRotation(FVector(Pos, Z), FRotator(0.0f, BodyYaw, 0.0f));
	Body->SetLocomotion(Speed, 0.0f);
}

FVector2D APSGuard::PredictPos(float Seconds) const
{
	if (State != EPSGuardState::Patrol || RoutePts.Num() == 0 || !Cfg)
	{
		return Pos;
	}
	float Left = Cfg->GuardWalkSpeed * Seconds;
	FVector2D P = Pos;
	int32 Idx = RouteIndex;
	for (int32 Guard = 0; Guard < 16 && Left > 0.0f; ++Guard)
	{
		const FVector2D To = RoutePts[Idx] - P;
		const float L = To.Size();
		if (L >= Left)
		{
			return P + To / FMath::Max(L, 1.0f) * Left;
		}
		Left -= L;
		P = RoutePts[Idx];
		Idx = (Idx + 1) % RoutePts.Num();
	}
	return P;
}

FVector APSGuard::PS_EyeLocation() const { return Vision->Eye(); }
FVector APSGuard::PS_ViewDir() const { return Vision->Dir(); }
float APSGuard::PS_ViewRange() const { return Vision->Range; }
float APSGuard::PS_ViewHalfAngle() const { return Vision->HalfAngle; }

bool APSGuard::PS_CanSee(const FVector& Point, float ExposureMul) const
{
	return State != EPSGuardState::Frozen && State != EPSGuardState::Kneel && Vision->CanSee(Point, ExposureMul);
}

float APSGuard::PS_PourSeenGain() const
{
	return Cfg ? Cfg->PourSeenGain : 0.5f;
}

void APSGuard::PS_Watch(AActor* Target)
{
	if (State != EPSGuardState::Patrol && State != EPSGuardState::Post)
	{
		return;
	}
	if (WatchCool > 0.0f || !Target)
	{
		return;
	}
	ResumeState = State;
	WatchTarget = Target;
	Enter(EPSGuardState::Watch);
	if (UPSEventSubsystem* Bus = UPSEventSubsystem::Get(this))
	{
		Bus->OnWatch.Broadcast(FVector(Pos, 0.0f), this);
	}
}

bool APSGuard::PS_IsBusy() const
{
	return State == EPSGuardState::Investigate || State == EPSGuardState::Kneel || State == EPSGuardState::Poke || State == EPSGuardState::Probe || State == EPSGuardState::Frozen;
}

void APSGuard::PS_Investigate(const FVector& Location)
{
	if (PS_IsBusy())
	{
		return;
	}
	Goal = PSNav::PushOut(Cfg, FVector2D(Location), AgentRadius, false);
	Enter(EPSGuardState::Investigate);
	if (UPSEventSubsystem* Bus = UPSEventSubsystem::Get(this))
	{
		Bus->OnPatchNoticed.Broadcast(FVector(Goal, 0.0f), this);
	}
}

void APSGuard::PS_Freeze(bool bFrozen)
{
	if (bFrozen)
	{
		Enter(EPSGuardState::Frozen);
		Body->SetLocomotion(0.0f, 0.0f);
	}
	else if (State == EPSGuardState::Frozen)
	{
		Enter(bPatrolling ? EPSGuardState::Patrol : EPSGuardState::Post);
	}
}

#undef LOCTEXT_NAMESPACE
