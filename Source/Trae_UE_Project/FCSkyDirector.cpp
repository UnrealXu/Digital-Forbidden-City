// -*- coding: utf-8 -*-
#include "FCSkyDirector.h"

#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SceneComponent.h"

#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraComponent.h"
#include "Math/RotationMatrix.h"

#include "ProceduralMeshComponent.h"

namespace
{
	// 北京纬度 φ 与固定太阳赤纬 δ（任务指定的简化模型，近似初夏）
	constexpr double FC_SunLatitudeDeg = 39.9;
	constexpr double FC_SunDeclinationDeg = 20.0;

	// 雨盒半边长（m）：雨滴分布在相机周围 50m 见方的盒子内
	constexpr float FC_RainHalfExtent = 25.f;
	constexpr int32 FC_RainDropCount = 2500;
}

AFCSkyDirector::AFCSkyDirector()
{
	PrimaryActorTick.bCanEverTick = true;

	// 降雨器：构造期即挂好，空 section 不产生任何渲染；仅雨天 Build + 显示。
	RainMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("RainMesh"));
	SetRootComponent(RainMesh);
	RainMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RainMesh->SetCastShadow(false);
	RainMesh->SetVisibility(false);
}

void AFCSkyDirector::BeginPlay()
{
	Super::BeginPlay();

	CacheSceneObjects();
	EnsureCloudMaterial();

	// 使用已保存的材质资产，打包版无需在运行时编译材质图。
	RainMaterial = CreateRainMaterial();
	if (RainMaterial)
	{
		RainMesh->SetMaterial(0, RainMaterial);
	}

	// 初始时刻 + 初始天气一次性铺好
	ApplyWeatherPreset();
	UpdateCelestialBodies();
	TryRecaptureSky(true);
}

void AFCSkyDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bAutoAdvance)
	{
		// 约 DayDurationSeconds 秒走完 24h
		const float HoursPerSecond = 24.f / FMath::Max(DayDurationSeconds, 1.f);
		SetTimeOfDay(TimeOfDay + HoursPerSecond * DeltaSeconds);
	}

	if (Weather == EFCWeather::Rain)
	{
		UpdateRain(DeltaSeconds);
	}
}

//============================================================
// 时间 API
//============================================================
void AFCSkyDirector::SetTimeOfDay(float Hours)
{
	// wrap 到 [0,24)
	float T = FMath::Fmod(Hours, 24.f);
	if (T < 0.f)
	{
		T += 24.f;
	}
	TimeOfDay = T;

	UpdateCelestialBodies();
	TryRecaptureSky(false);
}

void AFCSkyDirector::SetAutoAdvance(bool bAuto)
{
	bAutoAdvance = bAuto;
}

void AFCSkyDirector::ToggleAutoAdvance()
{
	bAutoAdvance = !bAutoAdvance;
}

//============================================================
// 天气 API
//============================================================
void AFCSkyDirector::SetWeather(EFCWeather NewWeather)
{
	if (Weather == NewWeather)
	{
		return;
	}
	Weather = NewWeather;
	ApplyWeatherPreset();
	UpdateCelestialBodies(); // 天光强度 = 天气系数 × 昼夜系数
	TryRecaptureSky(true);
}

float AFCSkyDirector::SmoothEdge(float A, float B, float X)
{
	const float T = FMath::Clamp((X - A) / (B - A), 0.f, 1.f);
	return T * T * (3.f - 2.f * T);
}

