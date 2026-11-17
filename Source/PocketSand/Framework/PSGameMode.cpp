// Pocket Sand - game mode.

#include "Framework/PSGameMode.h"
#include "Framework/PSEventSubsystem.h"
#include "Framework/PSPlayerController.h"
#include "Player/PSCharacter.h"
#include "AI/PSCrowdDirectorComponent.h"
#include "AI/PSGuard.h"
#include "AI/PSTowerGuard.h"
#include "AI/PSNav.h"
#include "Audio/PSSynth.h"
#include "Data/PSRoundConfig.h"
#include "World/PSSandGrid.h"
#include "World/PSYardBuilder.h"
#include "World/PSYardLife.h"
#include "Feedback/PSFeedback.h"
#include "World/PSVolleyballCourt.h"
#include "World/PSDoorStash.h"
#include "Components/PSHumanBodyComponent.h"
#include "FactoryKit/FactoryFlowSubsystem.h"
#include "PocketSand.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"

#define LOCTEXT_NAMESPACE "PocketSand"

APSGameMode::APSGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	PlayerControllerClass = APSPlayerController::StaticClass();
	DefaultPawnClass = APSCharacter::StaticClass();
	HUDClass = nullptr;
	Crowd = CreateDefaultSubobject<UPSCrowdDirectorComponent>(TEXT("Crowd"));
	ConfigAsset = TSoftObjectPtr<UPSRoundConfig>(FSoftObjectPath(TEXT("/Game/PocketSand/Data/DA_PS_Round.DA_PS_Round")));
	GridClass = APSSandGrid::StaticClass();
	BuilderClass = APSYardBuilder::StaticClass();
	LifeClass = APSYardLife::StaticClass();
	GuardClass = APSGuard::StaticClass();
	TowerClass = APSTowerGuard::StaticClass();
	CourtClass = APSVolleyballCourt::StaticClass();
	DoorClass = APSDoorStash::StaticClass();
}

void APSGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	LoadConfig();
}

void APSGameMode::LoadConfig()
{
	if (Config)
	{
		return;
	}
	Config = ConfigAsset.LoadSynchronous();
	if (!Config)
	{
		UE_LOG(LogPocketSand, Warning, TEXT("[PocketSand] DA_PS_Round missing - using built-in defaults."));
		Config = NewObject<UPSRoundConfig>(this, TEXT("DefaultRoundConfig"));
	}
}

void APSGameMode::ParseFlags()
{
	const TCHAR* Cmd = FCommandLine::Get();
	if (FParse::Param(Cmd, TEXT("autoplay")))
	{
		UE_LOG(LogPocketSand, Warning, TEXT("Autoplay mode requested via command line."));
		bAutoplayActive = true;
	}
	bAutoplayExit = FParse::Param(Cmd, TEXT("autoplayexit")) || FParse::Param(Cmd, TEXT("PSExit"));
	if (bAutoplayExit)
	{
		bAutoplayActive = true;
	}
	FString Defeat;
	if (FParse::Value(Cmd, TEXT("PSDefeat="), Defeat))
	{
		DefeatModeName = FName(*Defeat);
		bAutoplayActive = true;
	}
	else if (FParse::Param(Cmd, TEXT("PSDefeat")))
	{
		DefeatModeName = TEXT("found");
		bAutoplayActive = true;
	}
	bFixedRuns = FParse::Value(Cmd, TEXT("PSRuns="), Runs) || DefeatModeName != NAME_None;
	Runs = FMath::Max(1, Runs);
	FParse::Value(Cmd, TEXT("PSSeed="), BaseSeed);
	FParse::Value(Cmd, TEXT("PSSkill="), BotSkill);
	BotSkill = FMath::Clamp(BotSkill, 0.0f, 1.0f);
	FString Shots;
	if (FParse::Value(Cmd, TEXT("PSShots="), Shots, false))
	{
		TArray<FString> Parts;
		Shots.ParseIntoArray(Parts, TEXT(","));
		for (const FString& P : Parts)
		{
			ShotTimes.Add(FCString::Atof(*P));
		}
		ShotTimes.Sort();
	}
	bEventShots = FParse::Param(Cmd, TEXT("PSEventShots"));
	// Showcase (video, design guide 2.5.3): a shorter seeded round so the whole loop, every mechanic and the victory fit
	// in under 60 s from the start screen. Values come from the config.
	bShowcase = FParse::Param(Cmd, TEXT("PSShowcase"));
	if (bShowcase && Config)
	{
		bAutoplayActive = true;
		Config->BagsTotal = Config->ShowcaseBags;
		Config->RoundSeconds = Config->ShowcaseRoundSeconds;
		Config->SpikeInterval *= Config->ShowcasePhaseScale;
		if (!FParse::Param(Cmd, TEXT("PSSeedSet")) && !FString(Cmd).Contains(TEXT("PSSeed="))) { BaseSeed = Config->ShowcaseSeed; } // an explicit -PSSeed wins (seed sweeps)
		BotSkill = 1.0f;
	}
	FParse::Value(Cmd, TEXT("PSShotTag="), ShotTag);
	float TimeScale = 1.0f;
	if (FParse::Value(Cmd, TEXT("PSTimeScale="), TimeScale) && TimeScale > 0.0f)
	{
		GetWorldSettings()->MaxGlobalTimeDilation = FMath::Max(GetWorldSettings()->MaxGlobalTimeDilation, TimeScale);
		UGameplayStatics::SetGlobalTimeDilation(this, TimeScale);
	}
	if (FParse::Param(Cmd, TEXT("PSExportAudio")))
	{
		PSSynth::ExportAll(FPaths::ProjectDir() / TEXT("asset-library/Audio/Generated/PocketSand"));
	}
}

