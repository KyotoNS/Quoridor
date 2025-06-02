#pragma once

#include "CoreMinimal.h"
#include "Quoridor/Board/QuoridorBoard.h"
#include "MinimaxEngine.h"
#include "Async/Async.h"
#include "MinimaxBoardAI.generated.h"

/**
 * A minimal AI subclass that captures board state, calls the pure-data
 * engine, then executes the returned action.
 */
UCLASS()
class QUORIDOR_API AMinimaxBoardAI : public AQuoridorBoard
{
	GENERATED_BODY()

public:
	AMinimaxBoardAI();
	void BeginPlay();

	virtual void Tick(float DeltaTime) override;
	void RunMinimaxForParallelAlphaBeta(int32 Player);
	bool ForcePlaceWallForAI(int32 SlotX, int32 SlotY, int32 Length, bool bHorizontal);

	void RunMinimaxForParallel(int32 Player);
	void RunMinimaxForAlphaBeta(int32 Player);
	UPROPERTY()
	bool bIsAITurnRunning = false;
	float ElapsedTime;
	bool bDelayPassed;
	int AI1Player;
	int AI2Player;
	bool bMinimaxInProgress = false;
	bool bDelayBeforeNextAI = false;
	float AITurnDelayTimer = 3.0f;
	float AITurnDelayDuration = 3.0f; // Change to any delay you want

	
private:
	

	// Execute the chosen action on the board
	void ExecuteAction(const FMinimaxAction& Act);
};
