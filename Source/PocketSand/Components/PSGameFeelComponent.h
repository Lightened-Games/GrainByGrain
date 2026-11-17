// Pocket Sand - juice: every gameplay event gets a sound, most get dust or a shake; a find gets the red flash (HUD),
// a shake and a 50 ms hit-stop. Listens to the event bus only.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/PSTypes.h"
#include "PSGameFeelComponent.generated.h"

class UAudioComponent;

UCLASS(ClassGroup = (PocketSand), meta = (BlueprintSpawnableComponent))
class POCKETSAND_API UPSGameFeelComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPSGameFeelComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float HitStopSeconds = 0.05f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float HitStopDilation = 0.05f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float PourVolume = 0.55f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float YardVolume = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float StepVolume = 0.15f; // footsteps were too loud (user 2026-11-10)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float HeartbeatAt = 0.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float HeartbeatInterval = 0.75f;
	/** Music (synthesised): the slow theme on the start screen, the caper while playing, silence for the end stingers. Provisional (C8). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Music") float MenuMusicVolume = 0.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Music") float PlayMusicVolume = 0.3f;
	/** Fraction the play music keeps when suspicion is high, so the heartbeat reads. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Music") float MusicDuck = 0.45f;
	/** Fade rate (per second) between music states. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Music") float MusicFadeRate = 2.5f;

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void HitStop();
	void Shake(bool bBig, float Scale = 1.0f);

private:
	UPROPERTY() TObjectPtr<UAudioComponent> PourLoop;
	UPROPERTY() TObjectPtr<UAudioComponent> YardLoop;
	UPROPERTY() TObjectPtr<UAudioComponent> MenuMusic;
	UPROPERTY() TObjectPtr<UAudioComponent> PlayMusic;
	EPSMatchState MusicState = EPSMatchState::Menu;
	float MenuLevel = 0.0f;
	float PlayLevel = 0.0f;
	void UpdateMusic(float RealDt);
	double HitStopUntil = 0.0;
	float AppliedDilation = 1.0f;
	float BaseDilation = 1.0f;
	float PourLevel = 0.0f;
	bool bPouring = false;
	float Suspicion = 0.0f;
	float HeartClock = 0.0f;
	bool bHeavyStep = false;

	void ApplyDilation();
	UFUNCTION() void HandleState(EPSMatchState State, EPSDefeat Reason);
	UFUNCTION() void HandleCountdown(int32 SecondsLeft);
	UFUNCTION() void HandlePhase(int32 Index, FText Title, FText Sub);
	UFUNCTION() void HandleBagLoaded(int32 Count, FVector Location);
	UFUNCTION() void HandleBagEmptied(int32 Count, FVector Location);
	UFUNCTION() void HandlePour(bool bOn, FVector Location);
	UFUNCTION() void HandleNoticed(FVector Location, AActor* Who);
	UFUNCTION() void HandleFound(int32 Strikes, FVector Location, bool bProbe);
	UFUNCTION() void HandleMissed(FVector Location, AActor* Who);
	UFUNCTION() void HandlePoke(FVector Location, AActor* Who);
	UFUNCTION() void HandleWatch(FVector Location, AActor* Who);
	UFUNCTION() void HandleSuspicion(float Value, float Delta);
	UFUNCTION() void HandleRegion(FName RegionId, float Fraction, FVector Location);
	UFUNCTION() void HandleSpike(FVector Location, float Scattered);
	UFUNCTION() void HandleInput(FName KeyId, bool bSimulated);
	UFUNCTION() void HandlePrompt(FText Text, FVector WorldLocation, bool bDanger);
	UFUNCTION() void HandleFootstep(FVector Location);
	TSet<FName> FullRegions;
};
