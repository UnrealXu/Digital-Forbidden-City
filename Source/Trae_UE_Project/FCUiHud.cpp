// -*- coding: utf-8 -*-
#include "FCUiHud.h"

#include "FCData.h"
#include "FCFont.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/ComboBoxString.h"
#include "Components/Slider.h"
#include "Components/SizeBox.h"

#include "Kismet/GameplayStatics.h"
#include "FCSkyDirector.h"

#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"

namespace
{
	// ---- 统一配色 ----
	const FLinearColor HudPanelBg(0.03f, 0.06f, 0.09f, 0.88f);
	const FLinearColor HudBarBg(0.04f, 0.08f, 0.11f, 0.92f);
	const FLinearColor HudBarBgBottom(0.04f, 0.08f, 0.11f, 0.90f);
	const FLinearColor Gold(0.98f, 0.80f, 0.35f, 1.f);
	const FLinearColor CyanText(0.60f, 0.78f, 0.82f, 1.f);
	const FLinearColor BodyText(0.80f, 0.83f, 0.86f, 1.f);
	const FLinearColor TitleLight(0.95f, 0.94f, 0.90f, 1.f);
	const FLinearColor BtnTextLight(0.82f, 0.85f, 0.88f, 1.f);
	const FLinearColor MutedText(0.62f, 0.68f, 0.72f, 1.f);

	// 目录行配色
	const FLinearColor RowNormal(1.f, 1.f, 1.f, 0.f);                          // 默认透明
	const FLinearColor RowHover(0.15f, 0.28f, 0.33f, 0.55f);                   // 悬停
	const FLinearColor RowPressed(0.22f, 0.38f, 0.44f, 0.75f);                 // 按下
	const FLinearColor RowSelected(0.10f, 0.22f, 0.27f, 0.85f);                // 选中底
	const FLinearColor RowNameNormal(0.86f, 0.88f, 0.90f, 1.f);
	const FLinearColor RowNameGold(0.98f, 0.80f, 0.35f, 1.f);

	// 目录行按钮：三态均不绘制，底色全部由 RowBorder 承担（与 FCInfoCard 同款思路）。
	FButtonStyle MakeFlatButtonStyle()
	{
		FButtonStyle Style;
		Style.SetNormal(FSlateNoResource());
		Style.SetHovered(FSlateNoResource());
		Style.SetPressed(FSlateNoResource());
		return Style;
	}

	// 外壳底色：按钮本体三态全透明，底色/描边全部由外层 UBorder 承担
	//（与已验证正常的目录行、右侧面板同款渲染路径，最稳妥）。
	struct FShellStyle
	{
		FLinearColor Normal;
		FLinearColor Hover;
		FLinearColor Pressed;
	};

	const FShellStyle& ControlButtonShell()
	{
		static const FShellStyle Shell{
			FLinearColor(0.045f, 0.105f, 0.145f, 1.0f),
			FLinearColor(0.16f, 0.34f, 0.41f, 0.96f),
			FLinearColor(0.22f, 0.44f, 0.52f, 0.98f)
		};
		return Shell;
	}

	UTextBlock* NewText(UWidgetTree* Tree, const FText& Content, int32 Size, const FLinearColor& Color, bool bBold = false)
	{
		UTextBlock* T = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		T->SetText(Content);
		T->SetColorAndOpacity(FSlateColor(Color));
		T->SetFont(FcMakeSlateFont(Size, bBold));
		return T;
	}

	UBorder* MakeBorder(UWidgetTree* Tree, const FLinearColor& BgColor)
	{
		UBorder* B = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
		FSlateBrush Brush;
		Brush.TintColor = FSlateColor(BgColor);
		B->SetBrush(Brush);
		return B;
	}

