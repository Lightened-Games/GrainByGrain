// Pocket Sand - procedural audio (filters and playback adapted from Flash to Bang's FTBSynth).

#include "Audio/PSSynth.h"
#include "PocketSand.h"
#include "AudioDevice.h"
#include "Algo/Reverse.h"
#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Sound/SoundWaveProcedural.h"
#include "TimerManager.h"

namespace
{
	using PSSynth::FBuffer;
	using PSSynth::SampleRate;

	struct FOnePole
	{
		float Y = 0.0f;
		float Process(float X, float CutoffHz)
		{
			const float A = 1.0f - FMath::Exp(-UE_TWO_PI * FMath::Max(CutoffHz, 1.0f) / SampleRate);
			Y += A * (X - Y);
			return Y;
		}
	};

	/** Two-pole resonant band-pass (state variable). */
	struct FBand
	{
		float Low = 0.0f, Bnd = 0.0f;
		float Process(float X, float Hz, float Q)
		{
			const float F = 2.0f * FMath::Sin(UE_PI * FMath::Clamp(Hz, 20.0f, 8000.0f) / SampleRate);
			Low += F * Bnd;
			const float High = X - Low - Bnd / FMath::Max(Q, 0.5f);
			Bnd += F * High;
			return Bnd;
		}
	};

	FBuffer Make(float Seconds) { FBuffer B; B.SetNumZeroed(FMath::Max(1, FMath::RoundToInt(Seconds * SampleRate))); return B; }
	float TimeOf(int32 I) { return static_cast<float>(I) / SampleRate; }

	void Normalize(FBuffer& B, float Peak)
	{
		float Max = 0.0001f;
		for (float V : B) { Max = FMath::Max(Max, FMath::Abs(V)); }
		const float G = Peak / Max;
		for (float& V : B) { V *= G; }
	}

	void LoopCrossfade(FBuffer& B, float Seconds)
	{
		const int32 F = FMath::Min(FMath::RoundToInt(Seconds * SampleRate), B.Num() / 3);
		for (int32 I = 0; I < F; ++I)
		{
			const float A = static_cast<float>(I) / F;
			B[I] = B[I] * A + B[B.Num() - F + I] * (1.0f - A);
		}
		B.SetNum(B.Num() - F);
	}

	/** Short gravel crunch at Start: a cluster of filtered noise grains. */
	void AddCrunch(FBuffer& B, float Start, float Amp, float Cutoff, int32 Grains, FRandomStream& R)
	{
		FOnePole LP;
		for (int32 G = 0; G < Grains; ++G)
		{
			const int32 S = FMath::RoundToInt((Start + R.FRandRange(0.0f, 0.06f)) * SampleRate);
			const float A = Amp * R.FRandRange(0.4f, 1.0f);
			const int32 Len = R.RandRange(250, 900);
			for (int32 I = 0; I < Len && S + I < B.Num(); ++I)
			{
				B[S + I] += LP.Process(R.FRandRange(-1.0f, 1.0f), Cutoff) * A * FMath::Exp(-static_cast<float>(I) / (Len * 0.3f));
			}
		}
	}

	/** Soft reed-like note (odd harmonics through a low-pass) for the comedic stings. */
	void AddReed(FBuffer& B, float Start, float Len, float FreqA, float FreqB, float Amp, float Vibrato)
	{
		const int32 S = FMath::RoundToInt(Start * SampleRate);
		const int32 N = FMath::RoundToInt(Len * SampleRate);
		float Phase = 0.0f;
		FOnePole LP;
		for (int32 I = 0; I < N && S + I < B.Num(); ++I)
		{
			const float T = TimeOf(I);
			const float U = T / FMath::Max(Len, 0.01f);
			const float F = FMath::Lerp(FreqA, FreqB, U) * (1.0f + Vibrato * FMath::Sin(UE_TWO_PI * 5.5f * T) * FMath::Min(1.0f, T / 0.25f));
			Phase += F / SampleRate;
			const float P = FMath::Frac(Phase);
			const float Sq = (P < 0.5f ? 1.0f : -1.0f) * 0.6f + FMath::Sin(UE_TWO_PI * P) * 0.4f;
			const float Env = FMath::Min(1.0f, T / 0.03f) * FMath::Min(1.0f, (Len - T) / 0.08f);
			B[S + I] += LP.Process(Sq, 900.0f + 700.0f * Env) * Env * Amp;
		}
	}
}