void APSGameMode::BeginPlay()
{
	Super::BeginPlay();
	LoadConfig();
	ParseFlags();
	if (AWorldSettings* WS = GetWorldSettings())
	{
		WS->KillZ = Config->KillZ;
	}
	if (GEngine)
	{
		GEngine->bEnableOnScreenDebugMessages = false;
	}
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		PC->ConsoleCommand(TEXT("DisableAllScreenMessages"));
	}
	Phases = Config->GetPhases();
	if (bShowcase)
	{
		for (FPSPhaseRow& Ph : Phases)
		{
			Ph.StartSeconds *= Config->ShowcasePhaseScale;
		}
	}
	SpawnWorld();
	PSFeedback::Prewarm(this);
	if (UPSEventSubsystem* Bus = UPSEventSubsystem::Get(this))
	{
		Bus->OnBagEmptied.AddDynamic(this, &APSGameMode::HandleBagEmptied);
		Bus->OnBagLoaded.AddDynamic(this, &APSGameMode::HandleBagLoaded);
		Bus->OnSuspicion.AddDynamic(this, &APSGameMode::HandleSuspicion);
		Bus->OnRegionFill.AddDynamic(this, &APSGameMode::HandleRegionFill);
		Bus->OnPatchNoticed.AddDynamic(this, &APSGameMode::HandlePatchNoticed);
		Bus->OnPatchMissed.AddDynamic(this, &APSGameMode::HandlePatchMissed);
		Bus->OnWatch.AddDynamic(this, &APSGameMode::HandleWatch);
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("PSExportMeshes")) && Builder)
	{
		Builder->ExportMeshes(FPaths::ProjectDir() / TEXT("asset-library/Models/Generated/PocketSand"));
	}
	EnterMenu();
}

void APSGameMode::EnterMenu()
{
	if (Grid)
	{
		Grid->ResetSand();
	}
	Crowd->ResetCrowd(BaseSeed);
	if (VolleyCourt)
	{
		VolleyCourt->ResetCourt();
	}
	if (IPSSandCarrier* C = Carrier())
	{
		C->PS_ResetCarrier(FVector(Config->PlayerStart, 92.0f), Config->PlayerStartYaw);
	}
	EnterState(EPSMatchState::Menu);
}

void APSGameMode::HandleFlowStart()
{
	UE_LOG(LogPocketSand, Log, TEXT("[PocketSand] Flow start: countdown."));
	StartMatch();
}

void APSGameMode::AmbientStep(float DeltaSeconds)
{
	const float Step = 1.0f / FMath::Max(Config->SimHz, 10.0f);
	Accumulator += FMath::Min(DeltaSeconds, 0.25f);
	while (Accumulator >= Step)
	{
		Accumulator -= Step;
		TArray<FVector2D> Others;
		if (const APawn* H = Hero()) { Others.Add(FVector2D(H->GetActorLocation())); Crowd->HeroPos = Others[0]; }
		Crowd->bMenuClear = State == EPSMatchState::Menu;
		Crowd->Step(Step, 1.0f, Others);
		Crowd->bMenuClear = false;
		if (VolleyCourt) { VolleyCourt->Step(Step); }
	}
}

AActor* APSGameMode::EndingFocus(EPSDefeat Reason) const
{
	APawn* H = Hero();
	if (Reason == EPSDefeat::Whistle)
	{
		return TowerGuard ? static_cast<AActor*>(TowerGuard.Get()) : H;
	}
	if (Reason == EPSDefeat::Found || Reason == EPSDefeat::Caught)
	{
		// The ground guard nearest the find (found) or the hero (caught): the one who did it.
		const FVector From = Reason == EPSDefeat::Found ? LastFindLocation : (H ? H->GetActorLocation() : FVector::ZeroVector);
		AActor* Best = nullptr;
		float BestD = BIG_NUMBER;
		for (const TWeakObjectPtr<APSGuard>& G : Guards)
		{
			if (G.IsValid())
			{
				const float D = FVector::DistSquared2D(G->GetActorLocation(), From);
				if (D < BestD)
				{
					BestD = D;
					Best = G.Get();
				}
			}
		}
		return Best ? Best : H;
	}
	return H;
}

