// Pocket Sand - the match: Ready (3-2-1-GO) -> Active (90 s of yard time) -> Victory / Defeat -> R restart.
// Owns the fixed-step simulation (sand, crowd, guards, tower, court), the strikes and probe zones, the phases, the
// -autoplay / QA flags, screenshots and telemetry.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Interfaces/PSInterfaces.h"
#include "Data/PSTypes.h"
#include "PSGameMode.generated.h"

class UPSRoundConfig;
class APSSandGrid;
class APSYardBuilder;
class APSYardLife;
class APSGuard;
class APSTowerGuard;
class APSVolleyballCourt;
class APSDoorStash;
class UPSCrowdDirectorComponent;

UCLASS()
class POCKETSAND_API APSGameMode : public AGameModeBase, public IPSMatchInfo
{
	GENERATED_BODY()

public:
	APSGameMode();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PocketSand") TObjectPtr<UPSCrowdDirectorComponent> Crowd;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PocketSand") TSoftObjectPtr<UPSRoundConfig> ConfigAsset;
	UPROPERTY(EditAnywhere, Category = "PocketSand") TSubclassOf<APSSandGrid> GridClass;
	UPROPERTY(EditAnywhere, Category = "PocketSand") TSubclassOf<APSYardBuilder> BuilderClass;
	UPROPERTY(EditAnywhere, Category = "PocketSand") TSubclassOf<APSGuard> GuardClass;
	UPROPERTY(EditAnywhere, Category = "PocketSand") TSubclassOf<APSTowerGuard> TowerClass;
	UPROPERTY(EditAnywhere, Category = "PocketSand") TSubclassOf<APSVolleyballCourt> CourtClass;
	UPROPERTY(EditAnywhere, Category = "PocketSand") TSubclassOf<APSDoorStash> DoorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Autoplay") bool bAutoplayActive = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float EventShotDelay = 0.35f;
	/** QA: the victory/defeat event shot waits until the ending camera has settled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float EndingShotDelay = 2.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float TelemetrySampleSeconds = 10.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float NearMissSuspicion = 0.8f;

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// IPSMatchInfo
	virtual EPSMatchState PS_State() const override { return State; }
	virtual EPSDefeat PS_DefeatReason() const override { return DefeatReason; }
	virtual float PS_Elapsed() const override { return Elapsed; }
	virtual float PS_TimeLeft() const override;
	virtual const UPSRoundConfig* PS_Config() const override { return Config; }
	virtual APSSandGrid* PS_Grid() const override { return Grid; }
	virtual UPSCrowdDirectorComponent* PS_Crowd() const override { return Crowd; }
	virtual int32 PS_BagsInCell() const override { return BagsInCell; }
	virtual int32 PS_BagsGone() const override { return BagsGone; }
	virtual int32 PS_Strikes() const override { return Stats.Strikes; }
	virtual bool PS_IsAutoplay() const override { return bAutoplayActive; }
	virtual float PS_BotSkill() const override { return BotSkill; }
	virtual const FPSStats& PS_Stats() const override { return Stats; }
	virtual const TArray<FPSProbeZone>& PS_ProbeZones() const override { return ProbeZones; }
	virtual const TArray<TWeakObjectPtr<AActor>>& PS_Watchers() const override { return Watchers; }
	virtual int32 PS_PhaseIndex() const override { return PhaseIndex; }
	virtual float PS_NextSpikeTime() const override { return NextSpike; }
	virtual bool PS_TakeBagFromCell() override;
	virtual void PS_RequestRestart() override;
	virtual int32 PS_ReportFind(const FVector& Location, bool bProbe) override;
	virtual void PS_RequestInvestigate(const FVector& Location) override;
	virtual void PS_Prompt(FName Id, const FText& Text, const FVector& Where, bool bDanger) override;
	virtual void PS_TouchProbeZone(int32 Index, float Time) override;

	/** QA: the bot plays to lose (found | caught | whistle). */
	FName DefeatMode() const { return DefeatModeName; }
	const TArray<TWeakObjectPtr<APSGuard>>& GroundGuards() const { return Guards; }
	APSTowerGuard* Tower() const { return TowerGuard; }
	APSVolleyballCourt* Court() const { return VolleyCourt; }
	APSDoorStash* Door() const { return DoorStash; }
	bool IsSpikeWarning() const { return SpikeWarnLeft > 0.0f; }

