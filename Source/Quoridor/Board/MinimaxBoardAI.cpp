#include "MinimaxBoardAI.h"
#include "MinimaxEngine.h"
#include "Quoridor/Board/QuoridorBoard.h"
#include "Quoridor/Pawn/QuoridorPawn.h"
#include "Quoridor/tile/Tile.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

AMinimaxBoardAI::AMinimaxBoardAI()
{
    // Enable ticking on this AI board if you plan to do per-frame logic
    PrimaryActorTick.bCanEverTick = true;
}
void AMinimaxBoardAI::BeginPlay()
{
    Super::BeginPlay();

    // Tick stays enabled, we use ElapsedTime to wait before acting
    ElapsedTime = 0.0f;
    bDelayPassed = false;

    // Randomly choose which AI will be Player 1 or Player 2
    bool bAI1IsPlayer1 = FMath::RandBool();

    if (bAI1IsPlayer1)
    {
        UE_LOG(LogTemp, Warning, TEXT("Random Assignment: AI1 is Player 1, AI2 is Player 2"));
        AI1Player = 1;
        AI2Player = 2;
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Random Assignment: AI1 is Player 2, AI2 is Player 1"));
        AI1Player = 2;
        AI2Player = 1;
    }
}

void AMinimaxBoardAI::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    GEngine->AddOnScreenDebugMessage(1, 0.0f, FColor::Cyan,
        FString::Printf(TEXT("Turn: Player %d"), CurrentPlayerTurn));

    // Wait a bit before AI logic kicks in
    if (!bDelayPassed)
    {
        ElapsedTime += DeltaTime;
        if (ElapsedTime < 5.0f)
            return;

        bDelayPassed = true;
        UE_LOG(LogTemp, Warning, TEXT("AI logic delay passed, starting AI..."));
    }
    
    // if (bDelayPassed && CurrentPlayerTurn == AI1Player && !bIsAITurnRunning)
    // {
    //     AQuoridorPawn* P = GetPawnForPlayer(CurrentPlayerTurn);
    //     if (P && P->GetTile())
    //     {
    //         bIsAITurnRunning = true;
    //         RunMinimaxForCurrentPlayerAsync(AI1Player);
    //     }
    //     else
    //     {
    //         UE_LOG(LogTemp, Warning, TEXT("AI pawn not ready yet"));
    //     }
    // }
    if (bDelayPassed && CurrentPlayerTurn == AI2Player && !bIsAITurnRunning)
    {
        AQuoridorPawn* P = GetPawnForPlayer(CurrentPlayerTurn);
        if (P && P->GetTile())
        {
            bIsAITurnRunning = true;
            RunMinimaxForCurrentPlayerAsync2(AI2Player);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("AI pawn not ready yet"));
        }
    }
}


// void AMinimaxBoardAI::RunMinimaxForCurrentPlayerAsync(int32 Player)
// {
//     if (bMinimaxInProgress)
//         return;
//
//     const int32 AIPlayer = Player;  // Determine which player is AI right now
//
//     bMinimaxInProgress = true;
//
//     GetWorld()->GetTimerManager().SetTimerForNextTick([this, AIPlayer]()
//     {
//         FMinimaxState StateSnapshot = FMinimaxState::FromBoard(this);
//         int32 Depth = 3;
//
//         Async(EAsyncExecution::Thread, [this, StateSnapshot, Depth, AIPlayer]()
//         {
//             FMinimaxAction Action = MinimaxEngine::SolveParallel(StateSnapshot, Depth, AIPlayer);
//
//             AsyncTask(ENamedThreads::GameThread, [this, Action, AIPlayer]()
//             {
//                 ExecuteAction(Action);
//
//                 // Swap turn after action is done
//                 CurrentPlayerTurn = (AIPlayer == 1) ? 2 : 1;
//                 SelectedPawn = GetPawnForPlayer(CurrentPlayerTurn);  // Now it's the next player's turn
//                 bMinimaxInProgress = false;
//
//                 UE_LOG(LogTemp, Warning, TEXT("=> Engine chose: %s (score=%d)"),
//                     Action.bIsWall ?
//                         *FString::Printf(TEXT("Wall @(%d,%d) %s"), Action.SlotX, Action.SlotY, Action.bHorizontal ? TEXT("H") : TEXT("V")) :
//                         *FString::Printf(TEXT("Move to (%d,%d)"), Action.MoveX, Action.MoveY),
//                     Action.Score);
//             });
//         });
//     });
// }