FBuffer PSSynth::PourLoop(float Seconds, int32 Seed)
{
	FRandomStream R(Seed);
	FBuffer B = Make(Seconds + 0.4f);
	FOnePole HP, LP;
	for (int32 I = 0; I < B.Num(); ++I)
	{
		const float N = R.FRandRange(-1.0f, 1.0f);
		const float Hi = N - HP.Process(N, 2200.0f);
		B[I] = LP.Process(Hi, 7000.0f) * 0.35f;
	}
	// Grains: thousands of tiny ticks, denser in slow waves (the trickle surges as you walk).
	for (int32 G = 0; G < FMath::RoundToInt(Seconds * 900.0f); ++G)
	{
		const int32 S = R.RandRange(0, B.Num() - 60);
		const float A = R.FRandRange(0.1f, 0.6f) * (0.6f + 0.4f * FMath::Sin(UE_TWO_PI * 1.3f * TimeOf(S)));
		for (int32 I = 0; I < 40; ++I)
		{
			B[S + I] += R.FRandRange(-1.0f, 1.0f) * A * FMath::Exp(-I / 8.0f);
		}
	}
	LoopCrossfade(B, 0.4f);
	Normalize(B, 0.42f);
	return B;
}

FBuffer PSSynth::Step(bool bHeavy, int32 Seed)
{
	FRandomStream R(Seed);
	FBuffer B = Make(0.22f);
	for (int32 I = 0; I < B.Num(); ++I)
	{
		const float T = TimeOf(I);
		B[I] = FMath::Sin(UE_TWO_PI * (bHeavy ? 58.0f : 75.0f) * T) * FMath::Exp(-T / (bHeavy ? 0.04f : 0.025f)) * (bHeavy ? 0.9f : 0.5f);
	}
	AddCrunch(B, 0.0f, bHeavy ? 0.9f : 0.6f, 3200.0f, bHeavy ? 14 : 9, R);
	Normalize(B, bHeavy ? 0.5f : 0.38f);
	return B;
}

FBuffer PSSynth::BagLoad(int32 Seed)
{
	FRandomStream R(Seed);
	FBuffer B = Make(0.55f);
	FOnePole LP, LP2;
	for (int32 I = 0; I < B.Num(); ++I)
	{
		const float T = TimeOf(I);
		// Cloth thump.
		float V = (FMath::Sin(UE_TWO_PI * 90.0f * T) + LP.Process(R.FRandRange(-1, 1), 700.0f) * 2.0f) * FMath::Exp(-T / 0.06f);
		// Zip: rapid ticks rising in pitch.
		if (T > 0.16f && T < 0.42f)
		{
			const float Z = (T - 0.16f) / 0.26f;
			const float Tick = FMath::Frac(T * FMath::Lerp(60.0f, 140.0f, Z)) < 0.2f ? 1.0f : 0.0f;
			V += LP2.Process(R.FRandRange(-1, 1) * Tick, 4500.0f) * 0.6f;
		}
		B[I] = V;
	}
	Normalize(B, 0.5f);
	return B;
}

FBuffer PSSynth::BagEmpty(int32 Seed)
{
	FRandomStream R(Seed);
	FBuffer B = Make(0.45f);
	FOnePole LP;
	for (int32 I = 0; I < B.Num(); ++I)
	{
		const float T = TimeOf(I);
		const float Pff = LP.Process(R.FRandRange(-1, 1), FMath::Lerp(2500.0f, 600.0f, T / 0.3f)) * FMath::Min(1.0f, T / 0.02f) * FMath::Exp(-T / 0.12f);
		const float Tick = T > 0.22f ? FMath::Sin(UE_TWO_PI * 1320.0f * T) * FMath::Exp(-(T - 0.22f) / 0.05f) * 0.35f : 0.0f;
		B[I] = Pff + Tick;
	}
	Normalize(B, 0.45f);
	return B;
}

FBuffer PSSynth::SurfaceFull()
{
	FBuffer B = Make(0.5f);
	for (int32 I = 0; I < B.Num(); ++I)
	{
		const float T = TimeOf(I);
		B[I] = (FMath::Sin(UE_TWO_PI * 110.0f * T) + 0.5f * FMath::Sin(UE_TWO_PI * 165.0f * T)) * FMath::Exp(-T / 0.12f);
	}
	Normalize(B, 0.35f);
	return B;
}

FBuffer PSSynth::GuardNotice(int32 Seed)
{
	FRandomStream R(Seed);
	FBuffer B = Make(0.6f);
	FBand F1, F2;
	float Phase = 0.0f;
	for (int32 I = 0; I < B.Num(); ++I)
	{
		const float T = TimeOf(I);
		const float U = T / 0.55f;
		const float F0 = FMath::Lerp(118.0f, 168.0f, U * U);
		Phase += F0 / SampleRate;
		const float Pulse = FMath::Frac(Phase) < 0.12f ? 1.0f : -0.14f;
		// Nasal "mm" then open "?": formants move up.
		const float V = F1.Process(Pulse, FMath::Lerp(300.0f, 650.0f, U), 6.0f) + 0.6f * F2.Process(Pulse, FMath::Lerp(1100.0f, 1500.0f, U), 8.0f);
		const float Env = FMath::Min(1.0f, T / 0.05f) * FMath::Min(1.0f, (0.6f - T) / 0.12f);
		B[I] = V * Env;
	}
	Normalize(B, 0.45f);
	return B;
}

