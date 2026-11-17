// Pocket Sand - editor helpers for the content script (P24).

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PSEditorTools.generated.h"

class UNiagaraSystem;
class UMaterialInterface;

/** Editor-only helpers for the content scripts (P24): Python can't reach Niagara renderers or build brushes. */
UCLASS()
class POCKETSAND_API UPSEditorTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Point every renderer of the system at Material. bSpritesOnly disables non-sprite renderers. Returns renderers changed. */
	UFUNCTION(BlueprintCallable, Category = "PocketSand|Editor")
	static int32 SetNiagaraRendererMaterial(UNiagaraSystem* System, UMaterialInterface* Material, bool bSpritesOnly);

	/** Spawn a BlockingVolume with a real cube brush. Editor only. */
	UFUNCTION(BlueprintCallable, Category = "PocketSand|Editor", meta = (WorldContext = "WorldContextObject"))
	static AActor* SpawnBlockingVolume(UObject* WorldContextObject, FVector Location, FVector HalfExtent, FString Label);

	/** Bone names of a skeletal mesh (to pick attachment sockets). */
	UFUNCTION(BlueprintCallable, Category = "PocketSand|Editor")
	static TArray<FString> ListBones(class USkeletalMesh* Mesh);

	/** Create (or replace) a runtime composite UFont at PackagePath with "Regular" and "Bold" typefaces (FactoryKit
	 *  FontObject; Python can't author composite fonts). Returns true when the package was saved. */
	UFUNCTION(BlueprintCallable, Category = "PocketSand|Editor")
	static bool CreateCompositeFont(const FString& PackagePath, class UFontFace* Regular, class UFontFace* Bold);
};
