// Pocket Sand - HUD (factories adapted from The Fix / Shift Supervisor 8.7.0 recipe).

#include "UI/PSHUDWidget.h"
#include "Data/PSHUDStyle.h"
#include "FactoryKit/FactoryKitStyle.h"
#include "Data/PSRoundConfig.h"
#include "Framework/PSEventSubsystem.h"
#include "Framework/PSPlayerController.h"
#include "Interfaces/PSInterfaces.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Engine/World.h"
#include "Fonts/CompositeFont.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

#define LOCTEXT_NAMESPACE "PocketSand"

namespace
{
	void SetImageSize(UImage* Img, const FVector2D& Size)
	{
		if (!Img) { return; }
		FSlateBrush Brush = Img->GetBrush();
		Brush.ImageSize = Size;
		Img->SetBrush(Brush);
	}

	template <typename SlotT>
	SlotT* Pad(SlotT* Slot, const FMargin& Margin)
	{
		if (Slot) { Slot->SetPadding(Margin); }
		return Slot;
	}

	void ShearWidget(UWidget* W, float ShearDegrees)
	{
		if (!W) { return; }
		FWidgetTransform T = W->GetRenderTransform();
		T.Shear = FVector2D(ShearDegrees, 0.0f);
		W->SetRenderTransform(T);
	}

	void ScaleWidget(UWidget* W, float Scale)
	{
		if (!W) { return; }
		FWidgetTransform T = W->GetRenderTransform();
		T.Scale = FVector2D(Scale);
		W->SetRenderTransform(T);
	}

	float EaseOut(float T) { T = FMath::Clamp(T, 0.0f, 1.0f); return 1.0f - (1.0f - T) * (1.0f - T); }
}

IPSMatchInfo* UPSHUDWidget::Match() const
{
	const UWorld* World = GetWorld();
	return World ? Cast<IPSMatchInfo>(World->GetAuthGameMode()) : nullptr;
}

IPSSandCarrier* UPSHUDWidget::Carrier() const
{
	const APlayerController* PC = GetOwningPlayer();
	return PC ? Cast<IPSSandCarrier>(PC->GetPawn()) : nullptr;
}

FLinearColor UPSHUDWidget::TextC(float Opacity) const
{
	return FLinearColor(Style->Text.R, Style->Text.G, Style->Text.B, Style->Text.A * Opacity);
}

FText UPSHUDWidget::Clock(float Seconds) const
{
	const int32 S = FMath::Max(0, FMath::CeilToInt(Seconds));
	return FText::FromString(FString::Printf(TEXT("%02d:%02d"), S / 60, S % 60));
}

// ---------------------------------------------------------------------------------------------- factories

void UPSHUDWidget::EnsureFonts(const FSlateFontInfo& EngineDefault)
{
	if (bFontsBuilt)
	{
		return;
	}
	bFontsBuilt = true;
	const UFont* Roboto = Cast<UFont>(EngineDefault.FontObject);
	const FCompositeFont* RobotoComposite = Roboto ? Roboto->GetCompositeFont() : nullptr;
	auto Build = [this, RobotoComposite](const TSoftObjectPtr<UFontFace>& FaceRef) -> TSharedPtr<FCompositeFont>
	{
		UFontFace* Face = FaceRef.LoadSynchronous();
		if (!Face)
		{
			return nullptr;
		}
		Faces.Add(Face);
		TSharedPtr<FCompositeFont> Composite = MakeShared<FCompositeFont>();
		FTypefaceEntry& Entry = Composite->DefaultTypeface.Fonts.Add_GetRef(FTypefaceEntry(TEXT("Regular")));
		Entry.Font = FFontData(Face);
		if (RobotoComposite)
		{
			Composite->FallbackTypeface.Typeface = RobotoComposite->DefaultTypeface;
		}
		return Composite;
	};
	RegularFont = Build(Style->RegularFace);
	BoldFont = Build(Style->BoldFace);
}

UTextBlock* UPSHUDWidget::Txt(const FText& Text, int32 Size, const FLinearColor& Colour, EWeight Weight, int32 Tracking)
{
	UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Block->SetText(Text);
	FSlateFontInfo Font = Block->GetFont();
	EnsureFonts(Font);
	const TSharedPtr<FCompositeFont>& Composite = Weight == EWeight::Bold ? BoldFont : RegularFont;
	if (Composite.IsValid())
	{
		Font = FSlateFontInfo(Composite, Size, TEXT("Regular"));
	}
	else
	{
		Font.TypefaceFontName = Weight == EWeight::Bold ? FName(TEXT("Bold")) : FName(TEXT("Regular"));
	}
	Font.Size = Size;
	Font.LetterSpacing = Tracking;
	// Thin dark outline + drop shadow: every label reads on bright gravel and sand (user 2026-11-14: "add text shadow").
	Font.OutlineSettings.OutlineSize = Style ? Style->TextOutlineSize : 1;
	Font.OutlineSettings.OutlineColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.85f);
	Block->SetFont(Font);
	Block->SetColorAndOpacity(FSlateColor(Colour));
	Block->SetShadowOffset(Style ? Style->TextShadowOffset : FVector2D(1.0f, 1.5f));
	Block->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, Style ? Style->TextShadowOpacity : 0.75f));
	return Block;
}

UImage* UPSHUDWidget::Blk(const FLinearColor& Colour, const FVector2D& Size)
{
	UImage* Img = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::Box;
	Brush.ImageSize = Size;
	Img->SetBrush(Brush);
	Img->SetColorAndOpacity(Colour);
	Img->SetVisibility(ESlateVisibility::HitTestInvisible);
	return Img;
}

UHorizontalBox* UPSHUDWidget::MakeLine(FLine& Out, float Width, float Thickness, const FLinearColor& FillColour)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Out.Width = Width;
	Out.Thickness = Thickness;
	Out.Fill = Blk(FillColour, FVector2D(0.0f, Thickness));
	Out.Rest = Blk(Style->Track, FVector2D(Width, Thickness));
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Out.Fill)) { S->SetVerticalAlignment(VAlign_Center); }
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Out.Rest)) { S->SetVerticalAlignment(VAlign_Center); }
	return Row;
}

void UPSHUDWidget::SetLine(const FLine& Line, float Alpha, const FLinearColor& FillColour) const
{
	const float A = FMath::Clamp(Alpha, 0.0f, 1.0f);
	SetImageSize(Line.Fill, FVector2D(FMath::Max(0.0f, Line.Width * A), Line.Thickness));
	SetImageSize(Line.Rest, FVector2D(FMath::Max(0.0f, Line.Width * (1.0f - A)), Line.Thickness));
	if (Line.Fill) { Line.Fill->SetColorAndOpacity(FillColour); }
}

UCanvasPanelSlot* UPSHUDWidget::Place(UWidget* Widget, const FAnchors& Anchors, const FVector2D& Alignment, const FVector2D& Position)
{
	UCanvasPanelSlot* S = Root->AddChildToCanvas(Widget);
	if (S)
	{
		S->SetAnchors(Anchors);
		S->SetAlignment(Alignment);
		S->SetPosition(Position);
		S->SetAutoSize(true);
	}
	return S;
}

UCanvasPanelSlot* UPSHUDWidget::Float(UWidget* Widget)
{
	Widget->SetVisibility(ESlateVisibility::HitTestInvisible);
	Widget->SetRenderOpacity(0.0f);
	UCanvasPanelSlot* S = Root->AddChildToCanvas(Widget);
	if (S)
	{
		S->SetAlignment(FVector2D(0.5f, 1.0f));
		S->SetAutoSize(true);
	}
	return S;
}