//============================================================
// 场景对象缓存（缺失则运行时兜底 spawn）
//============================================================
void AFCSkyDirector::CacheSceneObjects()
{
	UWorld* World = GetWorld();

	// ---- DirectionalLight（太阳/月亮共用） ----
	{
		TArray<AActor*> Found;
		UGameplayStatics::GetAllActorsOfClass(this, ADirectionalLight::StaticClass(), Found);
		if (Found.Num() > 0)
		{
			CelestialLight = Cast<ADirectionalLight>(Found[0])
				->FindComponentByClass<UDirectionalLightComponent>();
		}
	}

	// ---- SkyLight ----
	{
		TArray<AActor*> Found;
		UGameplayStatics::GetAllActorsOfClass(this, ASkyLight::StaticClass(), Found);
		if (Found.Num() > 0)
		{
			SkyLight = Cast<ASkyLight>(Found[0])->FindComponentByClass<USkyLightComponent>();
		}
	}

	// ---- SkyAtmosphere（缺失兜底） ----
	{
		TArray<AActor*> Found;
		UGameplayStatics::GetAllActorsOfClass(this, ASkyAtmosphere::StaticClass(), Found);
		if (Found.Num() > 0)
		{
			SkyAtmosphere = Cast<ASkyAtmosphere>(Found[0])->GetComponent();
		}
	}

	// ---- VolumetricCloud（缺失兜底） ----
	{
		TArray<AActor*> Found;
		UGameplayStatics::GetAllActorsOfClass(this, AVolumetricCloud::StaticClass(), Found);
		if (Found.Num() > 0)
		{
			Clouds = Cast<AVolumetricCloud>(Found[0])
				->FindComponentByClass<UVolumetricCloudComponent>();
		}
	}

	// ---- ExponentialHeightFog ----
	{
		TArray<AActor*> Found;
		UGameplayStatics::GetAllActorsOfClass(this, AExponentialHeightFog::StaticClass(), Found);
		if (Found.Num() > 0)
		{
			Fog = Cast<AExponentialHeightFog>(Found[0])->GetComponent();
		}
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (!SkyAtmosphere)
	{
		ASkyAtmosphere* NewActor = World->SpawnActor<ASkyAtmosphere>(
			ASkyAtmosphere::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
		if (NewActor)
		{
			SkyAtmosphere = NewActor->GetComponent();
			UE_LOG(LogTemp, Warning, TEXT("[FC] SkyAtmosphere 缺失，已运行时兜底 spawn"));
		}
	}

	if (!Clouds)
	{
		AVolumetricCloud* NewActor = World->SpawnActor<AVolumetricCloud>(
			AVolumetricCloud::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
		if (NewActor)
		{
			Clouds = NewActor->FindComponentByClass<UVolumetricCloudComponent>();
			UE_LOG(LogTemp, Warning, TEXT("[FC] VolumetricCloud 缺失，已运行时兜底 spawn"));
		}
	}

	// 灯光必须 Movable：天空系统逐帧驱动，RecaptureSky 同样要求
	if (CelestialLight)
	{
		CelestialLight->SetMobility(EComponentMobility::Movable);

		// ---- 修复阴影闪烁（shadow acne / VSM 边缘抖动）----
		// 大平地 + 远景 + 移动相机时，默认偏置会让远处阴影边缘像素级黑白闪烁。
		// 适当提高深度偏置与斜面偏置；ContactShadow 在大面积上也会抖，关掉它。
		CelestialLight->SetShadowBias(1.0f);
		CelestialLight->SetShadowSlopeBias(1.0f);
		CelestialLight->ContactShadowLength = 0.f;
		CelestialLight->ContactShadowLengthInWS = false;
	}
	if (SkyLight)
	{
		SkyLight->SetMobility(EComponentMobility::Movable);
		BaseSkyLightIntensity = SkyLight->Intensity;
	}
}

void AFCSkyDirector::EnsureCloudMaterial()
{
	if (CloudMID || !Clouds)
	{
		return;
	}

	UMaterialInterface* BaseMaterial = Clouds->GetMaterial();
	if (!BaseMaterial)
	{
		// CDO 默认材质理论上一定存在，这里再兜一层
		BaseMaterial = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud.m_SimpleVolumetricCloud"));
	}

	if (BaseMaterial)
	{
		// MID outer 用 this：Director 销毁时随之回收，不污染引擎共享 MIC
		CloudMID = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		Clouds->SetMaterial(CloudMID);
	}
}

//============================================================
// 昼夜天体
//============================================================
void AFCSkyDirector::UpdateCelestialBodies()
{
	if (!CelestialLight)
	{
		return;
	}

	// ---- 太阳位置：标准日出（sunrise）方程 ----
	// 时角 H = 15°·(t-12)，正午 H=0
	const double H = FMath::DegreesToRadians(15.0 * (double)(TimeOfDay - 12.0));
	const double LatR = FMath::DegreesToRadians(FC_SunLatitudeDeg);
	const double DecR = FMath::DegreesToRadians(FC_SunDeclinationDeg);

	const double SinLat = FMath::Sin(LatR), CosLat = FMath::Cos(LatR);
	const double SinDec = FMath::Sin(DecR), CosDec = FMath::Cos(DecR);
	const double CosH = FMath::Cos(H), SinH = FMath::Sin(H);

	// 当地 ENU 分量：
	//   sin(h) = sinφ·sinδ + cosφ·cosδ·cosH
	//   南向 S  = sinφ·cosδ·cosH − cosφ·sinδ
	//   东向 E  = −cosδ·sinH（上午 H<0 → E>0）
	const double Up = SinLat * SinDec + CosLat * CosDec * CosH;
	const double South = SinLat * CosDec * CosH - CosLat * SinDec;
	const double East = -CosDec * SinH;

	// 映射 UE 世界：+X=东，+Y=南，+Z=上（由 FCTwinPawn 约定核实）
	FVector SunVec((float)East, (float)South, (float)Up);
	SunVec = SunVec.GetSafeNormal();

	const float Elevation = FMath::RadiansToDegrees(
		FMath::Asin(FMath::Clamp((float)Up, -1.f, 1.f)));

	// ---- 昼夜权重（地平附近 smoothstep，互不跳变） ----
	const float DayW = SmoothEdge(-2.f, 8.f, Elevation);    // 太阳：-2° 起亮，8° 满
	const float NightW = 1.f - SmoothEdge(-8.f, 0.f, Elevation); // 月亮：-8° 满，0° 退尽

	// ---- 太阳颜色：晨昏暖橙 → 正午亮白 ----
	const float NoonW = SmoothEdge(0.f, 45.f, Elevation);
	const FLinearColor HorizonColor(1.0f, 0.42f, 0.13f); // 暖橙
	const FLinearColor NoonColor(1.0f, 0.95f, 0.86f);    // 亮白微暖
	const FLinearColor SunColor = FLinearColor::LerpUsingHSV(HorizonColor, NoonColor, NoonW);

	const FLinearColor MoonColor(0.48f, 0.58f, 0.75f); // 月光冷蓝灰

	const float SunIntensity = 10.f * DayW;
	const float MoonIntensity = 0.15f * NightW;        // 月光明显弱于日光
	const float TotalIntensity = SunIntensity + MoonIntensity;

	// 加权混合光色（避免切换瞬间颜色跳变）
	const FLinearColor LightColor = TotalIntensity > KINDA_SMALL_NUMBER
		? (SunColor * SunIntensity + MoonColor * MoonIntensity) / TotalIntensity
		: HorizonColor;

	CelestialLight->SetIntensity(TotalIntensity);
	CelestialLight->SetLightColor(LightColor);

	// ---- 太阳/月亮方向切换 ----
	// 月亮取太阳对侧（满月模型）：夜间光向 = SunVec（= −MoonVec）
	const bool bIsNight = Elevation < 0.f;
	const FVector LightForward = bIsNight ? SunVec : -SunVec;
	CelestialLight->SetWorldRotation(LightForward.Rotation());

	// SkyAtmosphere 支持两盏大气光：日间太阳 index=0，夜间月亮 index=1
	CelestialLight->SetAtmosphereSunLightIndex(bIsNight ? 1 : 0);

	// ---- SkyLight 强度 = 关卡基准 × 天气系数 × 昼夜系数 ----
	if (SkyLight)
	{
		const float NightF = 1.f - SmoothEdge(-8.f, 2.f, Elevation); // 夜 0.10 … 昼 1
		const float DayNightSky = FMath::Lerp(0.10f, 1.f, 1.f - NightF);
		SkyLight->SetIntensity(BaseSkyLightIntensity * WeatherSkyScale * DayNightSky);
	}

	// 自动曝光会把微弱月光放大到接近白天。按太阳高度平滑压低夜间曝光，
	// 保留月光照亮的建筑轮廓，同时让夜空与白天有清楚的明暗区别。
	if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		if (UCameraComponent* PlayerCamera = PlayerPawn->FindComponentByClass<UCameraComponent>())
		{
			PlayerCamera->PostProcessSettings.bOverride_AutoExposureBias = DayW < 0.999f;
			if (DayW < 0.999f)
			{
				PlayerCamera->PostProcessSettings.AutoExposureBias = FMath::Lerp(-4.f, 0.f, DayW);
			}
			PlayerCamera->PostProcessBlendWeight = 1.f;
		}
	}
}

//============================================================
// 天气预设
//============================================================
void AFCSkyDirector::ApplyWeatherPreset()
{
	// 四档参数（云材质 scalar 名经 MCP 只读探查核实：
	// Cloud_GlobalCoverage / Cloud_GlobalDensity / StormClouds）
	struct FWeatherParams
	{
		float CloudCoverage; // 云覆盖 0..1
		float CloudDensity;  // 体积密度
		float Storm;         // 暴风雨云形态 0..1
		float FogDensity;    // 高度雾密度
		float SkyScale;      // 天光强度系数
		FLinearColor FogColor;
	};

	static const FWeatherParams Params[4] =
	{
		// 晴：少量白云、薄雾
		{ 0.15f, 0.60f, 0.0f, 0.00018f, 1.00f, FLinearColor(0.42f, 0.47f, 0.53f) },
		// 多云：云量过半
		{ 0.50f, 0.90f, 0.0f, 0.00040f, 0.75f, FLinearColor(0.45f, 0.48f, 0.51f) },
		// 阴：低云漫天、雾明显
		{ 0.80f, 1.30f, 0.15f, 0.00120f, 0.45f, FLinearColor(0.42f, 0.44f, 0.46f) },
		// 雨：风暴云 + 浓雾 + 冷灰天光
		{ 0.95f, 1.70f, 1.0f, 0.00180f, 0.30f, FLinearColor(0.38f, 0.40f, 0.43f) }
	};

	const FWeatherParams& Q = Params[(int32)Weather];
	WeatherSkyScale = Q.SkyScale;

	if (CloudMID)
	{
		CloudMID->SetScalarParameterValue(FName("Cloud_GlobalCoverage"), Q.CloudCoverage);
		CloudMID->SetScalarParameterValue(FName("Cloud_GlobalDensity"), Q.CloudDensity);
		CloudMID->SetScalarParameterValue(FName("StormClouds"), Q.Storm);
	}

	if (Fog)
	{
		Fog->SetFogDensity(Q.FogDensity);
		Fog->SetFogInscatteringColor(Q.FogColor);
	}

	// ---- 雨效开关 ----
	if (Weather == EFCWeather::Rain)
	{
		if (!bRainMeshBuilt)
		{
			InitRainDrops();
			bRainMeshBuilt = true;
		}
		RainMesh->SetVisibility(true);
	}
	else
	{
		StopRain();
	}
}

//============================================================
// SkyLight recapture（节流，避免拖动/播放时卡顿）
//============================================================
void AFCSkyDirector::TryRecaptureSky(bool bForce)
{
	if (!SkyLight)
	{
		return;
	}

	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (bForce || (Now - LastRecaptureSeconds) >= 0.25)
	{
		LastRecaptureSeconds = Now;
		SkyLight->RecaptureSky();
	}
}

//============================================================
// 程序化降雨
//============================================================
void AFCSkyDirector::InitRainDrops()
{
	RainDrops.Empty(FC_RainDropCount);
	RainDrops.AddZeroed(FC_RainDropCount);

	for (FRainDrop& Drop : RainDrops)
	{
		Drop.Offset = FVector(
		FMath::FRandRange(-FC_RainHalfExtent, FC_RainHalfExtent),
		FMath::FRandRange(-FC_RainHalfExtent, FC_RainHalfExtent),
		FMath::FRandRange(-FC_RainHalfExtent, FC_RainHalfExtent));
		Drop.FallSpeed = FMath::FRandRange(28.f, 42.f); // 雨滴末速（m/s）
		Drop.Length = FMath::FRandRange(0.8f, 1.8f);
	}

	// 以当前相机位置作为首个雨盒中心
	if (APlayerCameraManager* CM = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		RainCenter = CM->GetCameraLocation();
	}

	// 立即建一次 section（含三角形索引），之后全部走更快的 UpdateMeshSection
	UpdateRain(0.f);
}

void AFCSkyDirector::UpdateRain(float DeltaSeconds)
{
	if (RainDrops.Num() == 0)
	{
		return;
	}

	// ---- 雨盒中心：取玩家相机位置，0.1s 节流校准 ----
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if ((Now - LastCenterCalibrateSeconds) >= 0.1)
	{
		LastCenterCalibrateSeconds = Now;
		if (APlayerCameraManager* CM = UGameplayStatics::GetPlayerCameraManager(this, 0))
		{
			RainCenter = CM->GetCameraLocation();
		}
	}

	// 相机朝向：用于让雨线四边形面向相机
	FVector CamRight = FVector(1.f, 0.f, 0.f);
	if (APlayerCameraManager* CM = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		CamRight = FRotationMatrix(CM->GetCameraRotation()).GetScaledAxis(EAxis::Y);
	}

	// 下落方向带少量风偏（斜雨）
	const FVector FallDir = FVector(0.15f, 0.0f, -1.0f).GetSafeNormal();

	// 相机右向量投影到垂直于下落方向的平面，保证 billboard 宽度方向恒定
	FVector LineRight = CamRight - FallDir * (CamRight | FallDir);
	LineRight = LineRight.GetSafeNormal();

	constexpr float HalfWidth = 0.025f; // 雨线半宽 2.5cm
	constexpr float BoxSize = FC_RainHalfExtent * 2.f;

	// ---- CPU 推进 + wrap ----
	if (DeltaSeconds > 0.f)
	{
		for (FRainDrop& Drop : RainDrops)
		{
			Drop.Offset += FallDir * (Drop.FallSpeed * DeltaSeconds);

			// 离开盒子即 wrap（各轴独立）；雨盒中心每 0.1s 跟随相机，
			// 快速飞行时雨滴在盒边逐个换位，分布始终均匀。
			if (Drop.Offset.X > FC_RainHalfExtent) Drop.Offset.X -= BoxSize;
			else if (Drop.Offset.X < -FC_RainHalfExtent) Drop.Offset.X += BoxSize;
			if (Drop.Offset.Y > FC_RainHalfExtent) Drop.Offset.Y -= BoxSize;
			else if (Drop.Offset.Y < -FC_RainHalfExtent) Drop.Offset.Y += BoxSize;
			if (Drop.Offset.Z < -FC_RainHalfExtent) Drop.Offset.Z += BoxSize;
			else if (Drop.Offset.Z > FC_RainHalfExtent) Drop.Offset.Z -= BoxSize;
		}
	}

	// ---- 写顶点（每滴 4 顶点 / 2 三角形） ----
	const int32 DropCount = RainDrops.Num();
	const int32 VertexCount = DropCount * 4;

	TArray<FVector> Vertices;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FColor> Colors;
	TArray<FProcMeshTangent> Tangents;

	Vertices.SetNumUninitialized(VertexCount);
	Normals.SetNumUninitialized(VertexCount);
	UVs.SetNumUninitialized(VertexCount);
	Colors.SetNumUninitialized(VertexCount);
	Tangents.SetNumUninitialized(VertexCount);

	const FColor RainColor(200, 215, 230);

	for (int32 i = 0; i < DropCount; ++i)
	{
		const FRainDrop& Drop = RainDrops[i];

		const FVector Top = RainCenter + Drop.Offset;
		const FVector Bottom = Top + FallDir * Drop.Length;
		const FVector R = LineRight * HalfWidth;

		const int32 V = i * 4;
		Vertices[V + 0] = Top + R;
		Vertices[V + 1] = Top - R;
		Vertices[V + 2] = Bottom + R;
		Vertices[V + 3] = Bottom - R;

		// Unlit 材质不读法线，填统一值即可（数组长度必须与顶点一致）
		for (int32 K = 0; K < 4; ++K)
		{
			Normals[V + K] = FVector(0.f, 0.f, 1.f);
			Tangents[V + K] = FProcMeshTangent(1.f, 0.f, 0.f);
			Colors[V + K] = RainColor;
		}
		UVs[V + 0] = FVector2D(0.f, 0.f);
		UVs[V + 1] = FVector2D(1.f, 0.f);
		UVs[V + 2] = FVector2D(0.f, 1.f);
		UVs[V + 3] = FVector2D(1.f, 1.f);
	}

	if (!bRainMeshBuilt)
	{
		TArray<int32> Triangles;
		Triangles.SetNumUninitialized(DropCount * 6);
		for (int32 i = 0; i < DropCount; ++i)
		{
			const int32 V = i * 4;
			const int32 T = i * 6;
			Triangles[T + 0] = V + 0;
			Triangles[T + 1] = V + 2;
			Triangles[T + 2] = V + 1;
			Triangles[T + 3] = V + 1;
			Triangles[T + 4] = V + 2;
			Triangles[T + 5] = V + 3;
		}

		// 单 section、无碰撞（bCreateCollision=false）
		RainMesh->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, Colors, Tangents, false);
	}
	else
	{
		RainMesh->UpdateMeshSection(0, Vertices, Normals, UVs, Colors, Tangents);
	}
}

void AFCSkyDirector::StopRain()
{
	if (RainMesh)
	{
		RainMesh->SetVisibility(false);
	}
}

UMaterial* AFCSkyDirector::CreateRainMaterial()
{
	return LoadObject<UMaterial>(nullptr, TEXT("/Game/ForbiddenCity/Materials/M_FCRain.M_FCRain"));
}
