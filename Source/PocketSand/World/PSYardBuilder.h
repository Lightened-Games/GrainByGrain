// Pocket Sand - builds the yard from the round config: perimeter wall with fence and razor wire, the cell block with
// barred windows and the door, the guard tower, benches, the hoop, raised garden beds, the court with its net, the
// long-jump pit and the puddles. Code-built bevelled geometry under our PBR materials (P35).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/PSMeshLib.h"
#include "PSYardBuilder.generated.h"

class UProceduralMeshComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UHierarchicalInstancedStaticMeshComponent;
class APSSandGrid;
class UPSRoundConfig;
enum class EPSMat : uint8;

UCLASS()
class POCKETSAND_API APSYardBuilder : public AActor
{
	GENERATED_BODY()

public:
	APSYardBuilder();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PocketSand") TObjectPtr<UProceduralMeshComponent> Structure;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PocketSand") TObjectPtr<UProceduralMeshComponent> Surfaces;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PocketSand") TObjectPtr<UPointLightComponent> DoorLamp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float WallThickness = 40.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float FenceHeight = 243.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float FencePostSpacing = 400.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float RazorLoopSpacing = 42.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float RazorRadius = 32.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float WindowSpacing = 420.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float DoorLampIntensity = 60.0f;
	/** Tally marks scratched beside the cell door (micro-story): groups of five, distance left of the door, height. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Story") int32 TallyGroups = 4;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Story") float TallyOffset = 205.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Story") float TallyHeight = 112.0f;
	/** Wall lamps on the yard face of the south wall (it stands in its own shadow): fixture + warm shadowed spotlight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|WallLamps") int32 WallLampCount = 6;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|WallLamps") float WallLampHeight = 330.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|WallLamps") float WallLampIntensity = 560.0f;
	/** Every Nth lamp casts shadows (0 = none). Provisional (C8): 2 halves the shadow cost at midday, when the lamps are fill. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|WallLamps") int32 WallLampShadowEvery = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|WallLamps") float WallLampRadius = 800.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|WallLamps") float WallLampConeDeg = 62.0f;
	/** Downward tilt of the beam (90 = straight down); it leans toward the wall so the wall face and the ground both catch it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|WallLamps") float WallLampPitch = 50.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|WallLamps") FLinearColor WallLampColor = FLinearColor(1.0f, 0.8f, 0.58f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float BedEdgeHeight = 16.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float BedEdgeWidth = 12.0f;
	/** Yellow tint of a hiding surface at full (fraction of the way to sand yellow). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float FullTintStrength = 0.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FString PropRoot = TEXT("/Game/PocketSand/Props/");
	/** Weeds and grass tufts along the wall bases: mean spacing and scale range. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float WeedSpacing = 150.0f; // tidier wall-base strip (was 95)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FVector2D WeedScale = FVector2D(1.2f, 2.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") int32 YardWeeds = 0; // user 2026-11-13: "the random grass looks bad" (was 130)
	/** Loose tufts around the tower legs and bed edges (off: only the tidy wall-base strip stays). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") bool bLooseWeeds = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float FloodlightSpacing = 1100.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float FloodlightHeight = 540.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float FenceYaw = 0.0f;
	/** C2 exceptions: walk-through foliage and flush covers (tagged NoCollide). Everything else blocks characters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") TArray<FString> WalkThroughMeshes = { TEXT("grass_medium_01/"), TEXT("weed_plant_02/"), TEXT("shrub_04/"), TEXT("water_manhole_cover/"), TEXT("modular_urban_apartments_facade/") };
	/** Town skyline beyond the south, west and east walls (C4 far layer): Poly Haven modular facade kit over a code-built mass.
	 *  Unreachable backdrop, so no collision and no shadow casting (tagged NoCollide). Provisional (C8) until the user signs off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Skyline") bool bBuildSkyline = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Skyline") FString SkylineKit = TEXT("modular_urban_apartments_facade/SM_");
	/** Distance from the outer wall face to the nearest facade, plus a random extra per block for depth. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Skyline") float SkylineDistance = 900.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Skyline") float SkylineDistanceJitter = 700.0f;
	/** How far each row runs past the yard corners so the corners are not empty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Skyline") float SkylineOverhang = 1800.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Skyline") FIntPoint SkylineBays = FIntPoint(3, 6);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Skyline") FIntPoint SkylineFloors = FIntPoint(3, 6);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Skyline") FVector2D SkylineGap = FVector2D(150.0f, 700.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Skyline") float SkylineBlockDepth = 900.0f;
	/** Module size of the kit (one bay, one storey). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Skyline") float SkylineModule = 300.0f;
	/** Extra yaw so the kit's front face looks at the yard (depends on the import axis). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Skyline") float SkylineFaceYaw = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Skyline") int32 SkylineSeed = 4417;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Net") float NetBlockHalfThickness = 6.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Net") float NetObstacleSpacing = 60.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Net") float NetObstacleRadius = 35.0f;
	/** Solid props below this height become round steering obstacles sized from their bounds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float ObstacleMaxZ = 60.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float ObstacleFootprint = 0.85f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float ObstacleMinRadius = 15.0f;

	void Build(const UPSRoundConfig* Config, APSSandGrid* Grid);
	/** Writes every generated piece as glTF into asset-library (original work). */
	void ExportMeshes(const FString& Directory) const;

private:
	UPROPERTY() TObjectPtr<const UPSRoundConfig> Cfg;
	UPROPERTY() TMap<FName, TObjectPtr<UMaterialInstanceDynamic>> RegionMIDs;
	TMap<FName, FPSMeshData> Pieces;
	TMap<FName, float> RegionTint;

	FPSMeshData& Piece(FName Name) { return Pieces.FindOrAdd(Name); }
	void BuildPerimeter();
	void BuildCellBlock();
	void BuildTower();
	void BuildFurniture();
	void BuildRegions(APSSandGrid* Grid);
	void AddRazor(FPSMeshData& D, const FVector& A, const FVector& B);
	void AddFence(const FVector& A, const FVector& B, float BaseZ);
	void Commit(APSSandGrid* Grid);
	void BuildProps();
	void BuildSkyline();
	void AddProp(const FString& Mesh, const FVector& Location, float Yaw, float Scale, bool bCollision, float Pitch = 0.0f, float Roll = 0.0f);
	bool IsWalkThrough(const FString& Mesh) const;
	UPROPERTY() TMap<FString, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> PropComponents;
	/** Nothing pokes into anything (user 2026-11-13): ground props keep a footprint; foliage is placed last, clear of them. */
	struct FFootprint { FVector2D Centre; float Radius; FString Mesh; };
	TArray<FFootprint> Footprints;
	struct FPendingFoliage { FString Mesh; FVector Location; float Yaw; float Scale; };
	TArray<FPendingFoliage> PendingFoliage;
	bool bFlushingFoliage = false;
	void FlushFoliage();
	static bool IsFoliage(const FString& Mesh) { return Mesh.Contains(TEXT("grass")) || Mesh.Contains(TEXT("weed")) || Mesh.Contains(TEXT("shrub")); }
	/** Volleyball nets: (centre X, centre Y, half length) for the blocker and obstacles. */
	TArray<FVector4> Nets;

	UFUNCTION() void HandleRegionFill(FName RegionId, float Fraction, FVector Location);
};
