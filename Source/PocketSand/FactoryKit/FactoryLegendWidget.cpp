#include "FactoryLegendWidget.h"
#include "Components/Border.h"
#include "Components/HorizontalBoxSlot.h"
#include "FactoryKitStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"

TSharedRef<SWidget> UFactoryLegendWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildTree();
	}
	return Super::RebuildWidget();
}

void UFactoryLegendWidget::BuildTree()
{
	const UFactoryKitSettings* S = UFactoryKitSettings::Get();
	Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	List = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("LegendList"));
	UCanvasPanelSlot* LS = Root->AddChildToCanvas(List);
	LS->SetAutoSize(true);
	SetMenuPlacement(bMenuPlacement);

	FlashText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("KeyFlash"));
	FactoryKitStyle::Apply(FlashText, S->TitleTypeface, S->SmallSize, S->AccentColor, 80);
	UCanvasPanelSlot* FS = Root->AddChildToCanvas(FlashText);
	FS->SetAnchors(FAnchors(0.0f, 1.0f));
	FS->SetAlignment(FVector2D(0.0f, 1.0f));
	FS->SetPosition(FVector2D(S->LegendMargin.X, -S->LegendMargin.Y));
	FS->SetAutoSize(true);
	FlashText->SetRenderOpacity(0.0f);

	Rows.Reset();
	UHorizontalBox* Strip = nullptr;
	if (S->bLegendBottomRow)
	{
		Strip = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("LegendStrip"));
		// A dark band behind the row so the labels read on bright gravel (reference: control bar on a dark strip).
		UBorder* Band = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("LegendBand"));
		Band->SetBrush(FactoryKitStyle::BoxBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.62f), FLinearColor(1.0f, 1.0f, 1.0f, 0.08f), 6.0f, 1.0f));
		Band->SetPadding(FMargin(26.0f, 10.0f, 0.0f, 10.0f));
		Band->SetContent(Strip);
		List->AddChildToVerticalBox(Band);
	}
	if (!S->LegendTitle.IsEmpty() && !Strip)
	{
		UTextBlock* Head = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LegendTitle"));
		FactoryKitStyle::Apply(Head, S->TitleTypeface, S->SmallSize, S->AccentColor, 120);
		Head->SetText(S->LegendTitle);
		HeadText = Head;
		if (UVerticalBoxSlot* HS0 = List->AddChildToVerticalBox(Head)) { HS0->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f)); }
	}
	for (const FFactoryLegendRow& R : Pending)
	{
		UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		USizeBox* KeyBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		KeyBox->SetMinDesiredWidth(S->bLegendBottomRow ? 0.0f : S->SmallSize * 6.0f); // key column fits "MOUSE" / "SPACE" (PlazaDuty lesson)
		UTextBlock* K = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FactoryKitStyle::Apply(K, S->TitleTypeface, S->SmallSize, S->TextColor, 80);
		K->SetText(R.KeyText);
		// Keycap chip (reference: outlined key boxes before each action).
		UBorder* Cap = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Cap->SetBrush(FactoryKitStyle::BoxBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.45f), S->AccentColor * FLinearColor(1, 1, 1, 0.9f), 4.0f, 1.5f));
		Cap->SetPadding(FMargin(8.0f, 2.0f, 8.0f, 3.0f));
		Cap->SetHorizontalAlignment(HAlign_Center);
		Cap->SetContent(K);
		K->SetJustification(ETextJustify::Center);
		KeyBox->AddChild(Cap);
		UHorizontalBoxSlot* KS = H->AddChildToHorizontalBox(KeyBox);
		KS->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 0.0f));
		KS->SetVerticalAlignment(VAlign_Center);
		UTextBlock* L = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FactoryKitStyle::Apply(L, S->BodyTypeface, S->SmallSize, S->DimColor, 60);
		L->SetText(R.Label);
		H->AddChildToHorizontalBox(L)->SetVerticalAlignment(VAlign_Center);
		if (Strip)
		{
			Strip->AddChildToHorizontalBox(H)->SetPadding(FMargin(0.0f, 0.0f, 34.0f, 0.0f));
		}
		else
		{
			UVerticalBoxSlot* VS = List->AddChildToVerticalBox(H);
			VS->SetPadding(FMargin(0.0f, 3.0f));
		}
		Rows.Add({ R, K, L, 0.0f });
	}

	Hint = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("AutoplayHint"));
	FactoryKitStyle::Apply(Hint, S->BodyTypeface, S->SmallSize, S->DimColor, 60);
	Hint->SetText(S->AutoplayHintText);
	UCanvasPanelSlot* HS = Root->AddChildToCanvas(Hint);
	HS->SetAnchors(FAnchors(1.0f, 1.0f));
	HS->SetAlignment(FVector2D(1.0f, 1.0f));
	HS->SetPosition(FVector2D(-S->LegendMargin.X, -S->LegendMargin.Y));
	HS->SetAutoSize(true);
	Hint->SetRenderOpacity(0.0f);
}