void UPSHUDWidget::AddFade(int32 Edge, float Extent, const FLinearColor& Colour, int32 Slices, TArray<TObjectPtr<UImage>>* Out, float BandAnchorY)
{
	// Nested slices accumulate into a seamless soft scrim (Shift Supervisor AddFade).
	const int32 N = FMath::Max(1, Slices);
	const float SliceAlpha = 1.0f - FMath::Pow(1.0f - FMath::Clamp(Colour.A, 0.0f, 0.99f), 1.0f / N);
	for (int32 I = 0; I < N; ++I)
	{
		UImage* Slice = Blk(FLinearColor(Colour.R, Colour.G, Colour.B, SliceAlpha), FVector2D(4.0f));
		UCanvasPanelSlot* S = Root->AddChildToCanvas(Slice);
		if (!S)
		{
			continue;
		}
		const float Depth = Extent * (I + 1) / N;
		switch (Edge)
		{
		case 0: S->SetAnchors(FAnchors(0, 0, 1, 0)); S->SetOffsets(FMargin(0, 0, 0, Depth)); break;
		case 1: S->SetAnchors(FAnchors(0, 1, 1, 1)); S->SetOffsets(FMargin(0, -Depth, 0, Depth)); break;
		case 2: S->SetAnchors(FAnchors(0, 0, 0, 1)); S->SetOffsets(FMargin(0, 0, Depth, 0)); break;
		case 3: S->SetAnchors(FAnchors(1, 0, 1, 1)); S->SetOffsets(FMargin(-Depth, 0, Depth, 0)); break;
		default: S->SetAnchors(FAnchors(0, BandAnchorY, 1, BandAnchorY)); S->SetOffsets(FMargin(0, -Depth * 0.5f, 0, Depth)); break;
		}
		if (Out)
		{
			Out->Add(Slice);
		}
	}
}

// ---------------------------------------------------------------------------------------------- layout

TSharedRef<SWidget> UPSHUDWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildTree();
	}
	return Super::RebuildWidget();
}

void UPSHUDWidget::BuildTree()
{
	const IPSMatchInfo* M = Match();
	const UPSRoundConfig* Cfg = M ? M->PS_Config() : nullptr;
	Style = Cfg ? Cfg->HUDStyle.LoadSynchronous() : nullptr;
	if (!Style)
	{
		Style = NewObject<UPSHUDStyle>(this, TEXT("DefaultHUDStyle"));
	}
	Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	// Fine slices (96) so the soft top shade has no visible steps; no bottom shade (user 2026-11-09: "shadowy lines in
	// the bottom area"; the controls list it sat behind is gone, the corner text has its own shadow).
	AddFade(0, Style->TopScrimHeight, Style->Scrim, Style->ScrimSlices, nullptr);
	if (Style->bBottomScrim)
	{
		AddFade(1, Style->BottomScrimHeight, Style->Scrim, Style->ScrimSlices, nullptr);
	}
	const FLinearColor V = WithA(Style->Danger, 0.55f);
	AddFade(2, 240.0f, V, 18, &VignetteImages);
	AddFade(3, 240.0f, V, 18, &VignetteImages);
	AddFade(0, 160.0f, V, 14, &VignetteImages);
	AddFade(1, 160.0f, V, 14, &VignetteImages);
	for (UImage* Img : VignetteImages)
	{
		Img->SetRenderOpacity(0.0f);
	}
	BuildTags();
	BuildPrompts();
	BuildObjective();
	BuildClock();
	// The FactoryKit legend (bottom-left, shadowed, lit by input) replaces the game's own (design guide 2.5.8).
	if (Style->bOwnLegend)
	{
		BuildLegend();
	}
	BuildTrousers();
	BuildBanner();
	BuildEnd();
}

FVector2D UPSHUDWidget::ViewSize() const
{
	return UWidgetLayoutLibrary::GetViewportSize(this) / FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
}

void UPSHUDWidget::BuildObjective()
{
	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	BagsText = Txt(FText::AsNumber(0), Style->HeadlineSize, TextC(), EWeight::Regular);
	BagsOf = Txt(FText::GetEmpty(), Style->BodySize, TextC(Style->SecondaryOpacity), EWeight::Regular);
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(BagsText)) { S->SetVerticalAlignment(VAlign_Bottom); }
	if (UHorizontalBoxSlot* S = Pad(Row->AddChildToHorizontalBox(BagsOf), FMargin(10, 0, 0, 10))) { S->SetVerticalAlignment(VAlign_Bottom); }
	Pad(Col->AddChildToVerticalBox(Row), FMargin(0, -8, 0, 2));
	ObjectiveLabel = Txt(Style->ObjectiveLine, Style->LabelSize, TextC(Style->SecondaryOpacity), EWeight::Bold, Style->LabelTracking);
	Pad(Col->AddChildToVerticalBox(ObjectiveLabel), FMargin(0, 0, 0, 8));
	Col->AddChildToVerticalBox(MakeLine(Progress, Style->ProgressWidth, Style->LineThickness, Style->Accent));
	Place(Col, FAnchors(0, 0), FVector2D(0, 0), FVector2D(Style->Margin, Style->Margin - 6.0f));
	PlayCorners.Add(Col); // fades out under the end screen
}

void UPSHUDWidget::BuildClock()
{
	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	ClockText = Txt(FText::FromString(TEXT("01:30")), Style->HeadlineSize, TextC(), EWeight::Regular);
	if (UVerticalBoxSlot* S = Pad(Col->AddChildToVerticalBox(ClockText), FMargin(0, -8, 0, 2))) { S->SetHorizontalAlignment(HAlign_Right); }
	UHorizontalBox* Marks = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	PhaseLabel = Txt(LOCTEXT("YardTime", "YARD TIME"), Style->LabelSize, TextC(Style->SecondaryOpacity), EWeight::Bold, Style->LabelTracking);
	PhaseLabel->SetVisibility(ESlateVisibility::Collapsed);
	UTextBlock* Found = Txt(LOCTEXT("Found", "PATCHES FOUND"), Style->LabelSize, TextC(Style->SecondaryOpacity), EWeight::Bold, Style->LabelTracking);
	if (UHorizontalBoxSlot* S = Pad(Marks->AddChildToHorizontalBox(Found), FMargin(0, 0, 10, 0))) { S->SetVerticalAlignment(VAlign_Center); }
	const IPSMatchInfo* M = Match();
	const int32 N = M && M->PS_Config() ? M->PS_Config()->StrikesToLose : 3;
	for (int32 I = 0; I < N; ++I)
	{
		UImage* Mark = Blk(Style->Track, FVector2D(9.0f, 9.0f));
		Mark->SetRenderTransformAngle(45.0f);
		if (UHorizontalBoxSlot* S = Pad(Marks->AddChildToHorizontalBox(Mark), FMargin(5, 0))) { S->SetVerticalAlignment(VAlign_Center); }
		StrikeMarks.Add(Mark);
	}
	if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(Marks)) { S->SetHorizontalAlignment(HAlign_Right); }
	AutoplayText = Txt(LOCTEXT("Autoplay", "AUTOPLAY"), Style->LabelSize, TextC(Style->SecondaryOpacity * 0.8f), EWeight::Bold, Style->LabelTracking);
	AutoplayText->SetVisibility(ESlateVisibility::Collapsed);
	if (UVerticalBoxSlot* S = Pad(Col->AddChildToVerticalBox(AutoplayText), FMargin(0, 10, 0, 0))) { S->SetHorizontalAlignment(HAlign_Right); }
	Place(Col, FAnchors(1, 0), FVector2D(1, 0), FVector2D(-Style->Margin, Style->Margin - 6.0f));
	PlayCorners.Add(Col); // fades out under the end screen
}