FBuffer PSSynth::GuardShrug(int32 Seed)
{
	FBuffer B = GuardNotice(Seed);
	Algo::Reverse(B);
	Normalize(B, 0.3f);
	return B;
}

FBuffer PSSynth::StickTap(int32 Seed)
{
	FRandomStream R(Seed);
	FBuffer B = Make(0.5f);
	for (int32 K = 0; K < 2; ++K)
	{
		const int32 S = FMath::RoundToInt((0.02f + K * 0.22f) * SampleRate);
		for (int32 I = 0; I < 5000 && S + I < B.Num(); ++I)
		{
			const float T = TimeOf(I);
			B[S + I] += (FMath::Sin(UE_TWO_PI * 640.0f * T) * 0.6f + FMath::Sin(UE_TWO_PI * 1220.0f * T) * 0.3f) * FMath::Exp(-T / 0.03f);
		}
		AddCrunch(B, 0.02f + K * 0.22f, 0.5f, 2600.0f, 6, R);
	}
	Normalize(B, 0.45f);
	return B;
}

FBuffer PSSynth::Strike()
{
	FBuffer B = Make(1.4f);
	AddReed(B, 0.0f, 0.5f, 196.0f, 196.0f, 0.4f, 0.0f);
	AddReed(B, 0.0f, 0.5f, 233.1f, 233.1f, 0.3f, 0.0f);
	AddReed(B, 0.45f, 0.9f, 185.0f, 175.0f, 0.4f, 0.01f);
	AddReed(B, 0.45f, 0.9f, 220.0f, 207.7f, 0.3f, 0.01f);
	for (int32 I = 0; I < 8000; ++I)
	{
		const float T = TimeOf(I);
		B[I] += FMath::Sin(UE_TWO_PI * 55.0f * T) * FMath::Exp(-T / 0.09f) * 0.8f;
	}
	Normalize(B, 0.6f);
	return B;
}

FBuffer PSSynth::Heartbeat()
{
	FBuffer B = Make(0.7f);
	for (int32 K = 0; K < 2; ++K)
	{
		const int32 S = FMath::RoundToInt(K * 0.2f * SampleRate);
		for (int32 I = 0; I < 9000 && S + I < B.Num(); ++I)
		{
			const float T = TimeOf(I);
			B[S + I] += FMath::Sin(UE_TWO_PI * (K == 0 ? 52.0f : 46.0f) * T) * FMath::Exp(-T / 0.06f) * (K == 0 ? 1.0f : 0.7f);
		}
	}
	Normalize(B, 0.5f);
	return B;
}

FBuffer PSSynth::Spike(int32 Seed)
{
	FRandomStream R(Seed);
	FBuffer B = Make(0.9f);
	FOnePole LP, LP2;
	for (int32 I = 0; I < B.Num(); ++I)
	{
		const float T = TimeOf(I);
		const float Slap = LP.Process(R.FRandRange(-1, 1), 5000.0f) * FMath::Exp(-T / 0.012f) * 1.4f;
		const float Body = FMath::Sin(UE_TWO_PI * 85.0f * T) * FMath::Exp(-T / 0.08f);
		const float Whoomp = T > 0.05f ? LP2.Process(R.FRandRange(-1, 1), 900.0f) * FMath::Exp(-(T - 0.05f) / 0.22f) * 1.6f : 0.0f;
		B[I] = Slap + Body + Whoomp;
	}
	AddCrunch(B, 0.08f, 0.5f, 2000.0f, 18, R);
	Normalize(B, 0.62f);
	return B;
}

FBuffer PSSynth::BallBump(int32 Seed)
{
	FRandomStream R(Seed);
	FBuffer B = Make(0.25f);
	FOnePole LP;
	for (int32 I = 0; I < B.Num(); ++I)
	{
		const float T = TimeOf(I);
		B[I] = (FMath::Sin(UE_TWO_PI * 180.0f * T) + LP.Process(R.FRandRange(-1, 1), 1500.0f)) * FMath::Exp(-T / 0.03f);
	}
	Normalize(B, 0.32f);
	return B;
}

