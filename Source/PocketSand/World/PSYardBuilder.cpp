// Pocket Sand - yard builder.

#include "World/PSYardBuilder.h"
#include "AI/PSNav.h"
#include "World/PSArt.h"
#include "World/PSSandGrid.h"
#include "Data/PSRoundConfig.h"
#include "Framework/PSEventSubsystem.h"
#include "PocketSand.h"
#include "ProceduralMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/BoxComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "HAL/FileManager.h"

namespace
{
	FTransform At(const FVector& P, float Yaw = 0.0f) { return FTransform(FRotator(0.0f, Yaw, 0.0f), P); }

	/** Torus (one razor-wire loop) around local Z. */
	void AddTorus(FPSMeshData& D, const FTransform& Xf, float R, float Tube, int32 Segs)
	{
		TArray<FVector2D> Profile;
		for (int32 I = 0; I <= 6; ++I)
		{
			const float A = UE_TWO_PI * I / 6.0f;
			Profile.Add(FVector2D(R + FMath::Cos(A) * Tube, FMath::Sin(A) * Tube));
		}
		PSMesh::AddLathe(D, Xf, Profile, Segs, 2.0f);
	}
}

APSYardBuilder::APSYardBuilder()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Structure = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Structure"));
	Structure->SetupAttachment(RootComponent);
	Structure->bUseComplexAsSimpleCollision = true;
	Surfaces = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Surfaces"));
	Surfaces->SetupAttachment(RootComponent);
	Surfaces->bUseComplexAsSimpleCollision = true;
	DoorLamp = CreateDefaultSubobject<UPointLightComponent>(TEXT("DoorLamp"));
	DoorLamp->SetupAttachment(RootComponent);
	DoorLamp->SetLightColor(FLinearColor(1.0f, 0.78f, 0.52f));
	DoorLamp->SetAttenuationRadius(900.0f);
	DoorLamp->SetCastShadows(false);
}

void APSYardBuilder::Build(const UPSRoundConfig* Config, APSSandGrid* Grid)
{
	Cfg = Config;
	if (!Cfg)
	{
		return;
	}
	Pieces.Reset();
	Footprints.Reset();
	PendingFoliage.Reset();
	BuildPerimeter();
	BuildCellBlock();
	BuildTower();
	BuildFurniture();
	BuildRegions(Grid);
	if (bBuildSkyline)
	{
		BuildSkyline();
	}
	Commit(Grid);
	BuildProps();
	FlushFoliage();
	if (UPSEventSubsystem* Bus = UPSEventSubsystem::Get(this))
	{
		Bus->OnRegionFill.AddUniqueDynamic(this, &APSYardBuilder::HandleRegionFill);
	}
}

// ---------------------------------------------------------------------------------------------- perimeter

void APSYardBuilder::AddRazor(FPSMeshData& D, const FVector& A, const FVector& B)
{
	const FVector Dir = (B - A).GetSafeNormal();
	const float Len = FVector::Dist(A, B);
	const FRotator Face = Dir.Rotation();
	int32 K = 0;
	for (float S = 0.0f; S < Len; S += RazorLoopSpacing, ++K)
	{
		// Each loop faces along the wall, tilted a little so the coil reads as a spiral.
		const FQuat Q = FQuat(Face) * FQuat(FVector(0, 1, 0), FMath::DegreesToRadians(90.0f)) * FQuat(FVector(1, 0, 0), FMath::DegreesToRadians((K % 2) ? 14.0f : -14.0f));
		AddTorus(D, FTransform(Q, A + Dir * S), RazorRadius, 0.9f, 10);
	}
}

void APSYardBuilder::AddFence(const FVector& A, const FVector& B, float BaseZ)
{
	const FVector Dir = (B - A).GetSafeNormal2D();
	const float Len = FVector::Dist2D(A, B);
	const float Yaw = Dir.Rotation().Yaw;
	// Real chain-link panels (Poly Haven) go on in BuildProps; the razor coil runs along their tops.

	AddRazor(Piece("Razor"), FVector(A.X, A.Y, BaseZ + FenceHeight + RazorRadius * 0.7f), FVector(B.X, B.Y, BaseZ + FenceHeight + RazorRadius * 0.7f));
}

void APSYardBuilder::BuildPerimeter()
{
	const FVector2D S = Cfg->YardSize;
	const float H = Cfg->WallHeight;
	const float T = WallThickness;
	// South, west and east walls (the cell block closes the north side).
	PSMesh::AddBox(Piece("Concrete"), At(FVector(S.X * 0.5f, -T * 0.5f, H * 0.5f)), FVector(S.X * 0.5f + T, T * 0.5f, H * 0.5f), 3.0f, 0.5f);
	PSMesh::AddBox(Piece("Concrete"), At(FVector(-T * 0.5f, S.Y * 0.5f, H * 0.5f)), FVector(T * 0.5f, S.Y * 0.5f, H * 0.5f), 3.0f, 0.5f);
	PSMesh::AddBox(Piece("Concrete"), At(FVector(S.X + T * 0.5f, S.Y * 0.5f, H * 0.5f)), FVector(T * 0.5f, S.Y * 0.5f, H * 0.5f), 3.0f, 0.5f);
	// Coping on top and a dark footing strip where gravel meets wall.
	for (const TPair<FVector, FVector>& W : TArray<TPair<FVector, FVector>>{
		{ FVector(S.X * 0.5f, -T * 0.5f, 0.0f), FVector(S.X * 0.5f + T, T * 0.5f, 0.0f) },
		{ FVector(-T * 0.5f, S.Y * 0.5f, 0.0f), FVector(T * 0.5f, S.Y * 0.5f, 0.0f) },
		{ FVector(S.X + T * 0.5f, S.Y * 0.5f, 0.0f), FVector(T * 0.5f, S.Y * 0.5f, 0.0f) } })
	{
		PSMesh::AddBox(Piece("ConcreteDark"), At(W.Key + FVector(0, 0, H + 6.0f)), W.Value + FVector(6.0f, 6.0f, 6.0f), 2.0f, 0.5f);
		PSMesh::AddBox(Piece("ConcreteDark"), At(W.Key + FVector(0, 0, 14.0f)), W.Value + FVector(5.0f, 5.0f, 14.0f), 2.0f, 0.5f);
	}
	// Buttress piers every few metres break up the long walls.
	for (float X = 400.0f; X < S.X; X += 800.0f)
	{
		PSMesh::AddBox(Piece("Concrete"), At(FVector(X, 14.0f, H * 0.5f)), FVector(30.0f, 18.0f, H * 0.5f), 3.0f, 0.5f);
		Footprints.Add({ FVector2D(X, 14.0f), 36.0f, TEXT("wall/pier") });
	}
	for (float Y = 400.0f; Y < S.Y; Y += 800.0f)
	{
		PSMesh::AddBox(Piece("Concrete"), At(FVector(14.0f, Y, H * 0.5f)), FVector(18.0f, 30.0f, H * 0.5f), 3.0f, 0.5f);
		PSMesh::AddBox(Piece("Concrete"), At(FVector(S.X - 14.0f, Y, H * 0.5f)), FVector(18.0f, 30.0f, H * 0.5f), 3.0f, 0.5f);
		Footprints.Add({ FVector2D(14.0f, Y), 36.0f, TEXT("wall/pier") });
		Footprints.Add({ FVector2D(S.X - 14.0f, Y), 36.0f, TEXT("wall/pier") });
	}
	// Institutional two-tone paint on the yard faces: a painted dado up to 1.2 m with a dark pin-stripe above it,
	// so the long walls read as a prison yard rather than bare concrete (reviewer 2026-11-12: "walls can be improved").
	const float BandZ0 = 28.0f, BandZ1 = 120.0f;
	const float BandMid = (BandZ0 + BandZ1) * 0.5f, BandHalf = (BandZ1 - BandZ0) * 0.5f;
	PSMesh::AddBox(Piece("ConcretePaint"), At(FVector(S.X * 0.5f, 1.5f, BandMid)), FVector(S.X * 0.5f, 1.5f, BandHalf), 1.0f, 0.5f);
	PSMesh::AddBox(Piece("ConcretePaint"), At(FVector(1.5f, S.Y * 0.5f, BandMid)), FVector(1.5f, S.Y * 0.5f, BandHalf), 1.0f, 0.5f);
	PSMesh::AddBox(Piece("ConcretePaint"), At(FVector(S.X - 1.5f, S.Y * 0.5f, BandMid)), FVector(1.5f, S.Y * 0.5f, BandHalf), 1.0f, 0.5f);
	PSMesh::AddBox(Piece("ConcreteDark"), At(FVector(S.X * 0.5f, 2.5f, BandZ1 + 4.0f)), FVector(S.X * 0.5f, 2.5f, 4.0f), 1.0f, 0.5f);
	PSMesh::AddBox(Piece("ConcreteDark"), At(FVector(2.5f, S.Y * 0.5f, BandZ1 + 4.0f)), FVector(2.5f, S.Y * 0.5f, 4.0f), 1.0f, 0.5f);
	PSMesh::AddBox(Piece("ConcreteDark"), At(FVector(S.X - 2.5f, S.Y * 0.5f, BandZ1 + 4.0f)), FVector(2.5f, S.Y * 0.5f, 4.0f), 1.0f, 0.5f);
	const float Top = H + 12.0f;
	AddFence(FVector(-T * 0.5f, -T * 0.5f, 0.0f), FVector(S.X + T * 0.5f, -T * 0.5f, 0.0f), Top);
	AddFence(FVector(-T * 0.5f, -T * 0.5f, 0.0f), FVector(-T * 0.5f, S.Y, 0.0f), Top);
	AddFence(FVector(S.X + T * 0.5f, -T * 0.5f, 0.0f), FVector(S.X + T * 0.5f, S.Y, 0.0f), Top);
}