void UPSHUDWidget::BuildLegend()
{
	struct FRowDef { FName Key; FText Glyph; FText Label; };
	const TArray<FRowDef> Rows = {
		{ APSPlayerController::KeyMove, LOCTEXT("GlyphMove", "WASD"), LOCTEXT("RowMove", "Move") },
		{ APSPlayerController::KeyLook, LOCTEXT("GlyphLook", "MOUSE"), LOCTEXT("RowLook", "Look around") },
		{ APSPlayerController::KeyPour, LOCTEXT("GlyphPour", "SPACE"), LOCTEXT("RowPour", "Hold to pour") },
		{ APSPlayerController::KeyLoad, LOCTEXT("GlyphLoad", "E"), LOCTEXT("RowLoad", "Load a bag at the door") },
		{ APSPlayerController::KeyRestart, LOCTEXT("GlyphRestart", "R"), LOCTEXT("RowRestart", "Restart") },
	};
	LegendBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	for (const FRowDef& R : Rows)
	{
		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		FLegendRow Row;
		Row.Key = R.Key;
		Row.Glyph = Txt(R.Glyph, Style->LabelSize, TextC(0.9f), EWeight::Bold, 160);
		Row.Label = Txt(R.Label, Style->LabelSize, TextC(Style->SecondaryOpacity), EWeight::Regular, 40);
		USizeBox* KeyColumn = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		KeyColumn->SetWidthOverride(66.0f);
		KeyColumn->SetContent(Row.Glyph);
		if (UHorizontalBoxSlot* S = Pad(Line->AddChildToHorizontalBox(KeyColumn), FMargin(0, 0, 8, 0))) { S->SetVerticalAlignment(VAlign_Center); }
		if (UHorizontalBoxSlot* S = Line->AddChildToHorizontalBox(Row.Label)) { S->SetVerticalAlignment(VAlign_Center); }
		Pad(LegendBox->AddChildToVerticalBox(Line), FMargin(0, 4));
		Legend.Add(Row);
	}
	Place(LegendBox, FAnchors(0, 1), FVector2D(0, 1), FVector2D(Style->Margin, -Style->Margin));
}

void UPSHUDWidget::BuildTrousers()
{
	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	WaddleChip = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	// Danger-red text with the soft shadow, no plate (8.7: no boxed panels).
	UTextBlock* ChipText = Txt(LOCTEXT("Waddle", "WADDLING"), Style->LabelSize, Style->Danger * 1.5f, EWeight::Bold, Style->LabelTracking);
	if (UOverlaySlot* S = WaddleChip->AddChildToOverlay(ChipText)) { S->SetHorizontalAlignment(HAlign_Right); S->SetVerticalAlignment(VAlign_Center); }
	WaddleChip->SetRenderOpacity(0.0f);
	if (UVerticalBoxSlot* S = Pad(Col->AddChildToVerticalBox(WaddleChip), FMargin(0, 0, 0, 12))) { S->SetHorizontalAlignment(HAlign_Right); }
	UHorizontalBox* Bags = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	for (int32 I = 0; I < 2; ++I)
	{
		Pad(Bags->AddChildToHorizontalBox(MakeLine(BagLines[I], 54.0f, 5.0f, Style->Accent)), FMargin(I == 0 ? 0 : 10, 0, 0, 0));
	}
	if (UVerticalBoxSlot* S = Pad(Col->AddChildToVerticalBox(Bags), FMargin(0, 0, 0, 8))) { S->SetHorizontalAlignment(HAlign_Right); }
	TrouserLabel = Txt(LOCTEXT("Trousers", "IN YOUR TROUSERS"), Style->LabelSize, TextC(Style->SecondaryOpacity), EWeight::Bold, Style->LabelTracking);
	if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(TrouserLabel)) { S->SetHorizontalAlignment(HAlign_Right); }
	// Sits above the FactoryKit "autoplay · any key to take over" hint, which owns the bottom-right corner line.
	Place(Col, FAnchors(1, 1), FVector2D(1, 1), FVector2D(-Style->Margin, -Style->Margin - Style->CornerHintClearance));
	PlayCorners.Add(Col); // fades out under the end screen
}

void UPSHUDWidget::BuildBanner()
{
	AddFade(4, 230.0f, FLinearColor(0.004f, 0.005f, 0.008f, 0.6f), 26, &BannerBand, 0.3f);
	for (UImage* Img : BannerBand)
	{
		Img->SetRenderOpacity(0.0f);
	}
	BannerBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	BannerTitle = Txt(FText::GetEmpty(), Style->HeadlineSize, TextC(), EWeight::Bold, Style->HeadlineTracking);
	BannerTitle->SetJustification(ETextJustify::Center);
	BannerRule = Blk(Style->Accent, FVector2D(64.0f, 3.0f));
	ShearWidget(BannerRule, Style->ChipShear * 2.0f);
	BannerSub = Txt(FText::GetEmpty(), Style->BodySize, TextC(0.85f), EWeight::Regular, 20);
	BannerSub->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* S = BannerBox->AddChildToVerticalBox(BannerTitle)) { S->SetHorizontalAlignment(HAlign_Center); }
	if (UVerticalBoxSlot* S = Pad(BannerBox->AddChildToVerticalBox(BannerRule), FMargin(0, 6, 0, 10))) { S->SetHorizontalAlignment(HAlign_Center); }
	if (UVerticalBoxSlot* S = BannerBox->AddChildToVerticalBox(BannerSub)) { S->SetHorizontalAlignment(HAlign_Center); }
	BannerBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	BannerBox->SetRenderOpacity(0.0f);
	Place(BannerBox, FAnchors(0.5f, Style->BannerAnchorY), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);

	// Countdown: headline size (3 type sizes in total), between the banner and the hero so the centre stays clear.
	CountdownText = Txt(FText::GetEmpty(), Style->HeadlineSize, TextC(), EWeight::Regular);
	CountdownText->SetJustification(ETextJustify::Center);
	CountdownText->SetRenderOpacity(0.0f);
	Place(CountdownText, FAnchors(0.5f, Style->CountdownAnchorY), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);
}

void UPSHUDWidget::BuildPrompts()
{
	for (int32 I = 0; I < 2; ++I)
	{
		FPrompt P;
		P.Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		P.Text = Txt(FText::GetEmpty(), Style->LabelSize, TextC(), EWeight::Bold, Style->LabelTracking);
		if (UVerticalBoxSlot* S = P.Box->AddChildToVerticalBox(P.Text)) { S->SetHorizontalAlignment(HAlign_Center); }
		P.Rule = Blk(Style->Accent, FVector2D(40.0f, 2.0f));
		if (UVerticalBoxSlot* S = Pad(P.Box->AddChildToVerticalBox(P.Rule), FMargin(0, 4, 0, 0))) { S->SetHorizontalAlignment(HAlign_Center); }
		Float(P.Box);
		Prompts.Add(P);
	}
	for (int32 I = 0; I < 4; ++I)
	{
		FPopup P;
		P.Text = Txt(FText::GetEmpty(), Style->BodySize, Style->Accent, EWeight::Bold);
		P.Text->SetRenderOpacity(0.0f);
		if (UCanvasPanelSlot* S = Root->AddChildToCanvas(P.Text))
		{
			S->SetAlignment(FVector2D(0.0f, 1.0f));
			S->SetAutoSize(true);
		}
		Popups.Add(P);
	}
	// Suspicion line over the hero (only while it's above zero).
	SuspicionBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	SuspicionWord = Txt(LOCTEXT("Seen", "SEEN"), Style->LabelSize, Style->Danger * 1.4f, EWeight::Bold, Style->LabelTracking);
	if (UVerticalBoxSlot* S = Pad(SuspicionBox->AddChildToVerticalBox(SuspicionWord), FMargin(0, 0, 0, 4))) { S->SetHorizontalAlignment(HAlign_Center); }
	if (UVerticalBoxSlot* S = SuspicionBox->AddChildToVerticalBox(MakeLine(SuspicionLine, Style->SuspicionArcWidth, Style->LineThickness, Style->Danger))) { S->SetHorizontalAlignment(HAlign_Center); }
	Float(SuspicionBox);
	// "SHOWS" tag beside the ring while pouring where sand is visible.
	RingTag = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	RingTagText = Txt(LOCTEXT("Shows", "SHOWS"), Style->LabelSize, Style->Danger * 1.4f, EWeight::Bold, Style->LabelTracking);
	RingTag->AddChildToVerticalBox(RingTagText);
	Float(RingTag);
}

