// Pocket Sand - procedural audio. Every sound is synthesised here at runtime; -PSExportAudio writes the same buffers as
// WAVs into asset-library/Audio/Generated/PocketSand (original work, identical to what plays).

#pragma once

#include "CoreMinimal.h"

class UAudioComponent;

namespace PSSynth
{
	constexpr int32 SampleRate = 44100;
	using FBuffer = TArray<float>;

	/** Sand trickling out of a trouser hem (loopable). */
	POCKETSAND_API FBuffer PourLoop(float Seconds, int32 Seed);
	/** One gravel footstep; heavy = waddling with two full bags. */
	POCKETSAND_API FBuffer Step(bool bHeavy, int32 Seed);
	POCKETSAND_API FBuffer BagLoad(int32 Seed);
	POCKETSAND_API FBuffer BagEmpty(int32 Seed);
	POCKETSAND_API FBuffer SurfaceFull();
	/** The guard's "Hm?": a rising nasal hum. */
	POCKETSAND_API FBuffer GuardNotice(int32 Seed);
	POCKETSAND_API FBuffer GuardShrug(int32 Seed);
	POCKETSAND_API FBuffer StickTap(int32 Seed);
	POCKETSAND_API FBuffer Strike();
	POCKETSAND_API FBuffer Heartbeat();
	POCKETSAND_API FBuffer Spike(int32 Seed);
	POCKETSAND_API FBuffer BallBump(int32 Seed);
	POCKETSAND_API FBuffer Whistle(float Seconds);
	POCKETSAND_API FBuffer Bell(bool bGo);
	POCKETSAND_API FBuffer Victory();
	POCKETSAND_API FBuffer Defeat();
	POCKETSAND_API FBuffer KeyClick();
	POCKETSAND_API FBuffer Chime();
	/** Yard ambience: distant chatter, a ball bouncing, wind (loopable). */
	POCKETSAND_API FBuffer YardBed(float Seconds, int32 Seed);
	/** Music (loopable): the tiptoe caper while playing, and the same tune slower for the start screen. */
	POCKETSAND_API FBuffer MusicPlay(int32 Seed);
	POCKETSAND_API FBuffer MusicMenu(int32 Seed);
	/** Yard PA: crackle, ding-dong, a garbled announcement. */
	POCKETSAND_API FBuffer Tannoy(int32 Seed);

	POCKETSAND_API UAudioComponent* Play(const UObject* WorldContext, const FBuffer& Samples, float Volume = 1.0f, float Pitch = 1.0f);
	POCKETSAND_API UAudioComponent* PlayLoop(const UObject* WorldContext, const FBuffer& Samples, float Volume = 1.0f);
	POCKETSAND_API bool SaveWav(const FString& Path, const FBuffer& Samples);
	/** Writes every preset to Directory. */
	POCKETSAND_API void ExportAll(const FString& Directory);
}