// ---------------------------------------------------------------------------------------------- cell block

void APSYardBuilder::BuildCellBlock()
{
	const FVector2D S = Cfg->YardSize;
	const float H = Cfg->CellBlockHeight;
	const float Depth = 1000.0f;
	const float Y0 = S.Y;
	// Main mass, painted lower band, parapet.
	PSMesh::AddBox(Piece("Concrete"), At(FVector(S.X * 0.5f, Y0 + Depth * 0.5f, H * 0.5f)), FVector(S.X * 0.5f + WallThickness, Depth * 0.5f, H * 0.5f), 4.0f, 0.5f);
	PSMesh::AddBox(Piece("ConcretePaint"), At(FVector(S.X * 0.5f, Y0 + 1.0f, 70.0f)), FVector(S.X * 0.5f + WallThickness, 4.0f, 70.0f), 1.0f, 0.5f);
	PSMesh::AddBox(Piece("ConcreteDark"), At(FVector(S.X * 0.5f, Y0 + Depth * 0.5f, H + 30.0f)), FVector(S.X * 0.5f + WallThickness + 10.0f, Depth * 0.5f + 10.0f, 30.0f), 3.0f, 0.5f);
	// Floor-line bands between storeys.
	for (const float Z : { 310.0f, 610.0f })
	{
		PSMesh::AddBox(Piece("ConcreteDark"), At(FVector(S.X * 0.5f, Y0 - 5.0f, Z)), FVector(S.X * 0.5f + WallThickness, 8.0f, 9.0f), 2.0f, 0.5f);
	}
	// Pilasters.
	for (float X = 0.0f; X <= S.X; X += WindowSpacing * 2.0f)
	{
		PSMesh::AddBox(Piece("Concrete"), At(FVector(X, Y0 - 12.0f, H * 0.5f)), FVector(26.0f, 14.0f, H * 0.5f), 3.0f, 0.5f);
		Footprints.Add({ FVector2D(X, Y0 - 12.0f), 32.0f, TEXT("wall/pilaster") });
	}
	// Barred cell windows: two storeys, dark glass recess, sill, three bars.
	const float DoorX = Cfg->DoorLocation.X;
	for (float X = WindowSpacing * 0.5f; X < S.X; X += WindowSpacing)
	{
		for (const float Z : { 430.0f, 740.0f })
		{
			PSMesh::AddBox(Piece("Window"), At(FVector(X, Y0 - 1.0f, Z)), FVector(42.0f, 3.0f, 56.0f), 0.0f, 1.0f);
			PSMesh::AddBox(Piece("ConcreteDark"), At(FVector(X, Y0 - 10.0f, Z - 64.0f)), FVector(52.0f, 12.0f, 6.0f), 1.5f, 0.5f);
			for (int32 B = -1; B <= 1; ++B)
			{
				PSMesh::AddCylinder(Piece("Steel"), At(FVector(X + B * 22.0f, Y0 - 6.0f, Z)), 2.2f, 56.0f, 6, false);
			}
		}
		// Ground floor: small high windows except over the door.
		if (FMath::Abs(X - DoorX) > 300.0f)
		{
			PSMesh::AddBox(Piece("Window"), At(FVector(X, Y0 - 1.0f, 205.0f)), FVector(36.0f, 3.0f, 22.0f), 0.0f, 1.0f);
			PSMesh::AddBox(Piece("Steel"), At(FVector(X, Y0 - 5.0f, 205.0f)), FVector(38.0f, 2.0f, 2.0f), 0.0f, 1.0f);
		}
	}
	// The door: recessed steel door in a frame, a canopy, a caged lamp.
	PSMesh::AddBox(Piece("DoorFrame"), At(FVector(DoorX, Y0 - 14.0f, 160.0f)), FVector(150.0f, 16.0f, 160.0f), 4.0f, 0.5f);
	PSMesh::AddBox(Piece("Door"), At(FVector(DoorX, Y0 - 24.0f, 130.0f)), FVector(105.0f, 10.0f, 130.0f), 2.0f, 1.0f); // flat painted steel, 4 cm proud of the frame (coplanar faces z-fought: the dotted, blotchy door)
	// Plain single-leaf door (user 2026-11-09: plainer door): no centre seam strip, one handle.
	PSMesh::AddBox(Piece("Steel"), At(FVector(DoorX + 70.0f, Y0 - 37.0f, 120.0f)), FVector(10.0f, 3.0f, 2.5f), 1.0f, 1.0f);
	// Door detail (user 2026-11-12: "door quality should be increased"): raised stiles and rails that catch the light,
	// a wired vision panel, a steel kick plate, three hinges, a closer arm and a concrete step. The door face is at Y0 - 34.
	{
		const float F = Y0 - 34.0f;
		auto Rail = [&](float X, float Z, float HX, float HZ, const TCHAR* M, float Proud)
		{
			PSMesh::AddBox(Piece(M), At(FVector(DoorX + X, F - Proud, Z)), FVector(HX, Proud, HZ), 1.0f, 1.0f);
		};
		Rail(-95.0f, 130.0f, 8.0f, 128.0f, TEXT("Door"), 1.2f);   // stiles
		Rail(95.0f, 130.0f, 8.0f, 128.0f, TEXT("Door"), 1.2f);
		Rail(0.0f, 250.0f, 87.0f, 8.0f, TEXT("Door"), 1.2f);      // top, lock and bottom rails
		Rail(0.0f, 112.0f, 87.0f, 6.0f, TEXT("Door"), 1.2f);
		Rail(0.0f, 38.0f, 87.0f, 6.0f, TEXT("Door"), 1.2f);
		Rail(0.0f, 16.0f, 90.0f, 15.0f, TEXT("Steel"), 0.8f);      // kick plate
		Rail(0.0f, 190.0f, 24.0f, 30.0f, TEXT("SteelDark"), 1.8f); // vision panel surround
		Rail(0.0f, 190.0f, 18.0f, 24.0f, TEXT("Window"), 2.0f);    // glass (dark, faint warm glow from inside)
		for (const float BX : { -9.0f, 0.0f, 9.0f })              // security bars across the glass
		{
			PSMesh::AddBox(Piece("SteelDark"), At(FVector(DoorX + BX, F - 5.0f, 190.0f)), FVector(1.2f, 1.0f, 24.0f), 1.0f, 1.0f);
		}
		for (const float Z : { 40.0f, 130.0f, 220.0f })            // hinges on the left edge
		{
			PSMesh::AddBox(Piece("SteelDark"), At(FVector(DoorX - 104.0f, F - 3.0f, Z)), FVector(4.0f, 3.0f, 9.0f), 1.0f, 1.0f);
		}
		PSMesh::AddBox(Piece("SteelDark"), At(FVector(DoorX - 60.0f, F - 5.0f, 268.0f)), FVector(26.0f, 5.0f, 5.0f), 1.0f, 1.0f); // closer body
		PSMesh::AddBox(Piece("SteelDark"), At(FVector(DoorX - 20.0f, F - 8.0f, 268.0f)), FVector(22.0f, 2.0f, 2.0f), 1.0f, 1.0f); // closer arm
		PSMesh::AddBox(Piece("ConcreteDark"), At(FVector(DoorX, Y0 - 55.0f, 1.5f)), FVector(165.0f, 22.0f, 1.5f), 2.0f, 0.5f);   // threshold slab, 3 cm (taller kerbs jammed the hero)
	}
	// No canopy (user 2026-11-09): in its shadow the plain door was lit only by bounce light and rendered noisy.
	PSMesh::AddBox(Piece("Steel"), At(FVector(DoorX, Y0 - 40.0f, 365.0f)), FVector(16.0f, 10.0f, 20.0f), 2.0f, 1.0f);
	PSMesh::AddBox(Piece("Lamp"), At(FVector(DoorX, Y0 - 52.0f, 362.0f)), FVector(10.0f, 3.0f, 12.0f), 1.0f, 1.0f);
	DoorLamp->SetWorldLocation(FVector(DoorX, Y0 - 90.0f, 350.0f));
	DoorLamp->SetIntensity(DoorLampIntensity);
	// Micro-story: days scratched into the paint beside the door (groups of five, the fifth struck through).
	for (int32 G = 0; G < TallyGroups; ++G)
	{
		const float GX = DoorX - TallyOffset - G * 34.0f;
		for (int32 K = 0; K < 4; ++K)
		{
			PSMesh::AddBox(Piece("Concrete"), FTransform(FRotator(0.0f, 0.0f, (K % 2 ? 3.0f : -2.0f)), FVector(GX + K * 6.0f, Y0 - 1.2f, TallyHeight)), FVector(0.9f, 0.4f, 11.0f), 0.0f, 0.5f);
		}
		PSMesh::AddBox(Piece("Concrete"), FTransform(FRotator(0.0f, 0.0f, 0.0f).Quaternion() * FQuat(FVector(0, 1, 0), FMath::DegreesToRadians(-58.0f)), FVector(GX + 9.0f, Y0 - 1.6f, TallyHeight)), FVector(0.9f, 0.4f, 14.5f), 0.0f, 0.5f);
	}
	// Downpipes, floodlights and aircon units are Poly Haven props (BuildProps).

}