void UPSHUDWidget::BuildTags()
{
	const IPSMatchInfo* M = Match();
	const UPSRoundConfig* Cfg = M ? M->PS_Config() : nullptr;
	if (!Cfg)
	{
		return;
	}
	for (const FPSRegionDef& R : Cfg->Regions)
	{
		const FPSSurfaceRow Row = Cfg->GetSurface(R.Surface);
		if (Row.Tag.IsEmpty())
		{
			continue;
		}
		FWorldTag T;
		T.Id = R.Id;
		T.World = FVector(R.Center, R.TopZ + Style->WorldTagHeight);
		T.Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		T.Text = Txt(Row.Tag, Style->LabelSize, TextC(0.9f), EWeight::Bold, Style->LabelTracking);
		if (UVerticalBoxSlot* S = Pad(T.Box->AddChildToVerticalBox(T.Text), FMargin(0, 0, 0, 4))) { S->SetHorizontalAlignment(HAlign_Center); }
		if (UVerticalBoxSlot* S = T.Box->AddChildToVerticalBox(MakeLine(T.Fill, 46.0f, 2.0f, Style->Accent))) { S->SetHorizontalAlignment(HAlign_Center); }
		Float(T.Box);
		Tags.Add(T);
	}
	DoorTag = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UTextBlock* DoorText = Txt(LOCTEXT("DoorTag", "E  LOAD"), Style->LabelSize, Style->Accent * 1.25f, EWeight::Bold, Style->LabelTracking);
	if (UVerticalBoxSlot* S = Pad(DoorTag->AddChildToVerticalBox(DoorText), FMargin(0, 0, 0, 4))) { S->SetHorizontalAlignment(HAlign_Center); }
	if (UVerticalBoxSlot* S = DoorTag->AddChildToVerticalBox(Blk(Style->Accent, FVector2D(30.0f, 2.0f)))) { S->SetHorizontalAlignment(HAlign_Center); }
	Float(DoorTag);
}

UHorizontalBox* UPSHUDWidget::KeyChip(const FText& Key, const FText& Label, UTextBlock** OutLabel)
{
	UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UBorder* Cap = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Cap->SetBrush(FactoryKitStyle::BoxBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.5f), Style->Accent * FLinearColor(1, 1, 1, 0.95f), 4.0f, 1.5f));
	Cap->SetPadding(FMargin(10.0f, 3.0f, 10.0f, 4.0f));
	Cap->SetContent(Txt(Key, Style->LabelSize + 2, TextC(), EWeight::Bold, 60));
	H->AddChildToHorizontalBox(Cap)->SetVerticalAlignment(VAlign_Center);
	UTextBlock* L = Txt(Label, Style->LabelSize + 4, TextC(), EWeight::Bold, Style->LabelTracking);
	if (UHorizontalBoxSlot* S = H->AddChildToHorizontalBox(L)) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(12.0f, 0, 0, 0)); }
	if (OutLabel) { *OutLabel = L; }
	return H;
}

void UPSHUDWidget::BuildEnd()
{
	Dim = Blk(FLinearColor(0.0f, 0.0f, 0.004f, Style->EndDimOpacity), FVector2D(8.0f));
	Dim->SetRenderOpacity(0.0f);
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Dim))
	{
		S->SetAnchors(FAnchors(0, 0, 1, 1));
		S->SetOffsets(FMargin(0));
	}
	AddFade(4, 420.0f, FLinearColor(0.0f, 0.003f, 0.006f, 0.85f), 30, &EndBand, 0.45f);
	for (UImage* Img : EndBand)
	{
		Img->SetRenderOpacity(0.0f);
	}
	// Story moment, lower left (reference: "ABOARD KESTREL / Juno Vale is going home"): headline, accent rule, story line.
	EndBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	EndTitle = Txt(FText::GetEmpty(), FMath::RoundToInt(Style->HeadlineSize * 1.45f), TextC(), EWeight::Bold, Style->HeadlineTracking);
	EndReason = Txt(FText::GetEmpty(), Style->BodySize, Style->Accent * 1.3f, EWeight::Regular, 40);
	EndBox->AddChildToVerticalBox(EndTitle);
	Pad(EndBox->AddChildToVerticalBox(Blk(Style->Accent, FVector2D(140.0f, 3.0f))), FMargin(4, 6, 0, 14));
	Pad(EndBox->AddChildToVerticalBox(EndReason), FMargin(4, 0, 0, 0));
	EndBox->SetRenderOpacity(0.0f);
	EndBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	Place(EndBox, FAnchors(0.0f, 1.0f), FVector2D(0.0f, 1.0f), FVector2D(Style->Margin * 2.0f, -Style->Margin * 2.2f));

	// The report, right of centre (reference: FLIGHT LOG panel with stat rows, grade badge and two buttons).
	const float W = 600.0f;
	EndPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	EndPanel->SetBrush(FactoryKitStyle::BoxBrush(FLinearColor(0.012f, 0.016f, 0.024f, 0.9f), Style->Accent * FLinearColor(1, 1, 1, 0.55f), 6.0f, 1.5f));
	EndPanel->SetPadding(FMargin(34.0f, 28.0f));
	UVerticalBox* P = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	EndPanel->SetContent(P);
	UHorizontalBox* Head = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(Txt(LOCTEXT("ReportHead", "YARD REPORT"), Style->BodySize + 4, TextC(), EWeight::Bold, 160)))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(Txt(LOCTEXT("ReportBlock", "BLOCK C / CELL 14"), Style->LabelSize, TextC(Style->SecondaryOpacity), EWeight::Bold, 80)))
	{
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(24.0f, 0, 0, 0));
	}
	P->AddChildToVerticalBox(Head);
	Pad(P->AddChildToVerticalBox(Blk(Style->Accent, FVector2D(W, 2.0f))), FMargin(0, 10, 0, 8));
	static const FText Labels[] = {
		LOCTEXT("RowTime", "TIME IN THE YARD"), LOCTEXT("RowBags", "BAGS GONE"), LOCTEXT("RowFound", "PATCHES FOUND"),
		LOCTEXT("RowTrips", "TRIPS TO THE DOOR"), LOCTEXT("RowClose", "CLOSEST CALL"), LOCTEXT("RowWhere", "MOST SAND IN") };
	EndValues.Reset();
	for (const FText& Lb : Labels)
	{
		UHorizontalBox* R = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UHorizontalBoxSlot* S = R->AddChildToHorizontalBox(Txt(Lb, Style->LabelSize + 2, TextC(Style->SecondaryOpacity), EWeight::Bold, Style->LabelTracking)))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		UTextBlock* V = Txt(FText::GetEmpty(), Style->BodySize + 2, TextC(), EWeight::Bold, 60);
		R->AddChildToHorizontalBox(V)->SetVerticalAlignment(VAlign_Center);
		EndValues.Add(V);
		Pad(P->AddChildToVerticalBox(R), FMargin(0, 7, 0, 7));
		P->AddChildToVerticalBox(Blk(FLinearColor(1, 1, 1, 0.1f), FVector2D(W, 1.0f)));
	}
	// Verdict stamp (a rubber stamp in the world's voice) and the two buttons as keycaps.
	UHorizontalBox* Foot = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UVerticalBox* Verdict = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Verdict->AddChildToVerticalBox(Txt(LOCTEXT("VerdictHead", "WARDEN'S VERDICT"), Style->LabelSize, TextC(Style->SecondaryOpacity), EWeight::Bold, Style->LabelTracking));
	EndStamp = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	EndStamp->SetPadding(FMargin(18.0f, 4.0f, 18.0f, 6.0f));
	EndRank = Txt(FText::GetEmpty(), Style->HeadlineSize, Style->Accent, EWeight::Bold, 200);
	EndStamp->SetContent(EndRank);
	EndStamp->SetRenderTransformAngle(-7.0f);
	if (UVerticalBoxSlot* VS = Pad(Verdict->AddChildToVerticalBox(EndStamp), FMargin(4, 10, 0, 0))) { VS->SetHorizontalAlignment(HAlign_Left); }
	Foot->AddChildToHorizontalBox(Verdict)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UVerticalBox* Buttons = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UTextBlock* RestartLabel = nullptr;
	Pad(Buttons->AddChildToVerticalBox(KeyChip(LOCTEXT("KeyR", "R"), LOCTEXT("EndRetry", "TRY AGAIN"), &RestartLabel)), FMargin(0, 0, 0, 12));
	EndPrompt = RestartLabel;
	Buttons->AddChildToVerticalBox(KeyChip(LOCTEXT("KeyEsc", "ESC"), LOCTEXT("EndMenu", "MENU")));
	Foot->AddChildToHorizontalBox(Buttons)->SetVerticalAlignment(VAlign_Bottom);
	Pad(P->AddChildToVerticalBox(Foot), FMargin(0, 22, 0, 0));
	EndPanel->SetRenderOpacity(0.0f);
	EndPanel->SetVisibility(ESlateVisibility::HitTestInvisible);
	Place(EndPanel, FAnchors(1.0f, 0.5f), FVector2D(1.0f, 0.5f), FVector2D(-Style->Margin * 2.0f, -20.0f));
	EndStats = nullptr;
}

