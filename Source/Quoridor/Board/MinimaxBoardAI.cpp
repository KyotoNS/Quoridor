#include "MinimaxBoardAI.h"
#include "Quoridor/Pawn/QuoridorPawn.h"
#include "Quoridor/Tile/Tile.h"
#include "Quoridor/Wall/WallSlot.h"
#include "TimerManager.h"

AMinimaxBoardAI::AMinimaxBoardAI()
{
    // Konstruktor default
    MaxDepth = 2;
}

void AMinimaxBoardAI::BeginPlay()
{
    Super::BeginPlay();

    // Mulai AI jika giliran Pemain 2
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

    UE_LOG(LogTemp, Warning, TEXT("AI Turn Started for Player 2"));

    // Dapatkan langkah terbaik menggunakan Minimax
    FQuoridorMove BestMove = GetBestMoveForAI();

    // Terapkan langkah
    if (BestMove.bIsPawnMove)
    {
        AI->MovePawn(BestMove.PawnMove.X, BestMove.PawnMove.Y);
        UE_LOG(LogTemp, Warning, TEXT("AI Moved Pawn to (%d, %d)"), BestMove.PawnMove.X, BestMove.PawnMove.Y);
    }
    else
    {
        bIsPlacingWall = true;
        PendingWallLength = BestMove.WallLength;
        PendingWallOrientation = BestMove.WallOrientation;
        TryPlaceWall(BestMove.WallSlot, BestMove.WallLength);
        bIsPlacingWall = false;
        UE_LOG(LogTemp, Warning, TEXT("AI Placed Wall Length %d at (%d, %d), Orientation: %s"),
            BestMove.WallLength, BestMove.WallSlot->GridX, BestMove.WallSlot->GridY,
            BestMove.WallOrientation == EWallOrientation::Horizontal ? TEXT("Horizontal") : TEXT("Vertical"));
    }

    // Ganti giliran
    CurrentPlayerTurn = 1;
    UE_LOG(LogTemp, Warning, TEXT("AI Turn Ended. Switching to Player %d"), CurrentPlayerTurn);
}

FQuoridorMove AMinimaxBoardAI::GetBestMoveForAI()
{
    AQuoridorPawn* Player2Pawn = GetPawnForPlayer(2);
    AQuoridorPawn* Player1Pawn = GetPawnForPlayer(1);
    if (!Player2Pawn || !Player1Pawn)
    {
        return FQuoridorMove();
    }

    float BestValue = -TNumericLimits<float>::Max();
    FQuoridorMove BestMove;
    TArray<FQuoridorMove> LegalMoves = GetLegalMovesForPlayer(2);

    if (LegalMoves.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("No legal moves available for AI"));
        return FQuoridorMove();
    }

    for (const FQuoridorMove& Move : LegalMoves)
    {
        // Simulasikan langkah
        AQuoridorPawn* OriginalPawn = Move.bIsPawnMove ? Player2Pawn : nullptr;
        TArray<AWallSlot*> AffectedSlots;
        TArray<AActor*> SpawnedWalls;
        bool bSuccess = ApplyMove(Move, 2);

        if (bSuccess)
        {
            // Evaluasi dengan Minimax
            float MoveValue = Minimax(MaxDepth - 1, false, Player1Pawn, Player2Pawn);

            // Pilih langkah dengan nilai terbaik
            if (MoveValue > BestValue)
            {
                BestValue = MoveValue;
                BestMove = Move;
            }

            // Kembalikan status papan
            UndoMove(Move, 2, OriginalPawn, AffectedSlots, SpawnedWalls);
        }
    }

    if (!BestMove.WallSlot && !BestMove.bIsPawnMove)
    {
        // Jika tidak ada langkah terbaik, pilih langkah pertama sebagai fallback
        BestMove = LegalMoves[0];
        UE_LOG(LogTemp, Warning, TEXT("No best move found, using fallback move"));
    }

    return BestMove;
}

