// Pocket Sand - round config defaults (the yard layout lives here as data; DA_PS_Round inherits and can override it).

#include "Data/PSRoundConfig.h"
#include "Engine/DataTable.h"

#define LOCTEXT_NAMESPACE "PocketSand"

UPSRoundConfig::UPSRoundConfig()
{
	// Layout (D22): the yard was authored at 64 x 44 m for a 600 cm/s jog. The hero now runs on Rocketbox's native mocap
	// run (288 cm/s, clean knees and feet), so the yard is scaled with LayoutScale: interior points scale, anything within
	// WallKeep of a wall keeps its distance to that wall, and pieces that belong to a structure are placed relative to it.
	// Surfaces scale by RegionScale; human-sized things (props, characters, the pour footprint) keep their size.
	const FVector2D Authored(6400.0f, 4400.0f);
	const float L = LayoutScale;
	const float RS = RegionScale;
	YardSize = Authored * L;
	auto Axis = [this, L](float V, float Old, float New)
	{
		return V < WallKeep ? V : (Old - V < WallKeep ? New - (Old - V) : V * L);
	};
	auto P = [this, &Authored, &Axis](float X, float Y)
	{
		return FVector2D(Axis(X, Authored.X, YardSize.X), Axis(Y, Authored.Y, YardSize.Y));
	};
	DoorLocation = P(3400.0f, 4370.0f);
	PlayerStart = DoorLocation + FVector2D(0.0f, -280.0f);
	TowerLocation = FVector(P(6120.0f, 3250.0f), 0.0f);
	HoopLocation = P(1450.0f, 1850.0f);
	ApronHalf = FVector2D(520.0f, 330.0f) * RS;
	GuardSightRange = 1500.0f * L;
	TowerRange = 2100.0f * L;
	ProbeDetour = 700.0f * L;
	SpikeThrowMin = 300.0f * L;
	SpikeThrowMax = 600.0f * L;
	const FVector2D Tower(TowerLocation);

	auto Region = [this, RS](const TCHAR* Id, EPSSurface S, FVector2D C, FVector2D H, float Cap, float TopZ, bool bRound = false)
	{
		FPSRegionDef R;
		R.Id = FName(Id);
		R.Surface = S;
		R.Center = C;
		R.HalfSize = H * RS;
		R.Capacity = Cap;
		R.TopZ = TopZ;
		R.bRound = bRound;
		Regions.Add(R);
	};
	// Four raised garden beds along the north and south walls, the court mid-yard, the pit in the west, puddles under the tower.
	Region(TEXT("Bed1"), EPSSurface::Soil, P(1950.0f, 3990.0f), FVector2D(380.0f, 150.0f), 2.0f, 8.0f);
	Region(TEXT("Bed2"), EPSSurface::Soil, P(4900.0f, 3990.0f), FVector2D(380.0f, 150.0f), 2.0f, 8.0f);
	Region(TEXT("Bed3"), EPSSurface::Soil, P(1500.0f, 600.0f), FVector2D(380.0f, 150.0f), 2.0f, 8.0f);
	Region(TEXT("Bed4"), EPSSurface::Soil, P(3300.0f, 600.0f), FVector2D(380.0f, 150.0f), 2.0f, 8.0f);
	const FVector2D Court = P(3700.0f, 2300.0f);
	Region(TEXT("Court"), EPSSurface::Court, Court, FVector2D(680.0f, 380.0f), 3.0f, 3.0f);
	Region(TEXT("Pit"), EPSSurface::Pit, P(950.0f, 3250.0f), FVector2D(430.0f, 170.0f), 3.0f, 2.0f);
	// Puddles keep their place under the tower (authored offsets from the tower, scaled).
	Region(TEXT("Puddle1"), EPSSurface::Puddle, Tower + FVector2D(-700.0f, -730.0f) * L, FVector2D(175.0f, 175.0f), 1.2f, 1.0f, true);
	Region(TEXT("Puddle2"), EPSSurface::Puddle, Tower + FVector2D(-240.0f, -1070.0f) * L, FVector2D(150.0f, 150.0f), 1.2f, 1.0f, true);
	const float CourtHalfY = 380.0f * RS;
	// Guard B stands off the court's north side, beside (not on) the net post.
	GuardBPost = Court + FVector2D(150.0f, -CourtHalfY - 200.0f);

	// The west table sits between the wall and Bed1 (the scaled point would put its bin in the bed).
	Benches = { FVector2D(560.0f, YardSize.Y - 220.0f), P(2600.0f, 3600.0f), P(4600.0f, 1400.0f) };
	auto PropAt = [this](const TCHAR* Mesh, const FVector2D& XY, float Z, float Yaw, float Scale = 1.0f, bool bCollision = false, float Block = 0.0f)
	{
		FPSPropPlacement Pp;
		Pp.Mesh = Mesh;
		Pp.Location = FVector(XY, Z);
		Pp.Yaw = Yaw;
		Pp.Scale = Scale;
		Pp.bCollision = bCollision;
		Props.Add(Pp);
		if (Block > 0.0f)
		{
			Obstacles.Add({ XY, Block });
		}
	};
	auto Prop = [&PropAt, &P](const TCHAR* Mesh, float X, float Y, float Z, float Yaw, float Scale = 1.0f, bool bCollision = false, float Block = 0.0f)
	{
		PropAt(Mesh, P(X, Y), Z, Yaw, Scale, bCollision, Block);
	};
	// Picnic tables where inmates sit (long side along X), each with a bin and a couple of plastic chairs.
	for (int32 I = 0; I < Benches.Num(); ++I)
	{
		const FVector2D B = Benches[I];
		PropAt(TEXT("wooden_picnic_table/SM_ps_wooden_picnic_table_1k"), B, 0.0f, 90.0f + I * 4.0f, 1.0f, true);
		Obstacles.Add({ B + FVector2D(-80.0f, 0.0f), 125.0f });
		Obstacles.Add({ B + FVector2D(80.0f, 0.0f), 125.0f });
		PropAt(I % 2 ? TEXT("metal_trash_can/SM_metal_trash_can_rust") : TEXT("metal_trash_can/SM_metal_trash_can"), B + FVector2D(230.0f, 40.0f), 0.0f, 20.0f + I * 50.0f, 1.0f, true, 40.0f);
		PropAt(TEXT("plastic_monobloc_chair_01/SM_ps_plastic_monobloc_chair_01_1k"), B + FVector2D(-250.0f, -60.0f), 0.0f, 70.0f + I * 30.0f, 1.0f, true, 40.0f);
	}
	// Spectator chairs and a crate on the court's west sideline.
	const float Side = -680.0f * RS - 110.0f;
	PropAt(TEXT("plastic_monobloc_chair_01/SM_ps_plastic_monobloc_chair_01_1k"), Court + FVector2D(Side, -180.0f * RS), 0.0f, -10.0f, 1.0f, true, 38.0f);
	PropAt(TEXT("plastic_monobloc_chair_01/SM_ps_plastic_monobloc_chair_01_1k"), Court + FVector2D(Side - 10.0f, -40.0f * RS), 0.0f, 8.0f, 1.0f, true, 38.0f);
	PropAt(TEXT("plastic_crate_03/SM_ps_plastic_crate_03_1k"), Court + FVector2D(Side + 10.0f, 220.0f * RS), 0.0f, 25.0f, 1.0f, true, 30.0f);
	// Bins and a utility box by the door; planters either side of the apron.
	PropAt(TEXT("metal_trash_can/SM_metal_trash_can"), DoorLocation + FVector2D(650.0f * L, -40.0f), 0.0f, -15.0f, 1.0f, true, 40.0f);
	PropAt(TEXT("utility_box_02/SM_ps_utility_box_02_2k"), DoorLocation + FVector2D(-700.0f * L, 0.0f), 0.0f, 180.0f, 1.0f, true, 55.0f);
	PropAt(TEXT("planter_box_01/SM_ps_planter_box_01_1k"), DoorLocation + FVector2D(-ApronHalf.X - 60.0f, -ApronHalf.Y - 20.0f), 0.0f, 90.0f, 1.0f, true, 50.0f);
	PropAt(TEXT("planter_box_01/SM_ps_planter_box_01_1k"), DoorLocation + FVector2D(ApronHalf.X + 60.0f, -ApronHalf.Y - 20.0f), 0.0f, 90.0f, 1.0f, true, 50.0f);
	// Yard gate in the south wall (inside face): frame and leaves share one origin.
	const FVector2D Gate = P(4900.0f, 30.0f);
	PropAt(TEXT("large_iron_gate/SM_large_iron_gate_left_door"), Gate + FVector2D(-70.0f, 0.0f), 0.0f, 0.0f, 1.0f, false);
	PropAt(TEXT("large_iron_gate/SM_large_iron_gate_right_door"), Gate + FVector2D(75.0f, 0.0f), 0.0f, 0.0f, 1.0f, false);
	PropAt(TEXT("large_iron_gate/SM_large_iron_gate"), Gate, 0.0f, 0.0f, 1.0f, false);
	// Jersey barriers in front of the tower, clear of its legs and the puddles.
	PropAt(TEXT("concrete_road_barrier/SM_ps_concrete_road_barrier_1k"), Tower + FVector2D(-330.0f, -150.0f), 0.0f, 15.0f, 1.0f, true, 80.0f);
	PropAt(TEXT("concrete_road_barrier/SM_ps_concrete_road_barrier_1k"), Tower + FVector2D(-90.0f, -340.0f), 0.0f, 35.0f, 1.0f, true, 80.0f);
	// Clutter along the walls (out of every lane): crates, boxes, bags, tyres.
	Prop(TEXT("plastic_crate_03/SM_ps_plastic_crate_03_1k"), 250.0f, 4300.0f, 0.0f, 10.0f, 1.0f, true, 35.0f);
	Prop(TEXT("plastic_crate_03/SM_ps_plastic_crate_03_1k"), 260.0f, 4300.0f, 27.0f, 35.0f, 1.0f, false);
	Prop(TEXT("cardboard_box_01/SM_ps_cardboard_box_01_2k"), 360.0f, 4250.0f, 18.0f, -20.0f, 1.0f, true, 35.0f);
	Prop(TEXT("trashbag/SM_ps_trashbag_2k"), 150.0f, 4180.0f, 0.0f, 40.0f, 1.0f, true, 35.0f);
	// (The north-east corner bag sat inside a corner drum: removed 2026-11-13.)
	Prop(TEXT("old_tyre/SM_ps_old_tyre_1k"), 6290.0f, 1200.0f, 30.0f, 90.0f, 1.0f, true, 40.0f);
	Prop(TEXT("old_tyre/SM_ps_old_tyre_1k"), 6230.0f, 1150.0f, 8.0f, 0.0f, 1.0f, false);
	Prop(TEXT("old_tyre/SM_ps_old_tyre_1k"), 120.0f, 1700.0f, 30.0f, 0.0f, 1.0f, true, 40.0f);
	Prop(TEXT("cardboard_box_01/SM_ps_cardboard_box_01_2k"), 6280.0f, 300.0f, 18.0f, 30.0f, 1.0f, true, 35.0f);
	// Reviewer 2026-11-12 "move assets here and there, populate the ground more": barrels, tyres,
	// crates and bags along the walls (the beds already carry their garden kit). All kept off the walking lanes.
	Prop(TEXT("Barrel_01/SM_ps_Barrel_01_2k"), 6300.0f, 2000.0f, 0.0f, 0.0f, 1.0f, true, 45.0f);
	Prop(TEXT("Barrel_01/SM_ps_Barrel_01_2k"), 6290.0f, 2110.0f, 0.0f, 40.0f, 1.0f, true, 45.0f);
	Prop(TEXT("Barrel_01/SM_ps_Barrel_01_2k"), 110.0f, 2750.0f, 0.0f, 15.0f, 1.0f, true, 45.0f);
	Prop(TEXT("old_tyre/SM_ps_old_tyre_1k"), 2400.0f, 110.0f, 8.0f, 0.0f, 1.0f, true, 45.0f);
	Prop(TEXT("old_tyre/SM_ps_old_tyre_1k"), 2410.0f, 112.0f, 25.0f, 30.0f, 1.0f, false);
	Prop(TEXT("plastic_crate_03/SM_ps_plastic_crate_03_1k"), 5300.0f, 100.0f, 0.0f, 5.0f, 1.0f, true, 35.0f);
	Prop(TEXT("cardboard_box_01/SM_ps_cardboard_box_01_2k"), 5380.0f, 110.0f, 18.0f, -25.0f, 1.0f, true, 35.0f);
	Prop(TEXT("trashbag/SM_ps_trashbag_2k"), 120.0f, 900.0f, 0.0f, 10.0f, 1.0f, true, 35.0f);
	// Two more guards: one at the yard gate looking into the yard, one by the cell door looking along the block.
	GuardPosts = { FVector(P(4900.0f, 30.0f) + FVector2D(-190.0f, 140.0f), 90.0f), FVector(DoorLocation + FVector2D(-330.0f, -120.0f), -20.0f) };
	// Drain covers in the gravel.
	Prop(TEXT("water_manhole_cover/SM_water_manhole_cover_frame"), 2250.0f, 1900.0f, 0.5f, 0.0f);
	Prop(TEXT("water_manhole_cover/SM_water_manhole_cover"), 2250.0f, 1900.0f, 1.0f, 0.0f);
	Prop(TEXT("water_manhole_cover/SM_water_manhole_cover_frame"), 4700.0f, 3000.0f, 0.5f, 0.0f);
	Prop(TEXT("water_manhole_cover/SM_water_manhole_cover"), 4700.0f, 3000.0f, 1.0f, 0.0f);
	Obstacles.Add({ HoopLocation, 45.0f });
	Obstacles.Add({ Tower + FVector2D(-160.0f, -160.0f), 60.0f });
	Obstacles.Add({ Tower + FVector2D(160.0f, 160.0f), 60.0f });
	Obstacles.Add({ Tower + FVector2D(-160.0f, 160.0f), 60.0f });
	Obstacles.Add({ Tower + FVector2D(160.0f, -160.0f), 60.0f });
	Obstacles.Add({ Tower, 120.0f });
	// Volleyball net posts (just outside the court's long sides).
	Obstacles.Add({ Court + FVector2D(0.0f, -CourtHalfY - 90.0f), 30.0f });
	Obstacles.Add({ Court + FVector2D(0.0f, CourtHalfY + 90.0f), 30.0f });

	GuardARoute.Points = { P(700.0f, 1150.0f), P(2500.0f, 1150.0f), P(2650.0f, 1950.0f), P(2100.0f, 2900.0f), P(650.0f, 2550.0f) };
	GuardBRoute.Points = { P(3200.0f, 1150.0f), P(5200.0f, 1150.0f), P(5650.0f, 2900.0f), P(4600.0f, 3450.0f), P(2900.0f, 3250.0f) };

	PointsOfInterest = {
		P(1700.0f, 3600.0f),  // benches
		P(1650.0f, 2150.0f),  // hoop
		P(2800.0f, 2400.0f),  // court sideline west
		P(2400.0f, 1000.0f),  // bed row
		P(4250.0f, 3450.0f),  // by the door
		P(2100.0f, 3150.0f),  // pit run-up
		P(4950.0f, 2050.0f),  // east yard
		P(3300.0f, 1350.0f),  // yard centre
		P(4800.0f, 900.0f),  // bed row east
	};
}
TArray<FPSSurfaceRow> UPSRoundConfig::DefaultSurfaces()
{
	TArray<FPSSurfaceRow> Rows;
	auto Add = [&Rows](EPSSurface S, const FText& Tag, bool bAbsorbs, bool bSpills, float Show, float Trample, float Exposure, float TintStart)
	{
		FPSSurfaceRow R;
		R.Surface = S;
		R.Tag = Tag;
		R.bAbsorbs = bAbsorbs;
		R.bSpillsWhenFull = bSpills;
		R.ShowThreshold = Show;
		R.TrampleRate = Trample;
		R.ExposureMul = Exposure;
		R.FullTintStart = TintStart;
		Rows.Add(R);
	};
	Add(EPSSurface::Gravel, FText::GetEmpty(), false, false, 0.0012f, 3.0f, 1.0f, 1.0f);
	Add(EPSSurface::Soil, LOCTEXT("TagSoil", "SOIL"), true, false, 0.0015f, 0.0f, 1.0f, 0.75f);
	Add(EPSSurface::Court, LOCTEXT("TagCourt", "COURT"), true, true, 0.002f, 0.0f, 1.0f, 0.8f);
	Add(EPSSurface::Puddle, LOCTEXT("TagPuddle", "PUDDLE"), true, true, 0.002f, 0.0f, 1.0f, 0.8f);
	Add(EPSSurface::Pit, LOCTEXT("TagPit", "PIT"), true, false, 0.002f, 0.0f, 1.8f, 0.85f);
	Add(EPSSurface::Concrete, FText::GetEmpty(), false, false, 0.0008f, 1.2f, 1.0f, 1.0f);
	return Rows;
}