void APSGameMode::SpawnWorld()
{
	UWorld* W = GetWorld();
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Grid = W->SpawnActor<APSSandGrid>(GridClass, FTransform::Identity, P);
	if (Grid)
	{
		Grid->Init(Config);
	}
	Builder = W->SpawnActor<APSYardBuilder>(BuilderClass, FTransform::Identity, P);
	if (Builder)
	{
		Builder->Build(Config, Grid);
	}
	Life = LifeClass ? W->SpawnActor<APSYardLife>(LifeClass, FTransform::Identity, P) : nullptr;
	if (Life)
	{
		Life->Build(Config);
	}
	DoorStash = W->SpawnActor<APSDoorStash>(DoorClass, FTransform::Identity, P);
	if (DoorStash)
	{
		DoorStash->Setup(Config);
	}
	// Volleyball players removed (user 2026-11-10: hands never met the ball). The court stays as a hiding surface.
	VolleyCourt = Config->bVolleyballPlayers ? W->SpawnActor<APSVolleyballCourt>(CourtClass, FTransform::Identity, P) : nullptr;
	if (VolleyCourt)
	{
		VolleyCourt->Setup(Config);
		VolleyCourt->OnSpikeImpact.AddUObject(this, &APSGameMode::HandleSpikeImpact);
	}
	Crowd->Init(Config, Grid);
	// Guard A walks the west half with his stick; guard B watches the court, then patrols the east from Last Call.
	for (int32 I = 0; I < 2; ++I)
	{
		APSGuard* G = W->SpawnActor<APSGuard>(GuardClass, FTransform::Identity, P);
		if (!G)
		{
			continue;
		}
		if (I == 0)
		{
			G->Setup(Config, Config->GuardARoute.Points, false, FVector2D::ZeroVector, 0.0f, true);
		}
		else
		{
			G->Setup(Config, Config->GuardBRoute.Points, true, Config->GuardBPost, Config->GuardBPostYaw, false);
		}
		Guards.Add(G);
		Watchers.Add(G);
	}
	// Extra posted guards (gate, cell door): they watch their spot and fidget, and answer finds like the others.
	for (const FVector& Post : Config->GuardPosts)
	{
		if (APSGuard* G = W->SpawnActor<APSGuard>(GuardClass, FTransform::Identity, P))
		{
			G->Setup(Config, { FVector2D(Post) }, true, FVector2D(Post), Post.Z, false);
			Guards.Add(G);
			Watchers.Add(G);
		}
	}
	TowerGuard = W->SpawnActor<APSTowerGuard>(TowerClass, FTransform::Identity, P);
	if (TowerGuard)
	{
		TowerGuard->Setup(Config);
		Watchers.Add(TowerGuard);
	}
}

APawn* APSGameMode::Hero() const
{
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	return PC ? PC->GetPawn() : nullptr;
}

IPSSandCarrier* APSGameMode::Carrier() const
{
	return Cast<IPSSandCarrier>(Hero());
}

void APSGameMode::StartMatch()
{
	if (UFactoryFlowSubsystem* Flow = UFactoryFlowSubsystem::Get(this))
	{
		if (Flow->GetState() == EFactoryFlowState::Ending)
		{
			Flow->ResumePlay(0.6f); // in-place restart: back to the chase camera and the legend
		}
	}
	const int32 Seed = BaseSeed + RunIndex * 7919;
	Stats = FPSStats();
	ProbeZones.Reset();
	PromptsFired.Reset();
	Elapsed = 0.0f;
	Accumulator = 0.0f;
	SampleClock = 0.0f;
	PhaseIndex = -1;
	BagsInCell = Config->BagsTotal;
	BagsGone = 0;
	NextSpike = 1e9f;
	SpikeWarnLeft = 0.0f;
	bNearMissCounted = false;
	CountdownLeft = Config->CountdownSeconds;
	LastCountdownShown = -1;
	if (Grid)
	{
		Grid->ResetSand();
	}
	Crowd->ResetCrowd(Seed);
	for (const TWeakObjectPtr<AActor>& W : Watchers)
	{
		if (IPSWatcher* Watcher = Cast<IPSWatcher>(W.Get()))
		{
			Watcher->PS_ResetWatcher();
		}
	}
	if (VolleyCourt)
	{
		VolleyCourt->ResetCourt();
	}
	if (IPSSandCarrier* C = Carrier())
	{
		C->PS_ResetCarrier(FVector(Config->PlayerStart, 92.0f), Config->PlayerStartYaw);
	}
	UE_LOG(LogPocketSand, Log, TEXT("[PocketSand] Match %d/%d start, seed %d, skill %.2f%s"), RunIndex + 1, Runs, Seed, BotSkill, DefeatModeName != NAME_None ? *FString::Printf(TEXT(", defeat mode %s"), *DefeatModeName.ToString()) : TEXT(""));
	EnterState(EPSMatchState::Ready);
}

void APSGameMode::EnterState(EPSMatchState NewState, EPSDefeat Reason)
{
	State = NewState;
	DefeatReason = Reason;
	const bool bLock = NewState != EPSMatchState::Active;
	if (IPSSandCarrier* C = Carrier())
	{
		C->PS_LockMovement(bLock);
	}
	if (UPSEventSubsystem* Bus = UPSEventSubsystem::Get(this))
	{
		Bus->OnStateChanged.Broadcast(State, Reason);
	}
	if (NewState == EPSMatchState::Victory || NewState == EPSMatchState::Defeat)
	{
		EndTimer = 0.0f;
		for (const TWeakObjectPtr<AActor>& W : Watchers)
		{
			if (IPSWatcher* Watcher = Cast<IPSWatcher>(W.Get()))
			{
				Watcher->PS_Freeze(true);
			}
		}
		if (NewState == EPSMatchState::Victory)
		{
			++Victories;
		}
		if (const IPSSandCarrier* C = Carrier())
		{
			Stats.PeakSuspicion = FMath::Max(Stats.PeakSuspicion, C->PS_Suspicion());
		}
		Stats.Seconds = Elapsed;
		const TCHAR* ReasonText = Reason == EPSDefeat::Found ? TEXT("found") : Reason == EPSDefeat::Caught ? TEXT("caught") : Reason == EPSDefeat::Whistle ? TEXT("whistle") : TEXT("-");
		UE_LOG(LogPocketSand, Warning, TEXT("Match ended: %s  time %.1fs  bags gone %d/%d  strikes %d  reason %s  peak suspicion %.2f  trips %d  spikes %d  near misses %d  run %d/%d  avg fps %.0f"),
			NewState == EPSMatchState::Victory ? TEXT("VICTORY") : TEXT("DEFEAT"), Elapsed, BagsGone, Config->BagsTotal, Stats.Strikes, ReasonText, Stats.PeakSuspicion,
			Stats.Trips, Stats.Spikes, Stats.NearMisses, RunIndex + 1, Runs, FrameTime > 0.0f ? FrameCount / FrameTime : 0.0f);
		WriteTelemetry();
		QueueEventShot(NewState == EPSMatchState::Victory ? FName(TEXT("Victory")) : FName(TEXT("Defeat")), EndingShotDelay);
		if (UFactoryFlowSubsystem* Flow = UFactoryFlowSubsystem::Get(this))
		{
			Flow->PlayEnding(NewState == EPSMatchState::Victory, EndingFocus(Reason));
		}
	}
	if (NewState == EPSMatchState::Active)
	{
		PS_Prompt(TEXT("Load"), LOCTEXT("PromptLoad", "E  STUFF A BAG DOWN YOUR TROUSERS"), DoorStash ? DoorStash->GetActorLocation() : FVector(Config->DoorLocation, 0.0f), false);
	}
}