	// “描边层 Border(青色, Padding=1) -> 填充层 Border(深色) -> 文字”。
	// 不依赖 OutlineSettings（无资源 brush 在 5.8 填充渲染异常），
	// 两层均走已验证正常的纯色 TintColor 路径。
	UButton* MakeLabeledButton(UWidgetTree* Tree, const FString& Label, int32 FontSize,
		const FLinearColor& LabelColor, const FMargin& LabelPadding,
		UTextBlock*& OutLabel)
	{
		UButton* Btn = Tree->ConstructWidget<UButton>(UButton::StaticClass());
		Btn->SetStyle(MakeFlatButtonStyle());

		OutLabel = NewText(Tree, FText::FromString(Label), FontSize, LabelColor);

		UBorder* Fill = MakeBorder(Tree, ControlButtonShell().Normal);
		FMargin Combined = LabelPadding;
		Fill->SetPadding(Combined);
		Fill->SetContent(OutLabel);

		UBorder* Shell = MakeBorder(Tree, FLinearColor(0.42f, 0.78f, 0.86f, 1.0f));
		Shell->SetPadding(FMargin(1.5f));
		Shell->SetContent(Fill);

		Btn->SetContent(Shell);
		return Btn;
	}

}

//============================================================
// UFCCatalogRow
//============================================================
void UFCCatalogRow::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	RowButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	RowButton->SetStyle(MakeFlatButtonStyle());
	RowButton->OnClicked.AddDynamic(this, &UFCCatalogRow::HandleClicked);
	RowButton->OnHovered.AddDynamic(this, &UFCCatalogRow::HandleHovered);
	RowButton->OnUnhovered.AddDynamic(this, &UFCCatalogRow::HandleUnhovered);
	RowButton->OnPressed.AddDynamic(this, &UFCCatalogRow::HandlePressed);
	RowButton->OnReleased.AddDynamic(this, &UFCCatalogRow::HandleReleased);

	RowBorder = MakeBorder(WidgetTree, RowNormal);
	RowBorder->SetPadding(FMargin(14.f, 7.f));

	NameText = NewText(WidgetTree, FText::GetEmpty(), 16, RowNameNormal);
	RowBorder->SetContent(NameText);

	RowButton->SetContent(RowBorder);
	WidgetTree->RootWidget = RowButton;
}

void UFCCatalogRow::InitRow(int32 Index, bool bSelected)
{
	RowIndex = Index;
	SetSelected(bSelected);
}

void UFCCatalogRow::SetSelected(bool bSelected)
{
	bRowSelected = bSelected;

	if (NameText && RowIndex >= 0 && RowIndex < G_FCPoiCount)
	{
		const FString Prefix = bSelected ? TEXT("◆ ") : TEXT("  ");
		NameText->SetText(FText::FromString(Prefix + FString(G_FCPois[RowIndex].Name)));
		NameText->SetColorAndOpacity(FSlateColor(bSelected ? RowNameGold : RowNameNormal));
	}

	RefreshBackground();
}

void UFCCatalogRow::RefreshBackground()
{
	if (!RowBorder)
	{
		return;
	}

	FLinearColor Bg = RowNormal;
	if (bRowPressed)
	{
		Bg = RowPressed;
	}
	else if (bRowSelected)
	{
		Bg = RowSelected;
	}
	else if (bRowHovered)
	{
		Bg = RowHover;
	}

	FSlateBrush Brush;
	Brush.TintColor = FSlateColor(Bg);
	RowBorder->SetBrush(Brush);
}

void UFCCatalogRow::HandleClicked()
{
	OnRowClicked.Broadcast(RowIndex);
}

void UFCCatalogRow::HandleHovered()
{
	bRowHovered = true;
	RefreshBackground();
}

void UFCCatalogRow::HandleUnhovered()
{
	bRowHovered = false;
	bRowPressed = false;
	RefreshBackground();
}

void UFCCatalogRow::HandlePressed()
{
	bRowPressed = true;
	RefreshBackground();
}

void UFCCatalogRow::HandleReleased()
{
	bRowPressed = false;
	RefreshBackground();
}

