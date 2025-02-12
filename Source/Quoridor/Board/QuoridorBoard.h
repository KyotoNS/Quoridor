#pragma once  // Prevents multiple inclusions

#include "CoreMinimal.h"
#include "Quoridor/Tile/Tile.h"
#include "GameFramework/Actor.h"
#include "QuoridorBoard.generated.h"

class ATile;  // Forward declaration of ATile
class AQuoridorPawn;  // Forward declaration of AQuoridorPawn

UCLASS()
class QUORIDOR_API AQuoridorBoard : public AActor
{
	GENERATED_BODY()
    
public:    
	AQuoridorBoard();  // Constructor

protected:
	virtual void BeginPlay() override;  // Called when the game starts

	// Tile blueprint class used for spawning tiles
	UPROPERTY(EditDefaultsOnly, Category = "Board")
	TSubclassOf<ATile> TileClass;
	
	// Size of each tile in world units
	UPROPERTY(EditDefaultsOnly, Category = "Board")
	float TileSize = 100.0f;

	// Grid size (default: 9x9)
	UPROPERTY(EditDefaultsOnly, Category = "Board")
	int32 GridSize = 9;

	// Pawn blueprint class for spawning pawns
	UPROPERTY(EditDefaultsOnly, Category = "Pawns")
	TSubclassOf<AQuoridorPawn> PawnClass;
	
	// Handles clicks on pawns
	UFUNCTION(BlueprintCallable, Category = "Input")
	void HandlePawnClick(AQuoridorPawn* ClickedPawn);
    
	// Handles clicks on tiles
	UFUNCTION(BlueprintCallable, Category = "Input")
	void HandleTileClick(ATile* ClickedTile);
	
	// 2D array of tile references
	TArray<TArray<ATile*>> Tiles;

	// Keeps track of the current player turn (1 or 2)
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Gameplay")
	int32 CurrentPlayerTurn = 1;

	// Stores the selected pawn
	UPROPERTY(VisibleInstanceOnly)
	AQuoridorPawn* SelectedPawn;

	// Clears selection state after a move
	void ClearSelection();
	
private:
	// Spawns a pawn at a given position for a specific player
	void SpawnPawn(FIntPoint GridPosition, int32 PlayerNumber);
};
