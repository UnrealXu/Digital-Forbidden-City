// -*- coding: utf-8 -*-
// 故宫数字孪生 —— 纯 C++ UMG 界面（屏幕空间 HUD）：
//   UFCCatalogRow  目录单行（名称 + 选中/悬停反馈 + 点击委托）
//   UFCCatalogPanel 左侧宫殿目录（分区下拉 + 滚动列表 + 计数）
//   UFCHudShell    顶部标题栏 + 右侧说明 + 底部控制栏
// 所有控件均在 NativeOnInitialized 中 WidgetTree->ConstructWidget 构造，
// 不依赖任何 UMG 资产；字体统一走 FcMakeSlateFont（INLINE 仿宋，可显示中文）。
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ComboBoxString.h" // ESelectInfo::Type（句柄函数签名需要完整类型）
#include "FCUiHud.generated.h"

class UBorder;
class UButton;
class UTextBlock;
class UScrollBox;
class USlider;
class UVerticalBox;
class UHorizontalBox;
class UCanvasPanel;
class AFCSkyDirector;

//============================================================
// UFCCatalogRow —— 目录中的单行
//============================================================
UCLASS()
class TRAE_UE_PROJECT_API UFCCatalogRow : public UUserWidget
{
	GENERATED_BODY()

public:
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRowClicked, int32, Index);
	UPROPERTY(BlueprintAssignable, Category = "ForbiddenCity")
	FOnRowClicked OnRowClicked;

	/** Index=G_FCPois 下标；bSelected=是否当前选中。 */
	void InitRow(int32 Index, bool bSelected);

	/** 切换选中外观（底色 / 金色文字 / "◆ " 前缀）。 */
	void SetSelected(bool bSelected);

	virtual void NativeOnInitialized() override;

	int32 RowIndex = INDEX_NONE;

private:
	UFUNCTION() void HandleClicked();
	UFUNCTION() void HandleHovered();
	UFUNCTION() void HandleUnhovered();
	UFUNCTION() void HandlePressed();
	UFUNCTION() void HandleReleased();

	/** 根据 选中/悬停/按下 状态刷新 RowBorder 底色。 */
	void RefreshBackground();

	UPROPERTY() UBorder* RowBorder = nullptr;
	UPROPERTY() UTextBlock* NameText = nullptr;
	UPROPERTY() UButton* RowButton = nullptr;

	bool bRowSelected = false;
	bool bRowHovered = false;
	bool bRowPressed = false;
};

//============================================================
// UFCCatalogPanel —— 左侧宫殿目录
//============================================================
UCLASS()
class TRAE_UE_PROJECT_API UFCCatalogPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCatalogPoiSelected, int32, PoiIndex);
	UPROPERTY(BlueprintAssignable, Category = "ForbiddenCity")
	FOnCatalogPoiSelected OnCatalogPoiSelected;

	/** 按分区重建列表；空字符串 = 全部分区。 */
	void RefreshList(const FString& ZoneFilter);

	/** 外部（pawn / HUD）同步选中项。 */
	void SetSelectedIndex(int32 Index);

	virtual void NativeOnInitialized() override;

private:
	UFUNCTION() void HandleZoneChanged(FString Item, ESelectInfo::Type SelType);
	UFUNCTION() void HandleRowClicked(int32 Index);

	UPROPERTY() UComboBoxString* ZoneCombo = nullptr;
	UPROPERTY() UScrollBox* ListScroll = nullptr;
	UPROPERTY() UTextBlock* CountText = nullptr;

	FString CurrentZone;
	int32 SelectedIndex = INDEX_NONE;
};

//============================================================
// UFCHudShell —— 顶部标题栏 + 右侧说明 + 底部控制栏
//============================================================
UCLASS()
class TRAE_UE_PROJECT_API UFCHudShell : public UUserWidget
{
	GENERATED_BODY()

public:
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHudOverview);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHudPrev);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHudNext);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHudToggleTour);

	UPROPERTY(BlueprintAssignable, Category = "ForbiddenCity") FOnHudOverview OnOverview;
	UPROPERTY(BlueprintAssignable, Category = "ForbiddenCity") FOnHudPrev OnPrev;
	UPROPERTY(BlueprintAssignable, Category = "ForbiddenCity") FOnHudNext OnNext;
	UPROPERTY(BlueprintAssignable, Category = "ForbiddenCity") FOnHudToggleTour OnToggleTour;

	void SetStatusText(const FText& Text);
	void SetTourButtonText(const FText& Text);

	virtual void NativeOnInitialized() override;
	// 自动播放时由 NativeTick 轮询 SkyDirector 同步滑块与 HH:MM 文字。
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UFUNCTION() void BroadcastOverview();
	UFUNCTION() void BroadcastPrev();
	UFUNCTION() void BroadcastNext();
	UFUNCTION() void BroadcastToggleTour();

	//---- 24 小时时间轴 ----
	UFUNCTION() void HandleTimeValueChanged(float NewValue);
	UFUNCTION() void HandleTimeCaptureBegin();
	UFUNCTION() void HandleTimeCaptureEnd();
	UFUNCTION() void HandlePlayPauseClicked();

	//---- 天气四预设（动态委托无法携带自定义参数，各档一个薄包装） ----
	UFUNCTION() void HandleWeatherClear();
	UFUNCTION() void HandleWeatherCloudy();
	UFUNCTION() void HandleWeatherOvercast();
	UFUNCTION() void HandleWeatherRain();

	/** 请求天气并刷新四个按钮的金色/普通文字。 */
	void RequestWeather(int32 WeatherIndex);

	/** 获取（并缓存）关卡中的 AFCSkyDirector；失效后重新查找。 */
	AFCSkyDirector* GetSkyDirector();

	/** 按 HH:MM 格式化时刻。 */
	static FText FormatTimeText(float TimeOfDay);

	UPROPERTY() UTextBlock* StatusText = nullptr;
	UPROPERTY() UTextBlock* TourButtonLabel = nullptr;

	UPROPERTY() UTextBlock* TimeText = nullptr;          // HH:MM
	UPROPERTY() USlider* TimeSlider = nullptr;           // 0..24
	UPROPERTY() UTextBlock* PlayPauseLabel = nullptr;    // 播放/暂停
	UPROPERTY() TArray<UButton*> WeatherButtons;         // 晴/多云/阴/雨
	UPROPERTY() TArray<UTextBlock*> WeatherLabels;

	bool bUserDraggingTime = false;
	bool bIsAutoAdvancing = false;

	/** SkyDirector 弱引用缓存（不阻止 GC，失效则重新 GetAllActorsOfClass）。 */
	UPROPERTY() TWeakObjectPtr<AActor> CachedDirector;
};
