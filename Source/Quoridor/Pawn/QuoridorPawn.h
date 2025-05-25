//QuoridorPawn.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Quoridor/Board/QuoridorBoard.h"
#include "Quoridor/Wall/WallDefinition.h"
#include "QuoridorPawn.generated.h"

class ATile;

UCLASS()
class QUORIDOR_API AQuoridorPawn : public APawn
{
	GENERATED_BODY()

public:
	AQuoridorPawn();
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	AQuoridorBoard* BoardReference;

	UFUNCTION(BlueprintCallable)
	void MoveToTile(ATile* NewTile, bool bForceMove);

	UFUNCTION(BlueprintCallable)
	bool CanMoveToTile(const ATile* TargetTile) const;

	UFUNCTION(BlueprintCallable)
	bool RemoveWallOfLength(int32 INT32);

	UFUNCTION(BlueprintCallable)
	bool HasWallOfLength(int32 Length) const;
	void SimulateMovePawn(int32 NewX, int32 NewY);
	void RevertSimulatedMove();
	struct FPawnSimulationState
	{
		int32    X;
		int32    Y;
		ATile*   Tile;
	};

	// A stack of snapshots
	TArray<FPawnSimulationState> SimulationStack;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 GridX;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 GridY;

	UFUNCTION(BlueprintCallable)
	void SetGridPosition(int32 X, int32 Y);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	ATile* CurrentTile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Player")
	int32 PlayerNumber;

	UPROPERTY(BlueprintReadOnly)
	TArray<FWallDefinition> PlayerWalls;

	UFUNCTION(BlueprintCallable)
	bool HasRemainingWalls() const { return PlayerWalls.Num() > 0; }
	
	UFUNCTION(BlueprintCallable)
	void InitializePawn(ATile* StartTile, AQuoridorBoard* Board);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 GridSize;
	
	UFUNCTION(BlueprintCallable)
	int32 GetWallCountOfLength(int32 Length) const;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	EWallOrientation LastWallOrientation = EWallOrientation::Horizontal;
	
	UFUNCTION(BlueprintCallable)
	void MovePawn(int32 NewX, int32 NewY);
	
	UFUNCTION(BlueprintCallable)
	ATile* GetTile() const { return CurrentTile; }


protected:
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly, Category = "Pawn")
	UStaticMeshComponent* PawnMesh;

	UPROPERTY(VisibleAnywhere)
	class UBoxComponent* SelectionCollision;
};