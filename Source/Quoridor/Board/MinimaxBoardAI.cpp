// MinimaxBoardAI.cpp
#include "MinimaxBoardAI.h"
#include "Quoridor/Wall/WallSlot.h"
#include "Quoridor/Tile/Tile.h"
#include "Quoridor/Pawn/QuoridorPawn.h"

AMinimaxBoardAI::AMinimaxBoardAI()
{
    PrimaryActorTick.bCanEverTick = true;
}
void AMinimaxBoardAI::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    GEngine->AddOnScreenDebugMessage(1, 0.0f, FColor::Cyan,
        FString::Printf(TEXT("Turn: Player %d"), CurrentPlayerTurn));
}
void AMinimaxBoardAI::RunMinimaxForPlayer2()
{

    int32 BestScore = -999999;
    FIntPoint BestMove;
    FWallDefinition BestWall;
    AWallSlot* LastWallSlot = nullptr;
    bool bShouldPlaceWall = false;

    AQuoridorPawn* Player2 = GetPawnForPlayer(2);
    if (!Player2 || !Player2->CurrentTile) return;

    int32 OriginalX = Player2->GridX;
    int32 OriginalY = Player2->GridY;

    // Try all valid pawn moves
    TArray<ATile*> ValidMoves = GetAllValidMoves(Player2);
    for (ATile* Tile : ValidMoves)
    {
        Player2->SimulateMovePawn(Tile->GridX, Tile->GridY);
        int32 Score = Minimax(2, false);
        Player2->RevertSimulatedMove();

        if (Score > BestScore)
        {
            BestScore = Score;
            BestMove = FIntPoint(Tile->GridX, Tile->GridY);
            bShouldPlaceWall = false;
        }
    }

    // Try all valid wall placements
    if (Player2->HasRemainingWalls())
    {
        TArray<TPair<AWallSlot*, int32>> Walls = GetAllValidWalls();
        for (const TPair<AWallSlot*, int32>& Pair : Walls)
        {
            TMap<TPair<ATile*, ATile*>, bool> RemovedConnections;
            SimulateWallBlock({ Pair.Key }, RemovedConnections);

            int32 Score = Minimax(2, false);

            RevertWallBlock(RemovedConnections);

            if (Score > BestScore)
            {
                BestScore = Score;
                BestWall.Length = Pair.Value;
                BestWall.Orientation = Pair.Key->Orientation;
                LastWallSlot = Pair.Key;
                bShouldPlaceWall = true;
            }
        }
    }

    // --- Instead of executing, display the chosen action ---
    if (GEngine)
    {
        FString Message;

        if (bShouldPlaceWall && LastWallSlot)
        {
            Message = FString::Printf(TEXT("AI Action: Place Wall at (%d, %d), Length: %d, Orientation: %s"),
                LastWallSlot->GridX,
                LastWallSlot->GridY,
                BestWall.Length,
                BestWall.Orientation == EWallOrientation::Horizontal ? TEXT("Horizontal") : TEXT("Vertical"));
        }
        else
        {
            Message = FString::Printf(TEXT("AI Action: Move to (%d, %d)"), BestMove.X, BestMove.Y);
        }

        FString ScoreMsg = FString::Printf(TEXT("Best Minimax Score: %d"), BestScore);

        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, Message);
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow, ScoreMsg);
    }
    CurrentPlayerTurn = 1; // Return control to Player 1

}


int32 AMinimaxBoardAI::Minimax(int32 Depth, bool bMaximizingPlayer)
{
    if (Depth == 0)
    {
        return EvaluateBoard();
    }

    if (bMaximizingPlayer)
    {
        int32 MaxEval = -999999;
        AQuoridorPawn* Player2 = GetPawnForPlayer(2);
        int32 OriginalX = Player2->GridX;
        int32 OriginalY = Player2->GridY;

        for (ATile* Tile : GetAllValidMoves(Player2))
        {
            Player2->SimulateMovePawn(Tile->GridX, Tile->GridY);

            int32 Eval = Minimax(Depth - 1, false);
            Player2->RevertSimulatedMove();
            MaxEval = FMath::Max(MaxEval, Eval);
        }
        return MaxEval;
    }
    else
    {
        int32 MinEval = 999999;
        AQuoridorPawn* Player1 = GetPawnForPlayer(1);
        int32 OriginalX = Player1->GridX;
        int32 OriginalY = Player1->GridY;

        for (ATile* Tile : GetAllValidMoves(Player1))
        {
            Player1->SimulateMovePawn(Tile->GridX, Tile->GridY);
            int32 Eval = Minimax(Depth - 1, true);
            Player1->RevertSimulatedMove();
            MinEval = FMath::Min(MinEval, Eval);
        }
        return MinEval;
    }
}