// ---------------------------------------------------------------------------------------------- events

void UPSHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UPSEventSubsystem* Bus = UPSEventSubsystem::Get(this))
	{
		Bus->OnStateChanged.AddUniqueDynamic(this, &UPSHUDWidget::HandleState);
		Bus->OnCountdown.AddUniqueDynamic(this, &UPSHUDWidget::HandleCountdown);
		Bus->OnPhase.AddUniqueDynamic(this, &UPSHUDWidget::HandlePhase);
		Bus->OnBagEmptied.AddUniqueDynamic(this, &UPSHUDWidget::HandleBagEmptied);
		Bus->OnPatchFound.AddUniqueDynamic(this, &UPSHUDWidget::HandleFound);
		Bus->OnNotice.AddUniqueDynamic(this, &UPSHUDWidget::HandleNotice);
		Bus->OnPrompt.AddUniqueDynamic(this, &UPSHUDWidget::HandlePrompt);
		Bus->OnInput.AddUniqueDynamic(this, &UPSHUDWidget::HandleInput);
		Bus->OnRegionFill.AddUniqueDynamic(this, &UPSHUDWidget::HandleRegion);
		Bus->OnSuspicion.AddUniqueDynamic(this, &UPSHUDWidget::HandleSuspicion);
	}
	if (const IPSMatchInfo* M = Match())
	{
		if (AutoplayText)
		{
			AutoplayText->SetVisibility(ESlateVisibility::Collapsed); // FactoryKit shows "autoplay · any key to take over"
		}
		if (BagsOf && M->PS_Config())
		{
			BagsOf->SetText(FText::FromString(FString::Printf(TEXT("/ %d"), M->PS_Config()->BagsTotal)));
		}
		HandleState(M->PS_State(), EPSDefeat::None);
	}
}

void UPSHUDWidget::ShowBanner(const FText& Title, const FText& Sub, bool bDanger, float Duration)
{
	FBannerItem Item;
	Item.Title = Title;
	Item.Sub = Sub;
	Item.bDanger = bDanger;
	Item.Duration = Duration;
	BannerQueue.Add(Item);
}

void UPSHUDWidget::Popup(const FText& Text, const FVector2D& Anchor, const FLinearColor& Colour)
{
	for (FPopup& P : Popups)
	{
		if (P.Age >= Style->PopupSeconds)
		{
			P.Text->SetText(Text);
			P.Text->SetColorAndOpacity(FSlateColor(Colour));
			P.Anchor = Anchor;
			P.Age = 0.0f;
			return;
		}
	}
}

