// Pocket Sand - suspicion meter.

#include "Components/PSSuspicionComponent.h"
#include "Data/PSRoundConfig.h"
#include "Framework/PSEventSubsystem.h"

UPSSuspicionComponent::UPSSuspicionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPSSuspicionComponent::InitFromConfig(const UPSRoundConfig* Config)
{
	if (Config)
	{
		Decay = Config->Decay;
	}
}

void UPSSuspicionComponent::ResetValue()
{
	Current = 0.0f;
	PeakValue = 0.0f;
	LastBroadcast = 0.0f;
	bRising = false;
	bFullSent = false;
	if (UPSEventSubsystem* Bus = UPSEventSubsystem::Get(this))
	{
		Bus->OnSuspicion.Broadcast(0.0f, 0.0f);
	}
}

void UPSSuspicionComponent::Step(float Dt, float Gain)
{
	const float Before = Current;
	bRising = Gain > 0.0f;
	Current = FMath::Clamp(bRising ? Current + Gain * Dt : Current - Decay * Dt, 0.0f, 1.0f);
	PeakValue = FMath::Max(PeakValue, Current);
	if (FMath::Abs(Current - LastBroadcast) >= BroadcastStep || (Current == 0.0f && LastBroadcast > 0.0f) || (Current >= 1.0f && LastBroadcast < 1.0f))
	{
		if (UPSEventSubsystem* Bus = UPSEventSubsystem::Get(this))
		{
			Bus->OnSuspicion.Broadcast(Current, Current - LastBroadcast);
		}
		LastBroadcast = Current;
	}
	if (Current >= 1.0f && Before < 1.0f && !bFullSent)
	{
		bFullSent = true;
		OnFull.Broadcast();
	}
}