int32 AMinimaxBoardAI::EvaluateBoard()
{
    AQuoridorPawn* P1 = GetPawnForPlayer(1);
    AQuoridorPawn* P2 = GetPawnForPlayer(2);

    int32 P1Path = IsPathAvailableForPawn(P1) ? CalculateShortestPathLength(P1) : 10000;
    int32 P2Path = IsPathAvailableForPawn(P2) ? CalculateShortestPathLength(P2) : 10000;

    return (P1Path - P2Path) * 100 + (P2->PlayerWalls.Num() - P1->PlayerWalls.Num()) * 10;
}

TArray<ATile*> AMinimaxBoardAI::GetAllValidMoves(AQuoridorPawn* Pawn)
{
    TArray<ATile*> ValidMoves;
    if (!Pawn || !Pawn->CurrentTile) return ValidMoves;

    for (ATile* Neighbor : Pawn->CurrentTile->ConnectedTiles)
    {
        if (Pawn->CanMoveToTile(Neighbor))
        {
            ValidMoves.Add(Neighbor);
        }
    }
    return ValidMoves;
}

TArray<TPair<AWallSlot*, int32>> AMinimaxBoardAI::GetAllValidWalls()
{
    TArray<TPair<AWallSlot*, int32>> Valid;
    for (AWallSlot* Slot : WallSlots)
    {
        if (!Slot || Slot->bIsOccupied) continue;
        for (int32 L = 1; L <= 3; ++L)
        {
            if (SelectedPawn && SelectedPawn->HasWallOfLength(L))
            {
                TMap<TPair<ATile*, ATile*>, bool> Sim;
                SimulateWallBlock({ Slot }, Sim);
                bool B1 = IsPathAvailableForPawn(GetPawnForPlayer(1));
                bool B2 = IsPathAvailableForPawn(GetPawnForPlayer(2));
                RevertWallBlock(Sim);

                if (B1 && B2)
                {
                    Valid.Add(TPair<AWallSlot*, int32>(Slot, L));
                }
            }
        }
    }
    return Valid;
}

int32 AMinimaxBoardAI::CalculateShortestPathLength(AQuoridorPawn* Pawn)
{
    if (!Pawn || !Pawn->CurrentTile)
        return 10000;

    struct FNode
    {
        ATile* Tile;
        int32 G;

        FNode(ATile* InTile, int32 InG)
            : Tile(InTile), G(InG) {}
    };

    int32 TargetRow = (Pawn->PlayerNumber == 1) ? (GridSize - 1) : 0;

    TQueue<FNode*> Queue;
    Queue.Enqueue(new FNode(Pawn->CurrentTile, 0));

    TSet<ATile*> Visited;
    Visited.Add(Pawn->CurrentTile);

    while (!Queue.IsEmpty())
    {
        FNode* Current = nullptr;
        Queue.Dequeue(Current);

        if (Current->Tile->GridY == TargetRow)
        {
            int32 Result = Current->G;
            delete Current;

            // Clean up remaining nodes
            while (!Queue.IsEmpty())
            {
                FNode* Remaining;
                Queue.Dequeue(Remaining);
                delete Remaining;
            }

            return Result;
        }

        for (ATile* Neighbor : Current->Tile->ConnectedTiles)
        {
            if (!Neighbor || Visited.Contains(Neighbor))
                continue;

            Visited.Add(Neighbor);
            Queue.Enqueue(new FNode(Neighbor, Current->G + 1));
        }

        delete Current;
    }

    return 10000; // No path found
}

void AQuoridorPawn::SimulateMovePawn(int32 NewX, int32 NewY)
{
    // 1) push old
    SimulationStack.Push({ GridX, GridY, CurrentTile });

    // 2) detach from the old tile
    if (CurrentTile)
    {
        CurrentTile->SetPawnOnTile(nullptr);
    }

    // 3) update to the new tile
    GridX       = NewX;
    GridY       = NewY;
    CurrentTile = BoardReference->Tiles[NewY][NewX];
    CurrentTile->SetPawnOnTile(this);
}

void AQuoridorPawn::RevertSimulatedMove()
{
    if (SimulationStack.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT(
            "RevertSimulatedMove called with empty SimulationStack!"));
        return;
    }

    // Pop and restore
    FPawnSimulationState Old = SimulationStack.Pop();

    if (CurrentTile) CurrentTile->SetPawnOnTile(nullptr);
    GridX       = Old.X;
    GridY       = Old.Y;
    CurrentTile = Old.Tile;
    if (CurrentTile) CurrentTile->SetPawnOnTile(this);
}