float APSGameMode::PS_TimeLeft() const
{
	return FMath::Max(0.0f, Config->RoundSeconds - Elapsed);
}

bool APSGameMode::PS_TakeBagFromCell()
{
	if (BagsInCell <= 0 || State != EPSMatchState::Active)
	{
		return false;
	}
	--BagsInCell;
	return true;
}

void APSGameMode::PS_RequestRestart()
{
	if (State == EPSMatchState::Ready)
	{
		return;
	}
	UE_LOG(LogPocketSand, Log, TEXT("[PocketSand] Restart requested."));
	++RunIndex;
	Runs = FMath::Max(Runs, RunIndex + 1);
	StartMatch();
}

int32 APSGameMode::PS_ReportFind(const FVector& Location, bool bProbe)
{
	if (State != EPSMatchState::Active)
	{
		return Stats.Strikes;
	}
	++Stats.Strikes;
	LastFindLocation = Location;
	// Every find makes that part of the yard more dangerous: the stick guard will poke around it.
	bool bMerged = false;
	for (FPSProbeZone& Z : ProbeZones)
	{
		if (FVector2D::Distance(Z.Center, FVector2D(Location)) < Config->ProbeRadius)
		{
			bMerged = true;
			Z.LastPoke = Elapsed;
		}
	}
	if (!bMerged)
	{
		FPSProbeZone Z;
		Z.Center = FVector2D(Location);
		Z.Radius = Config->ProbeRadius;
		Z.LastPoke = Elapsed;
		ProbeZones.Add(Z);
	}
	UE_LOG(LogPocketSand, Log, TEXT("[PocketSand] Patch found (%s) at (%.0f, %.0f), t=%.1f: strike %d/%d"), bProbe ? TEXT("stick") : TEXT("eyes"), Location.X, Location.Y, Elapsed, Stats.Strikes, Config->StrikesToLose);
	if (UPSEventSubsystem* Bus = UPSEventSubsystem::Get(this))
	{
		Bus->OnPatchFound.Broadcast(Stats.Strikes, Location, bProbe);
	}
	QueueEventShot(TEXT("Found"));
	if (Stats.Strikes >= Config->StrikesToLose)
	{
		EnterState(EPSMatchState::Defeat, EPSDefeat::Found);
	}
	else
	{
		PS_Prompt(TEXT("Probe"), LOCTEXT("PromptProbe", "HE'LL POKE AROUND HERE NOW"), Location, true);
	}
	return Stats.Strikes;
}

void APSGameMode::PS_RequestInvestigate(const FVector& Location)
{
	APSGuard* Best = nullptr;
	float BestD = BIG_NUMBER;
	for (const TWeakObjectPtr<APSGuard>& G : Guards)
	{
		if (G.IsValid() && !G->PS_IsBusy())
		{
			const float D = FVector2D::DistSquared(G->Pos2D(), FVector2D(Location));
			if (D < BestD)
			{
				BestD = D;
				Best = G.Get();
			}
		}
	}
	if (Best)
	{
		UE_LOG(LogPocketSand, Log, TEXT("[PocketSand] Tower spotted yellow at (%.0f, %.0f): sending a guard."), Location.X, Location.Y);
		Best->PS_Investigate(Location);
	}
}

void APSGameMode::PS_Prompt(FName Id, const FText& Text, const FVector& Where, bool bDanger)
{
	if (PromptsFired.Contains(Id))
	{
		return;
	}
	PromptsFired.Add(Id);
	UE_LOG(LogPocketSand, Log, TEXT("[PocketSand] Prompt %s at t=%.1f"), *Id.ToString(), Elapsed);
	if (UPSEventSubsystem* Bus = UPSEventSubsystem::Get(this))
	{
		Bus->OnPrompt.Broadcast(Text, Where, bDanger);
	}
	QueueEventShot(FName(*(TEXT("Prompt_") + Id.ToString())));
}

void APSGameMode::PS_TouchProbeZone(int32 Index, float Time)
{
	if (ProbeZones.IsValidIndex(Index))
	{
		ProbeZones[Index].LastPoke = Time;
	}
}

void APSGameMode::Notice(const FText& Title, const FText& Sub, bool bDanger)
{
	if (UPSEventSubsystem* Bus = UPSEventSubsystem::Get(this))
	{
		Bus->OnNotice.Broadcast(Title, Sub, bDanger);
	}
}

void APSGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	RealTime += DeltaSeconds;
	++FrameCount;
	FrameTime += FApp::GetDeltaTime();
	TickScreenshots();

	if (State == EPSMatchState::Menu)
	{
		if (!bFlowBegun)
		{
			bFlowBegun = true;
			if (UFactoryFlowSubsystem* Flow = UFactoryFlowSubsystem::Get(this))
			{
				Flow->OnStartGame.AddUniqueDynamic(this, &APSGameMode::HandleFlowStart);
				Flow->Begin(bAutoplayActive);
			}
			else
			{
				StartMatch();
			}
		}
		AmbientStep(DeltaSeconds);
		return;
	}

	if (State == EPSMatchState::Ready)
	{
		CountdownLeft -= DeltaSeconds;
		const int32 Shown = FMath::CeilToInt(FMath::Max(CountdownLeft, 0.0f));
		if (Shown != LastCountdownShown)
		{
			LastCountdownShown = Shown;
			if (UPSEventSubsystem* Bus = UPSEventSubsystem::Get(this))
			{
				Bus->OnCountdown.Broadcast(Shown);
			}
		}
		// The yard is alive during the countdown; only the hero waits.
		AmbientStep(DeltaSeconds);
		if (CountdownLeft <= 0.0f)
		{
			EnterState(EPSMatchState::Active);
		}
		return;
	}

	if (State == EPSMatchState::Victory || State == EPSMatchState::Defeat)
	{
		EndTimer += DeltaSeconds;
		if (EndTimer >= Config->EndHoldSeconds && bAutoplayActive)
		{
			const bool bWon = State == EPSMatchState::Victory;
			EndTimer = -1000.0f;
			if (RunIndex + 1 < Runs)
			{
				++RunIndex;
				StartMatch();
			}
			else if (!bWon && !bFixedRuns)
			{
				// Autoplay keeps playing until it wins: replay with the next seed.
				++RunIndex;
				++Runs;
				StartMatch();
			}
			else if (bAutoplayExit)
			{
				UE_LOG(LogPocketSand, Warning, TEXT("[PocketSand] Autoplay finished: %d/%d victories. Exiting."), Victories, Runs);
				FPlatformMisc::RequestExit(false, TEXT("PocketSandAutoplay"));
			}
			else
			{
				++RunIndex;
				++Runs;
				StartMatch();
			}
		}
		return;
	}

	const float Step = 1.0f / FMath::Max(Config->SimHz, 10.0f);
	Accumulator += FMath::Min(DeltaSeconds, 0.25f);
	while (Accumulator >= Step && State == EPSMatchState::Active)
	{
		Accumulator -= Step;
		SimStep(Step);
	}
}

void APSGameMode::UpdatePhase()
{
	int32 Want = 0;
	for (int32 I = 0; I < Phases.Num(); ++I)
	{
		if (Elapsed >= Phases[I].StartSeconds)
		{
			Want = I;
		}
	}
	if (Want == PhaseIndex || !Phases.IsValidIndex(Want))
	{
		return;
	}
	PhaseIndex = Want;
	const FPSPhaseRow& P = Phases[PhaseIndex];
	for (int32 I = 0; I < Guards.Num(); ++I)
	{
		if (Guards[I].IsValid() && I == 1)
		{
			Guards[I]->SetPatrolling(P.bSecondGuardPatrols);
		}
	}
	if (P.bSpikes && NextSpike > 1e8f)
	{
		NextSpike = Elapsed + Config->SpikeTelegraph;
	}
	if (PhaseIndex > 0)
	{
		if (UPSEventSubsystem* Bus = UPSEventSubsystem::Get(this))
		{
			Bus->OnPhase.Broadcast(PhaseIndex, P.Title, P.Sub);
		}
		QueueEventShot(FName(*FString::Printf(TEXT("Phase%d"), PhaseIndex)));
	}
	UE_LOG(LogPocketSand, Log, TEXT("[PocketSand] Phase %d (%s) at t=%.1f"), PhaseIndex, *P.Title.ToString(), Elapsed);
}

void APSGameMode::SimStep(float Dt)
{
	Elapsed += Dt;
	UpdatePhase();
	const FPSPhaseRow Phase = Phases.IsValidIndex(PhaseIndex) ? Phases[PhaseIndex] : FPSPhaseRow();

	TArray<FVector2D> Others;
	if (const APawn* H = Hero())
	{
		Others.Add(FVector2D(H->GetActorLocation()));
	}
	for (const TWeakObjectPtr<APSGuard>& G : Guards)
	{
		if (G.IsValid())
		{
			Others.Add(G->Pos2D());
		}
	}
	Crowd->Step(Dt, Phase.CrowdSpeedMul, Others);
	for (const TWeakObjectPtr<APSGuard>& G : Guards)
	{
		if (G.IsValid())
		{
			G->Step(Dt, Phase.GuardSpeedMul, Elapsed);
		}
	}
	if (TowerGuard)
	{
		TowerGuard->Step(Dt, Phase.TowerSweepSeconds);
	}
	if (VolleyCourt)
	{
		VolleyCourt->Step(Dt);
		if (Phase.bSpikes && Elapsed >= NextSpike - Config->SpikeTelegraph && !VolleyCourt->IsSpiking() && NextSpike < 1e8f)
		{
			VolleyCourt->TriggerSpike();
			SpikeWarnLeft = Config->SpikeTelegraph + 0.5f;
			NextSpike += Config->SpikeInterval;
		}
	}
	SpikeWarnLeft = FMath::Max(0.0f, SpikeWarnLeft - Dt);
	if (Grid)
	{
		Grid->Step(Dt);
	}
	if (UPSEventSubsystem* Bus = UPSEventSubsystem::Get(this))
	{
		Bus->OnSimStep.Broadcast(Dt);
	}
	if (State != EPSMatchState::Active)
	{
		return;
	}
	CheckPrompts();
	static const bool bFootLog = FParse::Param(FCommandLine::Get(), TEXT("PSFootLog"));
	FootLogClock += Dt;
	if (bFootLog)
	{
		SampleFootSlide(Dt);
	}
	if (bFootLog && FootLogClock >= 2.0f)
	{
		FootLogClock = 0.0f;
		LogFeet();
	}

	// Telemetry and near-misses.
	if (const IPSSandCarrier* C = Carrier())
	{
		const float S = C->PS_Suspicion();
		Stats.PeakSuspicion = FMath::Max(Stats.PeakSuspicion, S);
		if (S >= NearMissSuspicion && !bNearMissCounted)
		{
			bNearMissCounted = true;
			++Stats.NearMisses;
			QueueEventShot(TEXT("NearMiss"));
		}
		else if (S < NearMissSuspicion * 0.5f)
		{
			bNearMissCounted = false;
		}
		if (!C->PS_IsPouring() && Hero() && Hero()->GetVelocity().Size2D() < 10.0f)
		{
			Stats.IdleSeconds += Dt;
		}
	}
	SampleClock += Dt;
	if (SampleClock >= TelemetrySampleSeconds)
	{
		SampleClock = 0.0f;
		Stats.SuspicionCurve.Add(Carrier() ? Carrier()->PS_Suspicion() : 0.0f);
	}

	if (Elapsed >= Config->RoundSeconds)
	{
		// The whistle: everyone inside. Whatever is still in the trousers or the cell is found at inspection.
		const int32 Left = BagsInCell + (Carrier() ? Carrier()->PS_Carried() : 0);
		if (Left > 0)
		{
			EnterState(EPSMatchState::Defeat, EPSDefeat::Whistle);
		}
		else
		{
			EnterState(EPSMatchState::Victory);
		}
	}
}

