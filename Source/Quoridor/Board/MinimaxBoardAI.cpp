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
    
    if (bDelayPassed && CurrentPlayerTurn == 2 && !bIsAITurnRunning)
    {
        AQuoridorPawn* P = GetPawnForPlayer(CurrentPlayerTurn);
        if (P && P->GetTile())
        {
            bIsAITurnRunning = true;
            RunMinimaxForCurrentPlayerAsync();
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("AI pawn not ready yet"));
        }
    }
}


void AMinimaxBoardAI::RunMinimaxForCurrentPlayerAsync()
{
    if (bMinimaxInProgress)
        return;

    const int32 AIPlayer = CurrentPlayerTurn;  // Determine which player is AI right now

    bMinimaxInProgress = true;

    GetWorld()->GetTimerManager().SetTimerForNextTick([this, AIPlayer]()
    {
        FMinimaxState StateSnapshot = FMinimaxState::FromBoard(this);
        int32 Depth = 4;

        Async(EAsyncExecution::Thread, [this, StateSnapshot, Depth, AIPlayer]()
        {
            FMinimaxAction Action = MinimaxEngine::SolveParallel(StateSnapshot, Depth, AIPlayer);

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
                UE_LOG(LogTemp, Error, TEXT("ExecuteAction: TryPlaceWall failed at (%d, %d) [%s]"),
                    Act.SlotX, Act.SlotY, *UEnum::GetValueAsString(Orientation));
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("ExecuteAction: Wall placed at (%d, %d) [%s]"),
                    Act.SlotX, Act.SlotY, *UEnum::GetValueAsString(Orientation));
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("ExecuteAction: Wall slot not found at (%d, %d) [%s]"),
                Act.SlotX, Act.SlotY, *UEnum::GetValueAsString(Orientation));
        }
    }
    else
    {
        if (Act.MoveY >= 0 && Act.MoveY < Tiles.Num() &&
            Act.MoveX >= 0 && Act.MoveX < Tiles[Act.MoveY].Num())
        {
            ATile* TargetTile = Tiles[Act.MoveY][Act.MoveX];
            AQuoridorPawn* Pawn = GetPawnForPlayer(CurrentPlayerTurn);

            if (TargetTile && Pawn)
            {
                UE_LOG(LogTemp, Warning, TEXT("ExecuteAction: Moving to tile (%d, %d)"), Act.MoveX, Act.MoveY);
                SelectedPawn = Pawn;
                Pawn->MoveToTile(TargetTile, true);  // Trust the minimax move
                CurrentPlayerTurn = (CurrentPlayerTurn == 1) ? 2 : 1;
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("ExecuteAction: Invalid TargetTile or Pawn"));
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("ExecuteAction: Invalid move coordinates (%d, %d)"),
                Act.MoveX, Act.MoveY);
        }
    }

    bIsAITurnRunning = false;
}




