// Pocket Sand - crowd director.

#include "AI/PSCrowdDirectorComponent.h"
#include "AI/PSInmate.h"
#include "AI/PSNav.h"
#include "Data/PSRoundConfig.h"
#include "Interfaces/PSInterfaces.h"
#include "World/PSSandGrid.h"
#include "PocketSand.h"
#include "Engine/World.h"

UPSCrowdDirectorComponent::UPSCrowdDirectorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	AgentClass = APSInmate::StaticClass();
}

void UPSCrowdDirectorComponent::Init(const UPSRoundConfig* Config, APSSandGrid* InGrid)
{
	Cfg = Config;
	Grid = InGrid;
	if (!Cfg || !GetWorld() || !AgentClass)
	{
		return;
	}
	FRandomStream Look(1337);
	const int32 Total = FMath::Max(0, Cfg->GroupCount) * FMath::Max(0, Cfg->GroupSize);
	for (int32 I = AgentStates.Num(); I < Total; ++I)
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* A = GetWorld()->SpawnActor<AActor>(AgentClass, FTransform(FVector(0.0f, 0.0f, -5000.0f)), P);
		if (IPSCrowdAgent* Agent = Cast<IPSCrowdAgent>(A))
		{
			// Yard denim in a few washes, varied skin and hair.
			FPSOutfit O;
			const float Wash = Look.FRandRange(0.75f, 1.2f);
			O.Top = FLinearColor(0.16f * Wash, 0.22f * Wash, 0.33f * Wash);
			O.Bottom = FLinearColor(0.09f, 0.11f, 0.16f) * Look.FRandRange(0.8f, 1.15f);
			O.Shoes = FLinearColor(0.035f, 0.032f, 0.03f);
			O.SkinTone = Look.FRand();
			O.Hair = Look.RandRange(0, 2);
			O.HairTone = Look.FRand() * 0.6f;
			O.SleeveEnd = Look.FRandRange(44.0f, 52.0f);
			if (Cfg->InmateAvatars.Num() > 0)
			{
				O.Avatar = Cfg->InmateAvatars[I % Cfg->InmateAvatars.Num()];
			}
			Agent->PS_Dress(Cfg, O);
		}
		FPSCrowdAgentState S;
		S.Actor = A;
		AgentStates.Add(S);
	}
}

void UPSCrowdDirectorComponent::PickNextPoi(FPSCrowdGroup& G, int32 Index)
{
	const int32 N = Cfg ? Cfg->PointsOfInterest.Num() : 0;
	if (N == 0)
	{
		G.Target = G.Pos;
		return;
	}
	int32 Next = Rng.RandRange(0, N - 1);
	for (int32 Tries = 0; Tries < 8; ++Tries)
	{
		// Never the same spot twice, and never the spot another group is already heading to.
		bool bTaken = Next == G.Poi;
		for (int32 K = 0; K < GroupStates.Num(); ++K)
		{
			bTaken |= K != Index && GroupStates[K].Poi == Next;
		}
		if (!bTaken)
		{
			break;
		}
		Next = (Next + 1 + Rng.RandRange(0, N - 2)) % N;
	}
	G.Poi = Next;
	G.Target = Cfg->PointsOfInterest[Next] + FVector2D(Rng.FRandRange(-120.0f, 120.0f), Rng.FRandRange(-120.0f, 120.0f));
	if (bRandomSpots)
	{
		// Anywhere in the yard (user 2026-11-09: "all in middle"), kept apart from where the other groups are heading.
		const FVector2D S = Cfg->YardSize;
		// Straight-line steering can't go around the court, beds, puddles or big props: only pick spots reachable in a
		// straight line (user 2026-11-14: inmates "stuck around the volleyball court trying to walk through").
		const auto PathClear = [this, &G](const FVector2D& To)
		{
			const FVector2D From = G.Pos;
			const float Len = FVector2D::Distance(From, To);
			const float Pad = Cfg->GroupRadius * 0.6f + RegionMargin;
			for (float U = 0.0f; U <= Len; U += 50.0f)
			{
				const FVector2D P = From + (To - From) * (Len > 1.0f ? U / Len : 0.0f);
				for (const FPSRegionDef& R : Cfg->Regions)
				{
					if (R.Contains(P, Pad)) { return false; }
				}
				const TArray<FPSObstacle>* Lists[] = { &Cfg->Obstacles, &Cfg->RuntimeObstacles };
				for (const TArray<FPSObstacle>* L : Lists)
				{
					for (const FPSObstacle& O : *L)
					{
						if (FVector2D::Distance(P, O.Center) < O.Radius + Cfg->GroupRadius * 0.6f) { return false; }
					}
				}
			}
			return true;
		};
		bool bFound = false;
		for (int32 Tries = 0; Tries < 40; ++Tries)
		{
			const FVector2D Cand(Rng.FRandRange(SpotMargin, S.X - SpotMargin), Rng.FRandRange(SpotMargin, S.Y - SpotMargin));
			bool bClear = true;
			for (int32 K = 0; K < GroupStates.Num(); ++K)
			{
				bClear &= K == Index || FVector2D::Distance(GroupStates[K].Target, Cand) > SpotSeparation;
			}
			bClear &= !bMenuClear || FVector2D::Distance(HeroPos, Cand) > MenuHeroClearRadius * 1.5f;
			const FVector2D T = PushOut(Cand, Cfg->GroupRadius);
			if (bClear && PathClear(T))
			{
				G.Target = T;
				bFound = true;
				break;
			}
		}
		if (!bFound)
		{
			G.Target = G.Pos; // nowhere reachable in a straight line this time: stay and chat, try again next dwell
		}
	}
	G.bMoving = true;
}

