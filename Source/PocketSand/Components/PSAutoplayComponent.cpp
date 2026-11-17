// Pocket Sand - autoplay bot.

#include "Components/PSAutoplayComponent.h"
#include "FactoryKit/FactoryFlowSubsystem.h"
#include "AI/PSCrowdDirectorComponent.h"
#include "AI/PSNav.h"
#include "Data/PSRoundConfig.h"
#include "Interfaces/PSInterfaces.h"
#include "World/PSSandGrid.h"
#include "PocketSand.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Misc/CommandLine.h"

UPSAutoplayComponent::UPSAutoplayComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

IPSMatchInfo* UPSAutoplayComponent::Match() const
{
	return GetWorld() ? Cast<IPSMatchInfo>(GetWorld()->GetAuthGameMode()) : nullptr;
}

bool UPSAutoplayComponent::SeenAt(IPSMatchInfo* M, const FVector2D& P, float Exposure) const
{
	for (const TWeakObjectPtr<AActor>& W : M->PS_Watchers())
	{
		if (const IPSWatcher* Watcher = Cast<IPSWatcher>(W.Get()))
		{
			if (Watcher->PS_CanSee(FVector(P, 90.0f), Exposure))
			{
				return true;
			}
		}
	}
	return false;
}

float UPSAutoplayComponent::SightThreat(IPSMatchInfo* M, const FVector2D& P, float Exposure) const
{
	// Seen now counts fully; a watcher close enough to turn and look counts a little.
	float Threat = 0.0f;
	for (const TWeakObjectPtr<AActor>& W : M->PS_Watchers())
	{
		if (const IPSWatcher* Watcher = Cast<IPSWatcher>(W.Get()))
		{
			if (Watcher->PS_CanSee(FVector(P, 90.0f), Exposure))
			{
				Threat += 1.0f;
			}
			else if (!Watcher->PS_IsTower() && FVector2D::Distance(FVector2D(Watcher->PS_EyeLocation()), P) < Watcher->PS_ViewRange() * 0.8f * Exposure)
			{
				Threat += 0.35f;
			}
		}
	}
	return Threat;
}

float UPSAutoplayComponent::RegionCap(IPSMatchInfo* M, const FPSRegionDef& R) const
{
	// Mid-game, keep court sand at the loose line (a spike throws loose sand at a guard); late, fill everything.
	const UPSRoundConfig* Cf = M ? M->PS_Config() : nullptr;
	if (R.Surface == EPSSurface::Court && Cf && M->PS_TimeLeft() >= LateGameSeconds)
	{
		return FMath::Min(R.Capacity, FMath::Max(Cf->SpikeLooseAbove, CourtMidGameCap));
	}
	return R.Capacity;
}

FVector2D UPSAutoplayComponent::StandPoint(const FPSRegionDef& R, const FVector2D& From) const
{
	if (R.bRound)
	{
		const float Rad = FMath::Max(R.HalfSize.X - RegionInset, 10.0f);
		const FVector2D D = From - R.Center;
		return R.Center + (D.IsNearlyZero() ? FVector2D::ZeroVector : D.GetSafeNormal() * FMath::Min(Rad, D.Size()));
	}
	const FVector2D In(FMath::Max(R.HalfSize.X - RegionInset, 5.0f), FMath::Max(R.HalfSize.Y - RegionInset, 5.0f));
	return FVector2D(FMath::Clamp(From.X, R.Center.X - In.X, R.Center.X + In.X), FMath::Clamp(From.Y, R.Center.Y - In.Y, R.Center.Y + In.Y));
}

