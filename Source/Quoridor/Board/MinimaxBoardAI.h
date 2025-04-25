#pragma once

#include "CoreMinimal.h"
#include "Quoridor/Board/QuoridorBoard.h"
#include "MinimaxBoardAI.generated.h"

UCLASS()
class QUORIDOR_API AMinimaxBoardAI : public AQuoridorBoard
{
	GENERATED_BODY()

public:
	AMinimaxBoardAI();

protected:
	virtual void BeginPlay() override;

	// Jalankan giliran AI (Player 2)
	void RunMinimaxForPlayer2();

	// Pilih langkah terbaik (sementara dummy)
	FIntPoint GetBestMoveForAI();

	FTimerHandle AIDelayHandle;
};