// ---------------------------------------------------------------------------------------------- tower

void APSYardBuilder::BuildTower()
{
	const FVector T = Cfg->TowerLocation;
	const float H = Cfg->TowerHeight;
	const float Leg = 160.0f;
	for (int32 Sx = -1; Sx <= 1; Sx += 2)
	{
		for (int32 Sy = -1; Sy <= 1; Sy += 2)
		{
			const FVector P(T.X + Sx * Leg, T.Y + Sy * Leg, 0.0f);
			PSMesh::AddBox(Piece("ConcreteDark"), At(P + FVector(0, 0, 15.0f)), FVector(26.0f, 26.0f, 15.0f), 3.0f, 0.5f);
			// Legs lean in slightly.
			const FVector Top(T.X + Sx * (Leg - 25.0f), T.Y + Sy * (Leg - 25.0f), H);
			const FVector M = (P + Top) * 0.5f;
			const FVector Axis = (Top - P).GetSafeNormal();
			PSMesh::AddCylinder(Piece("Steel"), FTransform(FRotationMatrix::MakeFromZ(Axis).ToQuat(), M), 11.0f, FVector::Dist(P, Top) * 0.5f, 10, true);
		}
	}
	// Horizontal braces.
	for (const float Z : { 220.0f, 430.0f })
	{
		const float L = Leg - 25.0f * Z / H;
		for (int32 Side = 0; Side < 4; ++Side)
		{
			const bool bX = Side < 2;
			const float Sg = (Side % 2) ? 1.0f : -1.0f;
			const FVector C = bX ? FVector(T.X, T.Y + Sg * L, Z) : FVector(T.X + Sg * L, T.Y, Z);
			PSMesh::AddBox(Piece("Steel"), At(C), bX ? FVector(L, 4.0f, 4.0f) : FVector(4.0f, L, 4.0f), 1.0f, 1.0f);
		}
	}
	// Platform, cabin walls (waist high), corner posts, roof with overhang, ladder.
	PSMesh::AddBox(Piece("SteelDark"), At(FVector(T.X, T.Y, H - 10.0f)), FVector(190.0f, 190.0f, 10.0f), 2.0f, 1.0f);
	for (int32 Side = 0; Side < 4; ++Side)
	{
		const bool bX = Side < 2;
		const float Sg = (Side % 2) ? 1.0f : -1.0f;
		const FVector C = bX ? FVector(T.X, T.Y + Sg * 180.0f, H + 55.0f) : FVector(T.X + Sg * 180.0f, T.Y, H + 55.0f);
		PSMesh::AddBox(Piece("ConcretePaint"), At(C), bX ? FVector(180.0f, 6.0f, 55.0f) : FVector(6.0f, 180.0f, 55.0f), 2.0f, 0.5f);
	}
	for (int32 Sx = -1; Sx <= 1; Sx += 2)
	{
		for (int32 Sy = -1; Sy <= 1; Sy += 2)
		{
			PSMesh::AddBox(Piece("Steel"), At(FVector(T.X + Sx * 178.0f, T.Y + Sy * 178.0f, H + 130.0f)), FVector(6.0f, 6.0f, 130.0f), 1.0f, 1.0f);
		}
	}
	PSMesh::AddTapered(Piece("SteelDark"), At(FVector(T.X, T.Y, H + 275.0f)), FVector(225.0f, 225.0f, 18.0f), FVector2D(0.6f, 0.6f), FVector2D::ZeroVector, 1.0f);
	PSMesh::AddBox(Piece("Lamp"), At(FVector(T.X, T.Y, H + 248.0f)), FVector(16.0f, 16.0f, 4.0f), 1.0f, 1.0f);
	for (int32 Sx = -1; Sx <= 1; Sx += 2)
	{
		PSMesh::AddBox(Piece("Steel"), At(FVector(T.X + Sx * 28.0f, T.Y + Leg + 30.0f, H * 0.5f)), FVector(3.0f, 3.0f, H * 0.5f), 0.5f, 1.0f);
	}
	for (float Z = 30.0f; Z < H; Z += 35.0f)
	{
		PSMesh::AddCylinder(Piece("Steel"), FTransform(FRotator(0.0f, 0.0f, 90.0f), FVector(T.X, T.Y + Leg + 30.0f, Z)), 1.8f, 28.0f, 6, false);
	}
}

// ---------------------------------------------------------------------------------------------- furniture

void APSYardBuilder::BuildFurniture()
{
	// Seating is Poly Haven picnic tables (config Props).

	// Basketball hoop: pole, arm, backboard with a painted square, rim.
	const FVector2D Hp = Cfg->HoopLocation;
	PSMesh::AddBox(Piece("ConcreteDark"), At(FVector(Hp.X, Hp.Y, 10.0f)), FVector(30.0f, 30.0f, 10.0f), 2.0f, 0.5f);
	PSMesh::AddCylinder(Piece("SteelDark"), At(FVector(Hp.X, Hp.Y, 160.0f)), 8.0f, 160.0f, 12, true);
	PSMesh::AddBox(Piece("SteelDark"), At(FVector(Hp.X, Hp.Y + 55.0f, 315.0f)), FVector(5.0f, 55.0f, 5.0f), 1.0f, 1.0f);
	PSMesh::AddBox(Piece("Rubber"), At(FVector(Hp.X, Hp.Y + 112.0f, 330.0f)), FVector(90.0f, 3.0f, 52.0f), 1.0f, 1.0f);
	PSMesh::AddBox(Piece("Steel"), At(FVector(Hp.X, Hp.Y + 108.0f, 315.0f)), FVector(28.0f, 1.0f, 20.0f), 0.0f, 1.0f);
	AddTorus(Piece("Steel"), At(FVector(Hp.X, Hp.Y + 140.0f, 305.0f)), 23.0f, 1.3f, 16);
	// Painted key on the gravel? No: the yard is gravel. A worn concrete pad under the hoop instead.
	PSMesh::AddBox(Piece("ConcreteDark"), At(FVector(Hp.X, Hp.Y + 300.0f, -3.0f)), FVector(260.0f, 300.0f, 4.0f), 2.0f, 0.5f);
}

// ---------------------------------------------------------------------------------------------- hiding surfaces