void UFactoryLegendWidget::SetRows(const TArray<FFactoryLegendRow>& InRows)
{
	Pending = InRows; // call before AddToViewport (the flow subsystem does)
}

void UFactoryLegendWidget::Pulse(FName ActionId)
{
	for (FRowWidgets& R : Rows)
	{
		if (R.Data.ActionId == ActionId) { R.Glow = 1.0f; R.Flash = 1.0f; }
	}
}

void UFactoryLegendWidget::PulseKey(const FKey& Key)
{
	for (FRowWidgets& R : Rows)
	{
		if (R.Data.Keys.Contains(Key.GetFName())) { R.Glow = 1.0f; R.Flash = 1.0f; }
	}
}

void UFactoryLegendWidget::SetAutoplayHint(bool bVisible)
{
	if (bVisible && !bHint) { HintTime = 0.0f; }
	bHint = bVisible;
}

void UFactoryLegendWidget::SetLegendVisible(bool bVisible)
{
	bLegendVisible = bVisible;
}

void UFactoryLegendWidget::SetMenuPlacement(bool bMenu)
{
	bMenuPlacement = bMenu;
	UCanvasPanelSlot* LS = List ? Cast<UCanvasPanelSlot>(List->Slot) : nullptr;
	if (!LS)
	{
		return;
	}
	const UFactoryKitSettings* S = UFactoryKitSettings::Get();
	if (bMenu && S->bLegendBottomRow)
	{
		// One centred row along the bottom, above the ENTER / ESC hint line.
		LS->SetAnchors(FAnchors(0.5f, 1.0f));
		LS->SetAlignment(FVector2D(0.5f, 1.0f));
		LS->SetPosition(FVector2D(0.0f, -S->LegendMargin.Y - S->SmallSize * 2.4f));
		return;
	}
	LS->SetAnchors(FAnchors(bMenu ? 1.0f : 0.0f, 1.0f));
	LS->SetAlignment(FVector2D(bMenu ? 1.0f : 0.0f, 1.0f));
	LS->SetPosition(bMenu ? FVector2D(-S->LegendMargin.X, -S->LegendMargin.Y - S->MenuLegendLift) : FVector2D(S->LegendMargin.X, -S->LegendMargin.Y));
}

void UFactoryLegendWidget::SetFlashEnabled(bool bEnabled)
{
	bFlash = bEnabled;
	if (!bFlash)
	{
		for (FRowWidgets& R : Rows) { R.Flash = 0.0f; }
	}
}

void UFactoryLegendWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const UFactoryKitSettings* S = UFactoryKitSettings::Get();
	LegendAlpha = FactoryKitStyle::EaseTo(LegendAlpha, bLegendVisible ? 1.0f : 0.0f, InDeltaTime, S->MotionSeconds);
	for (FRowWidgets& R : Rows)
	{
		R.Glow = FMath::Max(0.0f, R.Glow - InDeltaTime / FMath::Max(S->LegendPulseSeconds, 0.05f));
		const float G = FMath::InterpEaseOut(0.0f, 1.0f, R.Glow, 2.0f);
		R.Key->SetColorAndOpacity(FSlateColor(FMath::Lerp(S->TextColor, S->AccentColor, G)));
		R.Label->SetColorAndOpacity(FSlateColor(S->TextColor));
		R.Key->SetRenderOpacity(LegendAlpha);
		R.Label->SetRenderOpacity(LegendAlpha);
	}
	if (HeadText) { HeadText->SetRenderOpacity(LegendAlpha); }
	if (List)
	{
		// Rounded-box outlines ignore render opacity: hide the list outright once it has faded (keycaps showed empty in play).
		List->SetRenderOpacity(LegendAlpha);
		List->SetVisibility(LegendAlpha > 0.01f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	if (FlashText)
	{
		// Pressed keys, newest first, each fading over KeyFlashSeconds.
		FString Keys;
		float Alpha = 0.0f;
		for (FRowWidgets& R : Rows)
		{
			R.Flash = FMath::Max(0.0f, R.Flash - InDeltaTime / FMath::Max(S->KeyFlashSeconds, 0.05f));
			if (bFlash && R.Flash > 0.02f)
			{
				Keys += (Keys.IsEmpty() ? TEXT("") : TEXT("    ")) + R.Data.KeyText.ToString();
				Alpha = FMath::Max(Alpha, FMath::InterpEaseOut(0.0f, 1.0f, R.Flash, 2.0f));
			}
		}
		if (!Keys.IsEmpty() && Keys != LastFlash)
		{
			FlashText->SetText(FText::FromString(Keys));
			LastFlash = Keys;
		}
		FlashText->SetRenderOpacity(Alpha);
	}
	if (Hint)
	{
		HintTime += InDeltaTime;
		const float Target = !bHint ? 0.0f : (HintTime < S->AutoplayHintFullSeconds ? 1.0f : S->AutoplayHintDimAlpha);
		HintAlpha = FactoryKitStyle::EaseTo(HintAlpha, Target, InDeltaTime, S->MotionSeconds);
		Hint->SetRenderOpacity(HintAlpha);
	}
}
