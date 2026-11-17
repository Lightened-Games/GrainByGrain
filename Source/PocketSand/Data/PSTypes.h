// Pocket Sand - shared types: match states, surfaces, data-table rows, layout structs.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "PSTypes.generated.h"

UENUM(BlueprintType)
enum class EPSMatchState : uint8
{
	Ready,
	Active,
	Victory,
	Defeat,
	/** Start screen and intro (FactoryKit): the yard runs, the hero waits, the HUD is hidden. */
	Menu
};

UENUM(BlueprintType)
enum class EPSDefeat : uint8
{
	None,
	/** Guards found StrikesToLose suspicious patches. */
	Found,
	/** A guard watched sand coming out of your trousers (suspicion full). */
	Caught,
	/** Yard time ended with bags still in the cell or in your trousers. */
	Whistle
};

/** What a patch of yard is made of. Each has a row in DT_PS_Surfaces. */
UENUM(BlueprintType)
enum class EPSSurface : uint8
{
	Gravel,
	Soil,
	Court,
	Puddle,
	Pit,
	Concrete
};

/** One surface type: how sand behaves on it. */
USTRUCT(BlueprintType)
struct FPSSurfaceRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface") EPSSurface Surface = EPSSurface::Gravel;
	/** Small caps world tag ("SOIL"). Empty = no tag. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface") FText Tag;
	/** Swallows sand into its region until the region's capacity is used. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface") bool bAbsorbs = false;
	/** When full, extra sand is pushed out onto the gravel rim instead of piling on the surface (court, puddles). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface") bool bSpillsWhenFull = false;
	/** Bags per 25 cm cell above which shown sand is visible on this surface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface", meta = (ClampMin = "0")) float ShowThreshold = 0.0012f;
	/** Per-second fraction of shown sand that one inmate's feet grind out of sight (0 = feet don't hide it). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface", meta = (ClampMin = "0")) float TrampleRate = 0.0f;
	/** Multiplies guard sight range on a player standing on this surface (the long-jump pit is a stage). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface", meta = (ClampMin = "0")) float ExposureMul = 1.0f;
	/** Fraction of region capacity where the surface starts to turn yellow. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface", meta = (ClampMin = "0", ClampMax = "1")) float FullTintStart = 0.75f;
};

/** One escalation phase of the round. */
USTRUCT(BlueprintType)
struct FPSPhaseRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase") float StartSeconds = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase") FText Title;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase") FText Sub;
	/** Seconds for the tower beam to sweep across and back. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase") float TowerSweepSeconds = 9.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase") float CrowdSpeedMul = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase") float GuardSpeedMul = 1.0f;
	/** Guard B leaves his post by the court and walks the east patrol. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase") bool bSecondGuardPatrols = false;
	/** Volleyball spikes start in this phase. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase") bool bSpikes = false;
};

/** A hiding surface placed in the yard (garden bed, court, puddle, pit). */
USTRUCT(BlueprintType)
struct FPSRegionDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Region") FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Region") EPSSurface Surface = EPSSurface::Soil;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Region") FVector2D Center = FVector2D::ZeroVector;
	/** Rectangle half size, or (radius, radius) when bRound. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Region") FVector2D HalfSize = FVector2D(100.0f, 100.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Region") bool bRound = false;
	/** Bags of sand the surface swallows before it shows. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Region", meta = (ClampMin = "0")) float Capacity = 1.5f;
	/** Visual top height of the surface (raised beds, flush court). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Region") float TopZ = 4.0f;

	bool Contains(const FVector2D& P, float Inflate = 0.0f) const
	{
		if (bRound)
		{
			return FVector2D::DistSquared(P, Center) <= FMath::Square(HalfSize.X + Inflate);
		}
		return FMath::Abs(P.X - Center.X) <= HalfSize.X + Inflate && FMath::Abs(P.Y - Center.Y) <= HalfSize.Y + Inflate;
	}
};

/** Round obstacle agents and the bot steer around (bench, tower leg, hoop pole). */
USTRUCT(BlueprintType)
struct FPSObstacle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Obstacle") FVector2D Center = FVector2D::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Obstacle") float Radius = 60.0f;
};

USTRUCT(BlueprintType)
struct FPSRoute
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Route") TArray<FVector2D> Points;
};

/** One placed prop (Poly Haven CC0 mesh under /Game/PocketSand/Props/<Mesh>). */
USTRUCT(BlueprintType)
struct FPSPropPlacement
{
	GENERATED_BODY()

	/** "<slug>/SM_<name>" */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop") FString Mesh;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop") FVector Location = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop") float Yaw = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop") float Scale = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop") bool bCollision = false;
};

/** A spot a found patch left behind: the stick guard pokes the ground around it. */
USTRUCT(BlueprintType)
struct FPSProbeZone
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Probe") FVector2D Center = FVector2D::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Probe") float Radius = 350.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Probe") float LastPoke = -100.0f;
};

/** Outfit colours for a base-character body (our kit-band material). */
USTRUCT(BlueprintType)
struct FPSOutfit
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outfit") FLinearColor Top = FLinearColor(0.2f, 0.28f, 0.4f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outfit") FLinearColor Bottom = FLinearColor(0.12f, 0.16f, 0.24f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outfit") FLinearColor Shoes = FLinearColor(0.03f, 0.03f, 0.03f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outfit") float SkinTone = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outfit") int32 Hair = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outfit") float HairTone = 0.2f;
	/** Short sleeves (inmates) vs long sleeves (guards). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outfit") float SleeveEnd = 48.0f;
	/** Microsoft Rocketbox avatar (e.g. "Male_Adult_03"). Empty = Quaternius base character dressed by the kit material. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outfit") FString Avatar;
};

/** Round statistics (end screen + telemetry). */
USTRUCT(BlueprintType)
struct FPSStats
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Stats") int32 Trips = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Stats") int32 Strikes = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Stats") int32 NearMisses = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Stats") int32 Spikes = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Stats") float PeakSuspicion = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Stats") float Seconds = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Stats") float IdleSeconds = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Stats") TMap<FName, float> PouredBySurface;
	UPROPERTY(BlueprintReadOnly, Category = "Stats") TArray<float> SuspicionCurve;
};
