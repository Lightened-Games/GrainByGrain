// Pocket Sand - editor helpers for the content script (P24).

#include "Editor/PSEditorTools.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraRibbonRendererProperties.h"
#include "Materials/MaterialInterface.h"
#include "Engine/BlockingVolume.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "ReferenceSkeleton.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "AssetRegistry/AssetRegistryModule.h"
#if WITH_EDITOR
#include "Builders/CubeBuilder.h"
#include "ActorFactories/ActorFactory.h"
#endif

AActor* UPSEditorTools::SpawnBlockingVolume(UObject* WorldContextObject, FVector Location, FVector HalfExtent, FString Label)
{
#if WITH_EDITOR
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABlockingVolume* Volume = World->SpawnActor<ABlockingVolume>(ABlockingVolume::StaticClass(), FTransform(Location), Params);
	if (!Volume)
	{
		return nullptr;
	}
	UCubeBuilder* Builder = NewObject<UCubeBuilder>();
	Builder->X = HalfExtent.X * 2.0f;
	Builder->Y = HalfExtent.Y * 2.0f;
	Builder->Z = HalfExtent.Z * 2.0f;
	UActorFactory::CreateBrushForVolumeActor(Volume, Builder);
	Volume->SetActorLabel(Label);
	return Volume;
#else
	return nullptr;
#endif
}

TArray<FString> UPSEditorTools::ListBones(USkeletalMesh* Mesh)
{
	TArray<FString> Out;
	if (Mesh)
	{
		const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
		for (int32 I = 0; I < Ref.GetNum(); ++I)
		{
			Out.Add(Ref.GetBoneName(I).ToString());
		}
	}
	return Out;
}

int32 UPSEditorTools::SetNiagaraRendererMaterial(UNiagaraSystem* System, UMaterialInterface* Material, bool bSpritesOnly)
{
	int32 Changed = 0;
#if WITH_EDITOR
	if (!System || !Material)
	{
		return 0;
	}
	System->Modify();
	for (FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		FVersionedNiagaraEmitterData* Data = Handle.GetEmitterData();
		if (!Data)
		{
			continue;
		}
		for (UNiagaraRendererProperties* Renderer : Data->GetRenderers())
		{
			if (!Renderer)
			{
				continue;
			}
			Renderer->Modify();
			if (bSpritesOnly && !Renderer->IsA<UNiagaraSpriteRendererProperties>())
			{
				Renderer->SetIsEnabled(false);
				continue;
			}
			if (UNiagaraSpriteRendererProperties* Sprite = Cast<UNiagaraSpriteRendererProperties>(Renderer))
			{
				Sprite->Material = Material;
			}
			else if (UNiagaraRibbonRendererProperties* Ribbon = Cast<UNiagaraRibbonRendererProperties>(Renderer))
			{
				Ribbon->Material = Material;
			}
			else if (UNiagaraMeshRendererProperties* MeshRenderer = Cast<UNiagaraMeshRendererProperties>(Renderer))
			{
				MeshRenderer->bOverrideMaterials = true;
				MeshRenderer->OverrideMaterials.SetNum(FMath::Max(1, MeshRenderer->OverrideMaterials.Num()));
				for (FNiagaraMeshMaterialOverride& Override : MeshRenderer->OverrideMaterials)
				{
					Override.ExplicitMat = Material;
				}
			}
			else
			{
				continue;
			}
			FPropertyChangedEvent Event(nullptr);
			Renderer->PostEditChangeProperty(Event);
			++Changed;
		}
	}
	System->MarkPackageDirty();
#endif
	return Changed;
}

bool UPSEditorTools::CreateCompositeFont(const FString& PackagePath, UFontFace* Regular, UFontFace* Bold)
{
#if WITH_EDITOR
	if (!Regular || !Bold)
	{
		return false;
	}
	UPackage* Package = CreatePackage(*PackagePath);
	const FString Name = FPackageName::GetShortName(PackagePath);
	UFont* Font = FindObject<UFont>(Package, *Name);
	if (!Font)
	{
		Font = NewObject<UFont>(Package, *Name, RF_Public | RF_Standalone);
		FAssetRegistryModule::AssetCreated(Font);
	}
	Font->FontCacheType = EFontCacheType::Runtime;
	FCompositeFont& C = Font->GetMutableInternalCompositeFont();
	C.DefaultTypeface.Fonts.Reset();
	C.DefaultTypeface.Fonts.Add_GetRef(FTypefaceEntry(TEXT("Regular"))).Font = FFontData(Regular);
	C.DefaultTypeface.Fonts.Add_GetRef(FTypefaceEntry(TEXT("Bold"))).Font = FFontData(Bold);
	Font->MarkPackageDirty();
	FSavePackageArgs Args;
	Args.TopLevelFlags = RF_Public | RF_Standalone;
	const FString File = FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension());
	return UPackage::SavePackage(Package, Font, *File, Args);
#else
	return false;
#endif
}