void AMinimaxBoardAI::RunMinimaxForCurrentPlayerAsync2(int32 Player)
{
    if (bMinimaxInProgress)
        return;

    const int32 AIPlayer = Player;  // Determine which player is AI right now

    bMinimaxInProgress = true;

    GetWorld()->GetTimerManager().SetTimerForNextTick([this, AIPlayer]()
    {
        FMinimaxState StateSnapshot = FMinimaxState::FromBoard(this);
        int32 Depth = 2;

        Async(EAsyncExecution::Thread, [this, StateSnapshot, Depth, AIPlayer]()
        {
            FMinimaxAction Action = MinimaxEngine::Solve(StateSnapshot, Depth, AIPlayer);

            AsyncTask(ENamedThreads::GameThread, [this, Action, AIPlayer]()
            {
                ExecuteAction(Action);

                // Swap turn after action is done
                CurrentPlayerTurn = (AIPlayer == 1) ? 2 : 1;
                SelectedPawn = GetPawnForPlayer(CurrentPlayerTurn);  // Now it's the next player's turn
                bMinimaxInProgress = false;

                UE_LOG(LogTemp, Warning, TEXT("=> Engine chose: %s (score=%d)"),
                    Action.bIsWall ?
                        *FString::Printf(TEXT("Wall @(%d,%d) %s"), Action.SlotX, Action.SlotY, Action.bHorizontal ? TEXT("H") : TEXT("V")) :
                        *FString::Printf(TEXT("Move to (%d,%d)"), Action.MoveX, Action.MoveY),
                    Action.Score);
            });
        });
    });
}

void AMinimaxBoardAI::ExecuteAction(const FMinimaxAction& Act)
{
    if (Act.bIsWall)
    {
        // Determine orientation
        EWallOrientation Orientation = Act.bHorizontal ? EWallOrientation::Horizontal : EWallOrientation::Vertical;

        UE_LOG(LogTemp, Warning, TEXT("ExecuteAction: Trying to place wall at (%d, %d) [%s]"),
            Act.SlotX, Act.SlotY,
            *UEnum::GetValueAsString(Orientation));

        AWallSlot* SlotToUse = FindWallSlotAt(Act.SlotX, Act.SlotY, Orientation);
        if (SlotToUse)
        {
            PendingWallLength = Act.WallLength;
            PendingWallOrientation = Orientation;

            bool bSuccess = TryPlaceWall(SlotToUse, Act.WallLength);
            if (!bSuccess)
            {
                UE_LOG(LogTemp, Error, TEXT("ExecuteAction: TryPlaceWall failed for wall at (%d, %d) [%s]"),
                    Act.SlotX, Act.SlotY,
                    *UEnum::GetValueAsString(Orientation));
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("ExecuteAction: Wall placed at (%d, %d) [%s]"),
                    Act.SlotX, Act.SlotY,
                    *UEnum::GetValueAsString(Orientation));
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("ExecuteAction: Failed to find wall slot at (%d, %d) [%s]"),
                Act.SlotX, Act.SlotY,
                *UEnum::GetValueAsString(Orientation));
        }
    }
    else
    {
        // AI-controlled pawn move (bypass CanMoveToTile check)
        if (Act.MoveY >= 0 && Act.MoveY < Tiles.Num() &&
            Act.MoveX >= 0 && Act.MoveX < Tiles[Act.MoveY].Num())
        {
            ATile* TargetTile = Tiles[Act.MoveY][Act.MoveX];
            AQuoridorPawn* Pawn = GetPawnForPlayer(2);

            if (Pawn && TargetTile)
            {
                UE_LOG(LogTemp, Warning, TEXT("ExecuteAction: AI moving to tile (%d, %d) directly"), Act.MoveX, Act.MoveY);
                Pawn->MoveToTile(TargetTile, true);  // Direct move — skip CanMoveToTile
                CurrentPlayerTurn = 1;         // Advance to Player 1
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("ExecuteAction: Invalid target tile or pawn"));
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("ExecuteAction: Invalid move coordinates (%d, %d)"), Act.MoveX, Act.MoveY);
        }
    }

    bIsAITurnRunning = false;
}