void APSYardBuilder::BuildRegions(APSSandGrid* Grid)
{
	int32 Section = 0;
	for (const FPSRegionDef& R : Cfg->Regions)
	{
		FPSMeshData Top;
		const FVector C(R.Center, 0.0f);
		EPSMat SurfaceMat = EPSMat::Soil;
		switch (R.Surface)
		{
		case EPSSurface::Soil:
		{
			// Raised timber bed: soil block plus four edge boards.
			PSMesh::AddBox(Top, At(C + FVector(0, 0, R.TopZ * 0.5f)), FVector(R.HalfSize.X, R.HalfSize.Y, R.TopZ * 0.5f), 0.0f, 1.0f);
			const float E = BedEdgeWidth;
			const float Hh = BedEdgeHeight * 0.5f;
			PSMesh::AddBox(Piece("Edge"), At(C + FVector(0, R.HalfSize.Y + E * 0.5f, Hh)), FVector(R.HalfSize.X + E, E * 0.5f, Hh), 1.5f, 1.0f);
			PSMesh::AddBox(Piece("Edge"), At(C + FVector(0, -R.HalfSize.Y - E * 0.5f, Hh)), FVector(R.HalfSize.X + E, E * 0.5f, Hh), 1.5f, 1.0f);
			PSMesh::AddBox(Piece("Edge"), At(C + FVector(R.HalfSize.X + E * 0.5f, 0, Hh)), FVector(E * 0.5f, R.HalfSize.Y, Hh), 1.5f, 1.0f);
			PSMesh::AddBox(Piece("Edge"), At(C + FVector(-R.HalfSize.X - E * 0.5f, 0, Hh)), FVector(E * 0.5f, R.HalfSize.Y, Hh), 1.5f, 1.0f);
			SurfaceMat = EPSMat::Soil;
			break;
		}
		case EPSSurface::Court:
		{
			PSMesh::AddBox(Top, At(C + FVector(0, 0, R.TopZ * 0.5f - 1.0f)), FVector(R.HalfSize.X, R.HalfSize.Y, R.TopZ * 0.5f + 1.0f), 0.0f, 1.0f);
			// Kerb.
			const float K = 14.0f;
			for (int32 Sg = -1; Sg <= 1; Sg += 2)
			{
				PSMesh::AddBox(Piece("Kerb"), At(C + FVector(0, Sg * (R.HalfSize.Y + K * 0.5f), 6.0f)), FVector(R.HalfSize.X + K, K * 0.5f, 6.0f), 2.0f, 0.5f);
				PSMesh::AddBox(Piece("Kerb"), At(C + FVector(Sg * (R.HalfSize.X + K * 0.5f), 0, 6.0f)), FVector(K * 0.5f, R.HalfSize.Y, 6.0f), 2.0f, 0.5f);
			}
			// Boundary tape lines, net posts, the net and its top tape.
			const float LineZ = R.TopZ + 0.6f;
			const float In = 40.0f;
			PSMesh::AddBox(Piece("Rubber"), At(C + FVector(0, R.HalfSize.Y - In, LineZ)), FVector(R.HalfSize.X - In, 2.5f, 0.6f), 0.0f, 1.0f);
			PSMesh::AddBox(Piece("Rubber"), At(C + FVector(0, -R.HalfSize.Y + In, LineZ)), FVector(R.HalfSize.X - In, 2.5f, 0.6f), 0.0f, 1.0f);
			PSMesh::AddBox(Piece("Rubber"), At(C + FVector(R.HalfSize.X - In, 0, LineZ)), FVector(2.5f, R.HalfSize.Y - In, 0.6f), 0.0f, 1.0f);
			PSMesh::AddBox(Piece("Rubber"), At(C + FVector(-R.HalfSize.X + In, 0, LineZ)), FVector(2.5f, R.HalfSize.Y - In, 0.6f), 0.0f, 1.0f);
			const float PostY = R.HalfSize.Y + 90.0f;
			for (int32 Sg = -1; Sg <= 1; Sg += 2)
			{
				PSMesh::AddCylinder(Piece("Steel"), At(C + FVector(0, Sg * PostY, 122.0f)), 5.0f, 122.0f, 10, true);
			}
			PSMesh::AddQuad(Piece("Net"), C + FVector(0, -PostY, 140.0f), C + FVector(0, PostY, 140.0f), C + FVector(0, PostY, 238.0f), C + FVector(0, -PostY, 238.0f), FVector(1, 0, 0), 1.0f);
			Nets.Add(FVector4(C.X, C.Y, PostY, 0.0f)); // solid blocker + steering obstacles added in BuildProps
			PSMesh::AddBox(Piece("Rubber"), At(C + FVector(0, 0, 240.0f)), FVector(1.5f, PostY, 2.5f), 0.0f, 1.0f);
			SurfaceMat = EPSMat::Court;
			break;
		}
		case EPSSurface::Pit:
		{
			PSMesh::AddBox(Top, At(C + FVector(0, 0, R.TopZ * 0.5f - 1.0f)), FVector(R.HalfSize.X, R.HalfSize.Y, R.TopZ * 0.5f + 1.0f), 0.0f, 1.0f);
			const float K = 16.0f;
			for (int32 Sg = -1; Sg <= 1; Sg += 2)
			{
				PSMesh::AddBox(Piece("Kerb"), At(C + FVector(0, Sg * (R.HalfSize.Y + K * 0.5f), 7.0f)), FVector(R.HalfSize.X + K, K * 0.5f, 7.0f), 2.0f, 0.5f);
				PSMesh::AddBox(Piece("Kerb"), At(C + FVector(Sg * (R.HalfSize.X + K * 0.5f), 0, 7.0f)), FVector(K * 0.5f, R.HalfSize.Y, 7.0f), 2.0f, 0.5f);
			}
			// Take-off board and the run-up lane (concrete strip heading east).
			PSMesh::AddBox(Piece("Rubber"), At(C + FVector(R.HalfSize.X + 70.0f, 0, 1.0f)), FVector(10.0f, 60.0f, 1.5f), 0.5f, 1.0f);
			PSMesh::AddBox(Piece("ConcreteDark"), At(C + FVector(R.HalfSize.X + 560.0f, 0, -2.0f)), FVector(480.0f, 62.0f, 2.6f), 1.0f, 0.5f);
			SurfaceMat = EPSMat::Pit;
			break;
		}
		case EPSSurface::Puddle:
		{
			// Muddy rim (soil) under a dark water disc.
			PSMesh::AddCylinder(Top, At(C + FVector(0, 0, 0.2f)), R.HalfSize.X + 35.0f, 0.4f, 28, true);
			FPSMeshData Water;
			PSMesh::AddCylinder(Water, At(C + FVector(0, 0, R.TopZ)), R.HalfSize.X, 0.4f, 28, true);
			Piece("Water").Append(Water);
			SurfaceMat = EPSMat::Soil;
			break;
		}
		default:
			break;
		}
		UMaterialInterface* Base = PSArt::Mat(SurfaceMat);
		UMaterialInstanceDynamic* MID = Base ? UMaterialInstanceDynamic::Create(Base, this) : nullptr;
		if (MID && Grid)
		{
			Grid->ApplyMaskParams(MID);
			if (R.Surface == EPSSurface::Puddle)
			{
				MID->SetScalarParameterValue(TEXT("Brightness"), 0.35f);
			}
			RegionMIDs.Add(R.Id, MID);
		}
		Pieces.Add(FName(*FString::Printf(TEXT("Region_%s"), *R.Id.ToString())), Top);
		PSMesh::Commit(Surfaces, Section++, Top, MID, true);
	}
}

void APSYardBuilder::Commit(APSSandGrid* Grid)
{
	struct FMap { const TCHAR* Name; EPSMat Mat; bool bCollision; };
	const FMap Maps[] = {
		{ TEXT("Concrete"), EPSMat::Concrete, true },
		{ TEXT("ConcreteDark"), EPSMat::ConcreteDark, true },
		{ TEXT("ConcretePaint"), EPSMat::ConcretePaint, true },
		{ TEXT("Steel"), EPSMat::Steel, true },
		{ TEXT("SteelDark"), EPSMat::SteelDark, true },
		{ TEXT("Wood"), EPSMat::Wood, true },
		{ TEXT("Rubber"), EPSMat::Rubber, false },
		// The cell door and its frame: flat paint, no concrete texture (user 2026-11-09: "make it plain").
		{ TEXT("Door"), EPSMat::Door, true },
		{ TEXT("DoorFrame"), EPSMat::DoorFrame, true },
		// Low kerbs and bed boards stay visual (D9): solid, the 12 cm boards jam the 34 cm capsule (it cannot perch on them
		// to step over; the bot stuck for a whole round, 2026-11-09), so characters walk over them.
		{ TEXT("Kerb"), EPSMat::ConcreteDark, false },
		{ TEXT("Edge"), EPSMat::Wood, false },
		{ TEXT("ChainLink"), EPSMat::ChainLink, false },
		{ TEXT("Razor"), EPSMat::Razor, false },
		{ TEXT("Net"), EPSMat::Net, false },
		{ TEXT("Window"), EPSMat::Window, false },
		{ TEXT("Lamp"), EPSMat::Lamp, false },
		{ TEXT("Water"), EPSMat::Water, false },
	};
	int32 Section = 0;
	for (const FMap& M : Maps)
	{
		if (const FPSMeshData* D = Pieces.Find(FName(M.Name)))
		{
			PSMesh::Commit(Structure, Section++, *D, PSArt::Mat(M.Mat), M.bCollision);
		}
	}
	UE_LOG(LogPocketSand, Log, TEXT("[PocketSand] Yard built: %d structure sections, %d hiding surfaces."), Section, Cfg->Regions.Num());
}

// ---------------------------------------------------------------------------------------------- Poly Haven props

