// Tile.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Tile.generated.h"

UCLASS()
class QUORIDOR_API ATile : public AActor
{
	GENERATED_BODY()
    
public:    
	ATile();

	UFUNCTION(BlueprintCallable)
	void SetGridPosition(int32 X, int32 Y) { GridX = X; GridY = Y; }
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	AQuoridorPawn* PawnOnTile;

	UFUNCTION(BlueprintCallable)
	bool IsOccupied() const { return PawnOnTile != nullptr; }

	UFUNCTION(BlueprintCallable)
	void SetPawnOnTile(AQuoridorPawn* Pawn) { PawnOnTile = Pawn; }

	UFUNCTION(BlueprintCallable)
	bool IsAdjacent(const ATile* OtherTile) const;
	
protected:
	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* TileMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 GridX;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 GridY;
};