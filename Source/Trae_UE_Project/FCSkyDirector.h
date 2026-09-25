// -*- coding: utf-8 -*-
// 故宫数字孪生 —— 天空总控（AFCSkyDirector）：
//   ① 真实天空：SkyAtmosphere + 太阳/月亮（共用一盏 DirectionalLight，
//      日落后切换 AtmosphereSunLightIndex=1 作为月亮）+ VolumetricCloud 体积云；
//   ② 24 小时昼夜：TimeOfDay（0..24），按北京纬度 φ=39.9° 与固定太阳赤纬 δ=20°
//      用标准天文公式计算太阳高度角/方位角；
//   ③ 天气四预设：晴 / 多云 / 阴 / 雨；雨天启用自带的程序化降雨器
//      （UProceduralMeshComponent，约 2500 条雨线，CPU 更新、环绕玩家相机）。
// 由 AFCGameMode::BeginPlay 统一 spawn；场景中 SkyAtmosphere / VolumetricCloud
// 缺失时会运行时兜底 spawn。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FCSkyDirector.generated.h"

class UDirectionalLightComponent;
class USkyLightComponent;
class USkyAtmosphereComponent;
class UVolumetricCloudComponent;
class UExponentialHeightFogComponent;
class UProceduralMeshComponent;
class UMaterialInstanceDynamic;
class UMaterial;

// 天气四预设
UENUM(BlueprintType)
enum class EFCWeather : uint8
{
	Clear    UMETA(DisplayName = "晴"),
	Cloudy   UMETA(DisplayName = "多云"),
	Overcast UMETA(DisplayName = "阴"),
	Rain     UMETA(DisplayName = "雨")
};

UCLASS()
class TRAE_UE_PROJECT_API AFCSkyDirector : public AActor
{
	GENERATED_BODY()

public:
	AFCSkyDirector();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	//-------------------- 时间 API --------------------
	/** 设置当前时刻（小时，0..24，自动 wrap）；立即刷新太阳/月亮并节流 recapture 天光。 */
	UFUNCTION(BlueprintCallable, Category = "FC|Sky")
	void SetTimeOfDay(float Hours);

	/** 开/关自动流逝（约 DayDurationSeconds 秒走完 24h）。 */
	UFUNCTION(BlueprintCallable, Category = "FC|Sky")
	void SetAutoAdvance(bool bAuto);

	/** 切换自动流逝。 */
	UFUNCTION(BlueprintCallable, Category = "FC|Sky")
	void ToggleAutoAdvance();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "FC|Sky")
	float GetTimeOfDay() const { return TimeOfDay; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "FC|Sky")
	bool IsAutoAdvancing() const { return bAutoAdvance; }

	//-------------------- 天气 API --------------------
	/** 切换天气预设（云量 / 雾密度与颜色 / 天光强度 / 雨效）。 */
	UFUNCTION(BlueprintCallable, Category = "FC|Sky")
	void SetWeather(EFCWeather NewWeather);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "FC|Sky")
	EFCWeather GetWeather() const { return Weather; }

protected:
	//-------------------- 场景对象缓存 --------------------
	void CacheSceneObjects();

	UPROPERTY() TObjectPtr<UDirectionalLightComponent> CelestialLight = nullptr; // 太阳/月亮共用
	UPROPERTY() TObjectPtr<USkyLightComponent> SkyLight = nullptr;
	UPROPERTY() TObjectPtr<USkyAtmosphereComponent> SkyAtmosphere = nullptr;
	UPROPERTY() TObjectPtr<UVolumetricCloudComponent> Clouds = nullptr;
	UPROPERTY() TObjectPtr<UExponentialHeightFogComponent> Fog = nullptr;

	/** 体积云组件材质的动态实例（参数可随意改，不污染 /Engine 共享资产）。 */
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> CloudMID = nullptr;

	//-------------------- 时间参数 --------------------
	/** 当前时刻（小时，0..24）。 */
	UPROPERTY(EditAnywhere, Category = "FC|Sky", meta = (ClampMin = "0.0", ClampMax = "24.0"))
	float TimeOfDay = 9.0f;

	/** 自动流逝时走完 24h 的秒数（约 120 秒）。 */
	UPROPERTY(EditAnywhere, Category = "FC|Sky", meta = (ClampMin = "1.0"))
	float DayDurationSeconds = 120.0f;

	bool bAutoAdvance = false;

	//-------------------- 天气参数 --------------------
	UPROPERTY(EditAnywhere, Category = "FC|Sky")
	EFCWeather Weather = EFCWeather::Clear;

	//-------------------- 昼夜刷新 --------------------
	/** 按 TimeOfDay 重算太阳/月亮方向、光强、光色与 SkyAtmosphere 灯光索引。 */
	void UpdateCelestialBodies();

	/** 按 Weather 写云量、雾密度/颜色、天光强度（含昼夜系数合成）。 */
	void ApplyWeatherPreset();

	/** 确保 CloudMID 已基于云组件当前材质创建。 */
	void EnsureCloudMaterial();

	//-------------------- SkyLight recapture 节流 --------------------
	double LastRecaptureSeconds = -10.0; // 上次 RecaptureSky 的世界时间
	/** 距上次 recapture 超过 0.25s 才执行；bForce 立即执行（切换天气用）。 */
	void TryRecaptureSky(bool bForce = false);

	/** 记录关卡天光原始强度，作为天气/昼夜缩放基准。 */
	float BaseSkyLightIntensity = 1.0f;

	/** 当前天气对天光的强度系数（晴 1.0 … 雨 0.3），昼夜合成时使用。 */
	float WeatherSkyScale = 1.0f;

	/** smoothstep 辅助：A/B 边沿之间平滑 0→1。 */
	static float SmoothEdge(float A, float B, float X);

	//-------------------- 程序化降雨 --------------------
	/** 雨线几何（单 section procedural mesh，构造时即创建，空 section 不渲染）。 */
	UPROPERTY(VisibleAnywhere, Category = "FC|Rain")
	TObjectPtr<UProceduralMeshComponent> RainMesh;

	UPROPERTY() TObjectPtr<UMaterial> RainMaterial = nullptr;

	struct FRainDrop
	{
		FVector Offset = FVector::ZeroVector; // 相对雨盒中心（相机）
		float FallSpeed = 30.f;               // cm? 世界单位/秒（约 28-42 m/s）
		float Length = 1.2f;                  // 雨线长度（m）
	};

	TArray<FRainDrop> RainDrops;
	FVector RainCenter = FVector::ZeroVector;
	double LastCenterCalibrateSeconds = -10.0;
	bool bRainMeshBuilt = false;

	/** 2500 个雨滴的初始随机分布。 */
	void InitRainDrops();

	/** 每帧推进雨滴、wrap 回盒顶并 UpdateMeshSection。 */
	void UpdateRain(float DeltaSeconds);

	/** 切走雨天：隐藏 mesh、停止模拟。 */
	void StopRain();

	/** 加载已保存的 Unlit + Translucent 雨线材质。 */
	UMaterial* CreateRainMaterial();
};
