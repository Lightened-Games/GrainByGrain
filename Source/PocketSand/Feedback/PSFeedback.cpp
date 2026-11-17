// Pocket Sand - feedback helpers.

#include "Feedback/PSFeedback.h"
#include "Audio/PSSynth.h"
#include "World/PSArt.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetData.h"
#include "UObject/StrongObjectPtr.h"
#include "Data/PSRoundConfig.h"
#include "Interfaces/PSInterfaces.h"
#include "PocketSand.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"

namespace PSFeedback
{
	static TSet<FName> Missing;
	static TMap<FName, PSSynth::FBuffer> Buffers;

	static const PSSynth::FBuffer* Buffer(FName Cue)
	{
		if (const PSSynth::FBuffer* B = Buffers.Find(Cue))
		{
			return B;
		}
		const FString C = Cue.ToString();
		PSSynth::FBuffer B;
		if (C == TEXT("Pour")) { B = PSSynth::PourLoop(3.0f, 1); }
		else if (C == TEXT("Step")) { B = PSSynth::Step(false, 2); }
		else if (C == TEXT("StepHeavy")) { B = PSSynth::Step(true, 3); }
		else if (C == TEXT("BagLoad")) { B = PSSynth::BagLoad(4); }
		else if (C == TEXT("BagEmpty")) { B = PSSynth::BagEmpty(5); }
		else if (C == TEXT("SurfaceFull")) { B = PSSynth::SurfaceFull(); }
		else if (C == TEXT("Notice")) { B = PSSynth::GuardNotice(6); }
		else if (C == TEXT("Shrug")) { B = PSSynth::GuardShrug(7); }
		else if (C == TEXT("Stick")) { B = PSSynth::StickTap(8); }
		else if (C == TEXT("Strike")) { B = PSSynth::Strike(); }
		else if (C == TEXT("Heartbeat")) { B = PSSynth::Heartbeat(); }
		else if (C == TEXT("Spike")) { B = PSSynth::Spike(9); }
		else if (C == TEXT("Bump")) { B = PSSynth::BallBump(10); }
		else if (C == TEXT("Whistle")) { B = PSSynth::Whistle(1.3f); }
		else if (C == TEXT("Tick")) { B = PSSynth::Bell(false); }
		else if (C == TEXT("Go")) { B = PSSynth::Bell(true); }
		else if (C == TEXT("Victory")) { B = PSSynth::Victory(); }
		else if (C == TEXT("Defeat")) { B = PSSynth::Defeat(); }
		else if (C == TEXT("Key")) { B = PSSynth::KeyClick(); }
		else if (C == TEXT("Chime")) { B = PSSynth::Chime(); }
		else if (C == TEXT("Yard")) { B = PSSynth::YardBed(14.0f, 11); }
		else if (C == TEXT("MusicPlay")) { B = PSSynth::MusicPlay(21); }
		else if (C == TEXT("MusicMenu")) { B = PSSynth::MusicMenu(22); }
		else if (C == TEXT("Tannoy")) { B = PSSynth::Tannoy(23); }
		if (B.Num() == 0)
		{
			if (!Missing.Contains(Cue))
			{
				Missing.Add(Cue);
				UE_LOG(LogPocketSand, Warning, TEXT("[PocketSand] Unknown sound cue %s"), *C);
			}
			return nullptr;
		}
		return &Buffers.Add(Cue, MoveTemp(B));
	}

	void Sound(const UObject* Ctx, FName Cue, float Volume, float Pitch)
	{
		if (const PSSynth::FBuffer* B = Buffer(Cue))
		{
			PSSynth::Play(Ctx, *B, Volume, Pitch);
		}
	}

	UAudioComponent* Loop(const UObject* Ctx, FName Cue, float Volume)
	{
		const PSSynth::FBuffer* B = Buffer(Cue);
		return B ? PSSynth::PlayLoop(Ctx, *B, Volume) : nullptr;
	}

	void Prewarm(const UObject* Ctx)
	{
		static const TCHAR* Cues[] = { TEXT("Pour"), TEXT("Step"), TEXT("StepHeavy"), TEXT("BagLoad"), TEXT("BagEmpty"), TEXT("SurfaceFull"),
			TEXT("Notice"), TEXT("Shrug"), TEXT("Stick"), TEXT("Strike"), TEXT("Heartbeat"), TEXT("Spike"), TEXT("Bump"), TEXT("Whistle"),
			TEXT("Tick"), TEXT("Go"), TEXT("Victory"), TEXT("Defeat"), TEXT("Key"), TEXT("Chime"), TEXT("Yard"), TEXT("Tannoy") };
		for (const TCHAR* C : Cues) { Buffer(FName(C)); }
		// Every project material (a first LoadObject mid-round flushes async loading: a visible hitch).
		for (int32 M = 0; M < static_cast<int32>(EPSMat::Count); ++M) { PSArt::Mat(static_cast<EPSMat>(M)); }
		// Every animation clip (actions such as the bag-load interact or the spike jump load lazily on first play).
		static TArray<TStrongObjectPtr<UObject>> Held;
		TArray<FAssetData> Clips;
		// In -game from the editor the registry is still gathering at BeginPlay: scan this one folder synchronously first.
		IAssetRegistry::GetChecked().ScanPathsSynchronous({ TEXT("/Game/PocketSand/Characters/Rocketbox/Anims") }, false);
		IAssetRegistry::GetChecked().GetAssetsByPath(FName(TEXT("/Game/PocketSand/Characters/Rocketbox/Anims")), Clips, true);
		for (const FAssetData& A : Clips)
		{
			if (UObject* O = A.GetAsset()) { Held.Emplace(O); }
		}
		UE_LOG(LogPocketSand, Log, TEXT("[PocketSand] Prewarm: %d animation clips, %d sound cues."), Clips.Num(), UE_ARRAY_COUNT(Cues));
		static const TCHAR* Fx[] = { TEXT("Dust"), TEXT("Puff"), TEXT("Trickle") };
		for (const TCHAR* N : Fx)
		{
			// Far below the yard and tiny: loads the system and builds its render state before the round.
			Vfx(Ctx, FName(N), FVector(0.0f, 0.0f, -1500.0f), FRotator::ZeroRotator, 0.01f);
		}
	}

	UNiagaraComponent* Vfx(const UObject* Ctx, FName Name, const FVector& Location, const FRotator& Rotation, float Scale)
	{
		const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(Ctx, EGetWorldErrorMode::ReturnNull) : nullptr;
		const IPSMatchInfo* M = World ? Cast<IPSMatchInfo>(World->GetAuthGameMode()) : nullptr;
		const FString Root = M && M->PS_Config() ? M->PS_Config()->VfxRoot : FString(TEXT("/Game/PocketSand/VFX"));
		const FString Asset = TEXT("NS_PS_") + Name.ToString();
		const FString Path = FString::Printf(TEXT("%s/%s.%s"), *Root, *Asset, *Asset);
		UNiagaraSystem* N = FindObject<UNiagaraSystem>(nullptr, *Path);
		if (!N && !Missing.Contains(FName(*Path)))
		{
			N = LoadObject<UNiagaraSystem>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
			if (!N)
			{
				Missing.Add(FName(*Path));
				UE_LOG(LogPocketSand, Warning, TEXT("[PocketSand] Missing VFX %s"), *Path);
			}
		}
		if (!N || !World)
		{
			return nullptr;
		}
		return UNiagaraFunctionLibrary::SpawnSystemAtLocation(Ctx, N, Location, Rotation, FVector(Scale), true, true, ENCPoolMethod::AutoRelease);
	}
}
