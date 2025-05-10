#pragma once

#include "CoreMinimal.h"
#include "Quoridor/Board/QuoridorBoard.h"
#include "MinimaxBoardAI.generated.h"

struct FNode
{
	ATile* Tile;
	int32 G;
	FNode(ATile* InTile, int32 InG) : Tile(InTile), G(InG) {}
};
UCLASS()
class QUORIDOR_API AMinimaxBoardAI : public AQuoridorBoard
{
	GENERATED_BODY()

public:
	AMinimaxBoardAI();
	// Jalankan giliran AI (Pemain 2)
	void RunMinimaxForPlayer2();
	virtual void Tick(float DeltaTime) override;
protected:

	int32 EvaluateBoard(); // scoring function
	int32 Minimax(int Depth, bool bIsMaximizing);
	TArray<ATile*> GetAllValidMoves(AQuoridorPawn* Pawn);
	TArray<TPair<AWallSlot*, int32>> GetAllValidWalls();
	int32 CalculateShortestPathLength(AQuoridorPawn* Pawn);

};