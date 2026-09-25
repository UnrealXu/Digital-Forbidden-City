#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FCPoiMarker.generated.h"

class UStaticMeshComponent;
class UWidgetComponent;

/** 悬浮在空中的 3D 中文名称标签，标记故宫及北京城市 POI 之一。
 *  点击名称文字（或其周围的透明点击体）触发飞行导览；
 *  文字始终朝向相机，并按距离自适应缩放；距离过远或卡片打开时自动隐藏。 */
UCLASS()
class TRAE_UE_PROJECT_API AFCPoiMarker : public AActor
{
	GENERATED_BODY()

public:
	AFCPoiMarker();

	/** Places the label at the baked FCData coordinate for this POI index. */
	void Init(int32 InIndex);

	/** While the POI info card is open every label is hidden. Pass false
	 *  (e.g. when the card closes or a new flight starts) to restore distance culling. */
	static void SetAllMarkersHidden(bool bHidden);

	virtual void Tick(float DeltaSeconds) override;

	/** 每帧由 GameMode 在 PostPhysics 阶段调用：
	 *  收集本帧全部候选标签，按“重点优先、由近到远”排序后做矩形重叠剔除，
	 *  保证沿中轴远眺时只保留最近/最重要的标签，不依赖 Actor Tick 顺序。 */
	static void ResolveScreenCulling();

	/** 屏幕空间精确拾取：点击坐标落在哪个“当前可见标签”的屏幕矩形内，
	 *  返回其 PoiIndex；未命中返回 INDEX_NONE。比 3D HitBox 线 trace 可靠，
	 *  不会被标签后面的远景网格“穿透”。 */
	static int32 PickMarker(const FVector2D& ScreenPos);

	/** 自检：对每个当前可见标签的屏幕中心调用 PickMarker，返回
	 *  "poi=<索引> picked=<返回索引> match=true/false" 行；用于编辑器/PIE 验证。 */
	UFUNCTION(BlueprintCallable, Category = "ForbiddenCity")
	static TArray<FString> RunPickSelfTest();

private:
	/** 在给定世界/控制器下做屏幕矩形拾取（PickMarker 的实现）。 */
	static int32 PickMarkerInWorld(UWorld* World, APlayerController* PC, const FVector2D& ScreenPos);

	/** 世界空间中文名称文字（朝向相机的 billboard）。 */
	UPROPERTY(VisibleAnywhere, Category = "ForbiddenCity")
	UWidgetComponent* Label;

	/** 透明点击体，尺寸跟随文字，保证名称容易被点中。 */
	UPROPERTY(VisibleAnywhere, Category = "ForbiddenCity")
	UStaticMeshComponent* HitBox;

	int32 PoiIndex = INDEX_NONE;

private:
	/** 由 ResolveScreenCulling 统一设置最终显隐与碰撞。 */
	void ApplyScreenCull(bool bCulled);

	bool bShown = false;

	/** 屏幕空间去重：投影矩形与更近/更重要的标签重叠则隐藏。 */
	bool bScreenCulled = false;

	/** Init 时缓存：2=城市级地标，1=中轴线重点建筑，0=普通 POI。 */
	int32 Tier = 0;

	static bool bAllHidden;

	/** 按名称字数估算的文字基准宽度 / 高度（cm），用于驱动 HitBox 缩放。 */
	float BaseW = 0.f;
	static constexpr float BaseH = 5000.f;
};
