// Pocket Sand - automation test: sand is conserved (P44). 1,000 seeded pours, tramples, clears and spike transfers;
// poured == shown + hidden + removed, and nothing goes negative.

#include "Misc/AutomationTest.h"
#include "World/PSSandField.h"
#include "Data/PSRoundConfig.h"

#if WITH_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPSSandFieldConservationTest, "PocketSand.SandField.Conservation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FPSSandFieldConservationTest::RunTest(const FString& Parameters)
{
	const UPSRoundConfig* Cfg = GetDefault<UPSRoundConfig>();
	FPSSandField F;
	F.Init(Cfg->YardSize, Cfg->CellSize, UPSRoundConfig::DefaultSurfaces(), Cfg->Regions, Cfg->DoorLocation, Cfg->ApronHalf, Cfg->SpillOffset);
	FRandomStream R(12345);
	int32 CourtIndex = INDEX_NONE;
	for (int32 I = 0; I < F.Regions.Num(); ++I)
	{
		if (F.Regions[I].Def.Surface == EPSSurface::Court)
		{
			CourtIndex = I;
		}
	}
	for (int32 Step = 0; Step < 1000; ++Step)
	{
		const FVector2D P(R.FRandRange(0.0f, Cfg->YardSize.X), R.FRandRange(0.0f, Cfg->YardSize.Y));
		switch (R.RandRange(0, 9))
		{
		case 0: case 1: case 2: case 3:
			F.Deposit(P, R.FRandRange(0.0f, 0.05f), 45.0f);
			break;
		case 4: case 5:
			F.Trample(P, 120.0f, 1.0f / 30.0f, 1.0f);
			break;
		case 6:
			F.Clear(P, 260.0f, R.FRand() < 0.5f);
			break;
		case 7:
		{
			const float Got = F.TakeFromRegion(CourtIndex, R.FRandRange(0.0f, 0.2f));
			F.DepositShown(P, Got, 110.0f);
			break;
		}
		default:
			// Pour straight onto a hiding surface (fills, then spills).
			if (F.Regions.Num() > 0)
			{
				const FPSRegionDef& D = F.Regions[R.RandRange(0, F.Regions.Num() - 1)].Def;
				F.Deposit(D.Center, R.FRandRange(0.0f, 0.3f), 45.0f);
			}
			break;
		}
	}
	const double Total = F.TotalShown() + F.TotalHidden() + F.Removed;
	TestTrue(FString::Printf(TEXT("Conserved: poured %.6f vs shown+hidden+removed %.6f"), F.Poured, Total), FMath::Abs(Total - F.Poured) < 1e-3 * FMath::Max(1.0, F.Poured));
	bool bNonNegative = true;
	for (int32 I = 0; I < F.Shown.Num(); ++I)
	{
		bNonNegative &= F.Shown[I] >= -1e-6f && F.Hidden[I] >= -1e-6f;
	}
	TestTrue(TEXT("No negative cells"), bNonNegative);
	for (const FPSSandField::FRegion& Reg : F.Regions)
	{
		TestTrue(FString::Printf(TEXT("%s within capacity"), *Reg.Def.Id.ToString()), Reg.Absorbed <= Reg.Def.Capacity + 1e-3f);
	}
	return true;
}

#endif