//============================================================
// UFCCatalogPanel
//============================================================
void UFCCatalogPanel::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = RootCanvas;

	// ---- 面板外框：深蓝黑底 + 10 内边距 ----
	UBorder* PanelBorder = MakeBorder(WidgetTree, HudPanelBg);
	PanelBorder->SetPadding(FMargin(10.f));

	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());

	// 1) 标题
	UTextBlock* TitleText = NewText(WidgetTree,
		FText::FromString(FString::Printf(TEXT("宫殿目录 · %d处"), G_FCPoiCount)),
		18, Gold, true);
	VBox->AddChildToVerticalBox(TitleText)->SetPadding(FMargin(2.f, 2.f, 0.f, 8.f));

	// 2) 分区下拉
	ZoneCombo = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass());
	ZoneCombo->AddOption(TEXT("全部分区"));

	TArray<FString> Zones;
	for (int32 i = 0; i < G_FCPoiCount; ++i)
	{
		const FString ZoneName(G_FCPois[i].Zone);
		if (!Zones.Contains(ZoneName))
		{
			Zones.Add(ZoneName);
			ZoneCombo->AddOption(ZoneName);
		}
	}

	ZoneCombo->SetContentPadding(FMargin(8.f, 5.f));

	// 先绑定再设选中项：设置时触发一次 HandleZoneChanged（参数相同，多刷一次无害）。
	ZoneCombo->OnSelectionChanged.AddDynamic(this, &UFCCatalogPanel::HandleZoneChanged);
	ZoneCombo->SetSelectedOption(TEXT("全部分区"));

	VBox->AddChildToVerticalBox(ZoneCombo)->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));

	// 计数（次要小字）
	CountText = NewText(WidgetTree, FText::GetEmpty(), 12, MutedText);
	VBox->AddChildToVerticalBox(CountText)->SetPadding(FMargin(2.f, 0.f, 0.f, 4.f));

	// 3) 滚动列表：填满剩余高度
	ListScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	ListScroll->SetAnimateWheelScrolling(true);
	ListScroll->SetAllowOverscroll(true);
	UVerticalBoxSlot* ScrollSlot = VBox->AddChildToVerticalBox(ListScroll);
	FSlateChildSize ScrollSize(ESlateSizeRule::Fill);
	ScrollSize.Value = 1.f;
	ScrollSlot->SetSize(ScrollSize);

	PanelBorder->SetContent(VBox);

	UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(PanelBorder);
	PanelSlot->SetAnchors(FAnchors(0.f, 0.f));
	PanelSlot->SetAlignment(FVector2D(0.f, 0.f));
	PanelSlot->SetPosition(FVector2D(16.f, 74.f)); // 顶部给 56 高标题栏留位
	PanelSlot->SetSize(FVector2D(212.f, 760.f));
	PanelSlot->SetAutoSize(false);

	RefreshList(FString());
}

void UFCCatalogPanel::RefreshList(const FString& ZoneFilter)
{
	CurrentZone = ZoneFilter;

	if (!ListScroll)
	{
		return;
	}

	ListScroll->ClearChildren();

	int32 ShownCount = 0;
	for (int32 i = 0; i < G_FCPoiCount; ++i)
	{
		const FString ZoneName(G_FCPois[i].Zone);
		if (!ZoneFilter.IsEmpty() && ZoneName != ZoneFilter)
		{
			continue;
		}

		UFCCatalogRow* Row = CreateWidget<UFCCatalogRow>(this, UFCCatalogRow::StaticClass());
		Row->InitRow(i, i == SelectedIndex);
		Row->OnRowClicked.AddDynamic(this, &UFCCatalogPanel::HandleRowClicked);
		ListScroll->AddChild(Row);
		++ShownCount;
	}

	if (CountText)
	{
		CountText->SetText(FText::FromString(FString::Printf(TEXT("当前 %d 处"), ShownCount)));
	}
}

void UFCCatalogPanel::SetSelectedIndex(int32 Index)
{
	SelectedIndex = Index;
	RefreshList(CurrentZone);
}

void UFCCatalogPanel::HandleZoneChanged(FString Item, ESelectInfo::Type SelType)
{
	const FString Filter = (Item == TEXT("全部分区")) ? FString() : Item;
	RefreshList(Filter);
}

void UFCCatalogPanel::HandleRowClicked(int32 Index)
{
	SelectedIndex = Index;
	RefreshList(CurrentZone);
	OnCatalogPoiSelected.Broadcast(Index);
}

