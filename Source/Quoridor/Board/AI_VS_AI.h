#pragma once

#include "CoreMinimal.h"
#include "Quoridor/Board/QuoridorBoard.h"
#include "MinimaxEngine.h"
#include "Async/Async.h"
#include "AI_VS_AI.generated.h"

/**
 * A minimal AI subclass that captures board state, calls the pure-data
 * engine, then executes the returned action.
 */
UCLASS()
class QUORIDOR_API AAI_VS_AI : public AQuoridorBoard
{
	GENERATED_BODY()

public:
	AAI_VS_AI();
	void BeginPlay();

	virtual void Tick(float DeltaTime) override;
	bool ForcePlaceWallForAI(int32 SlotX, int32 SlotY, int32 Length, bool bHorizontal);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Settings")
	int32 AI1_AlgorithmChoice;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Settings")
	int32 AI2_AlgorithmChoice;
	void RunMinimax(int32 Player, int32 algo);
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