TArray<FPSPhaseRow> UPSRoundConfig::DefaultPhases()
{
	TArray<FPSPhaseRow> Rows;
	auto Add = [&Rows](float Start, const FText& Title, const FText& Sub, float Sweep, float Crowd, float Guard, bool bB, bool bSpikes)
	{
		FPSPhaseRow R;
		R.StartSeconds = Start;
		R.Title = Title;
		R.Sub = Sub;
		R.TowerSweepSeconds = Sweep;
		R.CrowdSpeedMul = Crowd;
		R.GuardSpeedMul = Guard;
		R.bSecondGuardPatrols = bB;
		R.bSpikes = bSpikes;
		Rows.Add(R);
	};
	Add(0.0f, LOCTEXT("P1", "YARD TIME"), FText::GetEmpty(), 9.0f, 1.0f, 1.0f, false, false);
	Add(30.0f, LOCTEXT("P2", "RALLY"), LOCTEXT("P2Sub", "Spikes throw loose court sand"), 8.0f, 1.08f, 1.05f, false, true);
	Add(60.0f, LOCTEXT("P3", "LAST CALL"), LOCTEXT("P3Sub", "Both guards walk. The tower sweeps faster."), 6.0f, 1.15f, 1.12f, true, true);
	return Rows;
}

FPSSurfaceRow UPSRoundConfig::GetSurface(EPSSurface Surface) const
{
	if (CachedSurfaces.Num() == 0)
	{
		if (const UDataTable* Table = SurfaceTable.LoadSynchronous())
		{
			Table->ForeachRow<FPSSurfaceRow>(TEXT("Surfaces"), [this](const FName&, const FPSSurfaceRow& Row) { CachedSurfaces.Add(Row); });
		}
		if (CachedSurfaces.Num() == 0)
		{
			CachedSurfaces = DefaultSurfaces();
		}
	}
	for (const FPSSurfaceRow& R : CachedSurfaces)
	{
		if (R.Surface == Surface)
		{
			return R;
		}
	}
	for (const FPSSurfaceRow& R : DefaultSurfaces())
	{
		if (R.Surface == Surface)
		{
			return R;
		}
	}
	return FPSSurfaceRow();
}

TArray<FPSPhaseRow> UPSRoundConfig::GetPhases() const
{
	TArray<FPSPhaseRow> Rows;
	if (const UDataTable* Table = PhaseTable.LoadSynchronous())
	{
		Table->ForeachRow<FPSPhaseRow>(TEXT("Phases"), [&Rows](const FName&, const FPSPhaseRow& Row) { Rows.Add(Row); });
	}
	if (Rows.Num() == 0)
	{
		Rows = DefaultPhases();
	}
	Rows.Sort([](const FPSPhaseRow& A, const FPSPhaseRow& B) { return A.StartSeconds < B.StartSeconds; });
	return Rows;
}

#undef LOCTEXT_NAMESPACE