//============================================================
// UFCHudShell
//============================================================
void UFCHudShell::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = RootCanvas;

	// ---------------- 顶部标题栏（横向拉伸，高 56） ----------------
	UBorder* TopBar = MakeBorder(WidgetTree, HudBarBg);

	UHorizontalBox* TopRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

	UTextBlock* MainTitle = NewText(WidgetTree,
		FText::FromString(TEXT("故宫 / 全域三维导览")), 22, TitleLight, true);
	UHorizontalBoxSlot* MainTitleSlot = TopRow->AddChildToHorizontalBox(MainTitle);
	MainTitleSlot->SetVerticalAlignment(VAlign_Center);
	MainTitleSlot->SetPadding(FMargin(20.f, 0.f, 0.f, 0.f));

	UTextBlock* SubTitle = NewText(WidgetTree,
		FText::FromString(TEXT("THE FORBIDDEN CITY")), 15, CyanText);
	UHorizontalBoxSlot* SubTitleSlot = TopRow->AddChildToHorizontalBox(SubTitle);
	SubTitleSlot->SetVerticalAlignment(VAlign_Center);
	SubTitleSlot->SetPadding(FMargin(28.f, 0.f, 0.f, 0.f));

	TopBar->SetContent(TopRow);

	UCanvasPanelSlot* TopSlot = RootCanvas->AddChildToCanvas(TopBar);
	TopSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 0.f));
	TopSlot->SetOffsets(FMargin(0.f, 0.f, 0.f, 56.f));
	TopSlot->SetAutoSize(false);

	// ---------------- 右侧说明面板（右上，232x360） ----------------
	UBorder* RightPanel = MakeBorder(WidgetTree, HudPanelBg);
	RightPanel->SetPadding(FMargin(16.f));

	UVerticalBox* InfoBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());

	auto AddInfoLine = [this, &InfoBox](const FString& Str, int32 Size, const FLinearColor& Color, bool bBold, float PadBottom)
	{
		UTextBlock* Line = NewText(WidgetTree, FText::FromString(Str), Size, Color, bBold);
		Line->SetWrapTextAt(196.f);
		Line->SetAutoWrapText(false);
		InfoBox->AddChildToVerticalBox(Line)->SetPadding(FMargin(0.f, 0.f, 0.f, PadBottom));
	};
	auto AddBlankLine = [this, &InfoBox](float Height)
	{
		UTextBlock* Blank = NewText(WidgetTree, FText::GetEmpty(), 13, FLinearColor::Transparent);
		InfoBox->AddChildToVerticalBox(Blank)->SetPadding(FMargin(0.f, Height * 0.5f));
	};

	AddInfoLine(TEXT("紫禁城 · 全域"), 19, Gold, true, 10.f);
	AddInfoLine(TEXT("从午门到神武门，探索中轴大殿、东西六宫与园林。"), 13, BodyText, false, 4.f);
	AddInfoLine(TEXT("点击地图标记或左侧目录，平滑飞行到目的地。"), 13, BodyText, false, 0.f);
	AddBlankLine(8.f);
	AddInfoLine(TEXT("左键拖动：围绕目标旋转"), 13, BodyText, false, 2.f);
	AddInfoLine(TEXT("右键拖动 / WASD：水平前后左右移动"), 13, BodyText, false, 2.f);
	AddInfoLine(TEXT("Q / E：上升 / 下降"), 13, BodyText, false, 2.f);
	AddInfoLine(TEXT("滚轮：缩放，松手平滑缓停"), 13, BodyText, false, 2.f);
	AddInfoLine(TEXT("Home：恢复全景中心"), 13, BodyText, false, 0.f);
	AddBlankLine(8.f);
	AddInfoLine(TEXT("公开资料近似重建，非测绘模型。"), 13, MutedText, false, 8.f);

	RightPanel->SetContent(InfoBox);

	UCanvasPanelSlot* RightSlot = RootCanvas->AddChildToCanvas(RightPanel);
	RightSlot->SetAnchors(FAnchors(1.f, 0.f));
	RightSlot->SetAlignment(FVector2D(1.f, 0.f));
	RightSlot->SetPosition(FVector2D(-24.f, 88.f));
	RightSlot->SetSize(FVector2D(232.f, 360.f));
	RightSlot->SetAutoSize(false);

	// ---------------- 底部控制栏（横向拉伸，高 44） ----------------
	UBorder* BottomBar = MakeBorder(WidgetTree, HudBarBgBottom);

	UHorizontalBox* BottomRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

	UTextBlock* OverviewLabel = nullptr;
	UButton* OverviewBtn = MakeLabeledButton(WidgetTree, TEXT("全域总览"), 14, BtnTextLight,
		FMargin(12.f, 4.f), OverviewLabel);
	OverviewBtn->OnClicked.AddDynamic(this, &UFCHudShell::BroadcastOverview);
	BottomRow->AddChildToHorizontalBox(OverviewBtn)->SetVerticalAlignment(VAlign_Center);

	UTextBlock* PrevLabel = nullptr;
	UButton* PrevBtn = MakeLabeledButton(WidgetTree, TEXT("上一处"), 14, BtnTextLight,
		FMargin(12.f, 4.f), PrevLabel);
	PrevBtn->OnClicked.AddDynamic(this, &UFCHudShell::BroadcastPrev);
	UHorizontalBoxSlot* PrevSlot = BottomRow->AddChildToHorizontalBox(PrevBtn);
	PrevSlot->SetVerticalAlignment(VAlign_Center);
	PrevSlot->SetPadding(FMargin(6.f, 0.f, 0.f, 0.f));

	UTextBlock* NextLabel = nullptr;
	UButton* NextBtn = MakeLabeledButton(WidgetTree, TEXT("下一处"), 14, BtnTextLight,
		FMargin(12.f, 4.f), NextLabel);
	NextBtn->OnClicked.AddDynamic(this, &UFCHudShell::BroadcastNext);
	BottomRow->AddChildToHorizontalBox(NextBtn)->SetVerticalAlignment(VAlign_Center);

	UButton* TourBtn = MakeLabeledButton(WidgetTree, TEXT("自动导览"), 14, BtnTextLight,
		FMargin(12.f, 4.f), TourButtonLabel);
	TourBtn->OnClicked.AddDynamic(this, &UFCHudShell::BroadcastToggleTour);
	UHorizontalBoxSlot* TourSlot = BottomRow->AddChildToHorizontalBox(TourBtn);
	TourSlot->SetVerticalAlignment(VAlign_Center);
	TourSlot->SetPadding(FMargin(6.f, 0.f, 0.f, 0.f));

	// ============ 24 小时时间轴 ============
	// 时刻文字 HH:MM
	TimeText = NewText(WidgetTree, FormatTimeText(9.f), 14, CyanText, true);
	UHorizontalBoxSlot* TimeTextSlot = BottomRow->AddChildToHorizontalBox(TimeText);
	TimeTextSlot->SetVerticalAlignment(VAlign_Center);
	TimeTextSlot->SetPadding(FMargin(22.f, 0.f, 8.f, 0.f));

	// 滑块（0..24，连续步长）：用 SizeBox 固定 220 宽，避免在横向盒中被压缩
	TimeSlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass());
	TimeSlider->SetMinValue(0.f);
	TimeSlider->SetMaxValue(24.f);
	TimeSlider->SetValue(9.f);
	TimeSlider->OnValueChanged.AddDynamic(this, &UFCHudShell::HandleTimeValueChanged);
	TimeSlider->OnMouseCaptureBegin.AddDynamic(this, &UFCHudShell::HandleTimeCaptureBegin);
	TimeSlider->OnMouseCaptureEnd.AddDynamic(this, &UFCHudShell::HandleTimeCaptureEnd);

	USizeBox* SliderSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	SliderSize->SetWidthOverride(220.f);
	SliderSize->SetHeightOverride(28.f);
	SliderSize->AddChild(TimeSlider);
	UHorizontalBoxSlot* SliderSlot2 = BottomRow->AddChildToHorizontalBox(SliderSize);
	SliderSlot2->SetVerticalAlignment(VAlign_Center);

	// 播放/暂停按钮
	UButton* PlayPauseBtn = MakeLabeledButton(WidgetTree, TEXT("播放"), 14, BtnTextLight,
		FMargin(10.f, 4.f), PlayPauseLabel);
	PlayPauseBtn->OnClicked.AddDynamic(this, &UFCHudShell::HandlePlayPauseClicked);
	UHorizontalBoxSlot* PlaySlot = BottomRow->AddChildToHorizontalBox(PlayPauseBtn);
	PlaySlot->SetVerticalAlignment(VAlign_Center);
	PlaySlot->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));

	// ============ 天气四预设按钮：晴 / 多云 / 阴 / 雨 ============
	static const TCHAR* const WeatherNames[4] = { TEXT("晴"), TEXT("多云"), TEXT("阴"), TEXT("雨") };
	WeatherButtons.SetNum(4);
	WeatherLabels.SetNum(4);
	for (int32 i = 0; i < 4; ++i)
	{
		UTextBlock* WLabel = nullptr;
		UButton* WBtn = MakeLabeledButton(WidgetTree, WeatherNames[i], 14, BtnTextLight,
			FMargin(10.f, 4.f), WLabel);
		WeatherButtons[i] = WBtn;
		WeatherLabels[i] = WLabel;

		UHorizontalBoxSlot* WSlot = BottomRow->AddChildToHorizontalBox(WBtn);
		WSlot->SetVerticalAlignment(VAlign_Center);
		WSlot->SetPadding(FMargin(i == 0 ? 10.f : 6.f, 0.f, 0.f, 0.f));
	}
	WeatherButtons[0]->OnClicked.AddDynamic(this, &UFCHudShell::HandleWeatherClear);
	WeatherButtons[1]->OnClicked.AddDynamic(this, &UFCHudShell::HandleWeatherCloudy);
	WeatherButtons[2]->OnClicked.AddDynamic(this, &UFCHudShell::HandleWeatherOvercast);
	WeatherButtons[3]->OnClicked.AddDynamic(this, &UFCHudShell::HandleWeatherRain);

	// 默认天气为晴：首档金色
	if (WeatherLabels[0])
	{
		WeatherLabels[0]->SetColorAndOpacity(FSlateColor(Gold));
	}

	StatusText = NewText(WidgetTree,
		FText::FromString(TEXT("自由浏览 ｜ 103处 ｜ 左键旋转 · 右键/WASD 水平移动 · 滚轮缩放")),
		13, FLinearColor(0.66f, 0.74f, 0.77f));
	UHorizontalBoxSlot* StatusSlot = BottomRow->AddChildToHorizontalBox(StatusText);
	StatusSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	StatusSlot->SetVerticalAlignment(VAlign_Center);
	StatusSlot->SetPadding(FMargin(18.f, 0.f, 0.f, 0.f));

	BottomBar->SetContent(BottomRow);
	BottomBar->SetPadding(FMargin(16.f, 0.f));

	UCanvasPanelSlot* BottomSlot = RootCanvas->AddChildToCanvas(BottomBar);
	// 水平拉伸轴：Left/Right 为边距(0)；垂直点锚(y=1)轴：Top=-44 为相对底边的位置，
	// Bottom=44 为控件尺寸（点锚轴上 Bottom 表示高度，不是边距，缺失会得到 0 高）。
	BottomSlot->SetAnchors(FAnchors(0.f, 1.f, 1.f, 1.f));
	BottomSlot->SetOffsets(FMargin(0.f, -44.f, 0.f, 44.f));
	BottomSlot->SetAutoSize(false);
}