FVector2D UPSAutoplayComponent::Steer(IPSMatchInfo* M, const FVector2D& Me, const FVector2D& To, float Dt)
{
	const UPSRoundConfig* Cfg = M->PS_Config();
	const FVector2D D = To - Me;
	// At low frame rates one frame's step can exceed the arrive radius, and the bot circles the target (glitch log: spin).
	const APlayerController* OwnerPC = Cast<APlayerController>(GetOwner());
	const APawn* Body = OwnerPC ? OwnerPC->GetPawn() : nullptr;
	const float FrameStep = Body ? Body->GetVelocity().Size2D() * Dt : 0.0f;
	if (D.Size() <= FMath::Max(ArriveRadius, FrameStep * ArriveStepFactor))
	{
		StuckClock = 0.0f;
		StuckFrom = Me;
		return FVector2D::ZeroVector;
	}
	FVector2D Dir = PSNav::Avoid(Cfg, Me, D.GetSafeNormal(), 40.0f, false, 200.0f);
	// Step around people (NPCs are solid now): push away from any body ahead within PeopleAvoidRadius.
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (!It->ActorHasTag(TEXT("PS.Body")))
		{
			continue;
		}
		const FVector2D O(It->GetActorLocation());
		const FVector2D ToO = O - Me;
		const float L = ToO.Size();
		if (L > 1.0f && L < PeopleAvoidRadius && FVector2D::DotProduct(ToO / L, Dir) > 0.2f && L < D.Size())
		{
			FVector2D Side(-ToO.Y / L, ToO.X / L);
			if (FVector2D::DotProduct(Side, Dir) < 0.0f) { Side = -Side; }
			Dir = (Dir + Side * (1.0f - L / PeopleAvoidRadius) * 1.6f).GetSafeNormal();
		}
	}
	// No zig-zag: the walking direction turns smoothly toward the steering direction.
	SmoothDir = SmoothDir.IsNearlyZero() ? Dir : (SmoothDir + (Dir - SmoothDir) * FMath::Min(1.0f, Dt * SteerSmoothing)).GetSafeNormal();
	Dir = SmoothDir;
	// Stuck recovery (P46): moved less than StuckDistance in StuckSeconds while trying -> sidestep, alternating sides.
	if (UnstickLeft > 0.0f)
	{
		UnstickLeft -= Dt;
		Dir = Dir.GetRotated(110.0f * UnstickSign);
	}
	else
	{
		StuckClock += Dt;
		if (StuckClock >= StuckSeconds)
		{
			if (FVector2D::Distance(Me, StuckFrom) < StuckDistance)
			{
				UnstickLeft = UnstickSeconds;
				UnstickSign = -UnstickSign;
			}
			StuckClock = 0.0f;
			StuckFrom = Me;
		}
	}
	return Dir;
}

