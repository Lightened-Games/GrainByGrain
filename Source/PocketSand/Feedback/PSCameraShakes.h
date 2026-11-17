// Pocket Sand - camera shakes (perlin patterns, EngineCameras).

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "PSCameraShakes.generated.h"

/** Strike / defeat. */
UCLASS()
class POCKETSAND_API UPSImpactShake : public UCameraShakeBase
{
	GENERATED_BODY()
public:
	UPSImpactShake(const FObjectInitializer& ObjectInitializer);
};

/** Spike thump, bag load, stick poke. */
UCLASS()
class POCKETSAND_API UPSMicroShake : public UCameraShakeBase
{
	GENERATED_BODY()
public:
	UPSMicroShake(const FObjectInitializer& ObjectInitializer);
};