void APSGameMode::CheckPrompts()
{
	const APawn* H = Hero();
	const IPSSandCarrier* C = Carrier();
	if (!H || !C || !Grid)
	{
		return;
	}
	const FVector L = H->GetActorLocation();
	const bool bCrowd = Crowd->CountNear(L, Config->CrowdCoverRadius) >= Config->CrowdCoverCount;
	if (C->PS_IsPouring())
	{
		if (bCrowd)
		{
			PS_Prompt(TEXT("Crowd"), LOCTEXT("PromptCrowd", "THEIR FEET TRAMPLE IT IN"), L, false);
		}
		else if (!Grid->Field().WouldHide(FVector2D(L)))
		{
			PS_Prompt(TEXT("Shows"), LOCTEXT("PromptShows", "YELLOW SHOWS ON GREY GRAVEL"), L, true);
		}
		else
		{
			PS_Prompt(TEXT("Hides"), LOCTEXT("PromptHides", "IT SINKS IN HERE  UNTIL IT'S FULL"), L, false);
		}
	}
	if (C->PS_IsWaddling())
	{
		PS_Prompt(TEXT("Waddle"), LOCTEXT("PromptWaddle", "TWO BAGS  YOU WADDLE  GUARDS STARE"), L, true);
	}
}

void APSGameMode::HandleBagEmptied(int32 Count, FVector Location)
{
	++BagsGone;
	QueueEventShot(TEXT("PourDone"), 0.05f); // QA: the crouched pour (-PSEventShots)
	UE_LOG(LogPocketSand, Log, TEXT("[PocketSand] Bag %d/%d gone at t=%.1f (%.0f, %.0f)"), BagsGone, Config->BagsTotal, Elapsed, Location.X, Location.Y);
	if (Grid)
	{
		const int32 R = Grid->Field().RegionAt(FVector2D(Location));
		const FName Key = R >= 0 ? Grid->Field().Regions[R].Def.Id : FName(TEXT("Gravel"));
		Stats.PouredBySurface.FindOrAdd(Key) += 1.0f;
	}
	if (BagsGone >= Config->BagsTotal && State == EPSMatchState::Active)
	{
		EnterState(EPSMatchState::Victory);
	}
}

void APSGameMode::HandleBagLoaded(int32 Count, FVector Location)
{
	if (Count == 1)
	{
		++Stats.Trips;
	}
	else
	{
		QueueEventShot(TEXT("Load2"), 0.5f); // QA: crouched at the door, facing it (-PSEventShots)
	}
	PS_Prompt(TEXT("Pour"), LOCTEXT("PromptPour", "HOLD SPACE  TRICKLE IT OUT AS YOU WALK"), Location, false);
}

void APSGameMode::HandleSuspicion(float Value, float Delta)
{
	if (Value >= 1.0f && State == EPSMatchState::Active)
	{
		UE_LOG(LogPocketSand, Log, TEXT("[PocketSand] Caught in the act at t=%.1f"), Elapsed);
		EnterState(EPSMatchState::Defeat, EPSDefeat::Caught);
	}
}

void APSGameMode::HandleRegionFill(FName RegionId, float Fraction, FVector Location)
{
	if (State != EPSMatchState::Active || !Grid)
	{
		return;
	}
	for (const FPSSandField::FRegion& R : Grid->Field().Regions)
	{
		if (R.Def.Id == RegionId && Fraction >= R.Row.FullTintStart && Fraction < 1.0f)
		{
			PS_Prompt(TEXT("Full"), LOCTEXT("PromptFull", "NEARLY FULL  THEN IT TURNS YELLOW"), Location, true);
		}
	}
}

void APSGameMode::HandlePatchNoticed(FVector Location, AActor* Who)
{
	QueueEventShot(TEXT("Noticed"));
	PS_Prompt(TEXT("Noticed"), LOCTEXT("PromptNoticed", "HE SAW YELLOW  WALK A CROWD OVER IT"), Location, true);
}

