#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FCFocusComponent.generated.h"

class UStaticMeshComponent;

/**
 * 建筑高亮/脉冲突出管理组件（挂在 AFCTwinPawn 上即可）。
 *
 * 同一时刻只维护一个焦点目标：
 * 给目标身上所有 StaticMeshComponent 开启 CustomDepth 并写入 Stencil 值，
 * 由后处理材质 /Game/ForbiddenCity/Materials/MPP_Outline 只描外轮廓并做金色脉冲。
 *
 * ---------------------------------------------------------------------------
 * 【摆关脚本命名 / Tag 约定】（SetFocusByName / SetFocusByPoiIndex 依赖）
 *  1. 建筑 Actor 按 FCData 的 POI 索引命名，推荐：POI<索引>_<拼音或英文名>
 *     例如 POI9_TaiHeDian（太和殿，索引 9）、POI0_WuMen（午门，索引 0）。
 *     名称前缀匹配 "POI<idx>_"（大小写不敏感）即视为命中。
 *  2. 同时建议给建筑 Actor 添加 ActorTag："poi_<索引>"，例如 poi_9。
 *     Tag 优先级最高，比名字匹配更稳。
 *  3. 名字兜底：传入的字符串与 Actor 名字做大小写不敏感的子串匹配，
 *     因此 BJ_TaiHeDian 这类拼音命名也可用 "TaiHeDian" 定位；
 *     直接传中文 POI 名（如 "太和殿"）时做精确名字匹配。
 *  4. 一栋建筑允许由多个 StaticMeshComponent / 多个子 Actor 部件拼合，
 *     但 SetFocusByName 只定位一个 AActor；若拼合部件是多个平级 Actor，
 *     请将它们挂到同一个根 Actor 下，或给所有部件加同一个 poi Tag
 *     （当前实现取第一个匹配项）。
 * ---------------------------------------------------------------------------
 */
UCLASS(ClassGroup = (ForbiddenCity), meta = (BlueprintSpawnableComponent))
class TRAE_UE_PROJECT_API UFCFocusComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFCFocusComponent();

	/** 高亮指定 Actor（自动收集其全部 StaticMeshComponent，支持一个 Actor 多 mesh）。
	 *  传空等同于 ClearFocus()。 */
	UFUNCTION(BlueprintCallable, Category = "ForbiddenCity|Focus")
	void SetFocus(AActor* Target);

	/** 高亮一组 Actor（如太和殿由 13 个 Taihe_ 部件 Actor 拼合）：
	 *  整组统一开启 CustomDepth，由后处理材质整体描边。 */
	UFUNCTION(BlueprintCallable, Category = "ForbiddenCity|Focus", meta = (DisplayName = "Set Focus Actors"))
	void SetFocusActors(const TArray<AActor*>& Targets);

	/** 清除当前高亮：关闭 CustomDepth。 */
	UFUNCTION(BlueprintCallable, Category = "ForbiddenCity|Focus")
	void ClearFocus();

	/** 按 FCData 的 POI 索引定位建筑（匹配 Tag poi_<idx> 或名字前缀 POI<idx>_）。 */
	UFUNCTION(BlueprintCallable, Category = "ForbiddenCity|Focus")
	bool SetFocusByPoiIndex(int32 PoiIndex);

	/** 按名字定位建筑：中文名精确匹配，英文名按 Tag / 前缀 / 子串匹配。 */
	UFUNCTION(BlueprintCallable, Category = "ForbiddenCity|Focus")
	bool SetFocusByName(const FString& Name);

	/** 当前高亮目标中的第一个（无则为空）。 */
	UFUNCTION(BlueprintPure, Category = "ForbiddenCity|Focus")
	AActor* GetFocusedActor() const
	{
		for (const TWeakObjectPtr<AActor>& A : FocusedActors)
		{
			if (A.IsValid())
			{
				return A.Get();
			}
		}
		return nullptr;
	}

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** 单个被高亮 mesh 的运行时状态。 */
	struct FFocusedMesh
	{
		UStaticMeshComponent* Mesh = nullptr;
	};

	void ApplyHighlight(const TArray<AActor*>& Targets);

	/** 在当前 World 中按 POI 索引查找建筑：Tag 命中时返回全部同 Tag Actor
	 *  （拼合部件），否则返回名字前缀匹配的 Actor；无命中返回 false。 */
	bool FindPoiActors(int32 KnownIdx, TArray<AActor*>& OutActors) const;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AActor>> FocusedActors;

	TArray<FFocusedMesh> FocusedMeshes;

	/** 写入 CustomDepth Stencil 的值，后处理材质据此识别选中目标。 */
	static constexpr int32 FocusStencilValue = 1;
};
