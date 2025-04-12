#pragma once

#include "CoreMinimal.h"
#include "WallSlot.h"
#include "WallDefinition.generated.h"


USTRUCT(BlueprintType)
struct FWallDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Length; // 1, 2, atau 3 tile

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EWallOrientation Orientation;

	FWallDefinition()
	{
		Length = 2;
		Orientation = EWallOrientation::Horizontal;
	}
};