void APSGameMode::HandlePatchMissed(FVector Location, AActor* Who)
{
	UE_LOG(LogPocketSand, Log, TEXT("[PocketSand] Guard checked (%.0f, %.0f): nothing there any more."), Location.X, Location.Y);
	++Stats.NearMisses;
}

void APSGameMode::HandleWatch(FVector Location, AActor* Who)
{
	QueueEventShot(TEXT("Watch"));
}

void APSGameMode::HandleSpikeImpact(const FVector& Location)
{
	if (!Grid || State != EPSMatchState::Active)
	{
		return;
	}
	++Stats.Spikes;
	FPSSandField& F = Grid->Field();
	int32 CourtIndex = INDEX_NONE;
	for (int32 I = 0; I < F.Regions.Num(); ++I)
	{
		if (F.Regions[I].Def.Surface == EPSSurface::Court)
		{
			CourtIndex = I;
		}
	}
	float Scattered = 0.0f;
	FVector Landing = Location;
	if (CourtIndex != INDEX_NONE)
	{
		const FPSSandField::FRegion& R = F.Regions[CourtIndex];
		const float Loose = R.Absorbed - Config->SpikeLooseAbove;
		if (Loose > 0.0f)
		{
			// The puff flies toward the nearest guard: right at his feet.
			FVector2D Target = R.Def.Center + FVector2D(0.0f, -R.Def.HalfSize.Y - 300.0f);
			float BestD = BIG_NUMBER;
			for (const TWeakObjectPtr<APSGuard>& G : Guards)
			{
				if (G.IsValid())
				{
					const float D = FVector2D::DistSquared(G->Pos2D(), R.Def.Center);
					if (D < BestD)
					{
						BestD = D;
						Target = G->Pos2D();
					}
				}
			}
			FVector2D Dir = (Target - FVector2D(Location)).GetSafeNormal();
			if (Dir.IsNearlyZero())
			{
				Dir = FVector2D(0.0f, -1.0f);
			}
			// Exit point on the court edge plus the throw distance.
			FVector2D Edge = FVector2D(Location);
			for (int32 K = 0; K < 200 && R.Def.Contains(Edge); ++K)
			{
				Edge += Dir * 10.0f;
			}
			const float Throw = FMath::Clamp(FVector2D::Distance(Edge, Target), Config->SpikeThrowMin, Config->SpikeThrowMax);
			const FVector2D Land = PSNav::ClampYard(Config, Edge + Dir * Throw);
			const float Want = Loose * Config->SpikeScatterFraction;
			Scattered = F.TakeFromRegion(CourtIndex, Want);
			F.DepositShown(Land, Scattered, 110.0f);
			Landing = FVector(Land, 0.0f);
		}
	}
	UE_LOG(LogPocketSand, Log, TEXT("[PocketSand] Spike at t=%.1f scattered %.3f bags to (%.0f, %.0f)"), Elapsed, Scattered, Landing.X, Landing.Y);
	if (UPSEventSubsystem* Bus = UPSEventSubsystem::Get(this))
	{
		Bus->OnSpike.Broadcast(Landing, Scattered);
	}
	QueueEventShot(TEXT("Spike"));
	PS_Prompt(TEXT("Spike"), Scattered > 0.0f ? LOCTEXT("PromptSpike", "THE SPIKE THREW YOUR SAND OUT") : LOCTEXT("PromptSpikeSafe", "SPIKES THROW LOOSE COURT SAND"), Scattered > 0.0f ? Landing : Location, true);
}

void APSGameMode::WriteTelemetry()
{
	TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
	J->SetStringField(TEXT("result"), State == EPSMatchState::Victory ? TEXT("victory") : TEXT("defeat"));
	J->SetNumberField(TEXT("reason"), static_cast<int32>(DefeatReason));
	J->SetNumberField(TEXT("seconds"), Elapsed);
	J->SetNumberField(TEXT("bags_gone"), BagsGone);
	J->SetNumberField(TEXT("strikes"), Stats.Strikes);
	J->SetNumberField(TEXT("trips"), Stats.Trips);
	J->SetNumberField(TEXT("spikes"), Stats.Spikes);
	J->SetNumberField(TEXT("near_misses"), Stats.NearMisses);
	J->SetNumberField(TEXT("idle_seconds"), Stats.IdleSeconds);
	J->SetNumberField(TEXT("peak_suspicion"), Stats.PeakSuspicion);
	J->SetNumberField(TEXT("seed"), BaseSeed + RunIndex * 7919);
	J->SetNumberField(TEXT("skill"), BotSkill);
	if (Grid)
	{
		J->SetNumberField(TEXT("sand_shown"), Grid->Field().TotalShown());
		J->SetNumberField(TEXT("sand_hidden"), Grid->Field().TotalHidden());
		J->SetNumberField(TEXT("sand_removed"), Grid->Field().Removed);
		J->SetNumberField(TEXT("sand_poured"), Grid->Field().Poured);
	}
	TSharedRef<FJsonObject> Surf = MakeShared<FJsonObject>();
	for (const TPair<FName, float>& P : Stats.PouredBySurface)
	{
		Surf->SetNumberField(P.Key.ToString(), P.Value);
	}
	J->SetObjectField(TEXT("bags_finished_on"), Surf);
	TArray<TSharedPtr<FJsonValue>> Curve;
	for (float V : Stats.SuspicionCurve)
	{
		Curve.Add(MakeShared<FJsonValueNumber>(V));
	}
	J->SetArrayField(TEXT("suspicion_curve"), Curve);
	FString Out;
	const TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(J, W);
	FFileHelper::SaveStringToFile(Out, *(FPaths::ProjectSavedDir() / TEXT("Telemetry") / FString::Printf(TEXT("match_%d.json"), RunIndex + 1)));
}