void UPSCrowdDirectorComponent::ResetCrowd(int32 Seed)
{
	Rng.Initialize(Seed);
	if (!Cfg)
	{
		return;
	}
	GroupStates.Reset();
	const int32 NG = FMath::Max(0, Cfg->GroupCount);
	const int32 NP = Cfg->PointsOfInterest.Num();
	for (int32 G = 0; G < NG; ++G)
	{
		FPSCrowdGroup S;
		S.Poi = NP > 0 ? (G * 2) % NP : 0;
		S.Pos = NP > 0 ? Cfg->PointsOfInterest[S.Poi] : FVector2D(Cfg->YardSize * 0.5f);
		if (bRandomSpots)
		{
			// Start spread over the yard: one column band per group, random depth.
			const FVector2D Y = Cfg->YardSize;
			const float Band = (Y.X - 2.0f * SpotMargin) / FMath::Max(NG, 1);
			for (int32 Try = 0; Try < 10; ++Try)
			{
				S.Pos = PushOut(FVector2D(SpotMargin + Band * (G + Rng.FRandRange(0.2f, 0.8f)), Rng.FRandRange(SpotMargin, Y.Y - SpotMargin)), Cfg->GroupRadius);
				if (FVector2D::Distance(S.Pos, Cfg->PlayerStart) > MenuHeroClearRadius * 1.5f) { break; } // not in the hero shot
			}
		}
		S.Target = S.Pos;
		S.DwellLeft = Rng.FRandRange(0.5f, 4.0f);
		S.bMoving = false;
		S.Size = Cfg->GroupSize;
		GroupStates.Add(S);
	}
	for (int32 I = 0; I < AgentStates.Num(); ++I)
	{
		FPSCrowdAgentState& A = AgentStates[I];
		A.Group = NG > 0 ? I % NG : 0;
		const float Ang = Rng.FRandRange(0.0f, UE_TWO_PI);
		const float Rad = Cfg->GroupRadius * FMath::Sqrt(Rng.FRandRange(0.15f, 1.0f));
		A.Slot = FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * Rad;
		A.Pos = PushOut(GroupStates.IsValidIndex(A.Group) ? GroupStates[A.Group].Pos + A.Slot : A.Slot, Cfg->AgentRadius);
		A.Vel = FVector2D::ZeroVector;
		A.Yaw = Rng.FRandRange(0.0f, 360.0f);
		A.SpeedMul = Rng.FRandRange(AgentSpeedRange.X, AgentSpeedRange.Y);
		A.FollowRate = Rng.FRandRange(FollowRateRange.X, FollowRateRange.Y);
		A.ChatYawOffset = Rng.FRandRange(-ChatYawSpread, ChatYawSpread);
		A.GestureClock = Rng.FRandRange(GestureInterval.X * 0.3f, GestureInterval.Y);
		A.Follow = 0.0f;
		if (IPSCrowdAgent* Agent = Cast<IPSCrowdAgent>(A.Actor.Get()))
		{
			Agent->PS_Cheer(false);
			Agent->PS_SetIdleStyle(Rng.FRand() < TalkFraction ? 1 : Rng.RandRange(0, 1) * 2);
			Agent->PS_SetPose(FVector(A.Pos, 0.0f), A.Yaw, 0.0f);
		}
	}
}

