﻿
#pragma once

#include "CoreMinimal.h"
#include "Quoridor/Tile/Tile.h"
#include "GameFramework/Actor.h"
#include "Quoridor/Wall/WallSlot.h"
#include "QuoridorBoard.generated.h"
	
class ATile;
class AQuoridorPawn;
class AWallSlot;


UENUM(BlueprintType)
enum class EPlayerType : uint8
{
	Human UMETA(DisplayName = "Human"),
	AI UMETA(DisplayName = "AI")
};

UCLASS()
class QUORIDOR_API AQuoridorBoard : public AActor
{
	GENERATED_BODY()
    
public:    
	AQuoridorBoard();
	UPROPERTY(BlueprintReadWrite, VisibleInstanceOnly)
	AQuoridorPawn* SelectedPawn;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Settings")
	int32 AI1_AlgorithmChoice;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Settings")
	int32 AI2_AlgorithmChoice;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Settings")
	int32 Depth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Settings")
	bool bAI1IsPlayer1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	USoundBase* PawnMoveSound;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	USoundBase* WallClickSound;
	UPROPERTY(BlueprintReadOnly)
	TArray<EPlayerType> PlayerTypes;

	UPROPERTY(BlueprintReadOnly)
	int32 CurrentPlayerTurn = 1;
	
	UPROPERTY(EditDefaultsOnly, Category = "Board")
	int32 GridSize = 9;
	
	UPROPERTY(BlueprintReadOnly)
	int32 TurnCount = 0;
	
	TArray<TArray<ATile*>> Tiles;
	UFUNCTION(BlueprintCallable)
	void Debug_PrintOccupiedWalls() const;
	

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TArray<AQuoridorPawn*> Pawns;
	
	UPROPERTY()
	TArray<AWallSlot*> WallSlots;
	
	
	UPROPERTY(VisibleAnywhere)
	TArray<AWallSlot*> HorizontalWallSlots;

	/** Menyimpan status apakah permainan telah berakhir atau masih berjalan. */
	UPROPERTY(BlueprintReadOnly, Category = "Quoridor|Game State")
	bool bIsGameFinished;
	UFUNCTION(BlueprintPure, Category = "Quoridor|Game State")
	bool IsGameFinished() const;
	UFUNCTION(BlueprintPure, Category = "Quoridor|Game State")
	int32 GetWinningTurn() const;
	/** Menyimpan nomor giliran (TurnCount) saat permainan dimenangkan. */
	UPROPERTY(BlueprintReadOnly, Category = "Quoridor|Game State")
	int32 WinningTurn;
	
	UPROPERTY(VisibleAnywhere)
	TArray<AWallSlot*> VerticalWallSlots;
	AWallSlot* FindWallSlotAt(int32 X, int32 Y, EWallOrientation Orientation)const;
	
	UFUNCTION(BlueprintCallable)
	bool IsPathAvailableForPawn(AQuoridorPawn* Pawn);
	ATile* GetTileAtWorldPosition(const FVector& WorldPosition);

	UFUNCTION(BlueprintCallable)
	void HandleWin(int32 WinningPlayer);
	void PrintLastComputedPath(int32 PlayerNumber);
	void SaveAIStatsToTextFile(int32 WinningPlayer, const FString& AIType, int32 TotalTurns, float ThinkingTime, const TArray<int32>& WallInventory);
	void SaveLosingAIStatsToTextFile(int32 WinningPlayer, const FString& AIType, int32 TotalTurns, float ThinkingTime, const TArray<int32>& WallInventory);
	void LogBoardState();

	UFUNCTION(BlueprintCallable)
	AQuoridorPawn* GetPawnForPlayer(int32 PlayerNumber);
	TMap<int32, TArray<FIntPoint>> CachedPaths;  // PlayerNumber -> path

protected:
	virtual void BeginPlay() override;

	void UpdateAllTileConnections();

	UPROPERTY(EditDefaultsOnly, Category = "Board")
	TSubclassOf<ATile> TileClass;
	
	UPROPERTY(EditDefaultsOnly, Category = "Board")
	float TileSize = 130.0f;

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
	UFUNCTION(BlueprintCallable, Category="Walls")
	int32 GetPlayerTotalWallCount(int32 PlayerNumber) const;
	UFUNCTION(BlueprintCallable, Category="Walls")
	int32 GetWallCountForPlayer(int32 PlayerNum, int32 WallLength) const;
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