float AMinimaxBoardAI::Minimax(int32 Depth, bool bIsMaximizing, AQuoridorPawn* Player1Pawn, AQuoridorPawn* Player2Pawn)
{
    // JANGAN periksa kemenangan di sini, kita akan memeriksanya setelah langkah diterapkan
    // untuk mencegah AI "melihat" kemenangan langsung dalam simulasi

    if (Depth == 0)
    {
        return EvaluateBoard(Player1Pawn, Player2Pawn);
    }

    int32 CurrentPlayer = bIsMaximizing ? 2 : 1;
    TArray<FQuoridorMove> LegalMoves = GetLegalMovesForPlayer(CurrentPlayer);
    AQuoridorPawn* CurrentPawn = CurrentPlayer == 1 ? Player1Pawn : Player2Pawn;

    if (LegalMoves.Num() == 0)
    {
        return EvaluateBoard(Player1Pawn, Player2Pawn);
    }

    int32 FinalRow1 = GridSize - 1; // Baris tujuan Pemain 1
    int32 FinalRow2 = 0;           // Baris tujuan Pemain 2

    if (bIsMaximizing)
    {
        float MaxEval = -TNumericLimits<float>::Max();
        for (const FQuoridorMove& Move : LegalMoves)
        {
            TArray<AWallSlot*> AffectedSlots;
            TArray<AActor*> SpawnedWalls;
            bool bSuccess = ApplyMove(Move, CurrentPlayer);

            if (bSuccess)
            {
                // Periksa kemenangan setelah langkah diterapkan
                if (CurrentPlayer == 1 && CurrentPawn->GridY == FinalRow1)
                {
                    UndoMove(Move, CurrentPlayer, CurrentPawn, AffectedSlots, SpawnedWalls);
                    return -1000.0f; // Pemain 1 menang
                }
                if (CurrentPlayer == 2 && CurrentPawn->GridY == FinalRow2)
                {
                    UndoMove(Move, CurrentPlayer, CurrentPawn, AffectedSlots, SpawnedWalls);
                    return 1000.0f;  // Pemain 2 menang
                }

                float Eval = Minimax(Depth - 1, false, Player1Pawn, Player2Pawn);
                MaxEval = FMath::Max(MaxEval, Eval);
                UndoMove(Move, CurrentPlayer, CurrentPawn, AffectedSlots, SpawnedWalls);
            }
        }
        return MaxEval;
    }
    else
    {
        float MinEval = TNumericLimits<float>::Max();
        for (const FQuoridorMove& Move : LegalMoves)
        {
            TArray<AWallSlot*> AffectedSlots;
            TArray<AActor*> SpawnedWalls;
            bool bSuccess = ApplyMove(Move, CurrentPlayer);

            if (bSuccess)
            {
                // Periksa kemenangan setelah langkah diterapkan
                if (CurrentPlayer == 1 && CurrentPawn->GridY == FinalRow1)
                {
                    UndoMove(Move, CurrentPlayer, CurrentPawn, AffectedSlots, SpawnedWalls);
                    return -1000.0f; // Pemain 1 menang
                }
                if (CurrentPlayer == 2 && CurrentPawn->GridY == FinalRow2)
                {
                    UndoMove(Move, CurrentPlayer, CurrentPawn, AffectedSlots, SpawnedWalls);
                    return 1000.0f;  // Pemain 2 menang
                }

                float Eval = Minimax(Depth - 1, true, Player1Pawn, Player2Pawn);
                MinEval = FMath::Min(MinEval, Eval);
                UndoMove(Move, CurrentPlayer, CurrentPawn, AffectedSlots, SpawnedWalls);
            }
        }
        return MinEval;
    }
}

float AMinimaxBoardAI::EvaluateBoard(AQuoridorPawn* Player1Pawn, AQuoridorPawn* Player2Pawn)
{
    int32 Distance1 = 0;
    int32 Distance2 = 0;

    if (Player1Pawn && Player1Pawn->CurrentTile)
    {
        Distance1 = GetShortestPathLength(Player1Pawn);
    }
    if (Player2Pawn && Player2Pawn->CurrentTile)
    {
        Distance2 = GetShortestPathLength(Player2Pawn);
    }

    // Skor: jarak Pemain 1 - jarak Pemain 2
    float Score = static_cast<float>(Distance1 - Distance2);

    // Berikan bobot lebih besar untuk tembok yang dapat memperlambat lawan
    // Hitung dampak tembok dengan mensimulasikan peningkatan jarak lawan
    if (Player2Pawn->PlayerWalls.Num() > 0)
    {
        float BestWallImpact = 0.0f;
        TArray<FQuoridorMove> WallMoves;
        for (const FQuoridorMove& Move : GetLegalMovesForPlayer(2))
        {
            if (!Move.bIsPawnMove)
            {
                WallMoves.Add(Move);
            }
        }

        for (const FQuoridorMove& WallMove : WallMoves)
        {
            TArray<AWallSlot*> AffectedSlots;
            TArray<AActor*> SpawnedWalls;
            bool bSuccess = ApplyMove(WallMove, 2);
            if (bSuccess)
            {
                int32 NewDistance1 = GetShortestPathLength(Player1Pawn);
                float Impact = static_cast<float>(NewDistance1 - Distance1);
                BestWallImpact = FMath::Max(BestWallImpact, Impact);
                UndoMove(WallMove, 2, Player2Pawn, AffectedSlots, SpawnedWalls);
            }
        }
        Score += BestWallImpact * 2.0f; // Bobot lebih besar untuk dampak tembok
    }

    // Bobot untuk jumlah tembok yang tersisa
    Score += Player2Pawn->PlayerWalls.Num() * 1.0f;
    Score -= Player1Pawn->PlayerWalls.Num() * 1.0f;

    return Score;
}

