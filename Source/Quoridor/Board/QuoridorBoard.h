#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Quoridor/Wall/QuoridorTypes.h"
#include "QuoridorBoard.generated.h"

class ATile;
class AQuoridorPawn;
class AWall;

UCLASS()
class QUORIDOR_API AQuoridorBoard : public AActor {
	GENERATED_BODY()
    
public:    
	AQuoridorBoard();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly) TSubclassOf<ATile> TileClass;
	UPROPERTY(EditDefaultsOnly) TSubclassOf<AQuoridorPawn> PawnClass;
	UPROPERTY(EditDefaultsOnly) TSubclassOf<AWall> WallClass;
    
	UPROPERTY(VisibleInstanceOnly) TArray<TArray<ATile*>> Tiles;
	UPROPERTY(VisibleInstanceOnly) int32 CurrentPlayerTurn = 1;
	UPROPERTY(VisibleInstanceOnly) AQuoridorPawn* SelectedPawn;

	UFUNCTION(BlueprintCallable) void HandlePawnClick(AQuoridorPawn* ClickedPawn);
	UFUNCTION(BlueprintCallable) void HandleTileClick(ATile* ClickedTile);

private:
	TArray<TArray<TArray<bool>>> WallGrid;
	TArray<AWall*> PlacedWalls;
	int32 PlayerWalls[2] = {10, 10};

	void SpawnPawn(FIntPoint GridPosition, int32 PlayerNumber);
	void ClearSelection();
	bool IsWallPlacementValid(FIntPoint GridPos, EWallOrientation Orientation);
	bool ValidatePathExists(int32 PlayerNumber);
	TArray<FIntPoint> FindPath(int32 StartX, int32 StartY, int32 TargetRow);
    
public:
	UFUNCTION(BlueprintCallable) 
	bool TryPlaceWall(FIntPoint GridPos, EWallOrientation Orientation, int32 PlayerNumber);
    
	UFUNCTION(BlueprintCallable)
	bool HasWallBetween(int32 TileX, int32 TileY, EDirection Direction) const;
	bool IsValidTile(int32 X, int32 Y) const;
};