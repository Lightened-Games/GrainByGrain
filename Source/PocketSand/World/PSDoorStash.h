// Pocket Sand - the cell-block door: the rest of the sand waits inside. Press E here to stuff a bag down your trousers.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/PSInterfaces.h"
#include "PSDoorStash.generated.h"

class UProceduralMeshComponent;
class UMaterialInstanceDynamic;
class UPSRoundConfig;

UCLASS()
class POCKETSAND_API APSDoorStash : public AActor, public IPSInteractable
{
	GENERATED_BODY()

public:
	APSDoorStash();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PocketSand") TObjectPtr<UProceduralMeshComponent> Chevron;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float ChevronHeight = 260.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float BobAmplitude = 12.0f;
	/** Chevron fades to NearFadeMin as the hero comes within NearFadeStart..+Range (cm) of the door. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float NearFadeStart = 200.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float NearFadeRange = 250.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float NearFadeMin = 0.25f;
	/** Same fade by camera distance to the chevron (cm), so it never fills the frame from close behind. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float CameraFadeStart = 500.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float CameraFadeRange = 500.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float BobSpeed = 2.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float ChevronSize = 70.0f;
	/** Where the player stands to load (in front of the door, on the apron). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float StandOff = 150.0f;

	void Setup(const UPSRoundConfig* Config);
	virtual void Tick(float DeltaSeconds) override;

	virtual bool PS_CanInteract(const AActor* User) const override;
	virtual bool PS_Interact(AActor* User) override;
	virtual FVector PS_InteractLocation() const override;
	virtual float PS_InteractRadius() const override;

private:
	UPROPERTY() TObjectPtr<const UPSRoundConfig> Cfg;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> ChevronMID;
	float Clock = 0.0f;
	class IPSMatchInfo* Match() const;
};