TArray<FQuoridorMove> AMinimaxBoardAI::GetLegalMovesForPlayer(int32 PlayerNumber)
{
    TArray<FQuoridorMove> LegalMoves;
    AQuoridorPawn* Pawn = GetPawnForPlayer(PlayerNumber);
    if (!Pawn || !Pawn->CurrentTile) return LegalMoves;

    // 1. Langkah pion
    for (ATile* Neighbor : Pawn->CurrentTile->ConnectedTiles)
    {
        if (Neighbor && Pawn->CanMoveToTile(Neighbor))
        {
            FQuoridorMove Move;
            Move.bIsPawnMove = true;
            Move.PawnMove = FIntPoint(Neighbor->GridX, Neighbor->GridY);
            LegalMoves.Add(Move);
        }
    }

    // 2. Penempatan tembok
    TArray<int32> AvailableLengths;
    for (int32 Length = 1; Length <= 3; ++Length)
    {
        if (Pawn->GetWallCountOfLength(Length) > 0)
        {
            AvailableLengths.Add(Length);
        }
    }

    for (AWallSlot* Slot : WallSlots)
    {
        if (!Slot || !Slot->CanPlaceWall()) continue;

        for (int32 Length : AvailableLengths)
        {
            // Simulasikan penempatan tembok untuk memeriksa validitas
            TArray<AWallSlot*> AffectedSlots;
            AffectedSlots.Add(Slot);
            bool bValid = true;
            for (int32 i = 1; i < Length; ++i)
            {
                int32 NextX = Slot->GridX + (Slot->Orientation == EWallOrientation::Horizontal ? i : 0);
                int32 NextY = Slot->GridY + (Slot->Orientation == EWallOrientation::Vertical ? i : 0);
                AWallSlot* NextSlot = FindWallSlotAt(NextX, NextY, Slot->Orientation);
                if (!NextSlot || NextSlot->bIsOccupied)
                {
                    bValid = false;
                    break;
                }
                AffectedSlots.Add(NextSlot);
            }

            if (bValid)
            {
                // Simulasikan pemblokiran tembok
                TMap<TPair<ATile*, ATile*>, bool> RemovedConnections;
                SimulateWallBlock(AffectedSlots, RemovedConnections);

                // Periksa apakah jalur masih ada untuk kedua pemain
                bool bPath1 = IsPathAvailableForPawn(GetPawnForPlayer(1));
                bool bPath2 = IsPathAvailableForPawn(GetPawnForPlayer(2));

                // Kembalikan koneksi
                RevertWallBlock(RemovedConnections);

                if (bPath1 && bPath2)
                {
                    FQuoridorMove Move;
                    Move.bIsPawnMove = false;
                    Move.WallSlot = Slot;
                    Move.WallLength = Length;
                    Move.WallOrientation = Slot->Orientation;
                    LegalMoves.Add(Move);
                }
            }
        }
    }

    return LegalMoves;
}

bool AMinimaxBoardAI::ApplyMove(const FQuoridorMove& Move, int32 PlayerNumber)
{
    AQuoridorPawn* Pawn = GetPawnForPlayer(PlayerNumber);
    if (!Pawn) return false;

    if (Move.bIsPawnMove)
    {
        ATile* TargetTile = Tiles[Move.PawnMove.Y][Move.PawnMove.X];
        if (Pawn->CanMoveToTile(TargetTile))
        {
            // Simpan posisi asli untuk undo
            Pawn->GridX = TargetTile->GridX;
            Pawn->GridY = TargetTile->GridY;
            if (Pawn->CurrentTile)
            {
                Pawn->CurrentTile->SetPawnOnTile(nullptr);
            }
            Pawn->CurrentTile = TargetTile;
            TargetTile->SetPawnOnTile(Pawn);
            Pawn->SetActorLocation(TargetTile->GetActorLocation() + FVector(0, 0, 50));
            return true;
        }
        return false;
    }
    else
    {
        bIsPlacingWall = true;
        PendingWallLength = Move.WallLength;
        PendingWallOrientation = Move.WallOrientation;
        bool bSuccess = TryPlaceWall(Move.WallSlot, Move.WallLength);
        bIsPlacingWall = false;
        return bSuccess;
    }
}

