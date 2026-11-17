// Pocket Sand - yard life.

#include "World/PSYardLife.h"
#include "World/PSArt.h"
#include "Data/PSRoundConfig.h"
#include "Feedback/PSFeedback.h"
#include "Interfaces/PSInterfaces.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"

APSYardLife::APSYardLife()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

UStaticMeshComponent* APSYardLife::AddPiece(USceneComponent* Parent, UStaticMesh* Mesh, const FTransform& Xf, const FLinearColor& Color, bool bSolid)
{
	if (!Mesh)
	{
		return nullptr;
	}
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(Mesh);
	C->SetupAttachment(Parent);
	C->SetRelativeTransform(Xf);
	C->SetMobility(EComponentMobility::Movable);
	// Cloth, rope and a flag up on the tower roof: nothing a character can reach or should block on (C2 exemption).
	C->SetCollisionEnabled(bSolid ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	C->SetCanEverAffectNavigation(bSolid);
	if (!bSolid)
	{
		C->ComponentTags.Add(TEXT("NoCollide"));
	}
	if (UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(PSArt::Mat(EPSMat::Cloth), this))
	{
		MID->SetVectorParameterValue(TEXT("Color"), Color);
		C->SetMaterial(0, MID);
	}
	C->RegisterComponent();
	AddInstanceComponent(C);
	return C;
}

void APSYardLife::Build(const UPSRoundConfig* Cfg)
{
	if (!Cfg)
	{
		return;
	}
	Rng.Initialize(Seed);
	UStaticMesh* Cube = PSArt::CubeMesh();
	UStaticMesh* Cyl = PSArt::CylinderMesh();
	const FVector2D S = Cfg->YardSize;

	// Laundry: a sagging line between two cell windows, garments pegged on it.
	const float X1 = WindowSpacing * (0.5f + LaundryWindows.X);
	const float X2 = FMath::Min(WindowSpacing * (0.5f + LaundryWindows.Y), S.X - WindowSpacing * 0.5f);
	const float Y = S.Y - LaundryOut;
	auto LinePoint = [&](float U) { return FVector(FMath::Lerp(X1, X2, U), Y, LaundryHeight - LaundrySag * 4.0f * U * (1.0f - U)); };
	const FLinearColor Rope(0.55f, 0.52f, 0.45f);
	const int32 RopeSegs = 10;
	for (int32 I = 0; I < RopeSegs; ++I)
	{
		const FVector A = LinePoint(static_cast<float>(I) / RopeSegs);
		const FVector B = LinePoint(static_cast<float>(I + 1) / RopeSegs);
		const FVector D = B - A;
		AddPiece(RootComponent, Cube, FTransform(D.Rotation(), (A + B) * 0.5f, FVector(D.Size() / 100.0f, 0.008f, 0.008f)), Rope, false);
	}
	// Short hooks back to the wall at each end.
	for (const FVector& End : { LinePoint(0.0f), LinePoint(1.0f) })
	{
		AddPiece(RootComponent, Cube, FTransform(FRotator::ZeroRotator, End + FVector(0.0f, LaundryOut * 0.5f, 0.0f), FVector(0.02f, LaundryOut / 100.0f, 0.02f)), FLinearColor(0.2f, 0.2f, 0.2f), false);
	}
	const int32 N = GarmentColors.Num();
	for (int32 I = 0; I < N; ++I)
	{
		const float U = (I + 0.75f + Rng.FRandRange(-0.15f, 0.15f)) / (N + 0.5f);
		USceneComponent* Pivot = NewObject<USceneComponent>(this);
		Pivot->SetupAttachment(RootComponent);
		Pivot->SetMobility(EComponentMobility::Movable);
		Pivot->SetRelativeLocation(LinePoint(U));
		Pivot->RegisterComponent();
		AddInstanceComponent(Pivot);
		const FVector2D Sz = GarmentSize * Rng.FRandRange(0.8f, 1.1f);
		AddPiece(Pivot, Cube, FTransform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, -Sz.Y * 0.5f - 1.0f), FVector(Sz.X / 100.0f, 0.012f, Sz.Y / 100.0f)), GarmentColors[I], false);
		Garments.Add({ Pivot, static_cast<float>(Rng.FRandRange(0.0f, UE_TWO_PI)), static_cast<float>(Rng.FRandRange(0.8f, 1.25f)) });
	}

	// Flag: pole on the tower roof, a chain of segments that ripple downwind.
	const FVector Base = Cfg->TowerLocation + FVector(0.0f, 0.0f, Cfg->TowerHeight + FlagRoofOffset);
	AddPiece(RootComponent, Cyl, FTransform(FRotator::ZeroRotator, Base + FVector(0.0f, 0.0f, FlagPoleHeight * 0.5f), FVector(0.045f, 0.045f, FlagPoleHeight / 100.0f)), FLinearColor(0.4f, 0.4f, 0.4f), false);
	USceneComponent* Parent = RootComponent;
	FVector At = Base + FVector(0.0f, 0.0f, FlagPoleHeight - FlagSize.Y * 0.5f - 4.0f);
	const float SegW = FlagSize.X / FMath::Max(FlagSegments, 1);
	for (int32 I = 0; I < FlagSegments; ++I)
	{
		USceneComponent* Joint = NewObject<USceneComponent>(this);
		Joint->SetupAttachment(Parent);
		Joint->SetMobility(EComponentMobility::Movable);
		Joint->SetRelativeLocation(I == 0 ? At : FVector(SegW, 0.0f, 0.0f));
		Joint->RegisterComponent();
		AddInstanceComponent(Joint);
		AddPiece(Joint, Cube, FTransform(FRotator::ZeroRotator, FVector(SegW * 0.5f, 0.0f, 0.0f), FVector(SegW / 100.0f * 1.04f, 0.01f, FlagSize.Y / 100.0f)), FlagColor, false);
		FlagJoints.Add(Joint);
		Parent = Joint;
	}
	NextTannoy = -1.0f;
}

