
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Tile.generated.h"

UCLASS()
class QUORIDOR_API ATile : public AActor
{
	GENERATED_BODY()
    
public:    
	// Constructor
	ATile();

	// Sets the grid position (X, Y) of the tile
	UFUNCTION(BlueprintCallable)
	void SetGridPosition(int32 X, int32 Y) { GridX = X; GridY = Y; }

	// Pointer to the pawn currently occupying the tile (if any)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	AQuoridorPawn* PawnOnTile;

	// Checks if the tile is occupied by a pawn
	UFUNCTION(BlueprintCallable)
	bool IsOccupied() const { return PawnOnTile != nullptr; }

	// Assigns a pawn to this tile
	UFUNCTION(BlueprintCallable)
	void SetPawnOnTile(AQuoridorPawn* Pawn) { PawnOnTile = Pawn; }

	// Determines if this tile is adjacent to another tile
	UFUNCTION(BlueprintCallable)
	bool IsAdjacent(const ATile* OtherTile) const;
	
protected:
	// The visual representation of the tile
	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* TileMesh;

	// X coordinate on the grid
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 GridX;

	// Y coordinate on the grid
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 GridY;
};
