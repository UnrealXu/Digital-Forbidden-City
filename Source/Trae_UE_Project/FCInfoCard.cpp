// -*- coding: utf-8 -*-
#include "FCInfoCard.h"

#include "FCData.h"
#include "FCFont.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"

namespace
{
	const FLinearColor Gold(0.98f, 0.78f, 0.32f, 1.f);
	const FLinearColor Ink(0.96f, 0.93f, 0.86f, 1.f);
	const FLinearColor Muted(0.80f, 0.76f, 0.66f, 1.f);

	// 按钮本身不绘制底色：三态均为 FSlateNoResource（NoDrawType，SButton 跳过
	// MakeBox，不会画成白块；点击响应不受影响）。深色半透明底由外层 UBorder 承担。
	FButtonStyle MakeFlatButtonStyle()
	{
		FButtonStyle Style;
		Style.SetNormal(FSlateNoResource());
		Style.SetHovered(FSlateNoResource());
		Style.SetPressed(FSlateNoResource());
		return Style;
	}

	UTextBlock* NewText(class UWidgetTree* Tree, const FText& Content, int32 Size, const FLinearColor& Color, bool bBold = false)
	{
		UTextBlock* T = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		T->SetText(Content);
		T->SetColorAndOpacity(FSlateColor(Color));
		T->SetFont(FcMakeSlateFont(Size, bBold));
		return T;
	}
}

void UFCInfoCard::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = Root;

	// ---- POI info card (right side, below the right-side description panel) ----
	CardBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	CardBorder->SetPadding(FMargin(16.f, 14.f));
	FSlateBrush CardBrush;
	CardBrush.TintColor = FSlateColor(FLinearColor(0.02f, 0.025f, 0.04f, 0.92f));
	CardBorder->SetBrush(CardBrush);

	UHorizontalBox* TitleRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

	TitleText = NewText(WidgetTree, FText::GetEmpty(), 26, Gold, true);
	UHorizontalBoxSlot* TitleSlot = TitleRow->AddChildToHorizontalBox(TitleText);
	TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	TitleSlot->SetVerticalAlignment(VAlign_Center);
	TitleSlot->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));

	UButton* CloseBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	UTextBlock* CloseLabel = NewText(WidgetTree, FText::FromString(TEXT("×")), 26, Ink);
	CloseBtn->SetContent(CloseLabel);
	CloseBtn->OnClicked.AddDynamic(this, &UFCInfoCard::OnCloseClicked);
	TitleRow->AddChildToHorizontalBox(CloseBtn)->SetVerticalAlignment(VAlign_Top);

	ZoneText = NewText(WidgetTree, FText::GetEmpty(), 17, Gold);
	AliasesText = NewText(WidgetTree, FText::GetEmpty(), 15, Muted);
	TagsText = NewText(WidgetTree, FText::GetEmpty(), 15, Muted);
	BodyText = NewText(WidgetTree, FText::GetEmpty(), 17, Ink);
	AliasesText->SetWrapTextAt(300.f);
	AliasesText->SetAutoWrapText(false);
	TagsText->SetWrapTextAt(300.f);
	TagsText->SetAutoWrapText(false);
	BodyText->SetWrapTextAt(300.f);
	BodyText->SetAutoWrapText(false);

	// 太和殿卡片专用：拆解 / 复位 按钮（其它 POI 卡片中保持 Collapsed）。
	ExplodeBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	ExplodeBtn->SetStyle(MakeFlatButtonStyle());
	ExplodeLabel = NewText(WidgetTree, FText::FromString(TEXT("拆解建筑")), 16, Gold);
	UBorder* ExplodePad = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	ExplodePad->SetPadding(FMargin(14.f, 8.f, 14.f, 8.f));
	FSlateBrush ExplodeBrush;
	ExplodeBrush.TintColor = FSlateColor(FLinearColor(0.05f, 0.04f, 0.02f, 0.75f));
	ExplodePad->SetBrush(ExplodeBrush);
	ExplodePad->SetContent(ExplodeLabel);
	ExplodeBtn->SetContent(ExplodePad);
	ExplodeBtn->OnClicked.AddDynamic(this, &UFCInfoCard::OnExplodeClicked);
	ExplodeBtn->SetVisibility(ESlateVisibility::Collapsed);

	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UVerticalBoxSlot* TS = VBox->AddChildToVerticalBox(TitleRow);
	TS->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
	VBox->AddChildToVerticalBox(ZoneText)->SetPadding(FMargin(0.f, 2.f));
	VBox->AddChildToVerticalBox(AliasesText)->SetPadding(FMargin(0.f, 6.f));
	VBox->AddChildToVerticalBox(TagsText)->SetPadding(FMargin(0.f, 6.f));
	VBox->AddChildToVerticalBox(BodyText)->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
	VBox->AddChildToVerticalBox(ExplodeBtn)->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
	CardBorder->SetContent(VBox);

	UCanvasPanelSlot* CardSlot = Root->AddChildToCanvas(CardBorder);
	CardSlot->SetAnchors(FAnchors(1.f, 0.f));
	CardSlot->SetAlignment(FVector2D(1.f, 0.f));
	CardSlot->SetPosition(FVector2D(-260.f, 120.f));
	CardSlot->SetSize(FVector2D(340.f, 0.f));
	CardSlot->SetAutoSize(true);

	CardBorder->SetVisibility(ESlateVisibility::Hidden);

	UE_LOG(LogTemp, Warning, TEXT("[FC] InfoCard NativeOnInitialized built Root=%s Card=%s Explode=%s"),
		Root ? TEXT("OK") : TEXT("NULL"),
		CardBorder ? TEXT("OK") : TEXT("NULL"),
		ExplodeBtn ? TEXT("OK") : TEXT("NULL"));
}

