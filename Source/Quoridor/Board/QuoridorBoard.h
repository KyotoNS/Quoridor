
#pragma once

#include "CoreMinimal.h"
#include "Quoridor/Tile/Tile.h"
#include "GameFramework/Actor.h"
#include "QuoridorBoard.generated.h"

class ATile;
class AQuoridorPawn;

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
	
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Gameplay")
	int32 CurrentPlayerTurn = 1;

	UPROPERTY(VisibleInstanceOnly)
	AQuoridorPawn* SelectedPawn;

	

	void ClearSelection();
	void SpawnWall(FVector Location, FRotator Rotation, FVector Scale);

private:
	void SpawnPawn(FIntPoint GridPosition, int32 PlayerNumber);
};