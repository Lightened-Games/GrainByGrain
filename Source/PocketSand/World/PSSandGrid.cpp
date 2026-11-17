// Pocket Sand - yard floor + sand rendering.

#include "World/PSSandGrid.h"
#include "World/PSArt.h"
#include "World/PSMeshLib.h"
#include "Data/PSRoundConfig.h"
#include "Framework/PSEventSubsystem.h"
#include "PocketSand.h"
#include "ProceduralMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/App.h"

APSSandGrid::APSSandGrid()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Floor = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Floor"));
	Floor->SetupAttachment(RootComponent);
	Floor->bUseComplexAsSimpleCollision = true;
	Floor->SetCollisionProfileName(TEXT("BlockAll"));
	Mounds = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Mounds"));
	Mounds->SetupAttachment(RootComponent);
	Mounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mounds->SetCastShadow(true);
	Mounds->NumCustomDataFloats = 0;
}

void APSSandGrid::Init(const UPSRoundConfig* Config)
{
	Cfg = Config;
	if (!Cfg)
	{
		return;
	}
	TArray<FPSSurfaceRow> Rows;
	for (int32 I = 0; I < 6; ++I)
	{
		Rows.Add(Cfg->GetSurface(static_cast<EPSSurface>(I)));
	}
	const FVector2D ApronCenter(Cfg->DoorLocation.X, Cfg->DoorLocation.Y - Cfg->ApronHalf.Y);
	Sand.Init(Cfg->YardSize, Cfg->CellSize, Rows, Cfg->Regions, ApronCenter, Cfg->ApronHalf, Cfg->SpillOffset);
	LastFill.Init(0.0f, Sand.Regions.Num());

	if (FApp::CanEverRender())
	{
		Mask = UTexture2D::CreateTransient(Sand.W, Sand.H, PF_B8G8R8A8, TEXT("T_PS_SandMask"));
		if (Mask)
		{
			Mask->Filter = TF_Bilinear;
			Mask->SRGB = false;
			Mask->AddressX = TA_Clamp;
			Mask->AddressY = TA_Clamp;
			Mask->NeverStream = true;
			Mask->UpdateResource();
		}
	}
	BuildFloor();
	if (UStaticMesh* Sphere = PSArt::SphereMesh())
	{
		Mounds->SetStaticMesh(Sphere);
		Mounds->SetMaterial(0, PSArt::Mat(EPSMat::Mound));
	}
	ResetSand();
}

void APSSandGrid::ApplyMaskParams(UMaterialInstanceDynamic* MID) const
{
	if (!MID || !Cfg)
	{
		return;
	}
	if (Mask)
	{
		MID->SetTextureParameterValue(TEXT("SandMask"), Mask);
	}
	MID->SetVectorParameterValue(TEXT("YardOrigin"), FLinearColor(Sand.Origin.X, Sand.Origin.Y, 0.0f, 0.0f));
	MID->SetVectorParameterValue(TEXT("YardSize"), FLinearColor(Sand.W * Sand.Cell, Sand.H * Sand.Cell, 0.0f, 0.0f));
}

void APSSandGrid::BuildFloor()
{
	if (!Cfg)
	{
		return;
	}
	const FVector2D Size = Cfg->YardSize;
	// Section 0: the gravel yard (the apron is painted by the ground material through the same mask).
	FPSMeshData Yard;
	PSMesh::AddQuad(Yard, FVector(0, 0, 0), FVector(Size.X, 0, 0), FVector(Size.X, Size.Y, 0), FVector(0, Size.Y, 0), FVector::UpVector, 1.0f);
	// Section 1: the concrete apron outside the cell-block door, raised 1 cm.
	FPSMeshData Apron;
	const FVector2D AC(Cfg->DoorLocation.X, Cfg->DoorLocation.Y - Cfg->ApronHalf.Y);
	const FVector2D AH = Cfg->ApronHalf;
	PSMesh::AddBox(Apron, FTransform(FVector(AC.X, AC.Y, -4.0f)), FVector(AH.X, AH.Y, 5.0f), 2.0f, 1.0f);
	// Section 2: outer ground well beyond the walls (never black void).
	FPSMeshData Outer;
	const FVector2D C = Size * 0.5f;
	const FVector2D H = Size * 0.5f * OuterGroundScale;
	PSMesh::AddQuad(Outer, FVector(C.X - H.X, C.Y - H.Y, -3.0f), FVector(C.X + H.X, C.Y - H.Y, -3.0f), FVector(C.X + H.X, C.Y + H.Y, -3.0f), FVector(C.X - H.X, C.Y + H.Y, -3.0f), FVector::UpVector, 1.0f);

	if (UMaterialInterface* M = PSArt::Mat(EPSMat::Gravel))
	{
		FloorMID = UMaterialInstanceDynamic::Create(M, this);
		ApplyMaskParams(FloorMID);
	}
	UMaterialInstanceDynamic* ApronMID = nullptr;
	if (UMaterialInterface* M = PSArt::Mat(EPSMat::Apron))
	{
		ApronMID = UMaterialInstanceDynamic::Create(M, this);
		ApplyMaskParams(ApronMID);
	}
	PSMesh::Commit(Floor, 0, Yard, FloorMID, true);
	PSMesh::Commit(Floor, 1, Apron, ApronMID, true);
	PSMesh::Commit(Floor, 2, Outer, PSArt::Mat(EPSMat::Outer), true);
}

