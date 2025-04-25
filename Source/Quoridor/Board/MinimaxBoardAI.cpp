#include "MinimaxBoardAI.h"
#include "Quoridor/Pawn/QuoridorPawn.h"
#include "Quoridor/Tile/Tile.h"
#include "TimerManager.h"

AMinimaxBoardAI::AMinimaxBoardAI()
{
	// Konstruktor default
}

void AMinimaxBoardAI::BeginPlay()
{
	Super::BeginPlay();

	// Mulai AI pertama kali jika Player 2 langsung jalan (opsional)
	if (CurrentPlayerTurn == 2)
	{
		GetWorldTimerManager().SetTimer(AIDelayHandle, this, &AMinimaxBoardAI::RunMinimaxForPlayer2, 1.0f, false);
	}
}

void AMinimaxBoardAI::RunMinimaxForPlayer2()
{
	if (CurrentPlayerTurn != 2) return;

	AQuoridorPawn* AI = GetPawnForPlayer(2);
	if (!AI) return;

	FIntPoint BestMove = GetBestMoveForAI();
	AI->MovePawn(BestMove.X, BestMove.Y);

	// Schedule AI jalan lagi nanti kalau giliran kembali ke Player 2
	GetWorldTimerManager().SetTimer(AIDelayHandle, this, &AMinimaxBoardAI::RunMinimaxForPlayer2, 1.0f, false);
}

FIntPoint AMinimaxBoardAI::GetBestMoveForAI()
{
	AQuoridorPawn* AI = GetPawnForPlayer(2);
	if (!AI || !AI->CurrentTile) return FIntPoint(AI->GridX, AI->GridY);

	TArray<FIntPoint> ValidMoves;

	// Cek semua arah legal
	for (ATile* Neighbor : AI->CurrentTile->ConnectedTiles)
	{
		if (!Neighbor || Neighbor->IsOccupied()) continue;

		ValidMoves.Add(FIntPoint(Neighbor->GridX, Neighbor->GridY));
	}

	// Dummy AI: ambil langkah pertama saja
	if (ValidMoves.Num() > 0)
	{
		return ValidMoves[0];
	}

	// Tidak ada langkah valid
	return FIntPoint(AI->GridX, AI->GridY);
}