FBuffer PSSynth::Whistle(float Seconds)
{
	FBuffer B = Make(Seconds);
	FRandomStream R(5);
	float P1 = 0.0f, P2 = 0.0f;
	for (int32 I = 0; I < B.Num(); ++I)
	{
		const float T = TimeOf(I);
		// Pea whistle: two close tones with a fast warble.
		const float Warble = 1.0f + 0.03f * FMath::Sin(UE_TWO_PI * 28.0f * T);
		P1 += 2650.0f * Warble / SampleRate;
		P2 += 2710.0f * Warble / SampleRate;
		const float Env = FMath::Min(1.0f, T / 0.03f) * FMath::Min(1.0f, (Seconds - T) / 0.1f);
		B[I] = (FMath::Sin(UE_TWO_PI * P1) + 0.7f * FMath::Sin(UE_TWO_PI * P2) + R.FRandRange(-0.15f, 0.15f)) * Env;
	}
	Normalize(B, 0.4f);
	return B;
}

FBuffer PSSynth::Bell(bool bGo)
{
	FBuffer B = Make(bGo ? 1.4f : 0.5f);
	const float F = bGo ? 880.0f : 660.0f;
	for (int32 I = 0; I < B.Num(); ++I)
	{
		const float T = TimeOf(I);
		const float Ring = FMath::Sin(UE_TWO_PI * F * T) + 0.5f * FMath::Sin(UE_TWO_PI * F * 2.76f * T) * FMath::Exp(-T / 0.15f) + 0.25f * FMath::Sin(UE_TWO_PI * F * 5.4f * T) * FMath::Exp(-T / 0.05f);
		B[I] = Ring * FMath::Exp(-T / (bGo ? 0.45f : 0.16f));
	}
	Normalize(B, bGo ? 0.5f : 0.36f);
	return B;
}

FBuffer PSSynth::Victory()
{
	// A sneaky little bassoon march, then a cheeky final note.
	FBuffer B = Make(2.6f);
	const float Notes[] = { 196.0f, 246.9f, 293.7f, 246.9f, 293.7f, 392.0f };
	for (int32 K = 0; K < 6; ++K)
	{
		AddReed(B, K * 0.22f, 0.16f, Notes[K], Notes[K], 0.5f, 0.0f);
	}
	AddReed(B, 1.4f, 1.0f, 392.0f, 392.0f, 0.5f, 0.012f);
	AddReed(B, 1.4f, 1.0f, 493.9f, 493.9f, 0.3f, 0.012f);
	Normalize(B, 0.58f);
	return B;
}

FBuffer PSSynth::Defeat()
{
	// Sad trombone: three falling notes and a long wobbling slide.
	FBuffer B = Make(2.8f);
	AddReed(B, 0.0f, 0.38f, 233.1f, 233.1f, 0.5f, 0.0f);
	AddReed(B, 0.42f, 0.38f, 220.0f, 220.0f, 0.5f, 0.0f);
	AddReed(B, 0.84f, 0.38f, 207.7f, 207.7f, 0.5f, 0.0f);
	AddReed(B, 1.26f, 1.4f, 196.0f, 174.0f, 0.55f, 0.035f);
	Normalize(B, 0.58f);
	return B;
}

FBuffer PSSynth::KeyClick()
{
	FBuffer B = Make(0.04f);
	FRandomStream R(31);
	for (int32 I = 0; I < B.Num(); ++I)
	{
		const float T = TimeOf(I);
		B[I] = (FMath::Sin(UE_TWO_PI * 2400.0f * T) * 0.5f + R.FRandRange(-0.3f, 0.3f)) * FMath::Exp(-T / 0.004f);
	}
	Normalize(B, 0.18f);
	return B;
}

FBuffer PSSynth::Chime()
{
	FBuffer B = Make(0.6f);
	for (int32 I = 0; I < B.Num(); ++I)
	{
		const float T = TimeOf(I);
		const float A = FMath::Sin(UE_TWO_PI * 988.0f * T) * FMath::Exp(-T / 0.12f);
		const float C = T > 0.11f ? FMath::Sin(UE_TWO_PI * 1319.0f * T) * FMath::Exp(-(T - 0.11f) / 0.18f) : 0.0f;
		B[I] = A + C;
	}
	Normalize(B, 0.22f);
	return B;
}