void APSYardBuilder::FlushFoliage()
{
	// Every plant uses its real canopy size (user 2026-11-16: shrubs spilled over the bed borders; "remove all the
	// overlapping throughout the game"): it is pushed fully clear of beds, court, puddles and obstacles, kept off the door
	// apron, and dropped if it would still touch a placed prop or another plant.
	bFlushingFoliage = true;
	int32 Kept = 0, Moved = 0, Dropped = 0;
	TArray<FFootprint> Plants;
	for (const FPendingFoliage& F : PendingFoliage)
	{
		const FString Name = FPaths::GetBaseFilename(F.Mesh);
		const UStaticMesh* SM = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s%s.%s"), *PropRoot, *F.Mesh, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
		const float Rad = SM ? FMath::Max(SM->GetBounds().BoxExtent.X, SM->GetBounds().BoxExtent.Y) * F.Scale * 0.8f : 25.0f;
		const FVector2D From(F.Location);
		FVector2D P = From;
		if (Cfg)
		{
			// Off the wall faces (no leaves through the concrete), then out of beds, court, puddles and obstacles.
			P.X = FMath::Clamp(P.X, Rad + 4.0f, Cfg->YardSize.X - Rad - 4.0f);
			P.Y = FMath::Clamp(P.Y, Rad + 4.0f, Cfg->YardSize.Y - Rad - 4.0f);
			P = PSNav::PushOut(Cfg, P, Rad + 8.0f, true, 22.0f);
		}
		bool bClear = true;
		for (const FFootprint& Fp : Footprints)
		{
			bClear &= FVector2D::Distance(P, Fp.Centre) > Fp.Radius + Rad + 6.0f;
		}
		for (const FFootprint& Pl : Plants)
		{
			bClear &= FVector2D::Distance(P, Pl.Centre) > (Pl.Radius + Rad) * 0.55f; // clumps may touch, never stack
		}
		if (Cfg)
		{
			for (const FPSRegionDef& Rg : Cfg->Regions)
			{
				bClear &= !Rg.Contains(P, Rad + 18.0f);
			}
			const FVector2D Door = Cfg->DoorLocation;
			bClear &= !(FMath::Abs(P.X - Door.X) < Cfg->ApronHalf.X + Rad + 20.0f && FMath::Abs(P.Y - (Door.Y - Cfg->ApronHalf.Y)) < Cfg->ApronHalf.Y + Rad + 20.0f);
		}
		if (bClear)
		{
			AddProp(F.Mesh, FVector(P, F.Location.Z), F.Yaw, F.Scale, false);
			Plants.Add({ P, Rad, F.Mesh });
			++Kept;
			Moved += FVector2D::Distance(P, From) > 1.0f ? 1 : 0;
		}
		else
		{
			++Dropped;
		}
	}
	PendingFoliage.Reset();
	bFlushingFoliage = false;
	UE_LOG(LogPocketSand, Log, TEXT("[PocketSand] Foliage: %d placed (%d nudged clear), %d skipped (no clear spot)."), Kept, Moved, Dropped);
}