void UPSHUDWidget::HandleState(EPSMatchState State, EPSDefeat Reason)
{
	// Start screen and intro: the FactoryKit menu and cinematic own the screen; the game HUD returns at the countdown.
	SetVisibility(State == EPSMatchState::Menu ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	if (State == EPSMatchState::Menu)
	{
		return;
	}
	if (State == EPSMatchState::Ready)
	{
		EndAge = -1.0f;
		BannerQueue.Reset();
		BannerAge = -1.0f;
		ActiveAge = 0.0f;
		PlayAge = 0.0f;
		ObjectiveShrink = 0.0f;
		LastStrikes = 0;
		const int32 Bags = Match() && Match()->PS_Config() ? Match()->PS_Config()->BagsTotal : 12;
		ShowBanner(FText::Format(Style->BannerTitle, Bags), Style->BannerSub, false, Style->BannerSeconds + 1.5f);
		if (BagsOf)
		{
			BagsOf->SetText(FText::FromString(FString::Printf(TEXT("/ %d"), Bags))); // the showcase round changes the total
		}
		for (FPrompt& P : Prompts) { P.Age = 100.0f; }
		return;
	}
	if (State != EPSMatchState::Victory && State != EPSMatchState::Defeat)
	{
		return;
	}
	const IPSMatchInfo* M = Match();
	if (!M || !M->PS_Config())
	{
		return;
	}
	const UPSRoundConfig* Cfg = M->PS_Config();
	FText Title, Line;
	if (State == EPSMatchState::Victory)
	{
		Title = LOCTEXT("WinTitle", "YARD CLEAN");
		Line = LOCTEXT("WinLine", "Every bag gone and not a grain in sight. The tunnel lives.");
	}
	else if (Reason == EPSDefeat::Found)
	{
		Title = LOCTEXT("FoundTitle", "FOUND IT");
		Line = LOCTEXT("FoundLine", "Three yellow patches. They're checking the laundry tonight.");
	}
	else if (Reason == EPSDefeat::Caught)
	{
		Title = LOCTEXT("CaughtTitle", "CAUGHT");
		Line = LOCTEXT("CaughtLine", "He watched it run out of your trouser leg.");
	}
	else
	{
		const int32 Left = M->PS_BagsInCell() + (Carrier() ? Carrier()->PS_Carried() : 0);
		Title = LOCTEXT("WhistleTitle", "WHISTLE");
		Line = FText::Format(LOCTEXT("WhistleLine", "Yard time's over. Inspection found {0} {0}|plural(one=bag,other=bags) of sand."), Left);
	}
	const FPSStats& S = M->PS_Stats();
	const bool bWin = State == EPSMatchState::Victory;
	// Where most sand went, in the yard's words.
	FName BestKey;
	float BestAmt = 0.0f;
	for (const TPair<FName, float>& KV : S.PouredBySurface) { if (KV.Value > BestAmt) { BestAmt = KV.Value; BestKey = KV.Key; } }
	const FString K = BestKey.ToString();
	const FText Where = BestAmt <= 0.0f ? LOCTEXT("WhereNone", "NOWHERE") : K.StartsWith(TEXT("Bed")) ? LOCTEXT("WhereBeds", "THE SOIL BEDS")
		: K.StartsWith(TEXT("Puddle")) ? LOCTEXT("WherePuddle", "THE PUDDLES") : K == TEXT("Court") ? LOCTEXT("WhereCourt", "THE COURT")
		: K == TEXT("Pit") ? LOCTEXT("WherePit", "THE JUMP PIT") : LOCTEXT("WhereGravel", "UNDER THE CROWD");
	const FText Values[] = {
		Clock(M->PS_Elapsed()),
		FText::Format(LOCTEXT("ValBags", "{0} / {1}"), M->PS_BagsGone(), Cfg->BagsTotal),
		FText::Format(LOCTEXT("ValFound", "{0} / {1}"), S.Strikes, Cfg->StrikesToLose),
		FText::AsNumber(S.Trips),
		FText::Format(LOCTEXT("ValClose", "{0} %"), FMath::RoundToInt(S.PeakSuspicion * 100.0f)),
		Where };
	for (int32 I = 0; I < EndValues.Num() && I < UE_ARRAY_COUNT(Values); ++I)
	{
		EndValues[I]->SetText(Values[I]);
	}
	if (EndValues.IsValidIndex(2)) { EndValues[2]->SetColorAndOpacity(FSlateColor(S.Strikes > 0 ? Style->Danger * 1.4f : TextC())); }
	// Verdict stamp: GHOST (clean win), LUCKY (won with finds), TOO SLOW (whistle), BUSTED (found or caught).
	const FText Rank = bWin ? (S.Strikes == 0 ? LOCTEXT("RankGhost", "GHOST") : LOCTEXT("RankLucky", "LUCKY"))
		: (Reason == EPSDefeat::Found || Reason == EPSDefeat::Caught ? LOCTEXT("RankBusted", "BUSTED") : LOCTEXT("RankSlow", "TOO SLOW"));
	const FLinearColor StampC = bWin ? Style->Accent * 1.25f : Style->Danger * 1.5f;
	EndRank->SetText(Rank);
	EndRank->SetColorAndOpacity(FSlateColor(StampC));
	EndStamp->SetBrush(FactoryKitStyle::BoxBrush(FLinearColor(StampC.R, StampC.G, StampC.B, 0.08f), StampC, 3.0f, 3.0f));
	EndTitle->SetText(Title);
	EndTitle->SetColorAndOpacity(FSlateColor(State == EPSMatchState::Victory ? TextC() : Style->Danger * 1.4f)); // coral sits near the danger red: victory reads white
	EndReason->SetText(Line);
	EndPrompt->SetText(bWin ? LOCTEXT("EndAgain", "PLAY AGAIN") : LOCTEXT("EndRetry", "TRY AGAIN"));
	EndAge = 0.0f;
	BannerQueue.Reset();
	BannerAge = -1.0f;
}

void UPSHUDWidget::HandleCountdown(int32 SecondsLeft)
{
	CountdownText->SetText(SecondsLeft > 0 ? FText::AsNumber(SecondsLeft) : LOCTEXT("Go", "GO"));
	CountdownText->SetColorAndOpacity(FSlateColor(SecondsLeft > 0 ? TextC() : Style->Accent * 1.3f));
	CountdownAge = 0.0f;
}

void UPSHUDWidget::HandlePhase(int32 Index, FText Title, FText Sub)
{
	PhaseLabel->SetText(Title);
	ShowBanner(Title, Sub, Index >= 2, Style->PhaseBannerSeconds);
}

void UPSHUDWidget::HandleBagEmptied(int32 Count, FVector Location)
{
	BagsPop = 1.0f;
	Popup(LOCTEXT("PlusOne", "+1"), FVector2D(Style->Margin + 120.0f, Style->Margin + 52.0f), Style->Accent * 1.3f);
}

void UPSHUDWidget::HandleFound(int32 Strikes, FVector Location, bool bProbe)
{
	VignetteStrength = 1.0f;
	StrikePop = 1.0f;
	const IPSMatchInfo* M = Match();
	const int32 Max = M && M->PS_Config() ? M->PS_Config()->StrikesToLose : 3;
	if (Strikes < Max)
	{
		ShowBanner(bProbe ? LOCTEXT("StickFound", "THE STICK FOUND SAND") : LOCTEXT("PatchFound", "HE FOUND A PATCH"),
			FText::Format(LOCTEXT("StrikeSub", "{0} of {1}. One more and they search the cells."), Strikes, Max), true, Style->EventBannerSeconds + 0.8f);
	}
}

void UPSHUDWidget::HandleNotice(FText Title, FText Sub, bool bDanger)
{
	ShowBanner(Title, Sub, bDanger, Style->EventBannerSeconds + (Sub.IsEmpty() ? 0.0f : 0.8f));
}

void UPSHUDWidget::HandlePrompt(FText Text, FVector WorldLocation, bool bDanger)
{
	FPrompt* Best = &Prompts[0];
	for (FPrompt& P : Prompts)
	{
		if (P.Age > Best->Age)
		{
			Best = &P;
		}
	}
	for (FPrompt& P : Prompts)
	{
		P.Age = 100.0f; // one prompt at a time: the newest replaces any other
	}
	Best->Text->SetText(Text);
	Best->Rule->SetColorAndOpacity(bDanger ? Style->Danger : Style->Accent);
	Best->World = WorldLocation + FVector(0.0f, 0.0f, 230.0f);
	Best->Age = 0.0f;
}

void UPSHUDWidget::HandleInput(FName KeyId, bool bSimulated)
{
	for (FLegendRow& R : Legend)
	{
		if (R.Key == KeyId)
		{
			R.Lit = 1.0f;
		}
	}
	ActiveAge = 0.0f;
}

void UPSHUDWidget::HandleRegion(FName RegionId, float Fraction, FVector Location)
{
	for (FWorldTag& T : Tags)
	{
		if (T.Id == RegionId)
		{
			T.Fraction = Fraction;
		}
	}
}

void UPSHUDWidget::HandleSuspicion(float Value, float Delta)
{
	const IPSMatchInfo* M = Match();
	const float At = M && M->PS_Config() ? M->PS_Config()->VignetteAt : 0.6f;
	if (Value >= At && Delta > 0.0f)
	{
		VignetteStrength = FMath::Max(VignetteStrength, FMath::Lerp(0.25f, 0.7f, (Value - At) / FMath::Max(1.0f - At, 0.01f)));
	}
}

// ---------------------------------------------------------------------------------------------- tick

bool UPSHUDWidget::Project(const FVector& World, FVector2D& Out) const
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return false;
	}
	FVector2D Screen;
	if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, World, Screen, false))
	{
		return false;
	}
	Out = Screen;
	return true;
}

void UPSHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!Style || !Root)
	{
		return;
	}
	const float Dt = FMath::Min(InDeltaTime, 0.1f);
	UpdateReadouts(Dt);
	UpdateBanner(Dt);
	UpdatePrompts(Dt);
	UpdateTags(Dt);
	UpdateFeedback(Dt);
	UpdateEnd(Dt);
}