FVector2D UPSCrowdDirectorComponent::PushOut(const FVector2D& P, float Radius) const
{
	return PSNav::PushOut(Cfg, P, Radius, true, RegionMargin);
}

FVector2D UPSCrowdDirectorComponent::Avoid(const FVector2D& From, const FVector2D& Dir, float Radius) const
{
	return PSNav::Avoid(Cfg, From, Dir, Radius, true, 220.0f, RegionMargin);
}

void UPSCrowdDirectorComponent::Step(float Dt, float SpeedMul, const TArray<FVector2D>& Others)
{
	if (!Cfg)
	{
		return;
	}
	const float Speed = Cfg->CrowdSpeed * SpeedMul;
	for (int32 GI = 0; GI < GroupStates.Num(); ++GI)
	{
		FPSCrowdGroup& G = GroupStates[GI];
		if (!G.bMoving)
		{
			G.Vel = FVector2D::ZeroVector;
			G.CurSpeed = 0.0f;
			G.DwellLeft -= Dt;
			if (G.DwellLeft <= 0.0f)
			{
				PickNextPoi(G, GI);
			}
			continue;
		}
		const FVector2D To = G.Target - G.Pos;
		const float L = To.Size();
		if (L < 40.0f)
		{
			G.bMoving = false;
			G.DwellLeft = Rng.FRandRange(Cfg->DwellMin, Cfg->DwellMax);
			continue;
		}
		const FVector2D Dir = Avoid(G.Pos, To / L, Cfg->GroupRadius * 0.6f);
		// Ease into the walk and slow down over the last couple of metres (no instant start/stop).
		const float WantSpeed = Speed * FMath::Clamp(L / FMath::Max(GroupSlowRadius, 1.0f), 0.35f, 1.0f);
		G.CurSpeed = FMath::FInterpConstantTo(G.CurSpeed, WantSpeed, Dt, Speed / FMath::Max(GroupAccelSeconds, 0.05f));
		G.Vel = Dir * G.CurSpeed;
		G.Pos = PushOut(G.Pos + G.Vel * Dt, Cfg->GroupRadius * 0.4f);
	}

	const float AR = Cfg->AgentRadius;
	for (int32 I = 0; I < AgentStates.Num(); ++I)
	{
		FPSCrowdAgentState& A = AgentStates[I];
		const FPSCrowdGroup& G = GroupStates.IsValidIndex(A.Group) ? GroupStates[A.Group] : GroupStates[0];
		const FVector2D Want = G.Pos + A.Slot;
		// Each person picks up the group's walk at their own pace (staggered starts) and walks at their own speed.
		A.Follow = FMath::FInterpConstantTo(A.Follow, G.bMoving ? 1.0f : 0.0f, Dt, A.FollowRate);
		FVector2D V = (Want - A.Pos) * SlotSteer * (0.35f + 0.65f * A.Follow) + G.Vel * A.Follow * A.SpeedMul;
		// Separation from other inmates.
		for (int32 J = 0; J < AgentStates.Num(); ++J)
		{
			if (J == I)
			{
				continue;
			}
			const FVector2D D = A.Pos - AgentStates[J].Pos;
			const float L2 = D.SizeSquared();
			const float Min = AR * 2.2f;
			if (L2 < Min * Min && L2 > 1.0f)
			{
				const float L = FMath::Sqrt(L2);
				V += D / L * (Min - L) * SeparationStrength * 4.0f;
			}
		}
		// Give way to the hero and the guards.
		for (int32 OI = 0; OI < Others.Num(); ++OI)
		{
			// Others[0] is the hero; on the start screen the crowd keeps well clear of him (no one crosses the hero shot).
			const float R = (OI == 0 && bMenuClear) ? MenuHeroClearRadius : GiveWayRadius;
			const FVector2D D = A.Pos - Others[OI];
			const float L = D.Size();
			if (L < R && L > 1.0f)
			{
				V += D / L * (R - L) * SeparationStrength * 5.0f;
			}
		}
		const float MaxV = FMath::Max(Speed, 120.0f) * MaxSpeedMul * A.SpeedMul;
		if (V.SizeSquared() > MaxV * MaxV)
		{
			V = V.GetSafeNormal() * MaxV;
		}
		// People walk where they face (C3): turn toward the wanted direction at a human rate and step along the facing
		// only. No sideways crab steps; below the walk threshold they stand still, so idle feet never slide.
		A.Desired = FMath::Lerp(A.Desired, V, FMath::Clamp(Dt * IntentSmoothing, 0.0f, 1.0f));
		FVector2D Wanted = FVector2D::ZeroVector;
		if (A.Desired.Size() > WalkThreshold)
		{
			const float WantYaw = FMath::RadiansToDegrees(FMath::Atan2(A.Desired.Y, A.Desired.X));
			const float Left = FMath::Abs(FMath::FindDeltaAngleDegrees(A.Yaw, WantYaw));
			A.Yaw = FMath::FixedTurn(A.Yaw, WantYaw, FMath::Clamp(Left * TurnEase, MinTurnRate, AgentTurnRate) * Dt);
			const FVector2D Facing(FMath::Cos(FMath::DegreesToRadians(A.Yaw)), FMath::Sin(FMath::DegreesToRadians(A.Yaw)));
			Wanted = Facing * FMath::Max(0.0f, FVector2D::DotProduct(A.Desired, Facing));
		}
		A.Vel = FMath::Lerp(A.Vel, Wanted, FMath::Clamp(Dt * 6.0f, 0.0f, 1.0f));
		A.Pos = PushOut(A.Pos + A.Vel * Dt, AR);
		// Hard spacing (user 2026-11-14: "no collision or clipping btw characters"): never closer than two body radii to
		// another inmate, or to the hero / a guard, whatever the soft steering wanted.
		{
			const float MinAgent = Cfg->AgentRadius * 2.0f + HardGap;
			for (const FPSCrowdAgentState& B : AgentStates)
			{
				if (&B == &A) { continue; }
				const FVector2D D = A.Pos - B.Pos;
				const float L = D.Size();
				if (L < MinAgent)
				{
					A.Pos += (L > 1.0f ? D / L : FVector2D(1.0f, 0.0f)) * (MinAgent - L);
				}
			}
			const float MinOther = Cfg->AgentRadius + OtherBodyRadius + HardGap;
			for (const FVector2D& O : Others)
			{
				const FVector2D D = A.Pos - O;
				const float L = D.Size();
				if (L < MinOther && L > 1.0f)
				{
					A.Pos += D / L * (MinOther - L);
				}
			}
			A.Pos = PushOut(A.Pos, AR);
		}
		const float Sp = A.Vel.Size();
		if (Sp <= WalkThreshold && !G.bMoving)
		{
			// Chatting: face into the group.
			const FVector2D In = G.Pos - A.Pos;
			if (In.SizeSquared() > 400.0f)
			{
				const float Face = FMath::RadiansToDegrees(FMath::Atan2(In.Y, In.X)) + A.ChatYawOffset;
				if (FMath::Abs(FMath::FindDeltaAngleDegrees(A.Yaw, Face)) > ChatTurnThreshold)
				{
					A.Yaw = FMath::FixedTurn(A.Yaw, Face, ChatTurnRate * Dt);
				}
			}
		}
		// Notice the hero walking by: turn to watch him a moment, with a small gesture.
		A.NoticeCool = FMath::Max(0.0f, A.NoticeCool - Dt);
		if (Sp <= WalkThreshold && !G.bMoving && !bMenuClear && Others.Num() > 0)
		{
			const FVector2D ToHero = Others[0] - A.Pos;
			if (A.NoticeLeft <= 0.0f && A.NoticeCool <= 0.0f && ToHero.Size() < NoticeRadius)
			{
				A.NoticeLeft = NoticeSeconds;
				A.NoticeCool = NoticeCooldown;
				A.GestureClock = FMath::Max(A.GestureClock, NoticeSeconds + 1.0f);
				if (IPSCrowdAgent* Agent = Cast<IPSCrowdAgent>(A.Actor.Get()); Agent && NoticeClips.Num() > 0)
				{
					Agent->PS_PlayAction(NoticeClips[Rng.RandRange(0, NoticeClips.Num() - 1)], false, 1.0f);
				}
			}
			if (A.NoticeLeft > 0.0f)
			{
				A.NoticeLeft -= Dt;
				const float Face = FMath::RadiansToDegrees(FMath::Atan2(ToHero.Y, ToHero.X));
				A.Yaw = FMath::FixedTurn(A.Yaw, Face, NoticeTurnRate * Dt);
			}
		}
		else
		{
			A.NoticeLeft = 0.0f;
		}
		if (Grid)
		{
			Grid->Field().Trample(A.Pos, Cfg->TrampleRadius, Dt, Sp > 25.0f ? 1.0f : Cfg->StandingTrampleMul);
		}
		// Small gestures while standing in a group (look around, scratch head, shrug): no frozen statues.
		if (Sp <= WalkThreshold && !G.bMoving && GestureClips.Num() > 0)
		{
			A.GestureClock -= Dt;
			if (A.GestureClock <= 0.0f)
			{
				A.GestureClock = Rng.FRandRange(GestureInterval.X, GestureInterval.Y);
				if (IPSCrowdAgent* Agent = Cast<IPSCrowdAgent>(A.Actor.Get()))
				{
					Agent->PS_PlayAction(GestureClips[Rng.RandRange(0, GestureClips.Num() - 1)], false, 1.0f);
				}
			}
		}
		if (IPSCrowdAgent* Agent = Cast<IPSCrowdAgent>(A.Actor.Get()))
		{
			Agent->PS_SetPose(FVector(A.Pos, 0.0f), A.Yaw, Sp < WalkThreshold ? 0.0f : Sp);
		}
	}
}