FBuffer PSSynth::YardBed(float Seconds, int32 Seed)
{
	FRandomStream R(Seed);
	FBuffer B = Make(Seconds + 0.6f);
	FOnePole Wind1, Wind2;
	FBand Vox[4], Vox2[4];
	float Phases[4] = { 0.0f, 0.3f, 0.6f, 0.1f };
	const float Pitch[4] = { 105.0f, 128.0f, 96.0f, 142.0f };
	for (int32 I = 0; I < B.Num(); ++I)
	{
		const float T = TimeOf(I);
		const float Gust = 0.5f + 0.5f * FMath::Sin(UE_TWO_PI * T * 2.0f / Seconds);
		float V = Wind2.Process(Wind1.Process(R.FRandRange(-1, 1), 260.0f + 300.0f * Gust), 500.0f) * 0.5f;
		// Distant voices: buzz through moving formants with syllable envelopes, muffled.
		for (int32 K = 0; K < 4; ++K)
		{
			Phases[K] += Pitch[K] * (1.0f + 0.06f * FMath::Sin(UE_TWO_PI * (0.7f + K * 0.3f) * T)) / SampleRate;
			const float Pulse = FMath::Frac(Phases[K]) < 0.1f ? 1.0f : -0.1f;
			const float Syl = FMath::Max(0.0f, FMath::Sin(UE_TWO_PI * (2.6f + K * 0.7f) * T + K)) * (0.5f + 0.5f * FMath::Sin(UE_TWO_PI * (0.23f + K * 0.11f) * T + K * 2.0f));
			const float F1 = 400.0f + 250.0f * FMath::Sin(UE_TWO_PI * (3.1f + K) * T);
			V += (Vox[K].Process(Pulse, F1, 5.0f) + 0.4f * Vox2[K].Process(Pulse, F1 * 2.6f, 6.0f)) * Syl * 0.25f;
		}
		B[I] = V;
	}
	// A basketball somewhere: a few bounces.
	for (float T0 = 1.0f; T0 < Seconds - 1.0f; T0 += R.FRandRange(2.5f, 4.5f))
	{
		for (int32 K = 0; K < 3; ++K)
		{
			const int32 S = FMath::RoundToInt((T0 + K * 0.45f) * SampleRate);
			for (int32 I = 0; I < 3000 && S + I < B.Num(); ++I)
			{
				const float T = TimeOf(I);
				B[S + I] += FMath::Sin(UE_TWO_PI * 150.0f * T) * FMath::Exp(-T / 0.025f) * 0.25f * (1.0f - K * 0.25f);
			}
		}
	}
	LoopCrossfade(B, 0.6f);
	Normalize(B, 0.3f);
	return B;
}

// ---------------------------------------------------------------------------------------------- music

namespace
{
	/** Equal-tempered pitch, semitones from A4. */
	float Note(int32 Semis) { return 440.0f * FMath::Pow(2.0f, Semis / 12.0f); }

	/** Karplus-Strong plucked string (pizzicato / upright bass). */
	void AddPluck(FBuffer& B, float Start, float Freq, float Amp, float Seconds, float Damp, FRandomStream& R)
	{
		const int32 S = FMath::RoundToInt(Start * SampleRate);
		const int32 Period = FMath::Max(2, FMath::RoundToInt(SampleRate / Freq));
		TArray<float> Line;
		Line.SetNumUninitialized(Period);
		FOnePole Soft;
		for (int32 I = 0; I < Period; ++I) { Line[I] = Soft.Process(R.FRandRange(-1.0f, 1.0f), Freq * 6.0f); }
		const int32 N = FMath::RoundToInt(Seconds * SampleRate);
		int32 Idx = 0;
		for (int32 I = 0; I < N; ++I)
		{
			const int32 W = (S + I) % B.Num(); // wraps: tails fold back to the start, so the loop is seamless
			const int32 Next = (Idx + 1) % Period;
			const float Out = Line[Idx];
			Line[Idx] = (Out + Line[Next]) * 0.5f * Damp;
			Idx = Next;
			B[W] += Out * Amp * FMath::Min(1.0f, (N - I) / (0.02f * SampleRate));
		}
	}

	/** Brushed shaker grain (high-passed noise). */
	void AddShaker(FBuffer& B, float Start, float Amp, FRandomStream& R)
	{
		const int32 S = FMath::RoundToInt(Start * SampleRate);
		FOnePole LP;
		for (int32 I = 0; I < 2600; ++I)
		{
			const float X = R.FRandRange(-1.0f, 1.0f);
			const float T = TimeOf(I);
			B[(S + I) % B.Num()] += (X - LP.Process(X, 3500.0f)) * Amp * FMath::Min(1.0f, T / 0.004f) * FMath::Exp(-T / 0.022f);
		}
	}

	/** Soft felt kick: a short sine with a falling pitch. */
	void AddThump(FBuffer& B, float Start, float Amp)
	{
		const int32 S = FMath::RoundToInt(Start * SampleRate);
		float Phase = 0.0f;
		for (int32 I = 0; I < 9000; ++I)
		{
			const float T = TimeOf(I);
			Phase += (52.0f + 70.0f * FMath::Exp(-T / 0.03f)) / SampleRate;
			B[(S + I) % B.Num()] += FMath::Sin(UE_TWO_PI * Phase) * Amp * FMath::Exp(-T / 0.09f);
		}
	}

	/** D minor caper: Dm Dm Bb A | Dm Gm A Dm (bass root, three chord tones), semitones from A4. */
	struct FChord { int32 Root; int32 Tones[3]; };
	const FChord Caper[8] = {
		{ -31, { -7, -4, 0 } }, { -31, { -7, -4, 0 } }, { -35, { -11, -7, -4 } }, { -36, { -12, -8, -5 } },
		{ -31, { -7, -4, 0 } }, { -38, { -14, -11, -7 } }, { -36, { -12, -8, -5 } }, { -31, { -7, -4, 0 } } };

