// Pocket Sand - the yard floor and the sand on it: owns FPSSandField, paints it into a mask texture the ground
// materials read, grows mound meshes where sand piles up, and broadcasts surface fill changes.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/PSSandField.h"
#include "PSSandGrid.generated.h"

class UProceduralMeshComponent;
class UInstancedStaticMeshComponent;
class UTexture2D;
class UMaterialInstanceDynamic;
class UPSRoundConfig;

UCLASS()
class POCKETSAND_API APSSandGrid : public AActor
{
	GENERATED_BODY()

public:
	APSSandGrid();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PocketSand") TObjectPtr<UProceduralMeshComponent> Floor;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PocketSand") TObjectPtr<UInstancedStaticMeshComponent> Mounds;

	/** Seconds between mask texture uploads. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float TextureInterval = 0.08f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float MoundInterval = 0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") int32 MaxMounds = 700;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float MoundMinHeight = 3.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float MoundMaxHeight = 16.0f;
	/** Outer ground beyond the walls (multiple of the yard size), so nothing floats in a void. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float OuterGroundScale = 4.0f;
	/** Broadcast a region fill change once it moves this much. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float FillBroadcastStep = 0.02f;

	void Init(const UPSRoundConfig* Config);
	void ResetSand();

	FPSSandField& Field() { return Sand; }
	const FPSSandField& Field() const { return Sand; }
	UTexture2D* MaskTexture() const { return Mask; }
	/** Ground-material parameters shared with every surface that shows sand (beds, pit). */
	void ApplyMaskParams(UMaterialInstanceDynamic* MID) const;

	float Pour(const FVector& Where, float Amount, float Radius);
	void Tick(float DeltaSeconds) override;
	/** Fixed-step update called by the GameMode (fill broadcasts). */
	void Step(float Dt);
	float SurfaceTopZ(const FVector2D& P) const;
	int32 NumMounds() const;

private:
	FPSSandField Sand;
	UPROPERTY() TObjectPtr<UTexture2D> Mask;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> FloorMID;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> OuterMID;
	UPROPERTY() TObjectPtr<const UPSRoundConfig> Cfg;
	TArray<float> LastFill;
	float TexClock = 0.0f;
	float MoundClock = 0.0f;
	bool bDirty = true;

	void BuildFloor();
	void UploadMask();
	void RebuildMounds();
};
