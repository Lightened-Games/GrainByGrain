// Pocket Sand - the HUD (P21, design guide 8.7.0), built in C++ from UPSHUDStyle.
// Normal play: bags gone + hairline (top left), yard clock + strike marks (top right), the quiet key legend (bottom
// left), the trouser bags (bottom right). Contextual: countdown, one banner at a time, first-time prompts, surface
// tags with fill lines, the LOAD tag over the door, a suspicion line over the hero, "+1" pops, the red vignette,
// end screens. It listens to UPSEventSubsystem and reads IPSMatchInfo; gameplay never touches it.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/PSTypes.h"
#include "PSHUDWidget.generated.h"

class UCanvasPanel;
class UCanvasPanelSlot;
class UTextBlock;
class UImage;
class UOverlay;
class UHorizontalBox;
class UVerticalBox;
class UWidget;
class UFontFace;
class UPSHUDStyle;
struct FCompositeFont;

UCLASS()
class POCKETSAND_API UPSHUDWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	enum class EWeight : uint8 { Regular, Bold };

	struct FLine
	{
		TObjectPtr<UImage> Fill;
		TObjectPtr<UImage> Rest;
		float Width = 100.0f;
		float Thickness = 2.0f;
	};

	struct FLegendRow
	{
		FName Key;
		TObjectPtr<UTextBlock> Glyph;
		TObjectPtr<UTextBlock> Label;
		float Lit = 0.0f;
	};

	struct FBannerItem
	{
		FText Title;
		FText Sub;
		bool bDanger = false;
		float Duration = 1.4f;
	};

	struct FPrompt
	{
		TObjectPtr<UVerticalBox> Box;
		TObjectPtr<UTextBlock> Text;
		TObjectPtr<UImage> Rule;
		FVector World = FVector::ZeroVector;
		float Age = 100.0f;
	};

	struct FPopup
	{
		TObjectPtr<UTextBlock> Text;
		FVector2D Anchor = FVector2D::ZeroVector;
		float Age = 100.0f;
	};

	struct FWorldTag
	{
		FName Id;
		TObjectPtr<UVerticalBox> Box;
		TObjectPtr<UTextBlock> Text;
		FLine Fill;
		FVector World = FVector::ZeroVector;
		float Fraction = 0.0f;
		float Shown = 0.0f;
		float Opacity = 0.0f;
	};

	UPROPERTY() TObjectPtr<UPSHUDStyle> Style;
	UPROPERTY() TArray<TObjectPtr<UFontFace>> Faces;
	TSharedPtr<FCompositeFont> RegularFont;
	TSharedPtr<FCompositeFont> BoldFont;
	bool bFontsBuilt = false;

	UPROPERTY() TObjectPtr<UCanvasPanel> Root;
	UPROPERTY() TArray<TObjectPtr<UImage>> VignetteImages;
	UPROPERTY() TObjectPtr<UImage> Dim;
	UPROPERTY() TObjectPtr<UTextBlock> BagsText;
	UPROPERTY() TObjectPtr<UTextBlock> BagsOf;
	UPROPERTY() TObjectPtr<UTextBlock> ObjectiveLabel;
	UPROPERTY() TObjectPtr<UTextBlock> ClockText;
	UPROPERTY() TObjectPtr<UTextBlock> PhaseLabel;
	UPROPERTY() TArray<TObjectPtr<UImage>> StrikeMarks;
	UPROPERTY() TObjectPtr<UTextBlock> AutoplayText;
	UPROPERTY() TObjectPtr<UVerticalBox> LegendBox;
	UPROPERTY() TObjectPtr<UTextBlock> TrouserLabel;
	UPROPERTY() TObjectPtr<UOverlay> WaddleChip;
	UPROPERTY() TObjectPtr<UTextBlock> CountdownText;
	UPROPERTY() TObjectPtr<UVerticalBox> BannerBox;
	UPROPERTY() TObjectPtr<UTextBlock> BannerTitle;
	UPROPERTY() TObjectPtr<UImage> BannerRule;
	UPROPERTY() TObjectPtr<UTextBlock> BannerSub;
	UPROPERTY() TArray<TObjectPtr<UImage>> BannerBand;
	UPROPERTY() TArray<TObjectPtr<UImage>> EndBand;
	UPROPERTY() TObjectPtr<UVerticalBox> EndBox;
	UPROPERTY() TObjectPtr<UTextBlock> EndTitle;
	UPROPERTY() TObjectPtr<UTextBlock> EndReason;
	UPROPERTY() TObjectPtr<UTextBlock> EndStats;
	UPROPERTY() TObjectPtr<UTextBlock> EndPrompt;
	/** End screen as an in-world report (user 2026-11-17 references: flight-log panel, stat rows, grade badge). */
	UPROPERTY() TObjectPtr<class UBorder> EndPanel;
	UPROPERTY() TObjectPtr<class UBorder> EndStamp;
	UPROPERTY() TObjectPtr<UTextBlock> EndRank;
	UPROPERTY() TArray<TObjectPtr<UTextBlock>> EndValues;
	UPROPERTY() TArray<TObjectPtr<UWidget>> PlayCorners;
	UHorizontalBox* KeyChip(const FText& Key, const FText& Label, UTextBlock** OutLabel = nullptr);
	UPROPERTY() TObjectPtr<UVerticalBox> SuspicionBox;
	UPROPERTY() TObjectPtr<UTextBlock> SuspicionWord;
	UPROPERTY() TObjectPtr<UVerticalBox> DoorTag;
	UPROPERTY() TObjectPtr<UVerticalBox> RingTag;
	UPROPERTY() TObjectPtr<UTextBlock> RingTagText;

	FLine Progress;
	FLine BagLines[2];
	FLine SuspicionLine;
	TArray<FLegendRow> Legend;
	TArray<FPrompt> Prompts;
	TArray<FPopup> Popups;
	TArray<FWorldTag> Tags;
	TArray<FBannerItem> BannerQueue;
	FBannerItem CurrentBanner;
	float BannerAge = -1.0f;
	float CountdownAge = 100.0f;
	float VignetteStrength = 0.0f;
	float EndAge = -1.0f;
	float ActiveAge = 0.0f;
	float PlayAge = 0.0f;
	float ShownBags = 0.0f;
	float ShownSuspicion = 0.0f;
	float BagsPop = 0.0f;
	float StrikePop = 0.0f;
	float ClockPulse = 0.0f;
	float SuspicionAlpha = 0.0f;
	float DoorAlpha = 0.0f;
	float RingAlpha = 0.0f;
	float ObjectiveShrink = 0.0f;
	int32 LastStrikes = 0;

	UTextBlock* Txt(const FText& Text, int32 Size, const FLinearColor& Colour, EWeight Weight, int32 Tracking = 0);
	UImage* Blk(const FLinearColor& Colour, const FVector2D& Size);
	UHorizontalBox* MakeLine(FLine& Out, float Width, float Thickness, const FLinearColor& FillColour);
	void SetLine(const FLine& Line, float Alpha, const FLinearColor& FillColour) const;
	UCanvasPanelSlot* Place(UWidget* Widget, const FAnchors& Anchors, const FVector2D& Alignment, const FVector2D& Position);
	UCanvasPanelSlot* Float(UWidget* Widget);
	void AddFade(int32 Edge, float Extent, const FLinearColor& Colour, int32 Slices, TArray<TObjectPtr<UImage>>* Out, float BandAnchorY = 0.3f);
	void EnsureFonts(const FSlateFontInfo& EngineDefault);
	FLinearColor TextC(float Opacity = 1.0f) const;
	static FLinearColor WithA(const FLinearColor& C, float Opacity) { return FLinearColor(C.R, C.G, C.B, Opacity); }

	void BuildTree();
	void BuildObjective();
	void BuildClock();
	void BuildLegend();
	void BuildTrousers();
	void BuildBanner();
	void BuildPrompts();
	void BuildTags();
	void BuildEnd();
	FVector2D ViewSize() const;
	bool Project(const FVector& World, FVector2D& Out) const;
	void ShowBanner(const FText& Title, const FText& Sub, bool bDanger, float Duration);
	void Popup(const FText& Text, const FVector2D& Anchor, const FLinearColor& Colour);
	class IPSMatchInfo* Match() const;
	class IPSSandCarrier* Carrier() const;
	FText Clock(float Seconds) const;

	void UpdateReadouts(float Dt);
	void UpdateBanner(float Dt);
	void UpdatePrompts(float Dt);
	void UpdateTags(float Dt);
	void UpdateFeedback(float Dt);
	void UpdateEnd(float Dt);

	UFUNCTION() void HandleState(EPSMatchState State, EPSDefeat Reason);
	UFUNCTION() void HandleCountdown(int32 SecondsLeft);
	UFUNCTION() void HandlePhase(int32 Index, FText Title, FText Sub);
	UFUNCTION() void HandleBagEmptied(int32 Count, FVector Location);
	UFUNCTION() void HandleFound(int32 Strikes, FVector Location, bool bProbe);
	UFUNCTION() void HandleNotice(FText Title, FText Sub, bool bDanger);
	UFUNCTION() void HandlePrompt(FText Text, FVector WorldLocation, bool bDanger);
	UFUNCTION() void HandleInput(FName KeyId, bool bSimulated);
	UFUNCTION() void HandleRegion(FName RegionId, float Fraction, FVector Location);
	UFUNCTION() void HandleSuspicion(float Value, float Delta);
};