private:
	UPROPERTY() TObjectPtr<UPSRoundConfig> Config;
	UPROPERTY() TObjectPtr<APSSandGrid> Grid;
	UPROPERTY() TObjectPtr<APSYardBuilder> Builder;
	UPROPERTY(EditAnywhere, Category = "PocketSand") TSubclassOf<APSYardLife> LifeClass;
	UPROPERTY() TObjectPtr<APSYardLife> Life;
	UPROPERTY() TObjectPtr<APSTowerGuard> TowerGuard;
	UPROPERTY() TObjectPtr<APSVolleyballCourt> VolleyCourt;
	UPROPERTY() TObjectPtr<APSDoorStash> DoorStash;
	TArray<TWeakObjectPtr<APSGuard>> Guards;
	TArray<TWeakObjectPtr<AActor>> Watchers;
	TArray<FPSProbeZone> ProbeZones;
	TArray<FPSPhaseRow> Phases;
	TSet<FName> PromptsFired;
	FPSStats Stats;

	EPSMatchState State = EPSMatchState::Ready;
	EPSDefeat DefeatReason = EPSDefeat::None;
	float Elapsed = 0.0f;
	float CountdownLeft = 3.0f;
	int32 LastCountdownShown = -1;
	float EndTimer = 0.0f;
	float Accumulator = 0.0f;
	float RealTime = 0.0f;
	float SampleClock = 0.0f;
	int32 PhaseIndex = -1;
	int32 BagsInCell = 12;
	int32 BagsGone = 0;
	float NextSpike = 1e9f;
	float SpikeWarnLeft = 0.0f;
	bool bNearMissCounted = false;
	float BotSkill = 1.0f;
	FName DefeatModeName = NAME_None;

	// QA
	bool bAutoplayExit = false;
	bool bFixedRuns = false;
	int32 Runs = 1;
	int32 RunIndex = 0;
	int32 Victories = 0;
	int32 BaseSeed = 7;
	TArray<float> ShotTimes;
	int32 NextShot = 0;
	bool bEventShots = false;
	TSet<FName> EventShotsTaken;
	TArray<TPair<float, FName>> PendingEventShots;
	FString ShotTag;
	int32 FrameCount = 0;
	float FrameTime = 0.0f;

	void LoadConfig();
	void ParseFlags();
	void SpawnWorld();
	void StartMatch();
	/** Start screen + intro (FactoryKit): the yard runs, the hero waits at the door, the HUD is hidden. */
	void EnterMenu();
	/** The yard lives without the match clock (menu, intro, countdown): crowd and volleyball step, the hero waits. */
	void AmbientStep(float DeltaSeconds);
	/** Who the ending camera looks at: the hero on victory, the guard or tower that ended it on defeat. */
	AActor* EndingFocus(EPSDefeat Reason) const;
	UFUNCTION() void HandleFlowStart();
	bool bFlowBegun = false;
	FVector LastFindLocation = FVector::ZeroVector;
	bool bShowcase = false;
	void EnterState(EPSMatchState NewState, EPSDefeat Reason = EPSDefeat::None);
	void SimStep(float Dt);
	void UpdatePhase();
	void CheckPrompts();
	void Notice(const FText& Title, const FText& Sub, bool bDanger);
	APawn* Hero() const;
	class IPSSandCarrier* Carrier() const;
	void WriteTelemetry();
	void TakeScreenshot(const FString& Name);
	void QueueEventShot(FName Id, float Delay = -1.0f);
	void TickScreenshots();
	void LogFeet();
	void SampleFootSlide(float Dt);
	TMap<FName, FVector> LastToes;
	float SlideSum = 0.0f;
	int32 SlideSamples = 0;
	float FootLogClock = 0.0f;

	void HandleSpikeImpact(const FVector& Location);
	UFUNCTION() void HandleBagEmptied(int32 Count, FVector Location);
	UFUNCTION() void HandleBagLoaded(int32 Count, FVector Location);
	UFUNCTION() void HandleSuspicion(float Value, float Delta);
	UFUNCTION() void HandleRegionFill(FName RegionId, float Fraction, FVector Location);
	UFUNCTION() void HandlePatchNoticed(FVector Location, AActor* Who);
	UFUNCTION() void HandlePatchMissed(FVector Location, AActor* Who);
	UFUNCTION() void HandleWatch(FVector Location, AActor* Who);
};
