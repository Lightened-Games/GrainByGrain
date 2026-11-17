// Pocket Sand - the match event bus. Gameplay broadcasts here; HUD, audio, juice and the bot listen.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Data/PSTypes.h"
#include "PSEventSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPSStateEvent, EPSMatchState, State, EPSDefeat, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPSCountdownEvent, int32, SecondsLeft);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FPSPhaseEvent, int32, Index, FText, Title, FText, Sub);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPSBagEvent, int32, Count, FVector, Location);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPSPourEvent, bool, bPouring, FVector, Location);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FPSPatchEvent, int32, Strikes, FVector, Location, bool, bProbe);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPSLocationEvent, FVector, Location, AActor*, Who);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPSSuspicionEvent, float, Value, float, Delta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FPSRegionEvent, FName, RegionId, float, Fraction, FVector, Location);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPSSpikeEvent, FVector, Location, float, Scattered);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FPSNoticeEvent, FText, Title, FText, Sub, bool, bDanger);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FPSPromptEvent, FText, Text, FVector, WorldLocation, bool, bDanger);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPSInputEvent, FName, KeyId, bool, bSimulated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPSFootstepEvent, FVector, Location);

UCLASS()
class POCKETSAND_API UPSEventSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "PocketSand") FPSStateEvent OnStateChanged;
	UPROPERTY(BlueprintAssignable, Category = "PocketSand") FPSCountdownEvent OnCountdown;
	UPROPERTY(BlueprintAssignable, Category = "PocketSand") FPSPhaseEvent OnPhase;
	/** Count = bags now in the trousers. */
	UPROPERTY(BlueprintAssignable, Category = "PocketSand") FPSBagEvent OnBagLoaded;
	/** Count = bags gone in total. */
	UPROPERTY(BlueprintAssignable, Category = "PocketSand") FPSBagEvent OnBagEmptied;
	UPROPERTY(BlueprintAssignable, Category = "PocketSand") FPSPourEvent OnPourChanged;
	/** A guard noticed a patch and is walking over. */
	UPROPERTY(BlueprintAssignable, Category = "PocketSand") FPSLocationEvent OnPatchNoticed;
	/** A guard rubbed the sand between his fingers: a strike. */
	UPROPERTY(BlueprintAssignable, Category = "PocketSand") FPSPatchEvent OnPatchFound;
	/** The crowd trampled it before he got there. */
	UPROPERTY(BlueprintAssignable, Category = "PocketSand") FPSLocationEvent OnPatchMissed;
	UPROPERTY(BlueprintAssignable, Category = "PocketSand") FPSLocationEvent OnPoke;
	UPROPERTY(BlueprintAssignable, Category = "PocketSand") FPSLocationEvent OnWatch;
	UPROPERTY(BlueprintAssignable, Category = "PocketSand") FPSSuspicionEvent OnSuspicion;
	UPROPERTY(BlueprintAssignable, Category = "PocketSand") FPSRegionEvent OnRegionFill;
	UPROPERTY(BlueprintAssignable, Category = "PocketSand") FPSSpikeEvent OnSpikeWarning;
	UPROPERTY(BlueprintAssignable, Category = "PocketSand") FPSSpikeEvent OnSpike;
	UPROPERTY(BlueprintAssignable, Category = "PocketSand") FPSNoticeEvent OnNotice;
	UPROPERTY(BlueprintAssignable, Category = "PocketSand") FPSPromptEvent OnPrompt;
	UPROPERTY(BlueprintAssignable, Category = "PocketSand") FPSInputEvent OnInput;
	UPROPERTY(BlueprintAssignable, Category = "PocketSand") FPSFootstepEvent OnFootstep;
	UPROPERTY(BlueprintAssignable, Category = "PocketSand") FPSFootstepEvent OnPilePuff;

	/** Native fixed-step hook (deterministic listeners). */
	DECLARE_MULTICAST_DELEGATE_OneParam(FPSSimStep, float);
	FPSSimStep OnSimStep;

	static UPSEventSubsystem* Get(const UObject* WorldContext);
};
