// Pocket Sand - plays the game's synthesised sounds by cue name and its Niagara systems (<VfxRoot>/NS_PS_<Name>).
// Missing assets are logged once and skipped.

#pragma once

#include "CoreMinimal.h"

class UNiagaraComponent;
class UAudioComponent;

namespace PSFeedback
{
	POCKETSAND_API void Sound(const UObject* WorldContext, FName Cue, float Volume = 1.0f, float Pitch = 1.0f);
	POCKETSAND_API UAudioComponent* Loop(const UObject* WorldContext, FName Cue, float Volume = 1.0f);
	/** Synthesises every sound cue and loads (and spawns once, out of sight) every VFX now, so nothing loads or compiles
	 *  on first use mid-round (glitch-log hitches). Call during the start screen. */
	POCKETSAND_API void Prewarm(const UObject* WorldContext);
	POCKETSAND_API UNiagaraComponent* Vfx(const UObject* WorldContext, FName Name, const FVector& Location, const FRotator& Rotation = FRotator::ZeroRotator, float Scale = 1.0f);
}
