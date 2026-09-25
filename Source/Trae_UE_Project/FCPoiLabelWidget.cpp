// -*- coding: utf-8 -*-
#include "FCPoiLabelWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "FCFont.h"

void UFCPoiLabelWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 双层 UBorder：外层 1.5px 青色描边感，内层深蓝黑半透明底。
	UBorder* OuterBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("OuterBorder"));
	OuterBorder->SetPadding(FMargin(1.f));
	FSlateBrush OuterBrush;
	OuterBrush.TintColor = FSlateColor(FLinearColor(0.55f, 0.78f, 0.82f, 0.85f));
	OuterBorder->SetBrush(OuterBrush);
	OuterBorder->SetHorizontalAlignment(HAlign_Center);
	OuterBorder->SetVerticalAlignment(VAlign_Center);
	WidgetTree->RootWidget = OuterBorder;

	UBorder* InnerBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InnerBorder"));
	InnerBorder->SetPadding(FMargin(10.f, 4.f, 12.f, 4.f));
	FSlateBrush InnerBrush;
	InnerBrush.TintColor = FSlateColor(FLinearColor(0.03f, 0.06f, 0.09f, 0.78f));
	InnerBorder->SetBrush(InnerBrush);
	InnerBorder->SetHorizontalAlignment(HAlign_Center);
	InnerBorder->SetVerticalAlignment(VAlign_Center);
	OuterBorder->SetContent(InnerBorder);

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Row"));
	InnerBorder->SetContent(Row);

	// 1) 青色菱形标记。
	UTextBlock* DiamondText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DiamondText"));
	DiamondText->SetText(FText::FromString(TEXT("◆ ")));
	DiamondText->SetAutoWrapText(false);
	DiamondText->SetColorAndOpacity(FSlateColor(FLinearColor(0.62f, 0.85f, 0.88f, 1.f)));
	DiamondText->SetFont(FcMakeSlateFont(30, true));
	DiamondText->SetShadowOffset(FVector2D::ZeroVector);
	UHorizontalBoxSlot* DiamondSlot = Row->AddChildToHorizontalBox(DiamondText);
	DiamondSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	DiamondSlot->SetVerticalAlignment(VAlign_Center);

	// 2) 金色 POI 名称（保留 LabelText 成员与 SetLabelText 接口）。
	LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LabelText"));
	LabelText->SetJustification(ETextJustify::Left);
	LabelText->SetAutoWrapText(false);
	LabelText->SetColorAndOpacity(FSlateColor(FLinearColor(0.98f, 0.80f, 0.35f, 1.f)));
	LabelText->SetFont(FcMakeSlateFont(34, false));
	// 有了深色底框，去掉文字阴影。
	LabelText->SetShadowOffset(FVector2D::ZeroVector);
	UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(LabelText);
	LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	LabelSlot->SetVerticalAlignment(VAlign_Center);
}

void UFCPoiLabelWidget::SetLabelText(const FText& InText)
{
	if (LabelText)
	{
		LabelText->SetText(InText);
	}
}
