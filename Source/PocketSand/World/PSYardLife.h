// Pocket Sand - yard life (design guide 2.5.5 Life plan): small things that move or sound on their own so the yard
// never looks frozen. Laundry hung out of a cell window on a line, a flag on the watchtower, and the tannoy crackling
// now and then while the round runs. All placement is relative to the round config; every value is tuning.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PSYardLife.generated.h"

class UPSRoundConfig;
class UStaticMeshComponent;

UCLASS()
class POCKETSAND_API APSYardLife : public AActor
{
	GENERATED_BODY()

public:
	APSYardLife();

	/** Laundry line strung between two cell windows (window indices along the cell block face, counted from the west). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Laundry") FIntPoint LaundryWindows = FIntPoint(2, 4);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Laundry") float LaundryHeight = 640.0f; // above the chase camera (about 330 cm), so a garment never fills the frame
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Laundry") float LaundryOut = 55.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Laundry") float LaundrySag = 22.0f;
	/** Garment size (cm: width, height) and colours (linear). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Laundry") FVector2D GarmentSize = FVector2D(52.0f, 66.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Laundry") TArray<FLinearColor> GarmentColors = {
		FLinearColor(0.80f, 0.80f, 0.76f), FLinearColor(0.16f, 0.22f, 0.36f), FLinearColor(0.85f, 0.32f, 0.08f),
		FLinearColor(0.55f, 0.55f, 0.52f), FLinearColor(0.16f, 0.22f, 0.36f) };
	/** Width of the cell block window bay (matches the yard builder's WindowSpacing). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Laundry") float WindowSpacing = 420.0f;

	/** Flag on the watchtower roof. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Flag") float FlagPoleHeight = 230.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Flag") float FlagRoofOffset = 292.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Flag") FVector2D FlagSize = FVector2D(110.0f, 70.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Flag") int32 FlagSegments = 5;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Flag") FLinearColor FlagColor = FLinearColor(0.75f, 0.07f, 0.04f);
	/** Wind direction (yaw, degrees) the flag streams toward. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Wind") float WindYaw = 200.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Wind") float WindSpeed = 1.7f;
	/** Sway of hanging garments (degrees) and flag ripple (degrees per segment). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Wind") float GarmentSwayDeg = 9.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Wind") float FlagRippleDeg = 16.0f;

	/** Tannoy crackle while the round is active: random interval range (s) and volume. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Tannoy") FVector2D TannoyInterval = FVector2D(22.0f, 38.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Tannoy") float TannoyVolume = 0.35f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Tannoy") int32 Seed = 5150;

	void Build(const UPSRoundConfig* Config);
	virtual void Tick(float DeltaSeconds) override;

private:
	struct FSwing { TObjectPtr<USceneComponent> Pivot; float Phase = 0.0f; float Rate = 1.0f; };
	TArray<FSwing> Garments;
	TArray<TObjectPtr<USceneComponent>> FlagJoints;
	float Clock = 0.0f;
	float NextTannoy = 0.0f;
	FRandomStream Rng;

	UStaticMeshComponent* AddPiece(USceneComponent* Parent, UStaticMesh* Mesh, const FTransform& Xf, const FLinearColor& Color, bool bSolid);
};