	void LoopFold(FBuffer& B, int32 Length)
	{
		for (int32 I = Length; I < B.Num(); ++I) { B[I % Length] += B[I]; }
		B.SetNum(Length);
	}
}

FBuffer PSSynth::Tannoy(int32 Seed)
{
	// Yard PA: a crackle, a two-tone chime, a garbled announcement through a tinny horn, a click off.
	FRandomStream R(Seed);
	FBuffer B = Make(3.4f);
	for (int32 I = 0; I < B.Num(); ++I)
	{
		const float T = TimeOf(I);
		const float On = FMath::Min(1.0f, T / 0.05f) * FMath::Clamp((3.25f - T) / 0.05f, 0.0f, 1.0f);
		// Hiss + sparse crackle pops.
		float V = R.FRandRange(-1.0f, 1.0f) * 0.06f * On;
		if (R.FRand() < 0.0009f) { V += R.FRandRange(-1.0f, 1.0f) * 0.8f; }
		// Ding-dong at 0.25 s and 0.6 s.
		for (const TPair<float, float>& Tone : { TPair<float, float>(0.25f, 659.3f), TPair<float, float>(0.6f, 523.3f) })
		{
			if (T > Tone.Key)
			{
				const float U = T - Tone.Key;
				V += (FMath::Sin(UE_TWO_PI * Tone.Value * U) + 0.3f * FMath::Sin(UE_TWO_PI * Tone.Value * 2.0f * U)) * FMath::Exp(-U / 0.35f) * 0.35f;
			}
		}
		B[I] = V;
	}
	// Garbled voice 1.2-3.0 s: buzz through moving formants, syllable envelope.
	FBand F1, F2;
	float Phase = 0.0f;
	for (int32 I = FMath::RoundToInt(1.2f * SampleRate); I < FMath::RoundToInt(3.0f * SampleRate) && I < B.Num(); ++I)
	{
		const float T = TimeOf(I);
		Phase += (118.0f + 14.0f * FMath::Sin(UE_TWO_PI * 1.3f * T)) / SampleRate;
		const float Pulse = FMath::Frac(Phase) < 0.12f ? 1.0f : -0.14f;
		const float Syl = FMath::Max(0.0f, FMath::Sin(UE_TWO_PI * 4.3f * T)) * (0.6f + 0.4f * FMath::Sin(UE_TWO_PI * 0.9f * T));
		const float Fm = 650.0f + 380.0f * FMath::Sin(UE_TWO_PI * 5.7f * T) + 160.0f * FMath::Sin(UE_TWO_PI * 2.3f * T);
		B[I] += (F1.Process(Pulse, Fm, 4.0f) + 0.5f * F2.Process(Pulse, Fm * 2.4f, 5.0f)) * Syl * 0.55f;
	}
	// Horn: band-limit everything and squash it a little.
	FBand Horn;
	for (float& V : B) { V = FMath::Clamp(Horn.Process(V, 1500.0f, 0.8f) * 2.2f, -0.8f, 0.8f); }
	Normalize(B, 0.45f);
	return B;
}

FBuffer PSSynth::MusicPlay(int32 Seed)
{
	// Tiptoe heist caper: walking pizzicato bass, off-beat plucks, brushed shaker, a sneaky bassoon motif. 8 bars, loops.
	FRandomStream R(Seed);
	const float Beat = 60.0f / 112.0f;
	const int32 Len = FMath::RoundToInt(32 * Beat * SampleRate);
	FBuffer B;
	B.SetNumZeroed(Len + SampleRate * 2);
	const int32 Walk[4] = { 0, 7, 12, 10 };
	for (int32 Bar = 0; Bar < 8; ++Bar)
	{
		const FChord& C = Caper[Bar];
		const float T0 = Bar * 4 * Beat;
		for (int32 Q = 0; Q < 4; ++Q)
		{
			// Bass: root, fifth, octave, approach (a walking tiptoe).
			AddPluck(B, T0 + Q * Beat, Note(C.Root + 12 + Walk[Q]) * 0.5f, 0.55f, Beat * 0.9f, 0.996f, R);
			// Off-beat pizzicato chord tones, quiet.
			AddPluck(B, T0 + (Q + 0.5f) * Beat, Note(C.Tones[(Q + Bar) % 3]), 0.22f, Beat * 0.5f, 0.985f, R);
			AddShaker(B, T0 + Q * Beat, 0.05f, R);
			AddShaker(B, T0 + (Q + 0.5f) * Beat, 0.09f, R);
		}
		AddThump(B, T0, 0.35f);
		AddThump(B, T0 + 2 * Beat, 0.25f);
	}
	// Motif in bars 1-2 and 5-6 (answers in 3 and 7 a step lower), staccato reed.
	const int32 Motif[6] = { -7, -4, -5, -7, -9, -7 };
	const float At[6] = { 0.0f, 0.5f, 1.0f, 1.5f, 2.5f, 3.0f };
	for (const int32 Bar : { 0, 2, 4, 6 })
	{
		const int32 Shift = (Bar == 2 || Bar == 6) ? -2 : 0;
		for (int32 K = 0; K < 6; ++K)
		{
			AddReed(B, Bar * 4 * Beat + At[K] * Beat, Beat * 0.35f, Note(Motif[K] + Shift), Note(Motif[K] + Shift), 0.16f, 0.0f);
		}
	}
	LoopFold(B, Len);
	Normalize(B, 0.42f);
	return B;
}

