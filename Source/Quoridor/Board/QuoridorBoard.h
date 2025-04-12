
#pragma once

#include "CoreMinimal.h"
#include "Quoridor/Tile/Tile.h"
#include "GameFramework/Actor.h"
#include "Quoridor/Wall/WallSlot.h"
#include "QuoridorBoard.generated.h"

class ATile;
class AQuoridorPawn;
class AWallSlot;
UCLASS()
class QUORIDOR_API AQuoridorBoard : public AActor
{
	GENERATED_BODY()
    
public:    
	AQuoridorBoard();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, Category = "Board")
	TSubclassOf<ATile> TileClass;
	
	UPROPERTY(EditDefaultsOnly, Category = "Board")
	float TileSize = 120.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Board")
	int32 GridSize = 9;

	UPROPERTY(EditDefaultsOnly, Category = "Pawns")
	TSubclassOf<AQuoridorPawn> PawnClass;
	
	UFUNCTION(BlueprintCallable, Category = "Input")
	void HandlePawnClick(AQuoridorPawn* ClickedPawn);
    
	UFUNCTION(BlueprintCallable, Category = "Input")
	void HandleTileClick(ATile* ClickedTile);
	
	TArray<TArray<ATile*>> Tiles;

	UPROPERTY(EditDefaultsOnly, Category = "Board")
	TSubclassOf<AActor> WallAroundClass;

	UPROPERTY(EditDefaultsOnly, Category = "Walls")
    TSubclassOf<AWallSlot> WallSlotClass;
    
    UPROPERTY()
    TArray<AWallSlot*> WallSlots;

	UPROPERTY(EditDefaultsOnly, Category = "Walls")
	TSubclassOf<AActor> WallPlacementClass;
    
    UFUNCTION(BlueprintCallable)
    bool TryPlaceWall(AWallSlot* StartSlot, int32 WallLength);
	
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Gameplay")
	int32 CurrentPlayerTurn = 1;

	bool bIsPlacingWall = false;
	int32 PendingWallLength = 0;

	UPROPERTY(VisibleInstanceOnly)
	AQuoridorPawn* SelectedPawn;
	
	UFUNCTION(BlueprintCallable)
	void StartWallPlacement(int32 WallLength);

	UFUNCTION(BlueprintCallable, Category="Walls")
	int32 GetCurrentPlayerWallCount(int32 WallLength) const;

	void ClearSelection();
	void SpawnWall(FVector Location, FRotator Rotation, FVector Scale);
	AWallSlot* FindWallSlotAt(int32 X, int32 Y, EWallOrientation Orientation);
	void HandleWallSlotClick(AWallSlot* ClickedSlot);

private:
	void SpawnPawn(FIntPoint GridPosition, int32 PlayerNumber);
};