void UFCHudShell::SetStatusText(const FText& Text)
{
	if (StatusText)
	{
		StatusText->SetText(Text);
	}
}

void UFCHudShell::SetTourButtonText(const FText& Text)
{
	if (TourButtonLabel)
	{
		TourButtonLabel->SetText(Text);
	}
}

void UFCHudShell::BroadcastOverview()
{
	OnOverview.Broadcast();
}

void UFCHudShell::BroadcastPrev()
{
	OnPrev.Broadcast();
}

void UFCHudShell::BroadcastNext()
{
	OnNext.Broadcast();
}

void UFCHudShell::BroadcastToggleTour()
{
	OnToggleTour.Broadcast();
}

//============================================================
// 24 小时时间轴
//============================================================
FText UFCHudShell::FormatTimeText(float TimeOfDay)
{
	// wrap 到 [0,24) 后换算总分钟数
	float T = FMath::Fmod(TimeOfDay, 24.f);
	if (T < 0.f)
	{
		T += 24.f;
	}

	int32 TotalMinutes = FMath::RoundToInt(T * 60.f) % (24 * 60);
	const int32 HH = TotalMinutes / 60;
	const int32 MM = TotalMinutes % 60;
	return FText::FromString(FString::Printf(TEXT("%02d:%02d"), HH, MM));
}

