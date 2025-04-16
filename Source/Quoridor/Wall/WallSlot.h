#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WallSlot.generated.h"

// Forward declaration untuk AQuoridorBoard
class AQuoridorBoard;

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

	UPROPERTY(VisibleAnywhere)
	class UBoxComponent* BoxCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "References")
	AQuoridorBoard* Board;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 GridX;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 GridY;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EWallOrientation Orientation;

	UPROPERTY(VisibleAnywhere)
	bool bIsOccupied = false;

	UFUNCTION(BlueprintCallable)
	bool CanPlaceWall() const { return !bIsOccupied; }

	UFUNCTION(BlueprintCallable)
	void SetOccupied(bool bOccupied) { bIsOccupied = bOccupied; }

	UFUNCTION(BlueprintCallable)
	void SetGridPosition(int32 X, int32 Y) { GridX = X; GridY = Y; }
	
};