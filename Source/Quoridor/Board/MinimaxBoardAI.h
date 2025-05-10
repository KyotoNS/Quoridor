#pragma once

#include "CoreMinimal.h"
#include "Quoridor/Board/QuoridorBoard.h"
#include "MinimaxEngine.h"
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
	UFUNCTION(BlueprintCallable, Category="AI")
	void RunMinimaxForPlayer2();

	virtual void Tick(float DeltaTime) override;
};