AFCSkyDirector* UFCHudShell::GetSkyDirector()
{
	if (CachedDirector.IsValid())
	{
		return Cast<AFCSkyDirector>(CachedDirector.Get());
	}

	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AFCSkyDirector::StaticClass(), Found);
	if (Found.Num() > 0)
	{
		CachedDirector = Found[0];
		return Cast<AFCSkyDirector>(Found[0]);
	}
	return nullptr;
}

void UFCHudShell::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	AFCSkyDirector* Director = GetSkyDirector();
	if (!Director)
	{
		return;
	}

	// 用户未拖动时轮询 Director：自动播放时滑块与 HH:MM 随之自动刷新
	if (!bUserDraggingTime)
	{
		const float T = Director->GetTimeOfDay();
		if (TimeSlider && !FMath::IsNearlyEqual(TimeSlider->GetValue(), T, 0.0005f))
		{
			TimeSlider->SetValue(T);
		}
		if (TimeText)
		{
			TimeText->SetText(FormatTimeText(T));
		}
	}

	// 同步播放/暂停按钮文案（正常只由本界面切换，多校验一次可应对外部状态变化）
	const bool bAuto = Director->IsAutoAdvancing();
	if (bAuto != bIsAutoAdvancing)
	{
		bIsAutoAdvancing = bAuto;
		if (PlayPauseLabel)
		{
			PlayPauseLabel->SetText(FText::FromString(bAuto ? TEXT("暂停") : TEXT("播放")));
		}
	}
}