void UPSAutoplayComponent::Decide(IPSMatchInfo* M, IPSSandCarrier* C, const FVector2D& Me)
{
	const UPSRoundConfig* Cfg = M->PS_Config();
	APSSandGrid* Grid = M->PS_Grid();
	const float Skill = M->PS_BotSkill();
	const int32 Carried = C->PS_Carried();

	// Find the door (any interactable).
	FVector2D Door = Cfg->DoorLocation - FVector2D(0.0f, 150.0f);
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (const IPSInteractable* I = Cast<IPSInteractable>(*It))
		{
			Door = FVector2D(I->PS_InteractLocation());
		}
	}

	if (Carried == 0 && M->PS_BagsInCell() <= 0)
	{
		Goal = EPSBotGoal::None;
		return;
	}
	if (Carried == 0 || (Goal == EPSBotGoal::Load && Carried < WantCarry && M->PS_BagsInCell() > 0))
	{
		if (Goal != EPSBotGoal::Load)
		{
			// Two bags halves the trips; a nervous (low skill) bot sometimes takes one.
			WantCarry = (M->PS_BagsInCell() >= 2 && (Skill > 0.4f || Rng.FRand() < 0.6f)) ? Cfg->MaxCarried : 1;
		}
		Goal = EPSBotGoal::Load;
		Target = Door;
		return;
	}

	// Score every hiding surface.
	float BestScore = -BIG_NUMBER;
	int32 BestRegion = INDEX_NONE;
	FVector2D BestPoint = Me;
	const float Speed = Cfg->RunSpeed * (C->PS_IsWaddling() ? Cfg->WaddleSpeedMul : 1.0f); // the bot runs the long legs
	const float SandOnMe = (Carried - 1) + C->PS_CurrentBag();
	if (Grid)
	{
		const FPSSandField& F = Grid->Field();
		for (int32 I = 0; I < F.Regions.Num(); ++I)
		{
			const FPSSandField::FRegion& R = F.Regions[I];
			const float Cap = RegionCap(M, R.Def);
			const float Free = Cap - R.Absorbed - CapacityMargin;
			if (Free <= 0.05f || AvoidUntil.FindRef(I) > M->PS_Elapsed())
			{
				continue;
			}
			const FVector2D P = StandPoint(R.Def, Me);
			bool bProbed = false;
			for (const FPSProbeZone& Z : M->PS_ProbeZones())
			{
				bProbed |= FVector2D::Distance(Z.Center, P) < Z.Radius + 60.0f;
			}
			if (bProbed)
			{
				continue;
			}
			const float Travel = FVector2D::Distance(Me, P) / FMath::Max(Speed, 1.0f);
			const float Threat = SightThreat(M, P, R.Row.ExposureMul);
			float Score = FMath::Min(Free, SandOnMe) * CapacityValue - Travel - Threat * SightPenaltySeconds;
			if (R.Def.Surface == EPSSurface::Court && M->PS_NextSpikeTime() - M->PS_Elapsed() < 4.0f && R.Absorbed > Cfg->SpikeLooseAbove * 0.8f)
			{
				Score -= 8.0f;
			}
			// Imperfect play: noise on every score.
			Score += Rng.FRandRange(-1.0f, 1.0f) * (1.0f - Skill) * 6.0f;
			// Hysteresis: stick with the current spot unless something clearly better appears.
			if (Goal == EPSBotGoal::Region && I == TargetRegion)
			{
				Score += StickBonus;
			}
			if (Score > BestScore)
			{
				BestScore = Score;
				BestRegion = I;
				BestPoint = P;
			}
		}
	}
	if (BestRegion != INDEX_NONE)
	{
		// Keep the same stand point while the region stays the choice (recomputing it from the moving hero drifted the target).
		const bool bSame = Goal == EPSBotGoal::Region && TargetRegion == BestRegion;
		Goal = EPSBotGoal::Region;
		TargetRegion = BestRegion;
		Target = bSame ? Target : BestPoint;
		return;
	}
	// Nothing left to hide in: follow the nearest crowd and trickle.
	if (UPSCrowdDirectorComponent* Crowd = M->PS_Crowd())
	{
		float BestD = BIG_NUMBER;
		for (int32 G = 0; G < Crowd->Groups().Num(); ++G)
		{
			const FVector2D P = Crowd->PredictGroup(G, 1.0f);
			const float D = FVector2D::DistSquared(P, Me) + (SeenAt(M, P, 1.0f) ? 4.0e6f : 0.0f);
			if (D < BestD)
			{
				BestD = D;
				TargetGroup = G;
				Target = P;
			}
		}
		Goal = EPSBotGoal::Crowd;
	}
}

void UPSAutoplayComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	IPSMatchInfo* M = Match();
	IPSPlayerInput* In = Cast<IPSPlayerInput>(GetOwner());
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	IPSSandCarrier* C = Cast<IPSSandCarrier>(Pawn);
	if (!M || !M->PS_IsAutoplay() || !In || !C || !M->PS_Config())
	{
		return;
	}
	const UPSRoundConfig* Cfg = M->PS_Config();
	// FactoryKit take-over: while a human drives, the bot stands down (and re-decides from scratch when it resumes).
	const UFactoryFlowSubsystem* Flow = UFactoryFlowSubsystem::Get(this);
	const bool bActive = M->PS_State() == EPSMatchState::Active && (!Flow || Flow->IsBotDriving());
	if (!bActive)
	{
		if (bWasActive)
		{
			In->PS_SetPour(false, true);
		}
		bWasActive = false;
		Goal = EPSBotGoal::None;
		AvoidUntil.Reset();
		ParkedClock = 0.0f;
		return;
	}
	if (!bWasActive)
	{
		bWasActive = true;
		Rng.Initialize(FMath::RoundToInt(M->PS_Elapsed() * 1000.0f) + 99);
		DecisionClock = 0.0f;
		bHoldPour = false;
	}
	const float Skill = M->PS_BotSkill();
	const FVector2D Me(Pawn->GetActorLocation());

	// QA defeat modes.
	FString Mode;
	FParse::Value(FCommandLine::Get(), TEXT("PSDefeat="), Mode);
	if (Mode.IsEmpty() && FParse::Param(FCommandLine::Get(), TEXT("PSDefeat")))
	{
		Mode = TEXT("found");
	}
	if (!Mode.IsEmpty())
	{
		if (Mode == TEXT("whistle"))
		{
			In->PS_SetMove(FVector2D::ZeroVector, true);
			return;
		}
		if (C->PS_Carried() == 0)
		{
			FVector2D Door = Cfg->DoorLocation - FVector2D(0.0f, 150.0f);
			In->PS_SetMove(Steer(M, Me, Door, DeltaTime), true);
			LoadClock -= DeltaTime;
			if (FVector2D::Distance(Me, Door) < Cfg->LoadRadius * 0.7f && LoadClock <= 0.0f)
			{
				In->PS_Load(true);
				LoadClock = Cfg->LoadSecondsPerBag + 0.1f;
			}
			In->PS_SetPour(false, true);
			return;
		}
		// found: walk into the open gravel in front of guard A and pour where only the trail is seen later.
		// caught: stand in a puddle under the tower (sand hides there, so nothing gets "found") and pour whenever the
		// tower's sweep is on the hero, until suspicion fills.
		// found: open gravel on guard A's patrol line at his first waypoint (west yard, away from every crowd stop, so the
		// sand is not trampled away before he walks over it).
		FVector2D Spot = Cfg->GuardARoute.Points.Num() > 0 ? Cfg->GuardARoute.Points[0] + FVector2D(150.0f, 120.0f) : Cfg->YardSize * 0.5f;
		if (Mode == TEXT("caught"))
		{
			for (const FPSRegionDef& R : Cfg->Regions)
			{
				if (R.Surface == EPSSurface::Puddle)
				{
					Spot = R.Center;
					break;
				}
			}
		}
		In->PS_SetMove(FVector2D::Distance(Me, Spot) < 40.0f ? FVector2D::ZeroVector : Steer(M, Me, Spot, DeltaTime), true);
		// caught pours only once it stands in the guard's face (pouring on the walk over empties the bags first, and
		// pouring at long range gets the sand found before suspicion fills).
		In->PS_SetPour(Mode == TEXT("caught") ? (FVector2D::Distance(Me, Spot) < CaughtPourRange && SeenAt(M, Me, 1.0f) && !C->PS_SandWouldShow()) : !SeenAt(M, Me, 1.0f), true);
		return;
	}

	// Decide (with skill-dependent reaction delay).
	DecisionClock -= DeltaTime;
	if (DecisionClock <= 0.0f)
	{
		DecisionClock = DecisionInterval * (1.0f + (1.0f - Skill)) + Rng.FRandRange(0.0f, MaxReactionDelay * (1.0f - Skill));
		Decide(M, C, Me);
	}
	if (Goal == EPSBotGoal::Crowd)
	{
		if (UPSCrowdDirectorComponent* Crowd = M->PS_Crowd())
		{
			if (Crowd->Groups().IsValidIndex(TargetGroup))
			{
				Target = Crowd->PredictGroup(TargetGroup, 0.6f);
			}
		}
	}

	// Move.
	FVector2D Move = Goal == EPSBotGoal::None ? FVector2D::ZeroVector : Steer(M, Me, Target, DeltaTime);
	if (Goal == EPSBotGoal::Load)
	{
		LoadClock -= DeltaTime;
		if (FVector2D::Distance(Me, Target) < Cfg->LoadRadius * 0.7f)
		{
			Move = FVector2D::ZeroVector;
			if (LoadClock <= 0.0f && !C->PS_IsLoading())
			{
				In->PS_Load(true);
				LoadClock = Cfg->LoadSecondsPerBag + 0.15f;
				if (C->PS_Carried() + 1 >= WantCarry || M->PS_BagsInCell() <= 1)
				{
					DecisionClock = FMath::Min(DecisionClock, Cfg->LoadSecondsPerBag + 0.2f);
				}
			}
		}
	}

	// Pour: only where it hides (a hiding surface with room, or inside a crowd) and only when nobody is looking.
	APSSandGrid* Grid = M->PS_Grid();
	float Exposure = 1.0f;
	bool bHides = false;
	if (Grid)
	{
		const int32 Idx = Grid->Field().IndexOf(Me);
		if (Idx != INDEX_NONE)
		{
			Exposure = Grid->Field().RowAt(Idx).ExposureMul;
		}
		bHides = !C->PS_SandWouldShow();
	}
	const bool bSeen = SeenAt(M, Me, Exposure);
	const float Susp = C->PS_Suspicion();
	if (Susp >= StopPourSuspicion)
	{
		bHoldPour = true;
	}
	else if (Susp <= ResumePourSuspicion)
	{
		bHoldPour = false;
	}
	bool bNearProbe = false;
	for (const FPSProbeZone& Z : M->PS_ProbeZones())
	{
		bNearProbe |= FVector2D::Distance(Z.Center, Me) < Z.Radius + 30.0f;
	}
	bool bRoom = true;
	if (Grid && Grid->Field().Regions.IsValidIndex(TargetRegion) && Goal == EPSBotGoal::Region && Grid->Field().RegionAt(Me) == TargetRegion)
	{
		const FPSSandField::FRegion& TR = Grid->Field().Regions[TargetRegion];
		// Same cap the region picker uses, or the bot can choose a spot it then refuses to pour on (seed 7 deadlock).
		bRoom = TR.Absorbed < RegionCap(M, TR.Def) - CapacityMargin * 0.5f;
		if (!bRoom)
		{
			DecisionClock = 0.0f;
		}
	}
	const bool bWant = C->PS_Carried() > 0 && bHides && bRoom && !bSeen && !bHoldPour && !bNearProbe && Goal != EPSBotGoal::Load;
	// Human-like hands: the pour key follows intent after a short delay.
	if (bWant != bPourIntent)
	{
		PourIntentClock += DeltaTime;
		if (PourIntentClock >= (1.0f - Skill) * 0.25f)
		{
			bPourIntent = bWant;
			PourIntentClock = 0.0f;
			In->PS_SetPour(bPourIntent, true);
		}
	}
	else
	{
		PourIntentClock = 0.0f;
	}
	// Standing on the target surface: stay put while pouring; if a watcher is looking, wait it out.
	if (Goal == EPSBotGoal::Region && (FVector2D::Distance(Me, Target) <= ArriveRadius || (bHides && FVector2D::Distance(Me, Target) <= ArriveRadius * 3.0f)))
	{
		Move = FVector2D::ZeroVector;
		if (Grid && Grid->Field().Regions.IsValidIndex(TargetRegion) && Grid->Field().Regions[TargetRegion].IsFull())
		{
			DecisionClock = 0.0f;
		}
		// Never wait it out forever (a guard parked on the court cost a whole match): give up on this spot for a while.
		ParkedClock = bWant ? 0.0f : ParkedClock + DeltaTime;
		if (ParkedClock > MaxParkSeconds && C->PS_Carried() > 0)
		{
			AvoidUntil.Add(TargetRegion, M->PS_Elapsed() + AvoidSeconds);
			ParkedClock = 0.0f;
			DecisionClock = 0.0f;
		}
	}
	else
	{
		ParkedClock = 0.0f;
	}
	// Run the long legs, walk the last stretch and near the door (reads like a person, not a sprinter).
	In->PS_SetRun(!Move.IsNearlyZero() && FVector2D::Distance(Me, Target) > Cfg->BotRunDistance, true);
	// Standing still on purpose (pouring, loading, waiting): not stuck. Without this the stuck timer ran while pouring and
	// the next walk started with a 110 deg sidestep (the bot set off the wrong way, then turned back).
	if (Move.IsNearlyZero())
	{
		StuckClock = 0.0f;
		UnstickLeft = 0.0f;
		StuckFrom = Me;
		SmoothDir = FVector2D::ZeroVector;
	}
	In->PS_SetMove(Move, true);
	static const bool bTrace = FParse::Param(FCommandLine::Get(), TEXT("PSBotLog"));
	TraceClock += DeltaTime;
	if (bTrace && TraceClock >= 1.0f)
	{
		TraceClock = 0.0f;
		UE_LOG(LogPocketSand, Log, TEXT("[Bot] t=%.1f goal=%d region=%d me=(%.0f,%.0f) target=(%.0f,%.0f) d=%.0f carried=%d bag=%.2f seen=%d hides=%d hold=%d pour=%d susp=%.2f move=(%.2f,%.2f)"),
			M->PS_Elapsed(), static_cast<int32>(Goal), TargetRegion, Me.X, Me.Y, Target.X, Target.Y, FVector2D::Distance(Me, Target), C->PS_Carried(), C->PS_CurrentBag(),
			bSeen ? 1 : 0, bHides ? 1 : 0, bHoldPour ? 1 : 0, C->PS_IsPouring() ? 1 : 0, Susp, Move.X, Move.Y);
	}
}
