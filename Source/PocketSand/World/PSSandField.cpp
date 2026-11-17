// Pocket Sand - sand simulation.

#include "World/PSSandField.h"

template <typename F>
void FPSSandField::ForCells(const FVector2D& P, float Radius, F&& Fn) const
{
	if (W <= 0 || H <= 0)
	{
		return;
	}
	const int32 X0 = FMath::Clamp(FMath::FloorToInt((P.X - Radius - Origin.X) / Cell), 0, W - 1);
	const int32 X1 = FMath::Clamp(FMath::FloorToInt((P.X + Radius - Origin.X) / Cell), 0, W - 1);
	const int32 Y0 = FMath::Clamp(FMath::FloorToInt((P.Y - Radius - Origin.Y) / Cell), 0, H - 1);
	const int32 Y1 = FMath::Clamp(FMath::FloorToInt((P.Y + Radius - Origin.Y) / Cell), 0, H - 1);
	const float R2 = Radius * Radius;
	for (int32 Y = Y0; Y <= Y1; ++Y)
	{
		for (int32 X = X0; X <= X1; ++X)
		{
			const FVector2D C(Origin.X + (X + 0.5f) * Cell, Origin.Y + (Y + 0.5f) * Cell);
			const float D2 = FVector2D::DistSquared(C, P);
			if (D2 <= R2)
			{
				Fn(Y * W + X, D2);
			}
		}
	}
}

void FPSSandField::Init(const FVector2D& Size, float CellSize, const TArray<FPSSurfaceRow>& SurfaceRows, const TArray<FPSRegionDef>& RegionDefs,
	const FVector2D& ApronCenter, const FVector2D& ApronHalf, float InSpillOffset)
{
	Cell = FMath::Max(CellSize, 5.0f);
	W = FMath::Max(1, FMath::CeilToInt(Size.X / Cell));
	H = FMath::Max(1, FMath::CeilToInt(Size.Y / Cell));
	SpillOffset = InSpillOffset;
	for (int32 I = 0; I < 6; ++I)
	{
		Rows[I] = FPSSurfaceRow();
		Rows[I].Surface = static_cast<EPSSurface>(I);
	}
	for (const FPSSurfaceRow& R : SurfaceRows)
	{
		Rows[FMath::Clamp(static_cast<int32>(R.Surface), 0, 5)] = R;
	}
	const int32 N = W * H;
	Surface.Init(static_cast<uint8>(EPSSurface::Gravel), N);
	Region.Init(-1, N);
	Regions.Reset();
	for (const FPSRegionDef& Def : RegionDefs)
	{
		FRegion& R = Regions.AddDefaulted_GetRef();
		R.Def = Def;
		R.Row = Rows[FMath::Clamp(static_cast<int32>(Def.Surface), 0, 5)];
	}
	for (int32 I = 0; I < N; ++I)
	{
		const FVector2D C = CellCenter(I);
		if (FMath::Abs(C.X - ApronCenter.X) <= ApronHalf.X && FMath::Abs(C.Y - ApronCenter.Y) <= ApronHalf.Y)
		{
			Surface[I] = static_cast<uint8>(EPSSurface::Concrete);
		}
		for (int32 R = 0; R < Regions.Num(); ++R)
		{
			if (Regions[R].Def.Contains(C))
			{
				Surface[I] = static_cast<uint8>(Regions[R].Def.Surface);
				Region[I] = static_cast<int16>(R);
				Regions[R].Cells.Add(I);
				break;
			}
		}
	}
	Reset();
}

void FPSSandField::Reset()
{
	const int32 N = W * H;
	Shown.Init(0.0f, N);
	Hidden.Init(0.0f, N);
	Active.Reset();
	for (FRegion& R : Regions)
	{
		R.Absorbed = 0.0f;
	}
	Poured = 0.0;
	Removed = 0.0;
}

bool FPSSandField::CellOf(const FVector2D& P, int32& OutX, int32& OutY) const
{
	OutX = FMath::FloorToInt((P.X - Origin.X) / Cell);
	OutY = FMath::FloorToInt((P.Y - Origin.Y) / Cell);
	return OutX >= 0 && OutY >= 0 && OutX < W && OutY < H;
}

int32 FPSSandField::IndexOf(const FVector2D& P) const
{
	int32 X, Y;
	if (!CellOf(P, X, Y))
	{
		return INDEX_NONE;
	}
	return Y * W + X;
}

FVector2D FPSSandField::CellCenter(int32 Index) const
{
	return FVector2D(Origin.X + ((Index % W) + 0.5f) * Cell, Origin.Y + ((Index / W) + 0.5f) * Cell);
}