void UPSCrowdDirectorComponent::SetCheer(bool bOn)
{
	for (FPSCrowdAgentState& A : AgentStates)
	{
		if (IPSCrowdAgent* Agent = Cast<IPSCrowdAgent>(A.Actor.Get()))
		{
			Agent->PS_Cheer(bOn);
		}
	}
}

int32 UPSCrowdDirectorComponent::CountNear(const FVector& Location, float Radius) const
{
	int32 N = 0;
	const FVector2D P(Location);
	const float R2 = Radius * Radius;
	for (const FPSCrowdAgentState& A : AgentStates)
	{
		N += FVector2D::DistSquared(A.Pos, P) <= R2 ? 1 : 0;
	}
	return N;
}

FVector2D UPSCrowdDirectorComponent::PredictGroup(int32 Group, float Seconds) const
{
	if (!GroupStates.IsValidIndex(Group))
	{
		return FVector2D::ZeroVector;
	}
	const FPSCrowdGroup& G = GroupStates[Group];
	if (!G.bMoving)
	{
		return G.Pos;
	}
	const FVector2D To = G.Target - G.Pos;
	const float L = To.Size();
	const float Travel = FMath::Min(L, G.Vel.Size() * Seconds);
	return L > 1.0f ? G.Pos + To / L * Travel : G.Pos;
}
