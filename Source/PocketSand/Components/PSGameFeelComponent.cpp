// Pocket Sand - game feel.

#include "Components/PSGameFeelComponent.h"
#include "Feedback/PSCameraShakes.h"
#include "Feedback/PSFeedback.h"
#include "Framework/PSEventSubsystem.h"
#include "Interfaces/PSInterfaces.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "HAL/PlatformTime.h"
#include "GameFramework/GameModeBase.h"
#include "Engine/World.h"

UPSGameFeelComponent::UPSGameFeelComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bTickEvenWhenPaused = true;
}

void UPSGameFeelComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UPSEventSubsystem* Bus = UPSEventSubsystem::Get(this))
	{
		Bus->OnStateChanged.AddDynamic(this, &UPSGameFeelComponent::HandleState);
		Bus->OnCountdown.AddDynamic(this, &UPSGameFeelComponent::HandleCountdown);
		Bus->OnPhase.AddDynamic(this, &UPSGameFeelComponent::HandlePhase);
		Bus->OnBagLoaded.AddDynamic(this, &UPSGameFeelComponent::HandleBagLoaded);
		Bus->OnBagEmptied.AddDynamic(this, &UPSGameFeelComponent::HandleBagEmptied);
		Bus->OnPourChanged.AddDynamic(this, &UPSGameFeelComponent::HandlePour);
		Bus->OnPatchNoticed.AddDynamic(this, &UPSGameFeelComponent::HandleNoticed);
		Bus->OnPatchFound.AddDynamic(this, &UPSGameFeelComponent::HandleFound);
		Bus->OnPatchMissed.AddDynamic(this, &UPSGameFeelComponent::HandleMissed);
		Bus->OnPoke.AddDynamic(this, &UPSGameFeelComponent::HandlePoke);
		Bus->OnWatch.AddDynamic(this, &UPSGameFeelComponent::HandleWatch);
		Bus->OnSuspicion.AddDynamic(this, &UPSGameFeelComponent::HandleSuspicion);
		Bus->OnRegionFill.AddDynamic(this, &UPSGameFeelComponent::HandleRegion);
		Bus->OnSpike.AddDynamic(this, &UPSGameFeelComponent::HandleSpike);
		Bus->OnInput.AddDynamic(this, &UPSGameFeelComponent::HandleInput);
		Bus->OnPrompt.AddDynamic(this, &UPSGameFeelComponent::HandlePrompt);
		Bus->OnFootstep.AddDynamic(this, &UPSGameFeelComponent::HandleFootstep);
	}
	float Scale = 1.0f;
	if (FParse::Value(FCommandLine::Get(), TEXT("PSTimeScale="), Scale) && Scale > 0.0f)
	{
		BaseDilation = Scale;
	}
	YardLoop = PSFeedback::Loop(this, TEXT("Yard"), YardVolume);
	PourLoop = PSFeedback::Loop(this, TEXT("Pour"), 0.0f);
	MenuMusic = PSFeedback::Loop(this, TEXT("MusicMenu"), 0.001f);
	PlayMusic = PSFeedback::Loop(this, TEXT("MusicPlay"), 0.001f);
	if (const IPSMatchInfo* M = GetWorld() ? Cast<IPSMatchInfo>(GetWorld()->GetAuthGameMode()) : nullptr)
	{
		MusicState = M->PS_State();
	}
}

void UPSGameFeelComponent::UpdateMusic(float RealDt)
{
	const bool bPlaying = MusicState == EPSMatchState::Ready || MusicState == EPSMatchState::Active;
	const float Duck = Suspicion >= HeartbeatAt ? MusicDuck : 1.0f;
	MenuLevel = FMath::FInterpConstantTo(MenuLevel, MusicState == EPSMatchState::Menu ? 1.0f : 0.0f, RealDt, MusicFadeRate);
	PlayLevel = FMath::FInterpConstantTo(PlayLevel, bPlaying ? Duck : 0.0f, RealDt, MusicState == EPSMatchState::Active || MusicState == EPSMatchState::Ready ? MusicFadeRate * 0.5f : MusicFadeRate * 2.0f);
	if (MenuMusic) { MenuMusic->SetVolumeMultiplier(FMath::Max(0.001f, MenuLevel * MenuMusicVolume)); }
	if (PlayMusic) { PlayMusic->SetVolumeMultiplier(FMath::Max(0.001f, PlayLevel * PlayMusicVolume)); }
}

void UPSGameFeelComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const float RealDt = FMath::Min(static_cast<float>(FApp::GetDeltaTime()), 0.1f);
	ApplyDilation();
	UpdateMusic(RealDt);
	// The trickle swells in fast and tails off (sand keeps running a moment after you let go).
	PourLevel = FMath::FInterpTo(PourLevel, bPouring ? 1.0f : 0.0f, RealDt, bPouring ? 14.0f : 5.0f);
	if (PourLoop)
	{
		PourLoop->SetVolumeMultiplier(FMath::Max(0.001f, PourLevel * PourVolume));
	}
	if (Suspicion >= HeartbeatAt)
	{
		HeartClock -= RealDt;
		if (HeartClock <= 0.0f)
		{
			HeartClock = FMath::Lerp(HeartbeatInterval, HeartbeatInterval * 0.55f, (Suspicion - HeartbeatAt) / FMath::Max(1.0f - HeartbeatAt, 0.01f));
			PSFeedback::Sound(this, TEXT("Heartbeat"), 0.7f);
		}
	}
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	const IPSSandCarrier* C = PC ? Cast<IPSSandCarrier>(PC->GetPawn()) : nullptr;
	bHeavyStep = C && C->PS_IsWaddling();
}

void UPSGameFeelComponent::ApplyDilation()
{
	const bool bHit = HitStopUntil > 0.0 && FPlatformTime::Seconds() < HitStopUntil;
	if (!bHit)
	{
		HitStopUntil = 0.0;
	}
	const float Want = BaseDilation * (bHit ? HitStopDilation : 1.0f);
	if (!FMath::IsNearlyEqual(Want, AppliedDilation, 1e-4f))
	{
		AppliedDilation = Want;
		UGameplayStatics::SetGlobalTimeDilation(this, Want);
	}
}

void UPSGameFeelComponent::HitStop()
{
	HitStopUntil = FPlatformTime::Seconds() + HitStopSeconds;
	ApplyDilation();
}

void UPSGameFeelComponent::Shake(bool bBig, float Scale)
{
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (PC && PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->StartCameraShake(bBig ? UPSImpactShake::StaticClass() : UPSMicroShake::StaticClass(), Scale);
	}
}

void UPSGameFeelComponent::HandleState(EPSMatchState State, EPSDefeat Reason)
{
	MusicState = State;
	if (State == EPSMatchState::Ready)
	{
		FullRegions.Reset();
		Suspicion = 0.0f;
	}
	if (State == EPSMatchState::Victory)
	{
		PSFeedback::Sound(this, TEXT("Whistle"), 0.6f);
		PSFeedback::Sound(this, TEXT("Victory"), 0.9f);
	}
	else if (State == EPSMatchState::Defeat)
	{
		if (Reason == EPSDefeat::Whistle)
		{
			PSFeedback::Sound(this, TEXT("Whistle"), 0.8f);
		}
		PSFeedback::Sound(this, TEXT("Defeat"), 0.9f);
		Shake(true, 0.7f);
	}
	if (State != EPSMatchState::Active)
	{
		bPouring = false;
	}
}

void UPSGameFeelComponent::HandleCountdown(int32 SecondsLeft)
{
	PSFeedback::Sound(this, SecondsLeft > 0 ? TEXT("Tick") : TEXT("Go"), 0.7f);
}

void UPSGameFeelComponent::HandlePhase(int32 Index, FText Title, FText Sub)
{
	PSFeedback::Sound(this, TEXT("Go"), 0.45f, 0.85f);
}