void UPSHUDWidget::UpdateReadouts(float Dt)
{
	IPSMatchInfo* M = Match();
	if (!M || !M->PS_Config())
	{
		return;
	}
	const UPSRoundConfig* Cfg = M->PS_Config();
	const int32 Gone = M->PS_BagsGone();
	ShownBags = FMath::FInterpTo(ShownBags, static_cast<float>(Gone), Dt, Style->CounterTweenSpeed);
	BagsText->SetText(FText::AsNumber(FMath::RoundToInt(ShownBags)));
	BagsPop = FMath::Max(0.0f, BagsPop - Dt * 2.5f);
	ScaleWidget(BagsText, 1.0f + 0.3f * BagsPop * BagsPop);
	SetLine(Progress, ShownBags / FMath::Max(1, Cfg->BagsTotal), Style->Accent);

	const float Left = M->PS_State() == EPSMatchState::Ready ? Cfg->RoundSeconds : M->PS_TimeLeft();
	ClockText->SetText(Clock(Left));
	if (M->PS_State() == EPSMatchState::Active && Left <= Cfg->LastSecondsWarning)
	{
		ClockPulse += Dt;
		ClockText->SetColorAndOpacity(FSlateColor(FLinearColor::LerpUsingHSV(TextC(), Style->Danger * 1.4f, 0.5f + 0.5f * FMath::Sin(ClockPulse * 6.0f))));
	}
	else
	{
		ClockText->SetColorAndOpacity(FSlateColor(TextC()));
	}
	const int32 Strikes = M->PS_Strikes();
	StrikePop = FMath::Max(0.0f, StrikePop - Dt * 2.0f);
	for (int32 I = 0; I < StrikeMarks.Num(); ++I)
	{
		const bool bOn = I < Strikes;
		StrikeMarks[I]->SetColorAndOpacity(bOn ? Style->Danger * 1.3f : Style->Track);
		FWidgetTransform T;
		T.Angle = 45.0f;
		T.Scale = FVector2D(bOn && I == Strikes - 1 ? 1.0f + 0.6f * StrikePop : 1.0f);
		StrikeMarks[I]->SetRenderTransform(T);
	}
	if (M->PS_State() == EPSMatchState::Ready)
	{
		PhaseLabel->SetText(LOCTEXT("YardTime", "YARD TIME"));
	}

	// Trousers: one hairline per bag, draining as you pour; the WADDLING chip when both are full.
	const IPSSandCarrier* C = Carrier();
	const int32 Carried = C ? C->PS_Carried() : 0;
	for (int32 I = 0; I < 2; ++I)
	{
		float A = 0.0f;
		if (I < Carried)
		{
			A = (I == Carried - 1) ? C->PS_CurrentBag() : 1.0f;
		}
		SetLine(BagLines[I], A, Style->Accent);
	}
	const bool bWaddle = C && C->PS_IsWaddling();
	WaddleChip->SetRenderOpacity(FMath::FInterpConstantTo(WaddleChip->GetRenderOpacity(), bWaddle && EndAge < 0.0f ? 1.0f : 0.0f, Dt, 6.0f));
	TrouserLabel->SetText(Carried == 0 ? LOCTEXT("TrousersEmpty", "TROUSERS EMPTY") : LOCTEXT("Trousers", "IN YOUR TROUSERS"));

	// Legend: lit rows fade back; the whole list dims when nothing has been pressed for a while.
	ActiveAge += Dt;
	const float LegendOpacity = ActiveAge > Style->LegendDimAfter ? FMath::Lerp(1.0f, Style->LegendDimOpacity, FMath::Clamp((ActiveAge - Style->LegendDimAfter) / 0.5f, 0.0f, 1.0f)) : 1.0f;
	if (LegendBox)
	{
		LegendBox->SetRenderOpacity(LegendOpacity);
	}
}

void UPSHUDWidget::UpdateBanner(float Dt)
{
	if (BannerAge < 0.0f && BannerQueue.Num() > 0)
	{
		CurrentBanner = BannerQueue[0];
		BannerQueue.RemoveAt(0);
		BannerAge = 0.0f;
		BannerTitle->SetText(CurrentBanner.Title);
		BannerTitle->SetColorAndOpacity(FSlateColor(CurrentBanner.bDanger ? Style->Danger * 1.4f : TextC()));
		BannerRule->SetColorAndOpacity(CurrentBanner.bDanger ? Style->Danger : Style->Accent);
		BannerSub->SetText(CurrentBanner.Sub);
		BannerSub->SetVisibility(CurrentBanner.Sub.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	float Alpha = 0.0f;
	if (BannerAge >= 0.0f)
	{
		BannerAge += Dt;
		const float Fade = Style->FadeSeconds;
		const float D = CurrentBanner.Duration;
		Alpha = BannerAge < Fade ? EaseOut(BannerAge / Fade) : (BannerAge > D ? 1.0f - EaseOut((BannerAge - D) / Fade) : 1.0f);
		if (BannerAge > D + Fade)
		{
			BannerAge = -1.0f;
			Alpha = 0.0f;
		}
		FWidgetTransform T;
		T.Translation = FVector2D(0.0f, (1.0f - Alpha) * 14.0f);
		BannerBox->SetRenderTransform(T);
	}
	BannerBox->SetRenderOpacity(Alpha);
	for (UImage* Img : BannerBand)
	{
		Img->SetRenderOpacity(Alpha);
	}
	CountdownAge += Dt;
	const float C = CountdownAge < 0.85f ? (CountdownAge < 0.12f ? EaseOut(CountdownAge / 0.12f) : 1.0f - FMath::Clamp((CountdownAge - 0.6f) / 0.25f, 0.0f, 1.0f)) : 0.0f;
	CountdownText->SetRenderOpacity(C);
	ScaleWidget(CountdownText, 1.0f + 0.25f * (1.0f - FMath::Min(CountdownAge / 0.2f, 1.0f)));
}

void UPSHUDWidget::UpdatePrompts(float Dt)
{
	const bool bBanner = BannerAge >= 0.0f;
	const FVector2D View = ViewSize();
	for (FPrompt& P : Prompts)
	{
		if (!bBanner)
		{
			P.Age += Dt;
		}
		const float D = Style->PromptSeconds;
		float A = P.Age < Style->FadeSeconds ? EaseOut(P.Age / Style->FadeSeconds) : (P.Age > D ? 1.0f - EaseOut((P.Age - D) / Style->FadeSeconds) : 1.0f);
		if (bBanner || P.Age > D + Style->FadeSeconds || EndAge >= 0.0f)
		{
			A = 0.0f;
		}
		FVector2D S2;
		if (A > 0.0f && Project(P.World, S2))
		{
			// Keep the whole (centred) prompt on screen: half its measured width plus the corner margin.
			const float Half = FMath::Min(P.Box->GetDesiredSize().X * 0.5f + Style->Margin, View.X * 0.5f);
			S2.X = FMath::Clamp(S2.X, Half, View.X - Half);
			// Off-screen targets clamp below the corner blocks, never into them (centre band of the frame).
			S2.Y = FMath::Clamp(S2.Y, FMath::Max(Style->TopScrimHeight + 30.0f, View.Y * Style->PromptMinYFraction), View.Y - Style->BottomScrimHeight);
			// Never on top of the SEEN warning over the hero's head: lift the prompt clear of it.
			const APawn* HeroPawn = GetOwningPlayer() ? GetOwningPlayer()->GetPawn() : nullptr;
			FVector2D Head;
			if (HeroPawn && Project(HeroPawn->GetActorLocation() + FVector(0.0f, 0.0f, 150.0f), Head)
				&& FMath::Abs(S2.X - Head.X) < Half + 60.0f && FMath::Abs(S2.Y - Head.Y) < 70.0f)
			{
				S2.Y = Head.Y - 75.0f;
			}
			if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(P.Box->Slot))
			{
				S->SetPosition(S2);
			}
			P.Box->SetRenderOpacity(A);
		}
		else
		{
			P.Box->SetRenderOpacity(0.0f);
		}
	}
	for (FPopup& P : Popups)
	{
		P.Age += Dt;
		const float T = P.Age / FMath::Max(Style->PopupSeconds, 0.01f);
		if (T >= 1.0f)
		{
			P.Text->SetRenderOpacity(0.0f);
			continue;
		}
		if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(P.Text->Slot))
		{
			S->SetPosition(P.Anchor - FVector2D(0.0f, EaseOut(T) * Style->PopupRise));
		}
		P.Text->SetRenderOpacity(T < 0.15f ? T / 0.15f : 1.0f - FMath::Max(0.0f, (T - 0.6f) / 0.4f));
	}
}

