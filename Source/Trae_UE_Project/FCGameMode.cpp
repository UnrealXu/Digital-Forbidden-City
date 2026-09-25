#include "FCGameMode.h"

#include "FCTwinPawn.h"
#include "FCPoiMarker.h"
#include "FCTaiheExploder.h"
#include "FCSkyDirector.h"
#include "FCData.h"
#include "Engine/World.h"

AFCGameMode::AFCGameMode()
{
	DefaultPawnClass = AFCTwinPawn::StaticClass();

	// Marker 在默认的 PrePhysics 组内登记候选，GameMode 在 PostPhysics
	// 统一排序剔除，保证同一帧内“重点优先、由近到远”的确定性结果。
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
}

void AFCGameMode::BeginPlay()
{
	Super::BeginPlay();

	// 复位标签全局开关：上局若在信息卡打开时结束，静态变量会残留为隐藏。
	AFCPoiMarker::SetAllMarkersHidden(false);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	for (int32 i = 0; i < G_FCPoiCount; ++i)
	{
		AFCPoiMarker* Marker = GetWorld()->SpawnActor<AFCPoiMarker>(
			AFCPoiMarker::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
		if (Marker)
		{
			Marker->Init(i);
		}
	}

	// 太和殿拆解器（自行在 BeginPlay 中收集 Taihe_ 部件，无需 Init）
	GetWorld()->SpawnActor<AFCTaiheExploder>(
		AFCTaiheExploder::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);

	GetWorld()->SpawnActor<AFCSkyDirector>(
		AFCSkyDirector::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
}

void AFCGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	AFCPoiMarker::ResolveScreenCulling();
}