void APSGameMode::SampleFootSlide(float Dt)
{
	// QA (-PSFootLog): horizontal speed of feet that stay planted (toe within 3 cm of the floor on consecutive steps).
	if (Dt <= 0.0f)
	{
		return;
	}
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		const UPSHumanBodyComponent* Body = It->FindComponentByClass<UPSHumanBodyComponent>();
		if (!Body || !Body->Driver())
		{
			continue;
		}
		for (int32 Side = 0; Side < 2; ++Side)
		{
			const FVector Toe = Body->BoneLocation(Body->ToeBone(Side == 0));
			const FName Key(*FString::Printf(TEXT("%s_%d"), *It->GetName(), Side));
			const bool bPlanted = Toe.Z < 3.0f;
			if (const FVector* Prev = LastToes.Find(Key))
			{
				if (bPlanted && Prev->Z < 3.0f)
				{
					SlideSum += FVector::Dist2D(*Prev, Toe) / Dt;
					++SlideSamples;
				}
			}
			LastToes.Add(Key, Toe);
		}
	}
}

void APSGameMode::LogFeet()
{
	UE_LOG(LogPocketSand, Log, TEXT("[FootLog] t=%.1f plantedFootSlide=%.1fcm/s over %d samples"), Elapsed, SlideSamples ? SlideSum / SlideSamples : 0.0f, SlideSamples);
	SlideSum = 0.0f;
	SlideSamples = 0;
	// QA (-PSFootLog): how high every character's lowest toe sits above the ground right under it.
	int32 N = 0;
	float Worst = 0.0f, Sum = 0.0f;
	FString WorstName;
	FCollisionQueryParams Q(TEXT("PSFootLog"), true);
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		const UPSHumanBodyComponent* Body = It->FindComponentByClass<UPSHumanBodyComponent>();
		if (!Body || !Body->Driver())
		{
			continue;
		}
		Q.AddIgnoredActor(*It);
		float Low = BIG_NUMBER;
		FVector At = FVector::ZeroVector;
		for (const FName Bone : { Body->ToeBone(true), Body->ToeBone(false) })
		{
			const FVector L = Body->BoneLocation(Bone);
			if (L.Z < Low)
			{
				Low = L.Z;
				At = L;
			}
		}
		FHitResult Hit;
		float Ground = 0.0f;
		if (GetWorld()->LineTraceSingleByChannel(Hit, At + FVector(0, 0, 60), At - FVector(0, 0, 300), ECC_Visibility, Q))
		{
			Ground = Hit.ImpactPoint.Z;
		}
		const float Gap = Low - Ground;
		if (N == 0)
		{
			const USkeletalMeshComponent* Rig = Body->Driver();
			FString Chain;
			for (int32 B = 0; B < FMath::Min(4, Rig->GetNumBones()); ++B)
			{
				const FName BN = Rig->GetBoneName(B);
				Chain += FString::Printf(TEXT(" %s=%.1f(local %s)"), *BN.ToString(), Rig->GetBoneLocation(BN).Z - Rig->GetComponentLocation().Z,
					*Rig->GetSocketTransform(BN, RTS_ParentBoneSpace).GetTranslation().ToString());
			}
			UE_LOG(LogPocketSand, Log, TEXT("[FootLog] %s rigZ=%.1f toe=%.1f ground=%.1f bones:%s"), *It->GetName(), Rig->GetComponentLocation().Z, Low, Ground, *Chain);
		}
		++N;
		Sum += Gap;
		if (FMath::Abs(Gap) > FMath::Abs(Worst))
		{
			Worst = Gap;
			WorstName = It->GetName();
		}
	}
	UE_LOG(LogPocketSand, Log, TEXT("[FootLog] t=%.1f characters=%d meanToeGap=%.1fcm worst=%.1fcm (%s)"), Elapsed, N, N ? Sum / N : 0.0f, Worst, *WorstName);
}

void APSGameMode::TakeScreenshot(const FString& Name)
{
	const FString Path = FPaths::ProjectSavedDir() / TEXT("Screenshots") / TEXT("PS") / (Name + ShotTag + TEXT(".png"));
	FScreenshotRequest::RequestScreenshot(Path, true, false);
	UE_LOG(LogPocketSand, Log, TEXT("[PocketSand] Screenshot %s"), *Path);
}

void APSGameMode::QueueEventShot(FName Id, float Delay)
{
	if (!bEventShots || EventShotsTaken.Contains(Id))
	{
		return;
	}
	EventShotsTaken.Add(Id);
	PendingEventShots.Add(TPair<float, FName>(RealTime + (Delay >= 0.0f ? Delay : EventShotDelay), Id));
}

void APSGameMode::TickScreenshots()
{
	while (ShotTimes.IsValidIndex(NextShot) && RealTime >= ShotTimes[NextShot])
	{
		TakeScreenshot(FString::Printf(TEXT("Shot_%04dds"), FMath::RoundToInt(ShotTimes[NextShot] * 10.0f)));
		++NextShot;
	}
	for (int32 I = PendingEventShots.Num() - 1; I >= 0; --I)
	{
		if (RealTime >= PendingEventShots[I].Key)
		{
			TakeScreenshot(FString::Printf(TEXT("Event_%s"), *PendingEventShots[I].Value.ToString()));
			PendingEventShots.RemoveAt(I);
		}
	}
}

#undef LOCTEXT_NAMESPACE