void UPSHUDWidget::UpdateTags(float Dt)
{
	const IPSMatchInfo* M = Match();
	if (!M || !M->PS_Config())
	{
		return;
	}
	const UPSRoundConfig* Cfg = M->PS_Config();
	const EPSMatchState St = M->PS_State();
	const bool bPlay = St == EPSMatchState::Ready || St == EPSMatchState::Active;
	if (St == EPSMatchState::Active)
	{
		PlayAge += Dt;
	}
	const float Loud = PlayAge < Style->TagsLoudSeconds ? 1.0f : Style->TagsQuietOpacity;
	const FVector2D View = ViewSize();
	const APlayerController* PC = GetOwningPlayer();
	const APawn* Hero = PC ? PC->GetPawn() : nullptr;
	// Tags give way to a live prompt (bigger type, 2026-11-12) and never stack on another tag (two puddles side by side).
	TArray<FBox2D> Busy;
	for (const FPrompt& Pr : Prompts)
	{
		if (Pr.Box && Pr.Box->GetRenderOpacity() > 0.01f)
		{
			if (const UCanvasPanelSlot* PS = Cast<UCanvasPanelSlot>(Pr.Box->Slot))
			{
				const FVector2D Sz = Pr.Box->GetDesiredSize();
				const FVector2D C = PS->GetPosition();
				Busy.Add(FBox2D(C - FVector2D(Sz.X * 0.5f + 40.0f, Sz.Y + 30.0f), C + FVector2D(Sz.X * 0.5f + 40.0f, Sz.Y + 30.0f)));
			}
		}
	}
	for (FWorldTag& T : Tags)
	{
		T.Shown = FMath::FInterpTo(T.Shown, T.Fraction, Dt, 6.0f);
		const bool bFull = T.Fraction >= 0.999f;
		T.Text->SetColorAndOpacity(FSlateColor(bFull ? Style->Danger * 1.4f : TextC(0.9f)));
		SetLine(T.Fill, T.Shown, bFull ? Style->Danger * 1.3f : Style->Accent);
		// Louder when you are near it.
		const float Near = Hero ? FMath::Clamp(1.6f - FVector::Dist2D(Hero->GetActorLocation(), T.World) / 1200.0f, 0.0f, 1.0f) : 0.0f;
		const float Want = bPlay && EndAge < 0.0f && BannerAge < 0.0f ? FMath::Max(Loud, Near) : 0.0f; // labels step back while a banner is up
		T.Opacity = FMath::FInterpConstantTo(T.Opacity, Want, Dt, 4.0f);
		FVector2D P;
		// Never inside the top corner blocks (bags / clock): a tag that projects there is hidden.
		const auto InCorner = [&](const FVector2D& Q)
		{
			return (Q.Y < Style->TagCornerExclusion.Y && (Q.X < Style->TagCornerExclusion.X || Q.X > View.X - Style->TagCornerExclusion.X))
				|| (Q.Y > View.Y - Style->TagLegendExclusion.Y && (Q.X < Style->TagLegendExclusion.X || Q.X > View.X - Style->TagLegendExclusion.X));
		};
		const auto Crowded = [&](const FVector2D& Q)
		{
			for (const FBox2D& B : Busy)
			{
				if (B.IsInside(Q)) { return true; }
			}
			return false;
		};
		if (T.Opacity > 0.01f && Project(T.World, P) && P.X > 0.0f && P.Y > 0.0f && P.X < View.X && P.Y < View.Y && !InCorner(P) && !Crowded(P))
		{
			const FVector2D Sz = T.Box->GetDesiredSize();
			Busy.Add(FBox2D(P - FVector2D(Sz.X * 0.5f + 30.0f, Sz.Y + 10.0f), P + FVector2D(Sz.X * 0.5f + 30.0f, Sz.Y + 10.0f)));
			if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(T.Box->Slot))
			{
				S->SetPosition(P);
			}
			T.Box->SetRenderOpacity(T.Opacity);
		}
		else
		{
			T.Box->SetRenderOpacity(0.0f);
		}
	}
	// Door tag: loud when the trousers are empty and bags are waiting.
	const IPSSandCarrier* C = Carrier();
	const bool bDoor = St == EPSMatchState::Active && M->PS_BagsInCell() > 0 && C && C->PS_Carried() < Cfg->MaxCarried && EndAge < 0.0f;
	DoorAlpha = FMath::FInterpConstantTo(DoorAlpha, bDoor ? (C->PS_Carried() == 0 ? 1.0f : 0.45f) : 0.0f, Dt, 5.0f);
	FVector2D DP;
	if (DoorAlpha > 0.01f && Project(FVector(Cfg->DoorLocation.X, Cfg->DoorLocation.Y - 60.0f, 330.0f), DP))
	{
		if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(DoorTag->Slot)) { S->SetPosition(DP); }
		DoorTag->SetRenderOpacity(DoorAlpha);
	}
	else
	{
		DoorTag->SetRenderOpacity(0.0f);
	}
	// Suspicion over the hero, and SHOWS beside the ring.
	const float Susp = C ? C->PS_Suspicion() : 0.0f;
	ShownSuspicion = FMath::FInterpTo(ShownSuspicion, Susp, Dt, Style->CounterTweenSpeed);
	SuspicionAlpha = FMath::FInterpConstantTo(SuspicionAlpha, Susp > 0.02f && bPlay && EndAge < 0.0f ? 1.0f : 0.0f, Dt, 5.0f);
	SetLine(SuspicionLine, ShownSuspicion, Style->Danger * 1.3f);
	SuspicionWord->SetText(Susp > 0.75f ? LOCTEXT("SeenHard", "HE'S STARING") : LOCTEXT("Seen", "SEEN"));
	FVector2D HP;
	if (Hero && SuspicionAlpha > 0.01f && Project(Hero->GetActorLocation() + FVector(0.0f, 0.0f, 150.0f), HP))
	{
		if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(SuspicionBox->Slot)) { S->SetPosition(HP); }
		SuspicionBox->SetRenderOpacity(SuspicionAlpha);
	}
	else
	{
		SuspicionBox->SetRenderOpacity(0.0f);
	}
	const bool bShows = C && C->PS_IsPouring() && C->PS_SandWouldShow() && St == EPSMatchState::Active;
	RingAlpha = FMath::FInterpConstantTo(RingAlpha, bShows ? 1.0f : 0.0f, Dt, 6.0f);
	FVector2D RP;
	if (Hero && RingAlpha > 0.01f && Project(Hero->GetActorLocation() + FVector(0.0f, 0.0f, -60.0f), RP))
	{
		if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(RingTag->Slot)) { S->SetPosition(RP + FVector2D(70.0f, 10.0f)); }
		RingTag->SetRenderOpacity(RingAlpha);
	}
	else
	{
		RingTag->SetRenderOpacity(0.0f);
	}
}

void UPSHUDWidget::UpdateFeedback(float Dt)
{
	VignetteStrength = FMath::Max(0.0f, VignetteStrength - Dt * 1.6f);
	for (UImage* Img : VignetteImages)
	{
		Img->SetRenderOpacity(VignetteStrength);
	}
	for (FLegendRow& R : Legend)
	{
		R.Lit = FMath::Max(0.0f, R.Lit - Dt / FMath::Max(Style->KeyLightSeconds, 0.01f));
		R.Glyph->SetColorAndOpacity(FSlateColor(FLinearColor::LerpUsingHSV(TextC(0.9f), Style->Accent * 1.35f, R.Lit)));
		R.Label->SetColorAndOpacity(FSlateColor(FLinearColor::LerpUsingHSV(TextC(Style->SecondaryOpacity), TextC(), R.Lit)));
	}
}

void UPSHUDWidget::UpdateEnd(float Dt)
{
	float A = 0.0f;
	if (EndAge >= 0.0f)
	{
		EndAge += Dt;
		A = EaseOut(EndAge / 0.4f);
	}
	Dim->SetRenderOpacity(A);
	EndBox->SetRenderOpacity(A);
	for (UWidget* W : PlayCorners)
	{
		if (W) { W->SetRenderOpacity(1.0f - A); }
	}
	if (EndPanel)
	{
		EndPanel->SetVisibility(A > 0.01f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden); // outlines ignore opacity
		EndPanel->SetRenderOpacity(A);
		FWidgetTransform PT;
		PT.Translation = FVector2D((1.0f - A) * 30.0f, 0.0f);
		EndPanel->SetRenderTransform(PT);
	}
	for (UImage* Img : EndBand)
	{
		Img->SetRenderOpacity(A);
	}
	FWidgetTransform T;
	T.Translation = FVector2D(0.0f, (1.0f - A) * 16.0f);
	EndBox->SetRenderTransform(T);
}

#undef LOCTEXT_NAMESPACE
