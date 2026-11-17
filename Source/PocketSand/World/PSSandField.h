// Pocket Sand - the sand simulation as a plain struct (P44 pattern): where every grain is, and whether it shows.
// Conserved: Poured == sum(Shown) + sum(Hidden) + Removed. PocketSand.SandField.Conservation tests it.

#pragma once

#include "CoreMinimal.h"
#include "Data/PSTypes.h"

struct POCKETSAND_API FPSSandField
{
	struct FRegion
	{
		FPSRegionDef Def;
		FPSSurfaceRow Row;
		float Absorbed = 0.0f;
		TArray<int32> Cells;
		float Fraction() const { return Def.Capacity > 0.0f ? FMath::Clamp(Absorbed / Def.Capacity, 0.0f, 1.0f) : 1.0f; }
		bool IsFull() const { return Absorbed >= Def.Capacity - 1e-4f; }
	};

	int32 W = 0;
	int32 H = 0;
	float Cell = 25.0f;
	FVector2D Origin = FVector2D::ZeroVector;
	float SpillOffset = 45.0f;

	TArray<float> Shown;
	TArray<float> Hidden;
	TArray<uint8> Surface;
	TArray<int16> Region;
	TArray<FRegion> Regions;
	FPSSurfaceRow Rows[6];
	TSet<int32> Active;
	double Poured = 0.0;
	double Removed = 0.0;

	void Init(const FVector2D& Size, float CellSize, const TArray<FPSSurfaceRow>& SurfaceRows, const TArray<FPSRegionDef>& RegionDefs,
		const FVector2D& ApronCenter, const FVector2D& ApronHalf, float InSpillOffset);
	void Reset();

	bool CellOf(const FVector2D& P, int32& OutX, int32& OutY) const;
	int32 IndexOf(const FVector2D& P) const;
	FVector2D CellCenter(int32 Index) const;
	const FPSSurfaceRow& RowAt(int32 Index) const { return Rows[FMath::Min<int32>(Surface[Index], 5)]; }
	bool IsShownCell(int32 Index) const { return Shown[Index] > RowAt(Index).ShowThreshold; }

	/** Pour Amount bags around P. Returns the amount that stayed visible. */
	float Deposit(const FVector2D& P, float Amount, float Radius);
	/** Drop sand that lands visibly (spike scatter, spills), regardless of surface. */
	void DepositShown(const FVector2D& P, float Amount, float Radius);
	/** Feet grind shown sand into the gravel (moves it to Hidden). Mul scales the surface trample rate. */
	void Trample(const FVector2D& P, float Radius, float Dt, float Mul);
	float ShownMass(const FVector2D& P, float Radius) const;
	float AnyMass(const FVector2D& P, float Radius) const;
	/** Guards rake it up: removes sand (and hidden sand too when bHidden). Returns bags removed. */
	float Clear(const FVector2D& P, float Radius, bool bHidden);
	/** Remove hidden sand from a region (spike). Returns bags taken. */
	float TakeFromRegion(int32 RegionIndex, float Amount);
	/** The heaviest visible patch inside a view cone. BinSize groups cells into candidate patches. */
	bool BestPatch(const FVector2D& Eye, const FVector2D& Dir, float Range, float CosHalf, float BinSize, float MinMass, FVector2D& OutCenter, float& OutMass) const;

	int32 RegionAt(const FVector2D& P) const;
	/** Would sand poured here right now be swallowed (true) or show (false)? */
	bool WouldHide(const FVector2D& P) const;
	/** The whole pour footprint (centre and four points at Radius) lands on surfaces with room. */
	bool FootprintHides(const FVector2D& P, float Radius) const;
	double TotalShown() const;
	double TotalHidden() const;

private:
	template <typename F> void ForCells(const FVector2D& P, float Radius, F&& Fn) const;
	void AddShown(int32 Index, float Amount);
	FVector2D SpillPoint(const FRegion& R, const FVector2D& From) const;
};