void APSYardBuilder::AddProp(const FString& Mesh, const FVector& Location, float Yaw, float Scale, bool bCollision, float Pitch, float Roll)
{
	if (IsFoliage(Mesh) && !bFlushingFoliage && Location.Z < 5.0f)
	{
		PendingFoliage.Add({ Mesh, Location, Yaw, Scale });
		return;
	}
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent>* Found = PropComponents.Find(Mesh);
	UHierarchicalInstancedStaticMeshComponent* C = Found ? Found->Get() : nullptr;
	if (!C)
	{
		const FString Name = FPaths::GetBaseFilename(Mesh);
		UStaticMesh* SM = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s%s.%s"), *PropRoot, *Mesh, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!SM)
		{
			UE_LOG(LogPocketSand, Warning, TEXT("[PocketSand] Missing prop mesh %s%s - run Tools/build_content.ps1 -Stages props"), *PropRoot, *Mesh);
			PropComponents.Add(Mesh, nullptr);
			return;
		}
		C = NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
		C->SetStaticMesh(SM);
		C->SetupAttachment(RootComponent);
		// C2: every prop is solid (blocks characters, affects navigation) except walk-through foliage and flush covers.
		const bool bSolid = !IsWalkThrough(Mesh);
		C->SetCollisionEnabled(bSolid ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		C->SetCollisionProfileName(bSolid ? TEXT("BlockAll") : TEXT("NoCollision"));
		C->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		C->SetCanEverAffectNavigation(bSolid);
		if (!bSolid)
		{
			C->ComponentTags.Add(TEXT("NoCollide"));
		}
		// Grass and weed tufts: no shadow casting (they sit in AO; saves the virtual shadow map budget, glitch log hitches).
		if (Mesh.Contains(TEXT("grass")) || Mesh.Contains(TEXT("weed")))
		{
			C->SetCastShadow(false);
		}
		C->RegisterComponent();
		AddInstanceComponent(C);
		PropComponents.Add(Mesh, C);
	}
	// Ground props never sit on a bed border, the court edge or a puddle rim: nudge them clear (user 2026-11-16).
	FVector Loc = Location;
	if (Cfg && !IsFoliage(Mesh) && Location.Z < 5.0f && C->GetStaticMesh() && !Mesh.Contains(TEXT("manhole")))
	{
		const FVector Ext = C->GetStaticMesh()->GetBounds().BoxExtent * Scale;
		const float M = FMath::Max(Ext.X, Ext.Y) * 0.8f + 14.0f;
		FVector2D P(Loc);
		for (const FPSRegionDef& R : Cfg->Regions)
		{
			if (!R.Contains(P, M)) { continue; }
			if (R.bRound)
			{
				const FVector2D D = P - R.Center;
				P = R.Center + (D.IsNearlyZero() ? FVector2D(1.0f, 0.0f) : D.GetSafeNormal()) * (R.HalfSize.X + M);
				continue;
			}
			const float Dx = R.HalfSize.X + M - FMath::Abs(P.X - R.Center.X);
			const float Dy = R.HalfSize.Y + M - FMath::Abs(P.Y - R.Center.Y);
			if (Dx < Dy) { P.X = R.Center.X + FMath::Sign(P.X - R.Center.X + 0.01f) * (R.HalfSize.X + M); }
			else { P.Y = R.Center.Y + FMath::Sign(P.Y - R.Center.Y + 0.01f) * (R.HalfSize.Y + M); }
		}
		if (!P.Equals(FVector2D(Loc), 0.5f))
		{
			UE_LOG(LogPocketSand, Log, TEXT("[PocketSand] Nudged %s off a bed/court/puddle edge by %.0f cm."), *FPaths::GetBaseFilename(Mesh), FVector2D::Distance(P, FVector2D(Loc)));
			Loc = FVector(P, Loc.Z);
		}
	}
	C->AddInstance(FTransform(FRotator(Pitch, Yaw, Roll), Loc, FVector(Scale)), true);
	// Ground-level footprints: foliage stays clear of them; two solid ones that intersect are logged (fix the layout).
	if (!IsFoliage(Mesh) && Loc.Z < 60.0f && C->GetStaticMesh())
	{
		const FVector Ext = C->GetStaticMesh()->GetBounds().BoxExtent * Scale;
		const float Rad = FMath::Min(FMath::Max(Ext.X, Ext.Y), 150.0f);
		if (Loc.Z < 5.0f && Pitch == 0.0f && Roll == 0.0f)
		{
			for (const FFootprint& Fp : Footprints)
			{
				const float D = FVector2D::Distance(FVector2D(Loc), Fp.Centre);
				// Pieces of one kit (facade bays, gate leaves) are built to sit together: only different props are checked.
				const bool bSameKit = Fp.Mesh.Left(Fp.Mesh.Find(TEXT("/"))) == Mesh.Left(Mesh.Find(TEXT("/")));
				if (D < (Rad + Fp.Radius) * 0.6f && D > 1.0f && !bSameKit)
				{
					UE_LOG(LogPocketSand, Warning, TEXT("[PocketSand] Prop overlap: %s at (%.0f, %.0f) r%.0f into %s at (%.0f, %.0f) r%.0f"),
						*FPaths::GetBaseFilename(Mesh), Loc.X, Loc.Y, Rad, *FPaths::GetBaseFilename(Fp.Mesh), Fp.Centre.X, Fp.Centre.Y, Fp.Radius);
				}
			}
		}
		Footprints.Add({ FVector2D(Loc), Rad, Mesh });
	}
	// Ground-level solid props also become steering obstacles, so inmates and guards walk around them.
	if (Cfg && C->IsCollisionEnabled() && Loc.Z < ObstacleMaxZ && C->GetStaticMesh())
	{
		// True footprint (user 2026-11-14: "characters are still going into objects"): centred on the mesh bounds, not the
		// pivot, and long props (barriers, tables, benches) become a row of circles along their length.
		const FBoxSphereBounds B = C->GetStaticMesh()->GetBounds();
		const FRotator Rot(0.0f, Yaw, 0.0f);
		const FVector Centre = Loc + Rot.RotateVector(B.Origin * Scale);
		const float Ex = B.BoxExtent.X * Scale, Ey = B.BoxExtent.Y * Scale;
		const float Major = FMath::Max(Ex, Ey), Minor = FMath::Min(Ex, Ey);
		if (Major >= ObstacleMinRadius)
		{
			if (Major > Minor * 1.6f)
			{
				const FVector Axis = Rot.RotateVector(Ex >= Ey ? FVector::ForwardVector : FVector::RightVector);
				const float R = FMath::Max(Minor, ObstacleMinRadius);
				const int32 N = FMath::Max(2, FMath::CeilToInt(Major / R));
				for (int32 I = 0; I < N; ++I)
				{
					const float U = N > 1 ? -Major + R + (2.0f * (Major - R)) * I / (N - 1) : 0.0f;
					Cfg->AddRuntimeObstacle(FVector2D(Centre + Axis * U), R);
				}
			}
			else
			{
				Cfg->AddRuntimeObstacle(FVector2D(Centre), Major * ObstacleFootprint);
			}
		}
	}
}

// ---------------------------------------------------------------------------------------------- skyline

void APSYardBuilder::BuildSkyline()
{
	// Kit pieces share one pivot per bay (wall plane + its window frame), so centre each bay on the wall's bounds.
	const FString WallRef = SkylineKit + TEXT("wall_standard_standard_01");
	const FString WallName = FPaths::GetBaseFilename(WallRef);
	const UStaticMesh* WallMesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s%s.%s"), *PropRoot, *WallRef, *WallName), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!WallMesh)
	{
		UE_LOG(LogPocketSand, Warning, TEXT("[PocketSand] Skyline kit missing (%s%s) - run Tools/build_content.ps1 -Stages props"), *PropRoot, *WallRef);
		return;
	}
	const FVector PivotToCentre = WallMesh->GetBounds().Origin * FVector(1.0f, 1.0f, 0.0f);
	const float M = SkylineModule;
	FRandomStream R(SkylineSeed);
	const FVector2D S = Cfg->YardSize;
	const float T = WallThickness;

	static const TCHAR* WindowSets[][2] = {
		{ TEXT("wall_window_centered_double_01"), TEXT("window_centered_double_01") },
		{ TEXT("wall_window_centered_large_01"), TEXT("window_centered_large_01") },
		{ TEXT("wall_window_offset_small_01"), TEXT("window_offset_small_01") } };
	auto Kit = [this](const TCHAR* Name) { return SkylineKit + Name; };

	// One row per open side: start corner, run direction, facing (towards the yard).
	struct FRow { FVector Start; FVector Run; FVector Face; double Length; };
	const TArray<FRow> Rows = {
		{ FVector(-T - SkylineOverhang, -T, 0.0f), FVector(1, 0, 0), FVector(0, 1, 0), S.X + 2.0f * (T + SkylineOverhang) },
		{ FVector(-T, -T - SkylineOverhang, 0.0f), FVector(0, 1, 0), FVector(1, 0, 0), S.Y + T + SkylineOverhang },
		{ FVector(S.X + T, -T - SkylineOverhang, 0.0f), FVector(0, 1, 0), FVector(-1, 0, 0), S.Y + T + SkylineOverhang } };
	int32 Bays = 0;
	for (const FRow& Row : Rows)
	{
		// Local +Y of the kit is its front face (plus SkylineFaceYaw if an import flips it).
		const float Yaw = Row.Face.Rotation().Yaw - 90.0f + SkylineFaceYaw;
		const FRotator Rot(0.0f, Yaw, 0.0f);
		float Along = R.FRandRange(0.0f, SkylineGap.Y);
		while (Along < Row.Length)
		{
			const int32 N = R.RandRange(SkylineBays.X, SkylineBays.Y);
			const int32 F = R.RandRange(SkylineFloors.X, SkylineFloors.Y);
			const float Back = SkylineDistance + R.FRandRange(0.0f, SkylineDistanceJitter);
			const int32 Set = R.RandRange(0, UE_ARRAY_COUNT(WindowSets) - 1);
			const FVector Front = Row.Start - Row.Face * Back;
			for (int32 B = 0; B < N; ++B)
			{
				const FVector BayCentre = Front + Row.Run * (Along + (B + 0.5f) * M);
				const FVector Pivot = BayCentre - Rot.RotateVector(PivotToCentre);
				for (int32 Fl = 0; Fl < F; ++Fl)
				{
					const FVector P = Pivot + FVector(0, 0, Fl * M);
					const bool bShop = Fl == 0 && (B % 2 == 0);
					AddProp(Kit(bShop ? TEXT("wall_door_window_small_01") : WindowSets[Set][0]), P, Yaw, 1.0f, false);
					AddProp(Kit(bShop ? TEXT("door_window_small_01") : WindowSets[Set][1]), P, Yaw, 1.0f, false);
					if (Fl > 0)
					{
						AddProp(Kit(TEXT("cornice_standard_standard_01")), P, Yaw, 1.0f, false);
					}
				}
				AddProp(Kit(TEXT("base_standard_01")), Pivot, Yaw, 1.0f, false);
				AddProp(Kit(TEXT("crown_standard_standard_01")), Pivot + FVector(0, 0, F * M), Yaw, 1.0f, false);
				++Bays;
			}
			// The block's mass behind the facade, so it reads as a building from any angle (and from the intro fly-in).
			const FVector Centre = Front + Row.Run * (Along + N * M * 0.5f) - Row.Face * (SkylineBlockDepth * 0.5f + 4.0f);
			const FVector Ext = Row.Run.GetAbs() * (N * M * 0.5f) + Row.Face.GetAbs() * (SkylineBlockDepth * 0.5f) + FVector(0, 0, F * M * 0.5f + 30.0f);
			PSMesh::AddBox(Piece("Concrete"), At(Centre + FVector(0, 0, F * M * 0.5f + 30.0f)), Ext, 3.0f, 0.5f);
			Along += N * M + R.FRandRange(SkylineGap.X, SkylineGap.Y);
		}
	}
	// Far backdrop: no shadows (saves the shadow budget; it never shades the yard at the midday sun angle).
	for (const TPair<FString, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>>& P : PropComponents)
	{
		if (P.Value && P.Key.StartsWith(SkylineKit))
		{
			P.Value->SetCastShadow(false);
			// The kit's glass mirrors the bright sky into pure white panes; use our dark window glass instead.
			const TArray<FName> Slots = P.Value->GetMaterialSlotNames();
			for (int32 I = 0; I < Slots.Num(); ++I)
			{
				if (Slots[I].ToString().Contains(TEXT("glass")))
				{
					P.Value->SetMaterial(I, PSArt::Mat(EPSMat::Window));
				}
			}
		}
	}
	UE_LOG(LogPocketSand, Log, TEXT("[PocketSand] Skyline: %d bays across %d rows (pivot offset %s)"), Bays, Rows.Num(), *PivotToCentre.ToString());
}

bool APSYardBuilder::IsWalkThrough(const FString& Mesh) const
{
	for (const FString& Prefix : WalkThroughMeshes)
	{
		if (Mesh.StartsWith(Prefix))
		{
			return true;
		}
	}
	return false;
}

void APSYardBuilder::BuildProps()
{
	Cfg->ResetRuntimeObstacles();
	FRandomStream R(9031);
	// Volleyball nets are solid (user 2026-11-09: the hero walked through the net): an invisible blocking slab from the
	// ground to the net top, and a row of steering obstacles so the bot and the crowd walk around the posts.
	for (const FVector4& N : Nets)
	{
		UBoxComponent* B = NewObject<UBoxComponent>(this);
		B->SetupAttachment(RootComponent);
		B->SetBoxExtent(FVector(NetBlockHalfThickness, N.Z, 125.0f));
		B->SetRelativeLocation(FVector(N.X, N.Y, 125.0f));
		B->SetCollisionProfileName(TEXT("BlockAll"));
		B->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		B->SetHiddenInGame(true);
		B->RegisterComponent();
		AddInstanceComponent(B);
		for (float Yo = -N.Z; Yo <= N.Z + 1.0f; Yo += NetObstacleSpacing)
		{
			Cfg->AddRuntimeObstacle(FVector2D(N.X, N.Y + Yo), NetObstacleRadius);
		}
	}
	const FVector2D S = Cfg->YardSize;
	const float WallTop = Cfg->WallHeight + 12.0f;
	// Chain-link panels (191 cm) with posts on top of the south, west and east walls.
	auto FenceRun = [this, WallTop](const FVector& A, const FVector& B)
	{
		const FVector D = (B - A).GetSafeNormal2D();
		const float Len = FVector::Dist2D(A, B);
		const float Yaw = D.Rotation().Yaw + FenceYaw;
		const float Panel = 191.0f;
		const int32 N = FMath::Max(1, FMath::RoundToInt(Len / Panel));
		const float Step = Len / N;
		for (int32 I = 0; I < N; ++I)
		{
			const FVector P = A + D * (Step * (I + 0.5f));
			AddProp(TEXT("modular_chainlink_fence/SM_modular_chainlink_fence_double"), FVector(P.X, P.Y, WallTop + 6.0f), Yaw, 1.0f, false);
			const FVector Q = A + D * (Step * I);
			AddProp(TEXT("modular_chainlink_fence/SM_modular_chainlink_fence_post"), FVector(Q.X, Q.Y, WallTop), Yaw, 1.0f, false);
		}
		AddProp(TEXT("modular_chainlink_fence/SM_modular_chainlink_fence_post"), FVector(B.X, B.Y, WallTop), Yaw, 1.0f, false);
	};
	const float T = WallThickness * 0.5f;
	FenceRun(FVector(-T, -T, 0), FVector(S.X + T, -T, 0));
	FenceRun(FVector(-T, -T, 0), FVector(-T, S.Y, 0));
	FenceRun(FVector(S.X + T, -T, 0), FVector(S.X + T, S.Y, 0));

	// Weeds and grass along every wall base, around the tower legs and at the bed ends; a few in the gravel.
	static const TCHAR* Tufts[] = {
		TEXT("weed_plant_02/SM_weed_plant_02_a_LOD0"), TEXT("weed_plant_02/SM_weed_plant_02_b_LOD0"), TEXT("weed_plant_02/SM_weed_plant_02_c_LOD0"),
		TEXT("grass_medium_01/SM_grass_medium_01_large_a_LOD0"), TEXT("grass_medium_01/SM_grass_medium_01_large_b_LOD0"), TEXT("grass_medium_01/SM_grass_medium_01_mid_a_LOD0"),
		TEXT("grass_medium_01/SM_grass_medium_01_mid_b_LOD0"), TEXT("grass_medium_01/SM_grass_medium_01_tall_a_LOD0"), TEXT("grass_medium_01/SM_grass_medium_01_tall_b_LOD0"),
		TEXT("grass_medium_01/SM_grass_medium_01_small_a_LOD0") };
	auto Tuft = [this, &R](const FVector2D& P)
	{
		AddProp(Tufts[R.RandRange(0, UE_ARRAY_COUNT(Tufts) - 1)], FVector(P, 0.0f), R.FRandRange(0.0f, 360.0f), R.FRandRange(WeedScale.X, WeedScale.Y), false);
	};
	auto Strip = [&](const FVector2D& A, const FVector2D& B, const FVector2D& Inward)
	{
		const float Len = FVector2D::Distance(A, B);
		const FVector2D D = (B - A) / FMath::Max(Len, 1.0f);
		for (float U = R.FRandRange(0.0f, WeedSpacing); U < Len; U += WeedSpacing * R.FRandRange(0.4f, 1.6f))
		{
			// Clumps: two or three tufts together.
			const int32 K = R.RandRange(1, 3);
			for (int32 J = 0; J < K; ++J)
			{
				Tuft(A + D * (U + R.FRandRange(-18.0f, 18.0f)) + Inward * R.FRandRange(4.0f, 22.0f));
			}
		}
	};
	Strip(FVector2D(30, 30), FVector2D(S.X - 30, 30), FVector2D(0, 1));
	Strip(FVector2D(30, 30), FVector2D(30, S.Y - 40), FVector2D(1, 0));
	Strip(FVector2D(S.X - 30, 30), FVector2D(S.X - 30, S.Y - 40), FVector2D(-1, 0));
	Strip(FVector2D(30, S.Y - 30), FVector2D(S.X - 30, S.Y - 30), FVector2D(0, -1));
	const FVector T3 = Cfg->TowerLocation;
	for (int32 Sx = -1; Sx <= 1; Sx += 2)
	{
		for (int32 Sy = -1; Sy <= 1; Sy += 2)
		{
			for (int32 J = 0; J < 4; ++J)
			{
				if (bLooseWeeds) Tuft(FVector2D(T3.X + Sx * 160.0f, T3.Y + Sy * 160.0f) + FVector2D(R.FRandRange(-45.0f, 45.0f), R.FRandRange(-45.0f, 45.0f)));
			}
		}
	}
	for (int32 I = 0; I < YardWeeds; ++I)
	{
		const FVector2D P(R.FRandRange(200.0f, S.X - 200.0f), R.FRandRange(200.0f, S.Y - 300.0f));
		bool bClear = true;
		for (const FPSRegionDef& Rg : Cfg->Regions)
		{
			bClear &= !Rg.Contains(P, 60.0f);
		}
		if (bClear)
		{
			Tuft(P);
		}
	}

	// Garden beds: a watering can and a spade leant at one end; grass along the outer edge.
	int32 Bed = 0;
	for (const FPSRegionDef& Rg : Cfg->Regions)
	{
		if (Rg.Surface != EPSSurface::Soil)
		{
			continue;
		}
		const float Side = (Bed++ % 2) ? 1.0f : -1.0f;
		const FVector2D End = Rg.Center + FVector2D(Side * (Rg.HalfSize.X + 45.0f), 0.0f);
		AddProp(TEXT("watering_can_metal_01/SM_ps_watering_can_metal_01_1k"), FVector(End + FVector2D(0.0f, -60.0f), 0.0f), R.FRandRange(0.0f, 360.0f), 1.3f, false);
		AddProp(TEXT("rusted_spade_01/SM_ps_rusted_spade_01_1k"), FVector(End + FVector2D(0.0f, 50.0f), 52.0f), 90.0f, 1.0f, false, 0.0f, -18.0f * Side);
		for (int32 J = 0; J < 6; ++J)
		{
			if (bLooseWeeds) Tuft(Rg.Center + FVector2D(R.FRandRange(-Rg.HalfSize.X, Rg.HalfSize.X), (J % 2 ? 1.0f : -1.0f) * (Rg.HalfSize.Y + 22.0f)));
		}
	}

	// South wall lamps: the wall's yard face stands in its own shadow, so each fixture carries a real shadowed spotlight.
	for (int32 I = 0; I < WallLampCount; ++I)
	{
		const float X = S.X * (I + 0.5f) / FMath::Max(WallLampCount, 1);
		AddProp(TEXT("security_light/SM_security_light"), FVector(X, 12.0f, WallLampHeight), 90.0f, 1.4f, false);
		AddProp(TEXT("security_light/SM_security_light_glass"), FVector(X, 12.0f, WallLampHeight), 90.0f, 1.4f, false);
		USpotLightComponent* L = NewObject<USpotLightComponent>(this);
		L->SetupAttachment(RootComponent);
		L->SetMobility(EComponentMobility::Movable);
		L->SetWorldLocationAndRotation(FVector(X, 45.0f, WallLampHeight - 12.0f), FRotator(-WallLampPitch, -90.0f, 0.0f));
		L->SetIntensity(WallLampIntensity);
		L->SetLightColor(WallLampColor);
		L->SetAttenuationRadius(WallLampRadius);
		L->SetOuterConeAngle(WallLampConeDeg);
		L->SetInnerConeAngle(WallLampConeDeg * 0.45f);
		L->SetCastShadows(WallLampShadowEvery > 0 && I % WallLampShadowEvery == 0);
		L->RegisterComponent();
		AddInstanceComponent(L);
	}

	// Cell block: floodlights high on the wall, downpipes, aircon units at ground level, wall lamps by the door.
	const float Y0 = S.Y;
	const float LightStep = FloodlightSpacing * Cfg->LayoutScale;
	for (float X = LightStep * 0.5f; X < S.X; X += LightStep)
	{
		AddProp(TEXT("security_light/SM_security_light"), FVector(X, Y0 - 20.0f, FloodlightHeight), -90.0f, 1.6f, false);
		AddProp(TEXT("security_light/SM_security_light_glass"), FVector(X, Y0 - 20.0f, FloodlightHeight), -90.0f, 1.6f, false);
	}
	for (const float X : { S.X * 0.11f, S.X * 0.39f, S.X * 0.67f, S.X * 0.95f })
	{
		for (float Z = 0.0f; Z < Cfg->CellBlockHeight; Z += 100.0f)
		{
			AddProp(TEXT("modular_metal_gutter/SM_modular_metal_gutter_section"), FVector(X, Y0 - 14.0f, Z), 0.0f, 1.0f, false);
		}
		AddProp(TEXT("modular_metal_gutter/SM_modular_metal_gutter_funnel"), FVector(X, Y0 - 14.0f, Cfg->CellBlockHeight), 0.0f, 1.0f, false);
		AddProp(TEXT("modular_metal_gutter/SM_modular_metal_gutter_outlet"), FVector(X, Y0 - 24.0f, 27.0f), 90.0f, 1.0f, false);
	}
	int32 Unit = 0;
	for (const float X : { S.X * 0.0625f, S.X * 0.195f, S.X * 0.86f, S.X * 0.92f })
	{
		AddProp(Unit++ % 2 ? TEXT("exterior_aircon_unit/SM_exterior_aircon_unit_rusted") : TEXT("exterior_aircon_unit/SM_exterior_aircon_unit"), FVector(X, Y0 - 30.0f, 32.0f), 180.0f, 1.0f, true);
	}
	const float DoorX = Cfg->DoorLocation.X;
	for (const float Sx : { -260.0f, 260.0f })
	{
		AddProp(TEXT("street_lamp_02/SM_ps_street_lamp_02_1k"), FVector(DoorX + Sx, Y0 - 10.0f, 300.0f), -90.0f, 1.0f, false);
	}
	// CCTV on the corners, looking into the yard.
	AddProp(TEXT("security_camera_02/SM_security_camera_02_mount"), FVector(80.0f, 80.0f, Cfg->WallHeight - 30.0f), 45.0f, 1.8f, false);
	AddProp(TEXT("security_camera_02/SM_security_camera_02"), FVector(80.0f, 80.0f, Cfg->WallHeight - 30.0f), 45.0f, 1.8f, false);
	AddProp(TEXT("security_camera_02/SM_security_camera_02_mount"), FVector(S.X - 80.0f, Y0 - 30.0f, 640.0f), -135.0f, 1.8f, false);
	AddProp(TEXT("security_camera_02/SM_security_camera_02"), FVector(S.X - 80.0f, Y0 - 30.0f, 640.0f), -135.0f, 1.8f, false);
	AddProp(TEXT("security_camera_02/SM_security_camera_02_mount"), FVector(80.0f, Y0 - 30.0f, 640.0f), -45.0f, 1.8f, false);
	AddProp(TEXT("security_camera_02/SM_security_camera_02"), FVector(80.0f, Y0 - 30.0f, 640.0f), -45.0f, 1.8f, false);

	// One-off props from the config (tables, bins, chairs, clutter, gate, planters, barriers, drains).
	for (const FPSPropPlacement& P : Cfg->Props)
	{
		AddProp(P.Mesh, P.Location, P.Yaw, P.Scale, P.bCollision);
	}
	// Second dressing pass (Poly Haven CC0): planting, a hydrant, barrels, a garden tap, compost, a service bay and roof ducts.
	{
		static const TCHAR* Bushes[] = { TEXT("shrub_02/SM_shrub_02_a"), TEXT("shrub_02/SM_shrub_02_b"), TEXT("shrub_02/SM_shrub_02_c"), TEXT("shrub_02/SM_shrub_02_d") };
		int32 Bush = 0;
		for (const FVector2D& P : { FVector2D(170, 170), FVector2D(120, S.Y * 0.33f), FVector2D(110, S.Y * 0.6f), FVector2D(S.X - 140, 170), FVector2D(S.X - 120, S.Y * 0.31f), FVector2D(160, S.Y - 170) })
		{
			AddProp(Bushes[Bush++ % UE_ARRAY_COUNT(Bushes)], FVector(P, 0.0f), R.FRandRange(0.0f, 360.0f), R.FRandRange(0.7f, 0.95f), false);
		}
		// Low hedges along the cell block either side of the door.
		for (const float X : { DoorX - 960.0f, DoorX - 700.0f, DoorX - 440.0f, DoorX + 440.0f, DoorX + 700.0f, DoorX + 960.0f })
		{
			AddProp(TEXT("shrub_01/SM_ps_shrub_01_1k"), FVector(X, Y0 - 34.0f, 0.0f), R.RandRange(0, 1) * 180.0f, 1.0f, false);
		}
		int32 Plot = 0;
		for (const FPSRegionDef& Rg : Cfg->Regions)
		{
			if (Rg.Surface != EPSSurface::Soil)
			{
				continue;
			}
			const float Side = (Plot++ % 2) ? 1.0f : -1.0f;
			const FVector2D Far = Rg.Center - FVector2D(Side * (Rg.HalfSize.X + 50.0f), 0.0f);
			AddProp(TEXT("compost_bag_02/SM_ps_compost_bag_02_1k"), FVector(Far + FVector2D(0.0f, -45.0f), 0.0f), R.FRandRange(0.0f, 360.0f), 1.15f, false);
			AddProp(TEXT("compost_bag_02/SM_ps_compost_bag_02_1k"), FVector(Far + FVector2D(12.0f, 30.0f), 0.0f), R.FRandRange(0.0f, 360.0f), 1.05f, false);
			for (int32 J = 0; J < 4; ++J)
			{
				const float U = R.FRandRange(-Rg.HalfSize.X + 40.0f, Rg.HalfSize.X - 40.0f);
				AddProp(TEXT("shrub_04/SM_ps_shrub_04_1k"), FVector(Rg.Center + FVector2D(U, (J % 2 ? 1.0f : -1.0f) * (Rg.HalfSize.Y + 30.0f)), 0.0f), R.FRandRange(0.0f, 360.0f), R.FRandRange(1.6f, 2.2f), false);
			}
		}
		// Hydrant and a garden tap on the cell block wall by the west beds.
		const FVector Hydrant(S.X - 520.0f, Y0 - 120.0f, 0.0f); // east of Bed2, out of the door-to-bed lanes
		for (const TCHAR* Part : { TEXT("fire_hydrant/SM_fire_hydrant_aged"), TEXT("fire_hydrant/SM_fire_hydrant_cap_01_aged"), TEXT("fire_hydrant/SM_fire_hydrant_cap_02_aged"),
				TEXT("fire_hydrant/SM_fire_hydrant_cap_03_aged"), TEXT("fire_hydrant/SM_fire_hydrant_chain_aged") })
		{
			AddProp(Part, Hydrant, 200.0f, 1.25f, false);
		}
		for (const TCHAR* Part : { TEXT("garden_hose_wall_mounted_01/SM_garden_hose_wall_bracket"), TEXT("garden_hose_wall_mounted_01/SM_garden_hose") })
		{
			AddProp(Part, FVector(DoorX - 1700.0f * Cfg->LayoutScale, Y0 - 4.0f, 95.0f), 90.0f, 1.3f, false);
		}
		// Oil drums in the dead corners and under the tower.
		for (const FVector2D& P : { FVector2D(S.X - 230, S.Y - 120), FVector2D(S.X - 170, S.Y - 170), FVector2D(S.X - 290, S.Y - 180), FVector2D(250, 140), FVector2D(310, 120) })
		{
			AddProp(TEXT("Barrel_01/SM_ps_Barrel_01_2k"), FVector(P, 0.0f), R.FRandRange(0.0f, 360.0f), 1.0f, true);
		}
		// Hand truck in the dead corner (by the door it stood in every walk out, and the bot dodged it toward the wall).
		AddProp(TEXT("hand_truck/SM_ps_hand_truck_1k"), FVector(S.X - 260.0f, Y0 - 80.0f, 0.0f), 200.0f, 1.0f, false);
		// A service bay: two roller shutters set into the cell block east of the door.
		for (const float X : { DoorX + 1700.0f * Cfg->LayoutScale, DoorX + 1700.0f * Cfg->LayoutScale + 110.0f })
		{
			AddProp(TEXT("rollershutter_door/SM_rollershutter_door"), FVector(X, Y0 - 14.0f, 0.0f), 0.0f, 1.0f, false);
		}
		// Ventilation duct along the top of the cell block, with braces and a few vents.
		const float DuctZ = Cfg->CellBlockHeight - 110.0f;
		int32 Seg = 0;
		for (float X = S.X * 0.14f; X < S.X - 300.0f; X += 60.0f, ++Seg)
		{
			AddProp(TEXT("modular_airduct_rectangular_01/SM_modular_airduct_rectangular_01_single_01"), FVector(X, Y0 - 40.0f, DuctZ), 90.0f, 1.0f, false);
			if (Seg % 8 == 0)
			{
				AddProp(TEXT("modular_airduct_rectangular_01/SM_modular_airduct_rectangular_01_brace_wire_01"), FVector(X, Y0 - 40.0f, DuctZ + 26.0f), 90.0f, 1.0f, false);
			}
			if (Seg % 13 == 6)
			{
				AddProp(TEXT("modular_airduct_rectangular_01/SM_modular_airduct_rectangular_01_vent_01"), FVector(X, Y0 - 74.0f, DuctZ), 90.0f, 1.0f, false);
			}
		}
	}

	// Grass in the planters.
	for (const FPSPropPlacement& P : Cfg->Props)
	{
		if (P.Mesh.Contains(TEXT("planter_box")))
		{
			for (int32 J = -1; J <= 1; ++J)
			{
				AddProp(Tufts[3 + (J + 1)], P.Location + FVector(0.0f, J * 28.0f, 40.0f), R.FRandRange(0.0f, 360.0f), 2.2f, false);
			}
		}
	}
	int32 Instances = 0;
	for (const TPair<FString, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>>& It : PropComponents)
	{
		Instances += It.Value ? It.Value->GetInstanceCount() : 0;
	}
	UE_LOG(LogPocketSand, Log, TEXT("[PocketSand] Props: %d meshes, %d instances."), PropComponents.Num(), Instances);
}
void APSYardBuilder::HandleRegionFill(FName RegionId, float Fraction, FVector Location)
{
	if (TObjectPtr<UMaterialInstanceDynamic>* MID = RegionMIDs.Find(RegionId))
	{
		float Start = 0.75f;
		for (const FPSRegionDef& R : Cfg->Regions)
		{
			if (R.Id == RegionId)
			{
				Start = Cfg->GetSurface(R.Surface).FullTintStart;
			}
		}
		// The surface starts turning yellow near capacity ("each bed only takes so much").
		const float T = FMath::Clamp((Fraction - Start) / FMath::Max(1.0f - Start, 0.01f), 0.0f, 1.0f);
		(*MID)->SetScalarParameterValue(TEXT("Fill"), T * FullTintStrength);
	}
}

void APSYardBuilder::ExportMeshes(const FString& Directory) const
{
	IFileManager::Get().MakeDirectory(*Directory, true);
	TArray<TPair<FString, const FPSMeshData*>> All;
	for (const TPair<FName, FPSMeshData>& P : Pieces)
	{
		All.Add(TPair<FString, const FPSMeshData*>(P.Key.ToString(), &P.Value));
	}
	const FString File = Directory / TEXT("PS_Yard_Generated.gltf");
	UE_LOG(LogPocketSand, Log, TEXT("[PocketSand] Mesh export %s %s"), *File, PSMesh::ExportGLTF(File, All) ? TEXT("ok") : TEXT("FAILED"));
}
