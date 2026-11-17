// Pocket Sand - every HUD style value (design guide 8.7.4). Military/tactical theme row: Barlow Condensed (OFL),
// coral accent (#FF6F59, UI chrome only; the sand itself stays yellow in the world), signal-red danger.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PSHUDStyle.generated.h"

class UFontFace;

UCLASS(BlueprintType)
class POCKETSAND_API UPSHUDStyle : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Type") TSoftObjectPtr<UFontFace> RegularFace = TSoftObjectPtr<UFontFace>(FSoftObjectPath(TEXT("/Game/PocketSand/UI/Fonts/FF_PS_Regular.FF_PS_Regular")));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Type") TSoftObjectPtr<UFontFace> BoldFace = TSoftObjectPtr<UFontFace>(FSoftObjectPath(TEXT("/Game/PocketSand/UI/Fonts/FF_PS_Bold.FF_PS_Bold")));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Type") int32 LabelSize = 20; // reviewer 2026-11-12 "UI can look a bit better": bigger type (was 14)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Type") int32 BodySize = 28; // was 20
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Type") int32 HeadlineSize = 64; // was 52
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Type") int32 LabelTracking = 320;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Type") int32 HeadlineTracking = 240;

	/** Off-white #EDEAE3 at 88 %. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colour") FLinearColor Text = FLinearColor(0.85f, 0.82f, 0.77f, 0.88f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colour") float SecondaryOpacity = 0.72f;
	/** Sand yellow #F2C230: the sand, the bags, the objective, fill arcs, lit legend rows. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colour") FLinearColor Accent = FLinearColor(1.0f, 0.159f, 0.100f, 1.0f); // coral #FF6F59 (UPGRADE_PLAN B1; sand stays yellow in the world)
	/** Signal red #E0483C: guards, strikes, suspicion, anything a guard can see. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colour") FLinearColor Danger = FLinearColor(0.75f, 0.065f, 0.045f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colour") FLinearColor Scrim = FLinearColor(0.004f, 0.005f, 0.008f, 0.6f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colour") FLinearColor Track = FLinearColor(1.0f, 1.0f, 1.0f, 0.16f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colour") float EndDimOpacity = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") float Margin = 40.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") float TopScrimHeight = 170.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") float BottomScrimHeight = 210.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") bool bBottomScrim = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") int32 ScrimSlices = 96;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") float ProgressWidth = 280.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") float LineThickness = 3.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") float ChipShear = -12.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") float WorldTagHeight = 90.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") float SuspicionArcWidth = 74.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion") float FadeSeconds = 0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion") float BannerSeconds = 3.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion") float EventBannerSeconds = 1.3f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion") float PhaseBannerSeconds = 2.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion") float PromptSeconds = 2.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion") float LegendDimAfter = 10.0f;
	/** Draw the game's own key legend (false: the FactoryKit legend is used instead). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") bool bOwnLegend = false;
	/** Soft drop shadow under all HUD text so it reads over sunlit gravel in the chase camera (no outline, no glow). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Type") float TextShadowOpacity = 0.9f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Type") int32 TextOutlineSize = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Type") FVector2D TextShadowOffset = FVector2D(2.0f, 2.5f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion") float LegendDimOpacity = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion") float KeyLightSeconds = 0.3f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion") float CounterTweenSpeed = 6.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion") float PopupSeconds = 1.1f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion") float PopupRise = 46.0f;
	/** World tags are loud while the yard is new, then quiet. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion") float TagsLoudSeconds = 14.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion") float TagsQuietOpacity = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Copy") FText BannerTitle = NSLOCTEXT("PocketSand", "BannerTitle", "MAKE {0} BAGS OF SAND VANISH");
	/** Vertical anchors of the opening banner and the countdown (centre stays clear for the hero, 8.7). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") float BannerAnchorY = 0.3f; // clear of the corner counters at the bigger type (2026-11-14)
	/** Bottom-right HUD block is lifted by this so the FactoryKit autoplay hint fits under it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") float CornerHintClearance = 34.0f;
	/** World prompts never sit higher than this fraction of the screen (clear of the top corner blocks). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") float PromptMinYFraction = 0.3f;
	/** World tags are hidden inside the top-left / top-right corner blocks (width, height in slate units). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") FVector2D TagCornerExclusion = FVector2D(380.0f, 230.0f);
	/** Same for the bottom corners (legend bottom-left, trouser block bottom-right). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") FVector2D TagLegendExclusion = FVector2D(460.0f, 330.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") float CountdownAnchorY = 0.37f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Copy") FText BannerSub = NSLOCTEXT("PocketSand", "BannerSub", "Before the whistle. Don't let a guard see yellow.");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Copy") FText ObjectiveLine = NSLOCTEXT("PocketSand", "ObjectiveLine", "BAGS GONE");
};
