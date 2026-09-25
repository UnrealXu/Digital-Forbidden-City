#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "FCGameMode.generated.h"

/** Spawns one AFCPoiMarker per baked POI at BeginPlay.
 *  Default pawn is AFCTwinPawn (set in the constructor). */
UCLASS()
class TRAE_UE_PROJECT_API AFCGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AFCGameMode();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
};