void UFCHudShell::HandleTimeValueChanged(float NewValue)
{
	// 程序化 SetValue 也会触发本事件；仅在用户拖动时回写 Director，避免反馈环互打。
	if (!bUserDraggingTime)
	{
		return;
	}

	if (AFCSkyDirector* Director = GetSkyDirector())
	{
		Director->SetTimeOfDay(NewValue);
	}
	if (TimeText)
	{
		TimeText->SetText(FormatTimeText(NewValue));
	}
}

void UFCHudShell::HandleTimeCaptureBegin()
{
	bUserDraggingTime = true;
}

void UFCHudShell::HandleTimeCaptureEnd()
{
	bUserDraggingTime = false;
}

void UFCHudShell::HandlePlayPauseClicked()
{
	AFCSkyDirector* Director = GetSkyDirector();
	if (!Director)
	{
		return;
	}

	Director->ToggleAutoAdvance();
	bIsAutoAdvancing = Director->IsAutoAdvancing();
	if (PlayPauseLabel)
	{
		PlayPauseLabel->SetText(FText::FromString(bIsAutoAdvancing ? TEXT("暂停") : TEXT("播放")));
	}
}

//============================================================
// 天气四预设
//============================================================
void UFCHudShell::RequestWeather(int32 WeatherIndex)
{
	if (AFCSkyDirector* Director = GetSkyDirector())
	{
		// EFCWeather 枚举顺序：Clear / Cloudy / Overcast / Rain，与按钮下标一致
		Director->SetWeather(static_cast<EFCWeather>(WeatherIndex));
	}

	// 当前天气金色文字，其余普通色
	for (int32 i = 0; i < WeatherLabels.Num(); ++i)
	{
		if (WeatherLabels[i])
		{
			WeatherLabels[i]->SetColorAndOpacity(
				FSlateColor(i == WeatherIndex ? Gold : BtnTextLight));
		}
	}
}

void UFCHudShell::HandleWeatherClear()    { RequestWeather(0); }
void UFCHudShell::HandleWeatherCloudy()   { RequestWeather(1); }
void UFCHudShell::HandleWeatherOvercast() { RequestWeather(2); }
void UFCHudShell::HandleWeatherRain()     { RequestWeather(3); }
