#pragma once

#include "CoreMinimal.h"
#include "Quoridor/Board/QuoridorBoard.h"
#include "MinimaxBoardAI.generated.h"

// Struktur untuk merepresentasikan langkah (perpindahan pion atau tembok)
USTRUCT()
struct FQuoridorMove
{
	GENERATED_BODY()

	// Jenis langkah: true untuk perpindahan pion, false untuk tembok
	UPROPERTY()
	bool bIsPawnMove;

	// Untuk perpindahan pion: koordinat tujuan
	UPROPERTY()
	FIntPoint PawnMove;

	// Untuk tembok: slot awal, panjang, dan orientasi
	UPROPERTY()
	AWallSlot* WallSlot;

	UPROPERTY()
	int32 WallLength;

	UPROPERTY()
	EWallOrientation WallOrientation;

	FQuoridorMove()
		: bIsPawnMove(true), PawnMove(FIntPoint(0, 0)), WallSlot(nullptr), WallLength(0), WallOrientation(EWallOrientation::Horizontal)
	{}
};

UCLASS()
class QUORIDOR_API AMinimaxBoardAI : public AQuoridorBoard
{
	GENERATED_BODY()

public:
	AMinimaxBoardAI();

	// Jalankan giliran AI (Pemain 2)
	void RunMinimaxForPlayer2();

protected:
	virtual void BeginPlay() override;

	// Pilih langkah terbaik menggunakan Minimax
	FQuoridorMove GetBestMoveForAI();

	// Fungsi Minimax
	float Minimax(int32 Depth, bool bIsMaximizing, AQuoridorPawn* Player1Pawn, AQuoridorPawn* Player2Pawn);

	// Heuristik untuk mengevaluasi status papan
	float EvaluateBoard(AQuoridorPawn* Player1Pawn, AQuoridorPawn* Player2Pawn);

	// Mendapatkan semua langkah legal untuk pemain tertentu
	TArray<FQuoridorMove> GetLegalMovesForPlayer(int32 PlayerNumber);

	// Menerapkan langkah sementara untuk simulasi
	bool ApplyMove(const FQuoridorMove& Move, int32 PlayerNumber);

	// Mengembalikan status papan ke sebelumnya
	void UndoMove(const FQuoridorMove& Move, int32 PlayerNumber, AQuoridorPawn* Pawn, TArray<AWallSlot*> AffectedSlots, TArray<AActor*> SpawnedWalls);
	int32 GetShortestPathLength(AQuoridorPawn* Pawn);

	// Kedalaman pencarian untuk Minimax
	UPROPERTY(EditAnywhere, Category = "AI")
	int32 MaxDepth = 2;

	FTimerHandle AIDelayHandle;
};