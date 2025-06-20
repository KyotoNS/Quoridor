﻿#pragma once

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

	void RunMinimax(int32 Player, int32 algo);
	void RunMinimaxForAlphaBeta(int32 Player);
	UPROPERTY()
	bool bIsAITurnRunning = false;
	float ElapsedTime;
	bool bDelayPassed;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Settings")
	int AI1Player;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Settings")
	int AI2Player;
	
	
	bool bMinimaxInProgress = false;
	bool bDelayBeforeNextAI = false;
	float AITurnDelayTimer = 2.0f;
	float AITurnDelayDuration = 2.0f; // Change to any delay you want

	UFUNCTION(BlueprintCallable, Category = "AI")
	float GetTotalThinkingTimeForPlayer(int32 PlayerNum) const;
	TArray<int32> InitialWallInventory;
	UPROPERTY(BlueprintReadWrite)
	FString AITypeName1 = "MinimaxParallel";
	FString AITypeName2 = "MinimaxParallel";
	// Tracks AI thinking start time
	double ThinkingStartTimeP1 = 0.0;
	double ThinkingStartTimeP2 = 0.0;

	// Total thinking time
	double TotalThinkingTimeP1 = 0.0;
	double TotalThinkingTimeP2 = 0.0;

private:
	

	// Execute the chosen action on the board
	void ExecuteAction(const FMinimaxAction& Act);
};
