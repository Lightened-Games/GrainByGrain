// Pocket Sand - flat-yard steering shared by the crowd, the guards and the bot: push out of round obstacles and
// hiding surfaces, slide along their edges, stay inside the walls. The yard is open, so no navmesh is needed.

#pragma once

#include "CoreMinimal.h"

class UPSRoundConfig;

namespace PSNav
{
	/** bRegions: also keep out of beds, court, pit and puddles (inmates and guards do; the hero walks on them). */
	POCKETSAND_API FVector2D PushOut(const UPSRoundConfig* Cfg, const FVector2D& P, float Radius, bool bRegions, float RegionMargin = 45.0f);
	/** Steer Dir around whatever lies within LookAhead in front of From. */
	POCKETSAND_API FVector2D Avoid(const UPSRoundConfig* Cfg, const FVector2D& From, const FVector2D& Dir, float Radius, bool bRegions, float LookAhead = 220.0f, float RegionMargin = 45.0f);
	POCKETSAND_API FVector2D AvoidSmooth(const UPSRoundConfig* Cfg, const FVector2D& From, const FVector2D& Dir, float Radius, float LookAhead, float Strength = 1.4f);
	/** Clamp inside the yard walls. */
	POCKETSAND_API FVector2D ClampYard(const UPSRoundConfig* Cfg, const FVector2D& P, float Edge = 140.0f);
}
