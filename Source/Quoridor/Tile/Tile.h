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
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 GridX;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 GridY;
	
	UFUNCTION(BlueprintCallable)
	void SetGridPosition(int32 X, int32 Y) { GridX = X; GridY = Y; }

	::AQuoridorPawn* GetOccupyingPawn() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	AQuoridorPawn* PawnOnTile;

	UFUNCTION(BlueprintCallable)
	bool IsOccupied() const { return PawnOnTile != nullptr; }

	UFUNCTION(BlueprintCallable)
	void SetPawnOnTile(AQuoridorPawn* Pawn) { PawnOnTile = Pawn; }

	UFUNCTION(BlueprintCallable)
	bool IsAdjacent(const ATile* OtherTile) const;
	
	UPROPERTY()
	TSet<ATile*> ConnectedTiles;

	// Fungsi bantu: Tambahkan dan hapus koneksi
	UFUNCTION()
	void AddConnection(ATile* OtherTile);

	UFUNCTION()
	void RemoveConnection(ATile* OtherTile);

	UFUNCTION()
	void ClearConnections();
	
protected:
	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* TileMesh;

	
};