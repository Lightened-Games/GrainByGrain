// Pocket Sand - interfaces between systems (no casts to concrete game classes).

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Data/PSTypes.h"
#include "PSInterfaces.generated.h"

class UPSRoundConfig;
class APSSandGrid;
class UPSCrowdDirectorComponent;

/** Something the player can use with E (the cell-block door stash). */
UINTERFACE(MinimalAPI, Blueprintable)
class UPSInteractable : public UInterface { GENERATED_BODY() };

class POCKETSAND_API IPSInteractable
{
	GENERATED_BODY()
public:
	virtual bool PS_CanInteract(const AActor* User) const = 0;
	virtual bool PS_Interact(AActor* User) = 0;
	virtual FVector PS_InteractLocation() const = 0;
	virtual float PS_InteractRadius() const = 0;
};

/** Read access to the match for HUD, juice, AI and the bot (implemented by the GameMode). */
UINTERFACE(MinimalAPI)
class UPSMatchInfo : public UInterface { GENERATED_BODY() };

class POCKETSAND_API IPSMatchInfo
{
	GENERATED_BODY()
public:
	virtual EPSMatchState PS_State() const = 0;
	virtual EPSDefeat PS_DefeatReason() const = 0;
	virtual float PS_Elapsed() const = 0;
	virtual float PS_TimeLeft() const = 0;
	virtual const UPSRoundConfig* PS_Config() const = 0;
	virtual APSSandGrid* PS_Grid() const = 0;
	virtual UPSCrowdDirectorComponent* PS_Crowd() const = 0;
	virtual int32 PS_BagsInCell() const = 0;
	virtual int32 PS_BagsGone() const = 0;
	virtual int32 PS_Strikes() const = 0;
	virtual bool PS_IsAutoplay() const = 0;
	virtual float PS_BotSkill() const = 0;
	virtual const FPSStats& PS_Stats() const = 0;
	virtual const TArray<FPSProbeZone>& PS_ProbeZones() const = 0;
	virtual const TArray<TWeakObjectPtr<AActor>>& PS_Watchers() const = 0;
	virtual int32 PS_PhaseIndex() const = 0;
	virtual float PS_NextSpikeTime() const = 0;
	virtual bool PS_TakeBagFromCell() = 0;
	virtual void PS_RequestRestart() = 0;
	/** A guard confirmed a patch (bProbe: found by the stick). Returns the strike count. */
	virtual int32 PS_ReportFind(const FVector& Location, bool bProbe) = 0;
	/** The tower spotted a patch: send the nearest ground guard. */
	virtual void PS_RequestInvestigate(const FVector& Location) = 0;
	virtual void PS_Prompt(FName Id, const FText& Text, const FVector& Where, bool bDanger) = 0;
	virtual void PS_TouchProbeZone(int32 Index, float Time) = 0;
};

/** The player's carried sand (implemented by the hero pawn). */
UINTERFACE(MinimalAPI)
class UPSSandCarrier : public UInterface { GENERATED_BODY() };

class POCKETSAND_API IPSSandCarrier
{
	GENERATED_BODY()
public:
	virtual int32 PS_Carried() const = 0;
	/** Sand left in the bag being poured (0..1). */
	virtual float PS_CurrentBag() const = 0;
	virtual bool PS_IsPouring() const = 0;
	virtual bool PS_IsWaddling() const = 0;
	virtual bool PS_IsLoading() const = 0;
	virtual float PS_Suspicion() const = 0;
	virtual void PS_AddBag() = 0;
	virtual void PS_SetWantPour(bool bWant) = 0;
	/** Walk by default; run while true (Shift, or the bot on long trips). */
	virtual void PS_SetRun(bool bRun) = 0;
	/** Would sand poured where you stand show right now? */
	virtual bool PS_SandWouldShow() const = 0;
	virtual void PS_ResetCarrier(const FVector& Location, float Yaw) = 0;
	virtual void PS_LockMovement(bool bLocked) = 0;
};

/** Anything with eyes (ground guards, the tower). */
UINTERFACE(MinimalAPI)
class UPSWatcher : public UInterface { GENERATED_BODY() };

class POCKETSAND_API IPSWatcher
{
	GENERATED_BODY()
public:
	virtual FVector PS_EyeLocation() const = 0;
	virtual FVector PS_ViewDir() const = 0;
	virtual float PS_ViewRange() const = 0;
	virtual float PS_ViewHalfAngle() const = 0;
	/** True when Point is inside this watcher's view (cone + range x Exposure). */
	virtual bool PS_CanSee(const FVector& Point, float ExposureMul) const = 0;
	virtual float PS_PourSeenGain() const = 0;
	virtual bool PS_IsTower() const = 0;
	/** The player's walk caught his eye: stop and watch for a while. */
	virtual void PS_Watch(AActor* Target) = 0;
	virtual bool PS_IsBusy() const = 0;
	virtual void PS_Investigate(const FVector& Location) = 0;
	virtual void PS_ResetWatcher() = 0;
	virtual void PS_Freeze(bool bFrozen) = 0;
};

/** The controls (keyboard and the autoplay bot go through the same calls, so the legend lights for both). */
UINTERFACE(MinimalAPI)
class UPSPlayerInput : public UInterface { GENERATED_BODY() };

class POCKETSAND_API IPSPlayerInput
{
	GENERATED_BODY()
public:
	/** World-space move direction on the ground plane (length <= 1). */
	virtual void PS_SetMove(const FVector2D& WorldDir, bool bSimulated) = 0;
	virtual void PS_SetPour(bool bPour, bool bSimulated) = 0;
	virtual void PS_SetRun(bool bRun, bool bSimulated) = 0;
	virtual bool PS_Load(bool bSimulated) = 0;
};

/** Kinematic agents the crowd director moves (inmates). */
UINTERFACE(MinimalAPI)
class UPSCrowdAgent : public UInterface { GENERATED_BODY() };

class POCKETSAND_API IPSCrowdAgent
{
	GENERATED_BODY()
public:
	virtual void PS_Dress(const UPSRoundConfig* Config, const FPSOutfit& Outfit) = 0;
	virtual void PS_SetPose(const FVector& Location, float Yaw, float Speed) = 0;
	/** Play a clip by name over locomotion (empty = stop). */
	virtual void PS_PlayAction(const FString& Clip, bool bLoop, float Rate) = 0;
	virtual void PS_SetIdleStyle(int32 Style) = 0;
	virtual void PS_Cheer(bool bOn) = 0;
};