void APSYardLife::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Clock += DeltaSeconds;
	const float W = Clock * WindSpeed;
	// Gusts: a slow swell on top of the steady breeze.
	const float Gust = 0.65f + 0.35f * FMath::Sin(W * 0.37f) * FMath::Sin(W * 0.11f + 1.3f);
	for (const FSwing& G : Garments)
	{
		if (G.Pivot)
		{
			const float A = GarmentSwayDeg * Gust * (0.6f * FMath::Sin(W * 1.9f * G.Rate + G.Phase) + 0.4f * FMath::Sin(W * 3.1f * G.Rate + G.Phase * 2.0f));
			G.Pivot->SetRelativeRotation(FRotator(0.0f, A * 0.3f, A));
		}
	}
	for (int32 I = 0; I < FlagJoints.Num(); ++I)
	{
		if (FlagJoints[I])
		{
			const float Ripple = FlagRippleDeg * Gust * FMath::Sin(W * 4.2f - I * 1.1f);
			const float Yaw = I == 0 ? WindYaw + 8.0f * FMath::Sin(W * 0.7f) + Ripple * 0.5f : Ripple;
			FlagJoints[I]->SetRelativeRotation(FRotator(I == 0 ? 0.0f : -2.0f * Gust, Yaw, 0.0f));
		}
	}
	// Tannoy: only while the round runs; the first one lands early (a moment inside the first 20 s).
	const IPSMatchInfo* M = GetWorld() ? Cast<IPSMatchInfo>(GetWorld()->GetAuthGameMode()) : nullptr;
	if (M && M->PS_State() == EPSMatchState::Active)
	{
		if (NextTannoy < 0.0f)
		{
			NextTannoy = Clock + Rng.FRandRange(TannoyInterval.X * 0.4f, TannoyInterval.X * 0.55f);
		}
		else if (Clock >= NextTannoy)
		{
			PSFeedback::Sound(this, TEXT("Tannoy"), TannoyVolume);
			NextTannoy = Clock + Rng.FRandRange(TannoyInterval.X, TannoyInterval.Y);
		}
	}
	else
	{
		NextTannoy = -1.0f;
	}
}
