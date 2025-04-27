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

	UE_LOG(LogTemp, Warning, TEXT("AI has wall counts: L1=%d, L2=%d, L3=%d"),
		AI->GetWallCountOfLength(1), AI->GetWallCountOfLength(2), AI->GetWallCountOfLength(3));

	bool bDoWall = true; // Force AI to always try placing wall first for testing

	if (bDoWall && AI->PlayerWalls.Num() > 0)
	{
		bool bWallPlaced = TryPlaceRandomWallForAI();
		if (!bWallPlaced)
		{
			UE_LOG(LogTemp, Warning, TEXT("AI failed placing wall. Will move pawn instead."));
			FIntPoint BestMove = GetBestMoveForAI();
			AI->MovePawn(BestMove.X, BestMove.Y);
		}
	}
	else
	{
		FIntPoint BestMove = GetBestMoveForAI();
		AI->MovePawn(BestMove.X, BestMove.Y);
	}

	CurrentPlayerTurn = 1;
	UE_LOG(LogTemp, Warning, TEXT("AI Turn ended. Switching to Player %d"), CurrentPlayerTurn);
}


bool AMinimaxBoardAI::TryPlaceRandomWallForAI()
{
	AQuoridorPawn* AI = GetPawnForPlayer(2);
	if (!AI || AI->PlayerWalls.Num() == 0) return false;

	TArray<AWallSlot*> ValidSlots;
	for (AWallSlot* Slot : WallSlots)
	{
		if (Slot && Slot->CanPlaceWall())
		{
			ValidSlots.Add(Slot);
		}
	}
	if (ValidSlots.Num() == 0) return false;

	TArray<int32> AvailableLengths;
	for (int32 L = 1; L <= 3; ++L)
	{
		if (AI->GetWallCountOfLength(L) > 0)
		{
			AvailableLengths.Add(L);
		}
	}
	if (AvailableLengths.Num() == 0) return false;

	const int32 MaxAttempts = 20;
	for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
	{
		AWallSlot* RandomSlot = ValidSlots[FMath::RandRange(0, ValidSlots.Num() - 1)];
		int32 RandomLength = AvailableLengths[FMath::RandRange(0, AvailableLengths.Num() - 1)];

		UE_LOG(LogTemp, Warning, TEXT("❌ AI TRY: Wall %d at (%d,%d)"),
			RandomLength, RandomSlot->GridX, RandomSlot->GridY);

		bIsPlacingWall = true;
		bool bSuccess = TryPlaceWall(RandomSlot, RandomLength);
		bIsPlacingWall = false;

		if (bSuccess)
		{
			UE_LOG(LogTemp, Warning, TEXT("✅ AI SUCCESS: Placed wall length %d at (%d,%d)"),
				RandomLength, RandomSlot->GridX, RandomSlot->GridY);
			return true;
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("❌ AI gave up placing wall after all attempts."));
	return false;
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
