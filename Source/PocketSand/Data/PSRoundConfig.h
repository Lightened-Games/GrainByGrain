// Pocket Sand - every round, layout and feel number in one data asset (DA_PS_Round). Code holds no tuning literals.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Data/PSTypes.h"
#include "PSRoundConfig.generated.h"

class UDataTable;
class UPSHUDStyle;

UCLASS(BlueprintType)
class POCKETSAND_API UPSRoundConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPSRoundConfig();

	// ---------------------------------------------------------------- round
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Round") float RoundSeconds = 120.0f; // PROVISIONAL: back with 12 inmates (2026-11-14); 135 was for 16
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Round") float CountdownSeconds = 3.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Round") int32 BagsTotal = 12;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Round") int32 MaxCarried = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Round") float LoadSecondsPerBag = 1.0f; // fits the reach-and-take clip (2026-11-10)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Round") float LoadRadius = 260.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Round") int32 StrikesToLose = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Round") float EndHoldSeconds = 5.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Round") float KillZ = -2000.0f;
	/** Fixed simulation rate for grid, crowd and guards (deterministic with a seed). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Round") float SimHz = 30.0f;
	/** -PSShowcase (the submission video): a shorter seeded round so menu, intro, every mechanic and the victory fit in
	 *  under 60 s from the start screen (design guide 2.5.3). Phase start times and the spike interval scale too. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Showcase") int32 ShowcaseBags = 5;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Showcase") float ShowcaseRoundSeconds = 52.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Showcase") float ShowcasePhaseScale = 0.45f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Showcase") int32 ShowcaseSeed = 3; // cleanest bot path in the seed sweep (2026-11-09)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Round") float LastSecondsWarning = 10.0f;

	// ---------------------------------------------------------------- player
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Player") float WalkSpeed = 150.0f; // brisk walk: Rocketbox m_walk_fast_01 native speed (probe 2026-11-10)
	/** Shift (or the bot on long trips): run on the native run clip (288 cm/s at rate 1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Player") float RunSpeed = 320.0f;
	/** The bot runs when its target is further than this, and walks the last stretch (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Player") float BotRunDistance = 250.0f; // PROVISIONAL
	/** Volleyball rally on the court (players, ball, spikes). Off: the court is just another hiding surface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Layout") bool bVolleyballPlayers = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Player") float WaddleSpeedMul = 0.8f;
	/** Bags of sand per second while Space is held. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Player") float PourRate = 0.7f;
	/** Radius of the footprint each pour step lands in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Player") float PourSpreadRadius = 45.0f;
	/** The trickle lands this far behind the hero's centre (trouser hems). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Player") float PourBehind = 22.0f;
	/** Third-person chase camera (free mouse look). False = the old fixed overhead camera with a small mouse swing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Camera") bool bThirdPerson = true;
	/** Starting pitch; third person keeps it high enough to read the guards' cones and the sand on the ground. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Camera") float CameraPitch = -20.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Camera") FVector2D CameraPitchRange = FVector2D(-55.0f, -8.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Camera") float CameraArm = 480.0f; // UPGRADE_PLAN E1: chase pulled in from 620 (crowd game: still reads trail and cones)
	/** Spring-arm pivot above the capsule centre (shoulder height) and sideways (over-the-shoulder). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Camera") FVector CameraPivot = FVector(0.0f, 30.0f, 75.0f);
	/** Starting yaw: behind the hero at the cell door, looking out over the yard. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Camera") float CameraBaseYaw = -90.0f;
	/** Overhead mode only: how far the mouse may swing the camera. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Camera") float CameraYawRange = 40.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Camera") float CameraFOV = 72.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Camera") float MouseYawSpeed = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Camera") float MousePitchSpeed = 0.7f;
	/** Autoplay (no mouse): the camera swings behind the hero's heading at this rate (1/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Camera") float AutoFollowRate = 2.0f;
	/** Autoplay camera: follow rate multiplier for heading changes of 150 deg or more. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Camera") float AutoFollowTurnBoost = 3.0f;
	/** Human play: the camera eases behind the hero after the mouse rests this long (0 = never). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Camera") float FollowAfterIdle = 1.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Camera") float HumanFollowRate = 0.8f;
	/** Autoplay: while the hero stands still the camera turns to look across the yard (rate 1/s; skipped near the centre). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Camera") float IdleLookRate = 1.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Camera") float IdleLookMinDistance = 400.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Player") FVector2D PlayerStart = FVector2D(3400.0f, 3900.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Player") float PlayerStartYaw = -90.0f;

	// ---------------------------------------------------------------- sand
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Sand") float CellSize = 25.0f;
	/** Cell density that renders as full-strength yellow. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Sand") float FullShowDensity = 0.012f;
	/** Cell density above which a sand mound mesh grows (standing still makes a pile). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Sand") float PileThreshold = 0.03f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Sand") float PileFullDensity = 0.14f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Sand") float TrampleRadius = 150.0f;
	/** Standing inmates still shuffle their feet: fraction of the walking trample rate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Sand") float StandingTrampleMul = 0.45f;
	/** Spill distance outside a full court or puddle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Sand") float SpillOffset = 45.0f;

	// ---------------------------------------------------------------- guards
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Guards") float GuardSightRange = 1500.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Guards") float GuardSightHalfAngle = 40.0f;
	/** The guard's gaze swings this far either side of where he walks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Guards") float GuardLookSweep = 55.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Guards") float GuardLookPeriod = 6.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Guards") float GuardWalkSpeed = 110.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Guards") float GuardInvestigateSpeed = 190.0f;
	/** Bags of shown sand within PatchRadius that count as a suspicious patch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Guards") float PatchNoticeMass = 0.10f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Guards") float PatchRadius = 150.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Guards") float PatchNoticeSeconds = 0.9f;
	/** On arrival the patch must still hold this fraction of the notice mass, or the crowd got there first. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Guards") float PatchConfirmFraction = 0.45f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Guards") float KneelSeconds = 2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Guards") float ClearRadius = 260.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Guards") float ProbeRadius = 350.0f;
	/** Bags of any sand (hidden too) within ProbeRadius the stick finds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Guards") float ProbeFindMass = 0.25f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Guards") float ProbeCooldown = 12.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Guards") float ProbeDetour = 700.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Guards") float PokeSeconds = 1.3f;
	/** A guard who sees you waddle stops and watches you this long. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Guards") float WatchSeconds = 2.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Guards") float WatchCooldown = 5.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Guards") FPSRoute GuardARoute;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Guards") FPSRoute GuardBRoute;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Guards") FVector2D GuardBPost = FVector2D(3700.0f, 1650.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Guards") float GuardBPostYaw = 90.0f;
	/** Extra posted guards (X, Y = spot, Z = facing yaw): yard gate and cell door (reviewer 2026-11-12: "add more guards or ppl"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Guards") TArray<FVector> GuardPosts;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Guards") FVector TowerLocation = FVector(6120.0f, 3250.0f, 0.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Guards") float TowerHeight = 620.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Guards") float TowerYawMin = 188.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Guards") float TowerYawMax = 262.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Guards") float TowerRange = 2100.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Guards") float TowerHalfAngle = 11.0f;

	// ---------------------------------------------------------------- suspicion
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Suspicion") float PourSeenGain = 0.55f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Suspicion") float TowerPourSeenGain = 0.9f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Suspicion") float WaddleSeenGain = 0.10f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Suspicion") float CrowdCoverMul = 0.25f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Suspicion") int32 CrowdCoverCount = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Suspicion") float CrowdCoverRadius = 220.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Suspicion") float Decay = 0.22f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Suspicion") float VignetteAt = 0.6f;

	// ---------------------------------------------------------------- crowd
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Crowd") int32 GroupCount = 6; // 12 inmates (user 2026-11-14 after the 16-inmate test: "too many npcs"); was 8
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Crowd") int32 GroupSize = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Crowd") float CrowdSpeed = 120.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Crowd") float DwellMin = 7.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Crowd") float DwellMax = 13.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Crowd") float GroupRadius = 110.0f; // pairs stand about a metre apart
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Crowd") float AgentRadius = 38.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Crowd") TArray<FVector2D> PointsOfInterest;

	// ---------------------------------------------------------------- volleyball
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Volleyball") float SpikeInterval = 18.0f;
	/** Court sand above this many bags is loose enough to fly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Volleyball") float SpikeLooseAbove = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Volleyball") float SpikeScatterFraction = 0.18f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Volleyball") float SpikeThrowMin = 300.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Volleyball") float SpikeThrowMax = 600.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Volleyball") float SpikeTelegraph = 2.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Volleyball") float RallyPassSeconds = 1.6f;

	// ---------------------------------------------------------------- layout
	/** Scale of the authored 64 x 44 m layout (D22, user choice 2026-11-08: native run + smaller yard). PROVISIONAL until confirmed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Layout") float LayoutScale = 0.6f;
	/** Scale of surface sizes (beds, court, pit, puddles, apron). PROVISIONAL until confirmed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Layout") float RegionScale = 0.75f;
	/** Authored points closer than this to a wall keep their wall distance when the layout scales. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Layout") float WallKeep = 400.0f;
	/** Set from LayoutScale in the constructor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Layout") FVector2D YardSize = FVector2D(6400.0f, 4400.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Layout") FVector2D DoorLocation = FVector2D(3400.0f, 4370.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Layout") float WallHeight = 420.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Layout") float CellBlockHeight = 900.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Layout") TArray<FPSRegionDef> Regions;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Layout") TArray<FPSObstacle> Obstacles;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Layout") TArray<FVector2D> Benches;
	/** One-off props (picnic tables, bins, chairs, clutter, gate, planters...). Repeated dressing is generated by APSYardBuilder. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Layout") TArray<FPSPropPlacement> Props;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Layout") FVector2D HoopLocation = FVector2D(1450.0f, 1850.0f);
	/** Concrete apron in front of the cell-block door (sand shows strongly there). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Layout") FVector2D ApronHalf = FVector2D(520.0f, 330.0f);

	// ---------------------------------------------------------------- data
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data") TSoftObjectPtr<UDataTable> SurfaceTable;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data") TSoftObjectPtr<UDataTable> PhaseTable;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data") TSoftObjectPtr<UPSHUDStyle> HUDStyle;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data") FString CharacterRoot = TEXT("/Game/PocketSand/Characters");
	/** Rocketbox (MIT) avatars: inmates wear the recoloured yard uniform, the hero keeps his striped tee. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data") TArray<FString> InmateAvatars = { TEXT("Male_Adult_02"), TEXT("Male_Adult_03"), TEXT("Male_Adult_04"), TEXT("Male_Adult_06"), TEXT("Male_Adult_07"), TEXT("Male_Adult_08"), TEXT("Male_Adult_12") };
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data") FString HeroAvatar = TEXT("Male_Adult_09"); // v2 hero: unused by any other game (UPGRADE_PLAN D1)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data") FString GuardAvatar = TEXT("Police_Male_02");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data") TArray<FString> PlayerAvatars = { TEXT("Sports_Male_02"), TEXT("Sports_Male_03") };
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data") FString MaterialRoot = TEXT("/Game/PocketSand/Materials");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data") FString VfxRoot = TEXT("/Game/PocketSand/VFX");

	// ---------------------------------------------------------------- helpers
	/** Surface row from the table, or the built-in default when the table is missing. */
	FPSSurfaceRow GetSurface(EPSSurface Surface) const;
	/** Phase rows sorted by start time (table or defaults). */
	TArray<FPSPhaseRow> GetPhases() const;

	static TArray<FPSSurfaceRow> DefaultSurfaces();
	static TArray<FPSPhaseRow> DefaultPhases();

private:
	mutable TArray<FPSSurfaceRow> CachedSurfaces;
public:
	/** Footprints of solid dressing props placed at runtime (the yard builder adds them; NPC steering avoids them too). */
	mutable TArray<FPSObstacle> RuntimeObstacles;
	void AddRuntimeObstacle(const FVector2D& Centre, float Radius) const { RuntimeObstacles.Add({ Centre, Radius }); }
	void ResetRuntimeObstacles() const { RuntimeObstacles.Reset(); }
};
