
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
	UPROPERTY(BlueprintReadWrite, VisibleInstanceOnly)
	AQuoridorPawn* SelectedPawn;
	
	UPROPERTY(EditDefaultsOnly, Category = "Board")
	int32 GridSize = 9;
	
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Gameplay")
	int32 CurrentPlayerTurn = 1;
	
	TArray<TArray<ATile*>> Tiles;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TArray<AQuoridorPawn*> Pawns;
	
	UPROPERTY()
	TArray<AWallSlot*> WallSlots;
	
	// Array untuk menyimpan slot tembok
	UPROPERTY(VisibleAnywhere)
	TArray<AWallSlot*> HorizontalWallSlots;
	
	UPROPERTY(VisibleAnywhere)
	TArray<AWallSlot*> VerticalWallSlots;
	
	UFUNCTION(BlueprintCallable)
	AWallSlot* FindWallSlotAt(int32 X, int32 Y, EWallOrientation Orientation);
	
	UFUNCTION(BlueprintCallable)
	bool IsPathAvailableForPawn(AQuoridorPawn* Pawn);

	UFUNCTION(BlueprintCallable)
	void HandleWin(int32 WinningPlayer);
	void LogBoardState();

	UFUNCTION(BlueprintCallable)
	AQuoridorPawn* GetPawnForPlayer(int32 PlayerNumber);
	
protected:
	virtual void BeginPlay() override;

	void UpdateAllTileConnections();

	UPROPERTY(EditDefaultsOnly, Category = "Board")
	TSubclassOf<ATile> TileClass;
	
	UPROPERTY(EditDefaultsOnly, Category = "Board")
	float TileSize = 120.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Pawns")
	TSubclassOf<AQuoridorPawn> PawnClass;
	
	UFUNCTION(BlueprintCallable, Category = "Input")
	void HandlePawnClick(AQuoridorPawn* ClickedPawn);
    
	UFUNCTION(BlueprintCallable, Category = "Input")
	void HandleTileClick(ATile* ClickedTile);

	UPROPERTY(EditDefaultsOnly, Category = "Board")
	TSubclassOf<AActor> WallAroundClass;

	UPROPERTY(EditDefaultsOnly, Category = "Walls")
    TSubclassOf<AWallSlot> WallSlotClass;

	UFUNCTION(BlueprintCallable)
	void ToggleWallOrientation();

	UPROPERTY(EditDefaultsOnly, Category = "Walls")
	TSubclassOf<AActor> WallPlacementClass;
    
    UFUNCTION(BlueprintCallable, Category = "Wall")
    bool TryPlaceWall(AWallSlot* StartSlot, int32 WallLength);

	bool bIsPlacingWall = false;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly)
	int32 PendingWallLength = 0;
	
	UPROPERTY(EditDefaultsOnly, Category="Walls")
	TSubclassOf<AActor> WallPreviewClass;

	UPROPERTY(BlueprintReadWrite)
	EWallOrientation PendingWallOrientation;

	// New: Store border walls for collision checks
	UPROPERTY(VisibleAnywhere, Category = "Board")
	TArray<AActor*> BorderWalls;
	
	UPROPERTY(VisibleAnywhere)
	TArray<AActor*> WallPreviewActors; // New: Store multiple preview actors
	virtual void Tick(float DeltaTime) override;
	UFUNCTION(BlueprintCallable)
	EWallOrientation GetPlayerOrientation(AQuoridorPawn* Pawn) const;
	
	UFUNCTION(BlueprintCallable)
	void StartWallPlacement(int32 WallLength);

	UFUNCTION(BlueprintCallable, Category="Walls")
	int32 GetCurrentPlayerWallCount(int32 WallLength) const;
	UFUNCTION(BlueprintCallable)
	void HideWallPreview();
	
	UFUNCTION(BlueprintCallable)
	void ShowWallPreviewAtSlot(class AWallSlot* HoveredSlot);
	void ClearSelection();
	void SpawnWall(FVector Location, FRotator Rotation, FVector Scale);
	void SimulateWallBlock(const TArray<AWallSlot*>& WallSlotsToSimulate, TMap<TPair<ATile*, ATile*>, bool>& OutRemovedConnections);
	void RevertWallBlock(const TMap<TPair<ATile*, ATile*>, bool>& RemovedConnections);
	

private:
	void SpawnPawn(FIntPoint GridPosition, int32 PlayerNumber);
	UPROPERTY()
	TMap<AQuoridorPawn*, EWallOrientation> PlayerOrientations;

	void LogAllPlayerOrientations();
	
	
};