// Pocket Sand - trouser bags.

#include "Components/PSPourComponent.h"
#include "Data/PSRoundConfig.h"
#include "World/PSSandGrid.h"

UPSPourComponent::UPSPourComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPSPourComponent::InitFromConfig(const UPSRoundConfig* Config)
{
	if (Config)
	{
		PourRate = Config->PourRate;
		SpreadRadius = Config->PourSpreadRadius;
		PourBehind = Config->PourBehind;
		MaxCarried = Config->MaxCarried;
	}
}

void UPSPourComponent::ResetBags()
{
	Carried_ = 0;
	Current = 0.0f;
	bWantPour = false;
	if (bPouring)
	{
		bPouring = false;
		OnPourChanged.Broadcast(false, GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector);
	}
}

bool UPSPourComponent::AddBag()
{
	if (Carried_ >= MaxCarried)
	{
		return false;
	}
	if (Carried_ == 0)
	{
		Current = 1.0f;
	}
	++Carried_;
	return true;
}

float UPSPourComponent::Step(float Dt, APSSandGrid* Grid, const FVector& FootLocation, bool bAllowed)
{
	const bool bNow = bAllowed && bWantPour && Carried_ > 0 && Grid != nullptr;
	if (bNow != bPouring)
	{
		bPouring = bNow;
		OnPourChanged.Broadcast(bPouring, FootLocation);
	}
	if (!bPouring)
	{
		return 0.0f;
	}
	const float Amount = FMath::Min(PourRate * Dt, Current);
	const float Shown = Grid->Pour(FootLocation, Amount, SpreadRadius);
	LastShown = Amount > 0.0f ? Shown / Amount : 0.0f;
	Current -= Amount;
	if (Current <= 1e-4f)
	{
		--Carried_;
		Current = Carried_ > 0 ? 1.0f : 0.0f;
		OnBagEmptied.Broadcast(FootLocation);
		if (Carried_ == 0)
		{
			bPouring = false;
			OnPourChanged.Broadcast(false, FootLocation);
		}
	}
	return Amount;
}