void FPSSandField::AddShown(int32 Index, float Amount)
{
	if (Amount <= 0.0f || !Shown.IsValidIndex(Index))
	{
		return;
	}
	Shown[Index] += Amount;
	Active.Add(Index);
}

FVector2D FPSSandField::SpillPoint(const FRegion& R, const FVector2D& From) const
{
	FVector2D Dir = From - R.Def.Center;
	if (Dir.IsNearlyZero())
	{
		Dir = FVector2D(0.0f, -1.0f);
	}
	if (R.Def.bRound)
	{
		return R.Def.Center + Dir.GetSafeNormal() * (R.Def.HalfSize.X + SpillOffset);
	}
	// Push straight out through the nearest rectangle edge.
	const float Ex = R.Def.HalfSize.X - FMath::Abs(Dir.X);
	const float Ey = R.Def.HalfSize.Y - FMath::Abs(Dir.Y);
	FVector2D Out = From;
	if (Ex < Ey)
	{
		Out.X = R.Def.Center.X + FMath::Sign(Dir.X) * (R.Def.HalfSize.X + SpillOffset);
	}
	else
	{
		Out.Y = R.Def.Center.Y + FMath::Sign(Dir.Y) * (R.Def.HalfSize.Y + SpillOffset);
	}
	return Out;
}

float FPSSandField::Deposit(const FVector2D& P, float Amount, float Radius)
{
	if (Amount <= 0.0f)
	{
		return 0.0f;
	}
	Poured += Amount;
	// Gaussian footprint, weights normalised so the whole amount lands.
	TArray<TPair<int32, float>, TInlineAllocator<64>> Cells;
	float Sum = 0.0f;
	const float Sigma2 = FMath::Max(Radius * Radius * 0.35f, 1.0f);
	ForCells(P, FMath::Max(Radius, Cell * 0.75f), [&](int32 I, float D2)
	{
		const float Wt = FMath::Exp(-D2 / Sigma2);
		Cells.Add(TPair<int32, float>(I, Wt));
		Sum += Wt;
	});
	if (Cells.Num() == 0 || Sum <= 0.0f)
	{
		// Off the grid: count it as gone (the bot never pours there).
		Removed += Amount;
		return 0.0f;
	}
	float ShownTotal = 0.0f;
	for (const TPair<int32, float>& C : Cells)
	{
		float A = Amount * C.Value / Sum;
		const int32 RI = Region[C.Key];
		if (RI >= 0)
		{
			FRegion& R = Regions[RI];
			if (R.Row.bAbsorbs)
			{
				const float Take = FMath::Min(A, FMath::Max(0.0f, R.Def.Capacity - R.Absorbed));
				R.Absorbed += Take;
				Hidden[C.Key] += Take;
				A -= Take;
			}
			if (A > 0.0f && R.Row.bSpillsWhenFull)
			{
				// "If it grows, someone notices": the extra runs out onto the gravel rim.
				const int32 Target = IndexOf(SpillPoint(R, CellCenter(C.Key)));
				if (Target != INDEX_NONE && Region[Target] < 0)
				{
					AddShown(Target, A);
					ShownTotal += A;
					continue;
				}
			}
		}
		if (A > 0.0f)
		{
			AddShown(C.Key, A);
			ShownTotal += A;
		}
	}
	return ShownTotal;
}

void FPSSandField::DepositShown(const FVector2D& P, float Amount, float Radius)
{
	if (Amount <= 0.0f)
	{
		return;
	}
	Poured += Amount;
	TArray<int32, TInlineAllocator<64>> Cells;
	ForCells(P, FMath::Max(Radius, Cell * 0.75f), [&](int32 I, float) { Cells.Add(I); });
	if (Cells.Num() == 0)
	{
		Removed += Amount;
		return;
	}
	for (int32 I : Cells)
	{
		AddShown(I, Amount / Cells.Num());
	}
}

void FPSSandField::Trample(const FVector2D& P, float Radius, float Dt, float Mul)
{
	if (Active.Num() == 0)
	{
		return;
	}
	const float R2 = FMath::Max(Radius * Radius, 1.0f);
	TArray<int32, TInlineAllocator<16>> Emptied;
	ForCells(P, Radius, [&](int32 I, float D2)
	{
		float& S = Shown[I];
		if (S <= 0.0f)
		{
			return;
		}
		const float Rate = RowAt(I).TrampleRate;
		if (Rate <= 0.0f)
		{
			return;
		}
		const float Fall = 1.0f - D2 / R2;
		const float F = 1.0f - FMath::Exp(-Rate * Mul * Dt * Fall);
		float Moved = S * F;
		if (S - Moved < 1e-5f)
		{
			Moved = S;
			Emptied.Add(I);
		}
		S -= Moved;
		const_cast<FPSSandField*>(this)->Hidden[I] += Moved;
	});
	for (int32 I : Emptied)
	{
		Active.Remove(I);
	}
}