void UPSGameFeelComponent::HandleBagLoaded(int32 Count, FVector Location)
{
	PSFeedback::Sound(this, TEXT("BagLoad"), 0.8f, Count > 1 ? 0.9f : 1.0f);
	PSFeedback::Vfx(this, TEXT("Dust"), Location + FVector(0.0f, 0.0f, -80.0f), FRotator::ZeroRotator, 0.6f);
	Shake(false, 0.5f);
}

void UPSGameFeelComponent::HandleBagEmptied(int32 Count, FVector Location)
{
	PSFeedback::Sound(this, TEXT("BagEmpty"), 0.8f);
	PSFeedback::Vfx(this, TEXT("Puff"), Location + FVector(0.0f, 0.0f, 20.0f), FRotator::ZeroRotator, 1.0f);
}

void UPSGameFeelComponent::HandlePour(bool bOn, FVector Location)
{
	bPouring = bOn;
}

void UPSGameFeelComponent::HandleNoticed(FVector Location, AActor* Who)
{
	PSFeedback::Sound(this, TEXT("Notice"), 0.8f, FMath::FRandRange(0.95f, 1.05f));
}

void UPSGameFeelComponent::HandleFound(int32 Strikes, FVector Location, bool bProbe)
{
	PSFeedback::Sound(this, TEXT("Strike"), 0.9f);
	PSFeedback::Vfx(this, TEXT("Dust"), Location + FVector(0.0f, 0.0f, 10.0f), FRotator::ZeroRotator, 1.0f);
	Shake(true, 0.8f);
	HitStop();
}

void UPSGameFeelComponent::HandleMissed(FVector Location, AActor* Who)
{
	PSFeedback::Sound(this, TEXT("Shrug"), 0.7f);
}

void UPSGameFeelComponent::HandlePoke(FVector Location, AActor* Who)
{
	PSFeedback::Sound(this, TEXT("Stick"), 0.7f);
	PSFeedback::Vfx(this, TEXT("Dust"), Location + FVector(0.0f, 0.0f, 5.0f), FRotator::ZeroRotator, 0.5f);
}

void UPSGameFeelComponent::HandleWatch(FVector Location, AActor* Who)
{
	PSFeedback::Sound(this, TEXT("Notice"), 0.45f, 0.8f);
}

void UPSGameFeelComponent::HandleSuspicion(float Value, float Delta)
{
	if (Value >= HeartbeatAt && Suspicion < HeartbeatAt)
	{
		HeartClock = 0.0f;
	}
	Suspicion = Value;
}

void UPSGameFeelComponent::HandleRegion(FName RegionId, float Fraction, FVector Location)
{
	if (Fraction >= 1.0f && !FullRegions.Contains(RegionId))
	{
		FullRegions.Add(RegionId);
		PSFeedback::Sound(this, TEXT("SurfaceFull"), 0.6f);
	}
}

void UPSGameFeelComponent::HandleSpike(FVector Location, float Scattered)
{
	PSFeedback::Sound(this, TEXT("Spike"), 0.95f);
	PSFeedback::Vfx(this, TEXT("Puff"), Location + FVector(0.0f, 0.0f, 15.0f), FRotator::ZeroRotator, Scattered > 0.0f ? 1.6f : 0.8f);
	Shake(false, 0.8f);
}

void UPSGameFeelComponent::HandleInput(FName KeyId, bool bSimulated)
{
	PSFeedback::Sound(this, TEXT("Key"), 0.25f);
}

void UPSGameFeelComponent::HandlePrompt(FText Text, FVector WorldLocation, bool bDanger)
{
	PSFeedback::Sound(this, TEXT("Chime"), 0.5f, bDanger ? 0.8f : 1.0f);
}

void UPSGameFeelComponent::HandleFootstep(FVector Location)
{
	PSFeedback::Sound(this, bHeavyStep ? TEXT("StepHeavy") : TEXT("Step"), StepVolume * FMath::FRandRange(0.85f, 1.0f), FMath::FRandRange(0.92f, 1.08f));
	if (bHeavyStep)
	{
		PSFeedback::Vfx(this, TEXT("Dust"), Location, FRotator::ZeroRotator, 0.35f);
	}
}