void AMinimaxBoardAI::UndoMove(const FQuoridorMove& Move, int32 PlayerNumber, AQuoridorPawn* Pawn, TArray<AWallSlot*> AffectedSlots, TArray<AActor*> SpawnedWalls)
{
    if (!Pawn) return;

    if (Move.bIsPawnMove)
    {
        // Kembalikan posisi pion
        if (Pawn->CurrentTile)
        {
            Pawn->CurrentTile->SetPawnOnTile(nullptr);
        }
        Pawn->SetGridPosition(Pawn->GridX, Pawn->GridY);
        Pawn->CurrentTile = Tiles[Pawn->GridY][Pawn->GridX];
        Pawn->CurrentTile->SetPawnOnTile(Pawn);
        Pawn->SetActorLocation(Pawn->CurrentTile->GetActorLocation() + FVector(0, 0, 50));
    }
    else
    {
        // Hapus tembok yang ditempatkan
        for (AWallSlot* Slot : AffectedSlots)
        {
            if (Slot)
            {
                Slot->SetOccupied(false);
            }
        }
        for (AActor* Wall : SpawnedWalls)
        {
            if (Wall)
            {
                Wall->Destroy();
            }
        }

        // Kembalikan tembok ke inventori pemain
        FWallDefinition NewWall;
        NewWall.Length = Move.WallLength;
        NewWall.Orientation = Move.WallOrientation;
        Pawn->PlayerWalls.Add(NewWall);

        // Perbarui koneksi petak
        UpdateAllTileConnections();
    }
}

// Fungsi tambahan untuk menghitung jarak terpendek (menggunakan A*)
int32 AMinimaxBoardAI::GetShortestPathLength(AQuoridorPawn* Pawn)
{
    if (!Pawn || !Pawn->CurrentTile) return GridSize;

    struct FNode
    {
        ATile* Tile;
        int32 GCost;
        int32 HCost;
        FNode* Parent;

        FNode(ATile* InTile, int32 InGCost, int32 InHCost, FNode* InParent = nullptr)
            : Tile(InTile), GCost(InGCost), HCost(InHCost), Parent(InParent) {}

        int32 FCost() const { return GCost + HCost; }
    };

    ATile* StartTile = Pawn->CurrentTile;
    int32 TargetRow = (Pawn->PlayerNumber == 1) ? (GridSize - 1) : 0;

    TArray<FNode*> OpenSet;
    TSet<ATile*> ClosedSet;

    auto Heuristic = [TargetRow](int32 X, int32 Y)
    {
        return FMath::Abs(TargetRow - Y);
    };

    OpenSet.Add(new FNode(StartTile, 0, Heuristic(StartTile->GridX, StartTile->GridY)));

    while (OpenSet.Num() > 0)
    {
        FNode* Current = OpenSet[0];
        int32 CurrentIndex = 0;
        for (int32 i = 0; i < OpenSet.Num(); ++i)
        {
            if (OpenSet[i]->FCost() < Current->FCost() ||
                (OpenSet[i]->FCost() == Current->FCost() && OpenSet[i]->HCost < Current->HCost))
            {
                Current = OpenSet[i];
                CurrentIndex = i;
            }
        }
        OpenSet.RemoveAt(CurrentIndex);
        ClosedSet.Add(Current->Tile);

        if ((Pawn->PlayerNumber == 1 && Current->Tile->GridY == TargetRow) ||
            (Pawn->PlayerNumber == 2 && Current->Tile->GridY == TargetRow))
        {
            int32 PathLength = Current->GCost;
            for (FNode* Node : OpenSet) delete Node;
            delete Current;
            return PathLength;
        }

        for (ATile* Neighbor : Current->Tile->ConnectedTiles)
        {
            if (!Neighbor || ClosedSet.Contains(Neighbor) || Neighbor->IsOccupied())
                continue;

            bool bAlreadyInOpenSet = false;
            for (FNode* Node : OpenSet)
            {
                if (Node->Tile == Neighbor)
                {
                    bAlreadyInOpenSet = true;
                    break;
                }
            }
            if (!bAlreadyInOpenSet)
            {
                int32 G = Current->GCost + 1;
                int32 H = Heuristic(Neighbor->GridX, Neighbor->GridY);
                OpenSet.Add(new FNode(Neighbor, G, H, Current));
            }
        }

        delete Current;
    }

    for (FNode* Node : OpenSet) delete Node;
    return GridSize; // Jika tidak ada jalur, kembalikan jarak maksimum
}