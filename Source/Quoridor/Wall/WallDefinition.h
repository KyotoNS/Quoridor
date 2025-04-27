#pragma once

#include "CoreMinimal.h"
#include "WallSlot.h"
#include "Quoridor/Board/MinimaxBoardAI.h"
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
	FWallDefinition(int32 InLength, EWallOrientation InOrientation)
		: Length(InLength), Orientation(InOrientation)
	{}
};