void APSSandGrid::ResetSand()
{
	Sand.Reset();
	for (float& F : LastFill)
	{
		F = 0.0f;
	}
	bDirty = true;
	UploadMask();
	RebuildMounds();
	if (UPSEventSubsystem* Bus = UPSEventSubsystem::Get(this))
	{
		for (int32 I = 0; I < Sand.Regions.Num(); ++I)
		{
			const FPSSandField::FRegion& R = Sand.Regions[I];
			Bus->OnRegionFill.Broadcast(R.Def.Id, 0.0f, FVector(R.Def.Center, R.Def.TopZ));
		}
	}
}

float APSSandGrid::Pour(const FVector& Where, float Amount, float Radius)
{
	bDirty = true;
	return Sand.Deposit(FVector2D(Where), Amount, Radius);
}

float APSSandGrid::SurfaceTopZ(const FVector2D& P) const
{
	const int32 R = Sand.RegionAt(P);
	return R >= 0 ? Sand.Regions[R].Def.TopZ : 0.0f;
}

int32 APSSandGrid::NumMounds() const
{
	return Mounds ? Mounds->GetInstanceCount() : 0;
}

void APSSandGrid::Step(float Dt)
{
	bDirty = true;
	UPSEventSubsystem* Bus = UPSEventSubsystem::Get(this);
	for (int32 I = 0; I < Sand.Regions.Num(); ++I)
	{
		const FPSSandField::FRegion& R = Sand.Regions[I];
		const float F = R.Fraction();
		if (FMath::Abs(F - LastFill[I]) >= FillBroadcastStep || (F >= 1.0f && LastFill[I] < 1.0f) || (F <= 0.0f && LastFill[I] > 0.0f))
		{
			LastFill[I] = F;
			if (Bus)
			{
				Bus->OnRegionFill.Broadcast(R.Def.Id, F, FVector(R.Def.Center, R.Def.TopZ));
			}
		}
	}
}

void APSSandGrid::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TexClock += DeltaSeconds;
	MoundClock += DeltaSeconds;
	if (TexClock >= TextureInterval && bDirty)
	{
		TexClock = 0.0f;
		UploadMask();
	}
	if (MoundClock >= MoundInterval)
	{
		MoundClock = 0.0f;
		RebuildMounds();
	}
}

void APSSandGrid::UploadMask()
{
	if (!Mask || !Mask->GetResource() || !Cfg)
	{
		return;
	}
	bDirty = false;
	const int32 N = Sand.W * Sand.H;
	uint8* Data = new uint8[N * 4];
	const float Full = FMath::Max(Cfg->FullShowDensity, 1e-5f);
	for (int32 I = 0; I < N; ++I)
	{
		const float S = Sand.Shown[I];
		const float Thr = Sand.RowAt(I).ShowThreshold;
		// R: visible sand strength. G: hidden sand (soil darkens slightly where it was raked in).
		const float Vis = S > Thr * 0.5f ? FMath::Clamp(S / Full, 0.0f, 1.0f) : 0.0f;
		const float Hid = FMath::Clamp(Sand.Hidden[I] / (Full * 2.0f), 0.0f, 1.0f);
		Data[I * 4 + 0] = 0;
		Data[I * 4 + 1] = static_cast<uint8>(Hid * 255.0f);
		Data[I * 4 + 2] = static_cast<uint8>(FMath::Sqrt(Vis) * 255.0f);
		Data[I * 4 + 3] = 255;
	}
	FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, Sand.W, Sand.H);
	Mask->UpdateTextureRegions(0, 1, Region, Sand.W * 4, 4, Data, [](uint8* SrcData, const FUpdateTextureRegion2D* Regions)
	{
		delete[] SrcData;
		delete Regions;
	});
}

void APSSandGrid::RebuildMounds()
{
	if (!Mounds || !Cfg || !Mounds->GetStaticMesh())
	{
		return;
	}
	TArray<FTransform> Xf;
	const float Pile = Cfg->PileThreshold;
	const float FullP = FMath::Max(Cfg->PileFullDensity, Pile + 1e-3f);
	for (int32 I : Sand.Active)
	{
		const float S = Sand.Shown[I];
		if (S < Pile)
		{
			continue;
		}
		const float A = FMath::Clamp((S - Pile) / (FullP - Pile), 0.0f, 1.0f);
		const FVector2D C = Sand.CellCenter(I);
		const float Top = Sand.Region[I] >= 0 ? Sand.Regions[Sand.Region[I]].Def.TopZ : 0.0f;
		const float Hgt = FMath::Lerp(MoundMinHeight, MoundMaxHeight, A);
		const float Wid = Sand.Cell * FMath::Lerp(1.4f, 2.1f, A);
		// Deterministic jitter per cell so heaps look poured, not gridded.
		const float J = FMath::Frac(FMath::Sin(I * 12.9898f) * 43758.5453f);
		Xf.Add(FTransform(FRotator(0.0f, J * 360.0f, 0.0f), FVector(C.X + (J - 0.5f) * 8.0f, C.Y + (0.5f - J) * 8.0f, Top), FVector(Wid / 100.0f, Wid * (0.85f + 0.3f * J) / 100.0f, Hgt * 2.0f / 100.0f)));
		if (Xf.Num() >= MaxMounds)
		{
			break;
		}
	}
	Mounds->ClearInstances();
	if (Xf.Num() > 0)
	{
		Mounds->AddInstances(Xf, false, false, false);
	}
}
