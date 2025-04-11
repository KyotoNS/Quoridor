#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WallSlot.generated.h"

UENUM(BlueprintType)
enum class EWallOrientation : uint8
{
	Horizontal,
	Vertical
};

UCLASS()
class QUORIDOR_API AWallSlot : public AActor
{
	GENERATED_BODY()
	
public:	
	AWallSlot();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EWallOrientation Orientation;

	UPROPERTY(VisibleAnywhere)
	bool bIsOccupied = false;

	UFUNCTION(BlueprintCallable)
	bool CanPlaceWall() const { return !bIsOccupied; }

	UFUNCTION(BlueprintCallable)
	void SetOccupied(bool bOccupied) { bIsOccupied = bOccupied; }
};
