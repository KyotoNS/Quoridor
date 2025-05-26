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
	void RunMinimax(int32 Player);

	void RunMinimaxForParallel(int32 Player);
	void RunMinimaxForAlphaBeta(int32 Player);
	UPROPERTY()
	bool bIsAITurnRunning = false;
	float ElapsedTime;
	bool bDelayPassed;
	int AI1Player;
	int AI2Player;
	bool bMinimaxInProgress = false;
	
private:
	// Future to hold the async result
	TFuture<FMinimaxAction> MinimaxFuture;
	

	// Execute the chosen action on the board
	void ExecuteAction(const FMinimaxAction& Act);
};
