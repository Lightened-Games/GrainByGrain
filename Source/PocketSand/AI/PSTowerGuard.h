// Pocket Sand - the guard tower over the puddles: a narrow beam sweeps back and forth. It sees pouring from far away,
// locks onto you for a moment when you catch its eye, and calls a ground guard over to any yellow it spots.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/PSInterfaces.h"
#include "PSTowerGuard.generated.h"

class UPSHumanBodyComponent;
class UPSVisionComponent;
class USpotLightComponent;
class UPSRoundConfig;

UCLASS()
class POCKETSAND_API APSTowerGuard : public AActor, public IPSWatcher
{
	GENERATED_BODY()

public:
	APSTowerGuard();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PocketSand") TObjectPtr<UPSHumanBodyComponent> Body;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PocketSand") TObjectPtr<UPSVisionComponent> Vision;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PocketSand") TObjectPtr<USpotLightComponent> Beam;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float BeamIntensity = 10.0f; // 900/260/150 bleached the hero white when he stood under the tower (2026-11-09)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float TrackSpeed = 70.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float ScanInterval = 0.25f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float ReportCooldown = 4.0f;

	void Setup(const UPSRoundConfig* Config);
	void ResetTower();
	void Step(float Dt, float SweepSeconds);
	float SweepYawAt(float Time, float SweepSeconds) const;

	// IPSWatcher
	virtual FVector PS_EyeLocation() const override;
	virtual FVector PS_ViewDir() const override;
	virtual float PS_ViewRange() const override;
	virtual float PS_ViewHalfAngle() const override;
	virtual bool PS_CanSee(const FVector& Point, float ExposureMul) const override;
	virtual float PS_PourSeenGain() const override;
	virtual bool PS_IsTower() const override { return true; }
	virtual void PS_Watch(AActor* Target) override;
	virtual bool PS_IsBusy() const override { return TrackLeft > 0.0f; }
	virtual void PS_Investigate(const FVector& Location) override {}
	virtual void PS_ResetWatcher() override { ResetTower(); }
	virtual void PS_Freeze(bool bFrozen) override { bFrozenState = bFrozen; }

private:
	UPROPERTY() TObjectPtr<const UPSRoundConfig> Cfg;
	float Clock = 0.0f;
	float Phase = 0.0f;
	float Yaw = 0.0f;
	float TrackLeft = 0.0f;
	float WatchCool = 0.0f;
	float ReportCool = 0.0f;
	float ScanClock = 0.0f;
	bool bFrozenState = false;
	TWeakObjectPtr<AActor> Track;
};
