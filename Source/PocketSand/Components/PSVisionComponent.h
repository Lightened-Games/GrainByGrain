// Pocket Sand - a pair of eyes: a view cone (range, half angle, gaze yaw), a soft red ground cone that shows it, and
// the patch scan (notices visible sand after a short stare). Attach to any watcher (ground guard, tower).

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "PSVisionComponent.generated.h"

class UProceduralMeshComponent;
class UMaterialInstanceDynamic;
class APSSandGrid;
class UPSRoundConfig;

UCLASS(ClassGroup = (PocketSand), meta = (BlueprintSpawnableComponent))
class POCKETSAND_API UPSVisionComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UPSVisionComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float Range = 1500.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float HalfAngle = 40.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float EyeHeight = 165.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float ConeOpacity = 0.12f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float AlertOpacity = 0.26f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") int32 ConeSegments = 20;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float ConeLift = 3.0f;
	/** Patch candidate size: cells are grouped into bins of this size. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float PatchBin = 220.0f;
	/** A candidate counts as the same patch if it moved less than this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float PatchStickiness = 260.0f;

	void InitCone(float InRange, float InHalfAngle);
	void SetGaze(const FVector& EyeWorld, float WorldYaw);
	float GazeYaw() const { return Yaw; }
	FVector Eye() const { return EyePos; }
	FVector Dir() const;
	bool CanSee(const FVector& Point, float ExposureMul) const;
	/** Scan for visible sand. Returns true once a patch has been stared at for NoticeSeconds; OutLocation = the patch. */
	bool ScanPatch(const APSSandGrid* Grid, float MinMass, float NoticeSeconds, float Dt, FVector& OutLocation);
	void ResetScan() { Stare = 0.0f; }
	float StareFraction(float NoticeSeconds) const { return NoticeSeconds > 0.0f ? FMath::Clamp(Stare / NoticeSeconds, 0.0f, 1.0f) : 0.0f; }
	/** Alert 0..1 thickens the ground cone (noticing, watching you). bShow false hides it. */
	void SetConeLook(float Alert, bool bShow);

private:
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Cone;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> ConeMID;
	FVector EyePos = FVector::ZeroVector;
	float Yaw = 0.0f;
	float Stare = 0.0f;
	FVector2D StareAt = FVector2D::ZeroVector;
	void BuildCone();
};
