#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FCTaiheExploder.generated.h"

class USceneComponent;
class AStaticMeshActor;

/** 将太和殿（13 个 Taihe_ 前缀的 StaticMeshActor）按五个高度层竖向拆解/复位。
 *  部件全部在世界原点附近 spawn，几何烘焙进顶点，因此拆解只改位置不改旋转。 */
UCLASS()
class TRAE_UE_PROJECT_API AFCTaiheExploder : public AActor
{
	GENERATED_BODY()

public:
	AFCTaiheExploder();

	/** 开始拆解（目标 Alpha=1）。 */
	UFUNCTION(BlueprintCallable, Category = "ForbiddenCity")
	void Explode();

	/** 开始复位（目标 Alpha=0）。 */
	UFUNCTION(BlueprintCallable, Category = "ForbiddenCity")
	void Assemble();

	/** 按 bTargetExploded 取反切换拆解/复位。 */
	UFUNCTION(BlueprintCallable, Category = "ForbiddenCity")
	void Toggle();

	/** 立即复位（Alpha 直接归零、部件归位），供飞离太和殿时调用。 */
	UFUNCTION(BlueprintCallable, Category = "ForbiddenCity")
	void AssembleNow();

	/** 是否处于拆解目标状态。 */
	UFUNCTION(BlueprintCallable, Category = "ForbiddenCity")
	bool IsExploded() const { return bTargetExploded; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	struct FPart
	{
		AStaticMeshActor* Actor = nullptr;
		FVector Base = FVector::ZeroVector;
		FVector Offset = FVector::ZeroVector;
	};

	TArray<FPart> Parts;

	/** 当前插值进度 0..1。 */
	float Alpha = 0.f;

	bool bTargetExploded = false;

	/** Alpha 趋近目标的速度（每秒变化率）。 */
	static constexpr float LerpSpeed = 2.2f;

	/** 按部件名返回五段竖向偏移（单位厘米，仅 +Z）。 */
	FVector ResolveOffset(const FString& Label) const;

	/** 按当前 Alpha 设置每个 Actor 位置 = Base + Offset*Alpha（sweep=false）。 */
	void ApplyParts() const;
};
