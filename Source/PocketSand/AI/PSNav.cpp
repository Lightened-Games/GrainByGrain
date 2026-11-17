// Pocket Sand - flat-yard steering.

#include "AI/PSNav.h"
#include "Data/PSRoundConfig.h"

FVector2D PSNav::ClampYard(const UPSRoundConfig* Cfg, const FVector2D& P, float Edge)
{
	if (!Cfg)
	{
		return P;
	}
	// The cell block's footing takes the last couple of metres along the north wall.
	return FVector2D(FMath::Clamp(P.X, Edge, Cfg->YardSize.X - Edge), FMath::Clamp(P.Y, Edge, Cfg->YardSize.Y - Edge - 60.0f));
}

FVector2D PSNav::PushOut(const UPSRoundConfig* Cfg, const FVector2D& P, float Radius, bool bRegions, float RegionMargin)
{
	if (!Cfg)
	{
		return P;
	}
	FVector2D Out = P;
	const TArray<FPSObstacle>* Lists[] = { &Cfg->Obstacles, &Cfg->RuntimeObstacles };
	for (const TArray<FPSObstacle>* List : Lists)
	for (const FPSObstacle& O : *List)
	{
		const FVector2D D = Out - O.Center;
		const float Min = O.Radius + Radius;
		const float L = D.Size();
		if (L < Min)
		{
			Out = O.Center + (L > 1.0f ? D / L : FVector2D(1.0f, 0.0f)) * Min;
		}
	}
	if (bRegions)
	{
		for (const FPSRegionDef& R : Cfg->Regions)
		{
			const float M = Radius + RegionMargin;
			if (!R.Contains(Out, M))
			{
				continue;
			}
			if (R.bRound)
			{
				const FVector2D D = Out - R.Center;
				Out = R.Center + (D.IsNearlyZero() ? FVector2D(1.0f, 0.0f) : D.GetSafeNormal()) * (R.HalfSize.X + M);
				continue;
			}
			const float Dx = R.HalfSize.X + M - FMath::Abs(Out.X - R.Center.X);
			const float Dy = R.HalfSize.Y + M - FMath::Abs(Out.Y - R.Center.Y);
			if (Dx < Dy)
			{
				Out.X = R.Center.X + FMath::Sign(Out.X - R.Center.X + 0.01f) * (R.HalfSize.X + M);
			}
			else
			{
				Out.Y = R.Center.Y + FMath::Sign(Out.Y - R.Center.Y + 0.01f) * (R.HalfSize.Y + M);
			}
		}
	}
	return ClampYard(Cfg, Out);
}

FVector2D PSNav::Avoid(const UPSRoundConfig* Cfg, const FVector2D& From, const FVector2D& Dir, float Radius, bool bRegions, float LookAhead, float RegionMargin)
{
	if (!Cfg || Dir.IsNearlyZero())
	{
		return Dir;
	}
	auto Slide = [&Dir, &From](const FVector2D& Centre)
	{
		const FVector2D ToC = (Centre - From).GetSafeNormal();
		FVector2D Tangent(-ToC.Y, ToC.X);
		if (FVector2D::DotProduct(Tangent, Dir) < 0.0f)
		{
			Tangent = -Tangent;
		}
		return (Tangent * 0.85f + Dir * 0.15f).GetSafeNormal();
	};
	const FVector2D Ahead = From + Dir * LookAhead;
	const FVector2D Mid = From + Dir * LookAhead * 0.5f;
	const TArray<FPSObstacle>* Lists[] = { &Cfg->Obstacles, &Cfg->RuntimeObstacles };
	for (const TArray<FPSObstacle>* List : Lists)
	for (const FPSObstacle& O : *List)
	{
		const float Min = O.Radius + Radius;
		if (FVector2D::DistSquared(Ahead, O.Center) < Min * Min || FVector2D::DistSquared(Mid, O.Center) < Min * Min)
		{
			if (FVector2D::DistSquared(From, O.Center) > Min * Min * 0.8f)
			{
				return Slide(O.Center);
			}
		}
	}
	if (bRegions)
	{
		for (const FPSRegionDef& R : Cfg->Regions)
		{
			if ((R.Contains(Ahead, Radius + RegionMargin) || R.Contains(Mid, Radius + RegionMargin)) && !R.Contains(From, Radius + RegionMargin * 0.5f))
			{
				return Slide(R.Center);
			}
		}
	}
	return Dir;
}

FVector2D PSNav::AvoidSmooth(const UPSRoundConfig* Cfg, const FVector2D& From, const FVector2D& Dir, float Radius, float LookAhead, float Strength)
{
	// Proportional avoidance for the bot: each obstacle near the path ahead bends it away from the obstacle, on the side
	// the path already passes (same side every frame), by how deep the overlap is. No sudden sideways dodges or U-turns.
	if (!Cfg || Dir.IsNearlyZero())
	{
		return Dir;
	}
	FVector2D Push = FVector2D::ZeroVector;
	const TArray<FPSObstacle>* Lists[] = { &Cfg->Obstacles, &Cfg->RuntimeObstacles };
	for (const TArray<FPSObstacle>* List : Lists)
	for (const FPSObstacle& O : *List)
	{
		const float Min = O.Radius + Radius;
		const FVector2D Rel = O.Center - From;
		const float Along = FVector2D::DotProduct(Rel, Dir);
		if (Along <= 0.0f || Along > LookAhead + Min || Rel.SizeSquared() < Min * Min * 0.8f)
		{
			continue; // behind, too far, or already brushing it (the capsule slides along)
		}
		const FVector2D Closest = From + Dir * FMath::Min(Along, LookAhead);
		FVector2D Away = Closest - O.Center;
		const float L = Away.Size();
		if (L >= Min)
		{
			continue;
		}
		if (L < 1.0f)
		{
			Away = FVector2D(-Dir.Y, Dir.X); // dead centre: a fixed side (left), stable frame to frame
		}
		else
		{
			Away /= L;
		}
		const float Near = 1.0f - Along / (LookAhead + Min); // closer obstacles bend harder
		Push += Away * (1.0f - L / Min) * (0.5f + Near) * Strength;
	}
	return (Dir + Push).GetSafeNormal();
}