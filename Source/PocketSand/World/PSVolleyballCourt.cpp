// Pocket Sand - volleyball court.

#include "World/PSVolleyballCourt.h"
#include "AI/PSInmate.h"
#include "Data/PSRoundConfig.h"
#include "Feedback/PSFeedback.h"
#include "Framework/PSEventSubsystem.h"
#include "Interfaces/PSInterfaces.h"
#include "World/PSArt.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

APSVolleyballCourt::APSVolleyballCourt()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Ball = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ball"));
	Ball->SetupAttachment(RootComponent);
	Ball->SetUsingAbsoluteLocation(true);
	Ball->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Ball->ComponentTags.Add(TEXT("NoCollide")); // a ball in flight
	PlayerClass = APSInmate::StaticClass();
}

FVector2D APSVolleyballCourt::Centre() const
{
	if (Cfg)
	{
		for (const FPSRegionDef& R : Cfg->Regions)
		{
			if (R.Surface == EPSSurface::Court)
			{
				return R.Center;
			}
		}
	}
	return FVector2D(GetActorLocation());
}

void APSVolleyballCourt::Setup(const UPSRoundConfig* Config)
{
	Cfg = Config;
	if (!Cfg || !GetWorld())
	{
		return;
	}
	Ball->SetStaticMesh(PSArt::SphereMesh());
	Ball->SetMaterial(0, PSArt::Mat(EPSMat::Ball));
	Ball->SetWorldScale3D(FVector(BallScale));
	const FVector2D C = Centre();
	const FVector2D Offset = PlayerOffset * Cfg->RegionScale;
	// West pair, then east pair (the net runs north-south through the centre).
	Spots = { C + FVector2D(-Offset.X, -Offset.Y), C + FVector2D(-Offset.X, Offset.Y), C + FVector2D(Offset.X, Offset.Y), C + FVector2D(Offset.X, -Offset.Y) };
	FRandomStream Look(4242);
	for (int32 I = 0; I < Spots.Num(); ++I)
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* A = GetWorld()->SpawnActor<AActor>(PlayerClass, FTransform(FVector(Spots[I], 3.0f)), P);
		if (IPSCrowdAgent* Agent = Cast<IPSCrowdAgent>(A))
		{
			FPSOutfit O;
			// Shirts off for volleyball: a grey vest over skin reads differently from the walking crowd.
			O.Top = FLinearColor(0.42f, 0.42f, 0.4f) * Look.FRandRange(0.8f, 1.1f);
			O.Bottom = FLinearColor(0.09f, 0.11f, 0.16f);
			O.Shoes = FLinearColor(0.05f, 0.05f, 0.05f);
			O.SkinTone = Look.FRand();
			O.Hair = Look.RandRange(0, 2);
			O.SleeveEnd = 18.0f;
			if (Cfg->PlayerAvatars.Num() > 0)
			{
				O.Avatar = Cfg->PlayerAvatars[I % Cfg->PlayerAvatars.Num()];
			}
			Agent->PS_Dress(Cfg, O);
			Agent->PS_SetIdleStyle(0);
		}
		Players.Add(A);
	}
	ResetCourt();
}

void APSVolleyballCourt::ResetCourt()
{
	Rng.Initialize(77);
	From = 0;
	To = 2;
	Clock = 0.0f;
	Leg = Cfg ? Cfg->RallyPassSeconds : 1.6f;
	SpikeStage = 0;
	const FVector2D C = Centre();
	const FVector2D Offset = PlayerOffset * Cfg->RegionScale;
	for (int32 I = 0; I < Players.Num(); ++I)
	{
		if (IPSCrowdAgent* Agent = Cast<IPSCrowdAgent>(Players[I].Get()))
		{
			const FVector2D Face = C - Spots[I];
			Agent->PS_SetPose(FVector(Spots[I], 3.0f), FMath::RadiansToDegrees(FMath::Atan2(0.0f, Face.X)), 0.0f);
			Agent->PS_PlayAction(FString(), false, 1.0f);
		}
	}
}

FVector APSVolleyballCourt::HandPos(int32 Index) const
{
	return FVector(Spots.IsValidIndex(Index) ? Spots[Index] : FVector2D::ZeroVector, HandHeight);
}

void APSVolleyballCourt::Hit(int32 Index, bool bSpike)
{
	if (IPSCrowdAgent* Agent = Players.IsValidIndex(Index) ? Cast<IPSCrowdAgent>(Players[Index].Get()) : nullptr)
	{
		Agent->PS_PlayAction(bSpike ? TEXT("Punch_Cross") : TEXT("Interact"), false, bSpike ? 1.4f : 1.6f);
	}
	PSFeedback::Sound(this, TEXT("Bump"), bSpike ? 0.0f : 0.35f, FMath::FRandRange(0.94f, 1.06f));
}

