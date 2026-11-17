// Pocket Sand - an inmate in the yard: a body the crowd director (or the court) moves.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/PSInterfaces.h"
#include "PSInmate.generated.h"

class UPSHumanBodyComponent;
class UPSRoundConfig;

UCLASS()
class POCKETSAND_API APSInmate : public AActor, public IPSCrowdAgent
{
	GENERATED_BODY()

public:
	APSInmate();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PocketSand") TObjectPtr<UPSHumanBodyComponent> Body;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PocketSand") TObjectPtr<class UCapsuleComponent> Blocker;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float TurnSpeed = 7.0f;

	virtual void PS_Dress(const UPSRoundConfig* Config, const FPSOutfit& Outfit) override;
	virtual void PS_PlayAction(const FString& Clip, bool bLoop, float Rate) override;
	virtual void PS_SetPose(const FVector& Location, float Yaw, float Speed) override;
	virtual void PS_SetIdleStyle(int32 Style) override;
	virtual void PS_Cheer(bool bOn) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	float ShownYaw = 0.0f;
	float WantYaw = 0.0f;
	int32 IdleStyle = -1;
};