FBuffer PSSynth::MusicMenu(int32 Seed)
{
	// The same tune, slower and lazier: a bass on 1 and 3, a held reed pad, a soft shaker. 8 bars, loops.
	FRandomStream R(Seed);
	const float Beat = 60.0f / 84.0f;
	const int32 Len = FMath::RoundToInt(32 * Beat * SampleRate);
	FBuffer B;
	B.SetNumZeroed(Len + SampleRate * 3);
	for (int32 Bar = 0; Bar < 8; ++Bar)
	{
		const FChord& C = Caper[Bar];
		const float T0 = Bar * 4 * Beat;
		AddPluck(B, T0, Note(C.Root + 12) * 0.5f, 0.5f, Beat * 1.8f, 0.997f, R);
		AddPluck(B, T0 + 2 * Beat, Note(C.Root + 19) * 0.5f, 0.38f, Beat * 1.8f, 0.997f, R);
		AddReed(B, T0, 4 * Beat, Note(C.Tones[1]), Note(C.Tones[1]), 0.05f, 0.006f);
		AddReed(B, T0, 4 * Beat, Note(C.Tones[2]), Note(C.Tones[2]), 0.04f, 0.006f);
		for (int32 Q = 0; Q < 4; ++Q)
		{
			AddShaker(B, T0 + (Q + 0.5f) * Beat, 0.045f, R);
		}
		if (Bar % 2 == 0)
		{
			AddPluck(B, T0 + 1.5f * Beat, Note(C.Tones[0] + 12), 0.18f, Beat, 0.99f, R);
			AddPluck(B, T0 + 3.0f * Beat, Note(C.Tones[2] + 12), 0.14f, Beat, 0.99f, R);
		}
	}
	LoopFold(B, Len);
	Normalize(B, 0.38f);
	return B;
}

// ---------------------------------------------------------------------------------------------- playback

namespace
{
	TArray<int16> ToPCM(const FBuffer& Samples)
	{
		TArray<int16> PCM;
		PCM.SetNumUninitialized(Samples.Num());
		for (int32 I = 0; I < Samples.Num(); ++I)
		{
			PCM[I] = static_cast<int16>(FMath::Clamp(Samples[I], -1.0f, 1.0f) * 32000.0f);
		}
		return PCM;
	}

	UWorld* WorldOf(const UObject* Ctx)
	{
		UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(Ctx, EGetWorldErrorMode::ReturnNull) : nullptr;
		return (World && World->GetAudioDeviceRaw()) ? World : nullptr;
	}
}

UAudioComponent* PSSynth::Play(const UObject* WorldContext, const FBuffer& Samples, float Volume, float Pitch)
{
	UWorld* World = WorldOf(WorldContext);
	if (!World || Samples.Num() == 0)
	{
		return nullptr;
	}
	const TArray<int16> PCM = ToPCM(Samples);
	USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(GetTransientPackage());
	Wave->SetSampleRate(SampleRate);
	Wave->NumChannels = 1;
	Wave->Duration = static_cast<float>(Samples.Num()) / SampleRate;
	Wave->SoundGroup = SOUNDGROUP_Default;
	Wave->bLooping = false;
	Wave->QueueAudio(reinterpret_cast<const uint8*>(PCM.GetData()), PCM.Num() * sizeof(int16));
	UAudioComponent* Comp = UGameplayStatics::SpawnSound2D(World, Wave, Volume, Pitch, 0.0f, nullptr, false, true);
	if (!Comp)
	{
		return nullptr;
	}
	TWeakObjectPtr<UAudioComponent> Weak(Comp);
	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([Weak]() { if (Weak.IsValid()) { Weak->Stop(); } }), Wave->Duration / FMath::Max(Pitch, 0.1f) + 0.1f, false);
	return Comp;
}