void APSVolleyballCourt::TriggerSpike()
{
	if (SpikeStage > 0 || !Cfg)
	{
		return;
	}
	SpikeStage = 1;
	SpikeClock = 0.0f;
	// West front player sets, the other jumps and smashes into the east half.
	const FVector2D C = Centre();
	const FVector2D Offset = PlayerOffset * Cfg->RegionScale;
	SpikeFrom = HandPos(1) + FVector(0.0f, 0.0f, 80.0f);
	SpikeTo = FVector(C + FVector2D(Rng.FRandRange(180.0f, 420.0f), Rng.FRandRange(-220.0f, 220.0f)), 4.0f);
	if (UPSEventSubsystem* Bus = UPSEventSubsystem::Get(this))
	{
		Bus->OnSpikeWarning.Broadcast(SpikeTo, 0.0f);
	}
}

void APSVolleyballCourt::Step(float Dt)
{
	if (!Cfg || Spots.Num() < 4)
	{
		return;
	}
	if (SpikeStage > 0)
	{
		SpikeClock += Dt;
		const float Tele = Cfg->SpikeTelegraph;
		if (SpikeStage == 1)
		{
			// Set: the ball floats up over the spiker while everyone looks.
			const float U = FMath::Clamp(SpikeClock / Tele, 0.0f, 1.0f);
			const FVector Start = Ball->GetComponentLocation();
			Ball->SetWorldLocation(FMath::Lerp(Start, SpikeFrom + FVector(0.0f, 0.0f, SpikeApex * FMath::Sin(U * UE_PI) * 0.6f), FMath::Clamp(Dt * 4.0f, 0.0f, 1.0f)));
			if (SpikeClock > Tele - 0.45f)
			{
				if (IPSCrowdAgent* Agent = Cast<IPSCrowdAgent>(Players[1].Get()))
				{
					Agent->PS_PlayAction(TEXT("Jump_Start"), false, 1.2f);
				}
			}
			if (SpikeClock >= Tele)
			{
				SpikeStage = 2;
				SpikeClock = 0.0f;
				SpikeFrom = Ball->GetComponentLocation();
				Hit(1, true);
			}
		}
		else if (SpikeStage == 2)
		{
			const float Dur = 0.28f;
			const float U = FMath::Clamp(SpikeClock / Dur, 0.0f, 1.0f);
			Ball->SetWorldLocation(FMath::Lerp(SpikeFrom, SpikeTo + FVector(0.0f, 0.0f, 11.0f), U));
			if (U >= 1.0f)
			{
				SpikeStage = 3;
				SpikeClock = 0.0f;
				OnSpikeImpact.Broadcast(SpikeTo);
			}
		}
		else
		{
			// Bounce and roll, then the server picks it up and the rally restarts.
			const float U = FMath::Clamp(SpikeClock / 1.2f, 0.0f, 1.0f);
			const FVector Roll = SpikeTo + FVector((Spots[3] - FVector2D(SpikeTo)) * U, 11.0f + 120.0f * FMath::Sin(FMath::Min(U * 3.0f, 1.0f) * UE_PI) * (1.0f - U));
			Ball->SetWorldLocation(Roll);
			if (U >= 1.0f)
			{
				SpikeStage = 0;
				From = 3;
				To = 0;
				Clock = 0.0f;
				for (AActor* P : Players)
				{
					if (IPSCrowdAgent* Agent = Cast<IPSCrowdAgent>(P))
					{
						Agent->PS_PlayAction(FString(), false, 1.0f);
					}
				}
			}
		}
		return;
	}
	// Rally: lob across the net (and sometimes to a team-mate) on a parabola.
	Clock += Dt;
	const float U = FMath::Clamp(Clock / Leg, 0.0f, 1.0f);
	const FVector A = HandPos(From);
	const FVector B = HandPos(To);
	FVector P = FMath::Lerp(A, B, U);
	P.Z += PassApex * 4.0f * U * (1.0f - U);
	Ball->SetWorldLocation(P);
	if (U >= 1.0f)
	{
		Hit(To, false);
		From = To;
		const bool bWest = From < 2;
		To = Rng.FRand() < 0.3f ? (bWest ? 1 - From : 5 - From) : (bWest ? Rng.RandRange(2, 3) : Rng.RandRange(0, 1));
		Clock = 0.0f;
		Leg = (Cfg->RallyPassSeconds) * Rng.FRandRange(0.85f, 1.15f);
	}
}