void UFCInfoCard::ShowPoi(int32 PoiIndex)
{
	if (PoiIndex < 0 || PoiIndex >= G_FCPoiCount)
	{
		return;
	}
	const FFCPoi& P = G_FCPois[PoiIndex];

	TitleText->SetText(FText::FromString(P.Name));
	ZoneText->SetText(FText::FromString(P.Zone));

	const FString AliasStr(P.Aliases);
	AliasesText->SetText(AliasStr.IsEmpty() ? FText::GetEmpty()
		: FText::FromString(FString(TEXT("亦称：")) + AliasStr));
	AliasesText->SetVisibility(AliasStr.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);

	TagsText->SetText(FText::FromString(FString(TEXT("看点：")) + FString(P.Tags)));
	BodyText->SetText(FText::FromString(P.Info));

	// 只有太和殿（PoiIndex==2）提供拆解/复位入口；同时复位拆解状态与文案。
	if (PoiIndex == 2)
	{
		bExploded = false;
		ExplodeLabel->SetText(FText::FromString(TEXT("拆解建筑")));
		ExplodeBtn->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		ExplodeBtn->SetVisibility(ESlateVisibility::Collapsed);
	}

	CardBorder->SetVisibility(ESlateVisibility::Visible);
	UE_LOG(LogTemp, Warning, TEXT("[FC] ShowPoi idx=%d name=%s"), PoiIndex, P.Name);
}

void UFCInfoCard::HideCard()
{
	if (CardBorder)
	{
		CardBorder->SetVisibility(ESlateVisibility::Hidden);
	}
}

void UFCInfoCard::OnCloseClicked()
{
	HideCard();
	OnCardClosed.Broadcast();
}

void UFCInfoCard::OnExplodeClicked()
{
	// 仅切换拆解状态与按钮文案并广播，信息卡保持显示。
	bExploded = !bExploded;
	ExplodeLabel->SetText(FText::FromString(bExploded ? TEXT("复位组装") : TEXT("拆解建筑")));
	OnExplodeToggle.Broadcast();
}