UAudioComponent* PSSynth::PlayLoop(const UObject* WorldContext, const FBuffer& Samples, float Volume)
{
	UWorld* World = WorldOf(WorldContext);
	if (!World || Samples.Num() == 0)
	{
		return nullptr;
	}
	TSharedRef<TArray<int16>> PCM = MakeShared<TArray<int16>>(ToPCM(Samples));
	USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(GetTransientPackage());
	Wave->SetSampleRate(SampleRate);
	Wave->NumChannels = 1;
	Wave->Duration = INDEFINITELY_LOOPING_DURATION;
	Wave->SoundGroup = SOUNDGROUP_Default;
	Wave->bLooping = false;
	Wave->QueueAudio(reinterpret_cast<const uint8*>(PCM->GetData()), PCM->Num() * sizeof(int16));
	Wave->OnSoundWaveProceduralUnderflow.BindLambda([PCM](USoundWaveProcedural* InWave, int32 SamplesRequired)
	{
		InWave->QueueAudio(reinterpret_cast<const uint8*>(PCM->GetData()), PCM->Num() * sizeof(int16));
	});
	return UGameplayStatics::SpawnSound2D(World, Wave, Volume, 1.0f, 0.0f, nullptr, false, true);
}

bool PSSynth::SaveWav(const FString& Path, const FBuffer& Samples)
{
	const TArray<int16> PCM = ToPCM(Samples);
	TArray<uint8> Out;
	auto U32 = [&Out](uint32 V) { Out.Append(reinterpret_cast<const uint8*>(&V), 4); };
	auto U16 = [&Out](uint16 V) { Out.Append(reinterpret_cast<const uint8*>(&V), 2); };
	const uint32 DataBytes = PCM.Num() * 2;
	Out.Append(reinterpret_cast<const uint8*>("RIFF"), 4); U32(36 + DataBytes);
	Out.Append(reinterpret_cast<const uint8*>("WAVE"), 4);
	Out.Append(reinterpret_cast<const uint8*>("fmt "), 4); U32(16); U16(1); U16(1); U32(SampleRate); U32(SampleRate * 2); U16(2); U16(16);
	Out.Append(reinterpret_cast<const uint8*>("data"), 4); U32(DataBytes);
	Out.Append(reinterpret_cast<const uint8*>(PCM.GetData()), DataBytes);
	return FFileHelper::SaveArrayToFile(Out, *Path);
}

void PSSynth::ExportAll(const FString& Directory)
{
	const TArray<TPair<FString, FBuffer>> All = {
		{ TEXT("Player/PS_Pour_Loop"), PourLoop(3.0f, 1) },
		{ TEXT("Player/PS_Step_Light"), Step(false, 2) },
		{ TEXT("Player/PS_Step_Heavy"), Step(true, 3) },
		{ TEXT("Player/PS_Bag_Load"), BagLoad(4) },
		{ TEXT("Player/PS_Bag_Empty"), BagEmpty(5) },
		{ TEXT("World/PS_Surface_Full"), SurfaceFull() },
		{ TEXT("Guards/PS_Guard_Notice"), GuardNotice(6) },
		{ TEXT("Guards/PS_Guard_Shrug"), GuardShrug(7) },
		{ TEXT("Guards/PS_Stick_Tap"), StickTap(8) },
		{ TEXT("Guards/PS_Strike"), Strike() },
		{ TEXT("Guards/PS_Heartbeat"), Heartbeat() },
		{ TEXT("World/PS_Spike"), Spike(9) },
		{ TEXT("World/PS_Ball_Bump"), BallBump(10) },
		{ TEXT("UI/PS_Whistle_End"), Whistle(1.3f) },
		{ TEXT("UI/PS_Bell_Tick"), Bell(false) },
		{ TEXT("UI/PS_Bell_Go"), Bell(true) },
		{ TEXT("UI/PS_Victory"), Victory() },
		{ TEXT("UI/PS_Defeat"), Defeat() },
		{ TEXT("UI/PS_Key_Click"), KeyClick() },
		{ TEXT("UI/PS_Prompt_Chime"), Chime() },
		{ TEXT("Ambience/PS_Yard_Loop"), YardBed(14.0f, 11) },
		{ TEXT("Music/PS_Music_Play_Loop"), MusicPlay(21) },
		{ TEXT("Music/PS_Music_Menu_Loop"), MusicMenu(22) },
		{ TEXT("World/PS_Tannoy"), Tannoy(23) },
	};
	for (const TPair<FString, FBuffer>& Pair : All)
	{
		const FString File = Directory / (Pair.Key + TEXT(".wav"));
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(File), true);
		UE_LOG(LogPocketSand, Log, TEXT("[PocketSand] Audio export %s %s"), *File, SaveWav(File, Pair.Value) ? TEXT("ok") : TEXT("FAILED"));
	}
}