float FPSSandField::ShownMass(const FVector2D& P, float Radius) const
{
	float M = 0.0f;
	ForCells(P, Radius, [&](int32 I, float) { if (IsShownCell(I)) { M += Shown[I]; } });
	return M;
}

float FPSSandField::AnyMass(const FVector2D& P, float Radius) const
{
	float M = 0.0f;
	ForCells(P, Radius, [&](int32 I, float) { M += Shown[I] + Hidden[I]; });
	return M;
}

float FPSSandField::Clear(const FVector2D& P, float Radius, bool bHidden)
{
	float Got = 0.0f;
	ForCells(P, Radius, [&](int32 I, float)
	{
		Got += Shown[I];
		Shown[I] = 0.0f;
		Active.Remove(I);
		if (bHidden && Hidden[I] > 0.0f)
		{
			const float Hd = Hidden[I];
			Got += Hd;
			if (Region[I] >= 0)
			{
				FRegion& R = Regions[Region[I]];
				R.Absorbed = FMath::Max(0.0f, R.Absorbed - Hd);
			}
			Hidden[I] = 0.0f;
		}
	});
	Removed += Got;
	return Got;
}

float FPSSandField::TakeFromRegion(int32 RegionIndex, float Amount)
{
	if (!Regions.IsValidIndex(RegionIndex) || Amount <= 0.0f)
	{
		return 0.0f;
	}
	FRegion& R = Regions[RegionIndex];
	float Pool = 0.0f;
	for (int32 I : R.Cells)
	{
		Pool += Hidden[I];
	}
	if (Pool <= 0.0f)
	{
		return 0.0f;
	}
	const float Take = FMath::Min(Amount, Pool);
	const float F = Take / Pool;
	for (int32 I : R.Cells)
	{
		Hidden[I] -= Hidden[I] * F;
	}
	R.Absorbed = FMath::Max(0.0f, R.Absorbed - Take);
	// The taken sand leaves the books here and comes back through DepositShown (which counts it as poured).
	Poured -= Take;
	return Take;
}

bool FPSSandField::BestPatch(const FVector2D& Eye, const FVector2D& Dir, float Range, float CosHalf, float BinSize, float MinMass, FVector2D& OutCenter, float& OutMass) const
{
	struct FBin { float Mass = 0.0f; FVector2D Sum = FVector2D::ZeroVector; };
	TMap<int64, FBin> Bins;
	const float R2 = Range * Range;
	const FVector2D D = Dir.GetSafeNormal();
	for (int32 I : Active)
	{
		if (!IsShownCell(I))
		{
			continue;
		}
		const FVector2D C = CellCenter(I);
		const FVector2D To = C - Eye;
		const float L2 = To.SizeSquared();
		if (L2 > R2 || L2 < 1.0f)
		{
			continue;
		}
		if (FVector2D::DotProduct(To / FMath::Sqrt(L2), D) < CosHalf)
		{
			continue;
		}
		const int64 Key = (static_cast<int64>(FMath::FloorToInt(C.X / BinSize)) << 32) ^ static_cast<int64>(static_cast<uint32>(FMath::FloorToInt(C.Y / BinSize)));
		FBin& B = Bins.FindOrAdd(Key);
		B.Mass += Shown[I];
		B.Sum += C * Shown[I];
	}
	OutMass = 0.0f;
	for (const TPair<int64, FBin>& It : Bins)
	{
		if (It.Value.Mass > OutMass)
		{
			OutMass = It.Value.Mass;
			OutCenter = It.Value.Sum / FMath::Max(It.Value.Mass, 1e-6f);
		}
	}
	return OutMass >= MinMass;
}

int32 FPSSandField::RegionAt(const FVector2D& P) const
{
	const int32 I = IndexOf(P);
	return I == INDEX_NONE ? -1 : Region[I];
}

bool FPSSandField::WouldHide(const FVector2D& P) const
{
	const int32 RI = RegionAt(P);
	return RI >= 0 && Regions[RI].Row.bAbsorbs && !Regions[RI].IsFull();
}

bool FPSSandField::FootprintHides(const FVector2D& P, float Radius) const
{
	return WouldHide(P) && WouldHide(P + FVector2D(Radius, 0.0f)) && WouldHide(P - FVector2D(Radius, 0.0f)) && WouldHide(P + FVector2D(0.0f, Radius)) && WouldHide(P - FVector2D(0.0f, Radius));
}

double FPSSandField::TotalShown() const
{
	double S = 0.0;
	for (float V : Shown) { S += V; }
	return S;
}

double FPSSandField::TotalHidden() const
{
	double S = 0.0;
	for (float V : Hidden) { S += V; }
	return S;
}
