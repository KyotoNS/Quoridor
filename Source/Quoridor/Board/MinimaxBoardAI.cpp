#include "MinimaxBoardAI.h"
#include "MinimaxEngine.h"
#include "Quoridor/Board/QuoridorBoard.h"
#include "Quoridor/tile/Tile.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

AMinimaxBoardAI::AMinimaxBoardAI()
{
    // Enable ticking on this AI board if you plan to do per-frame logic
    PrimaryActorTick.bCanEverTick = true;
}
void AMinimaxBoardAI::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    GEngine->AddOnScreenDebugMessage(1, 0.0f, FColor::Cyan,
        FString::Printf(TEXT("Turn: Player %d"), CurrentPlayerTurn));
}
void AMinimaxBoardAI::RunMinimaxForPlayer2Async()
{
    if (bMinimaxInProgress)
        return;

    bMinimaxInProgress = true;

    // Snapshot the board into a thread-safe struct
    FMinimaxState StateSnapshot = FMinimaxState::FromBoard(this);
    int32 Depth = 1;

    // Launch background thread
    Async(EAsyncExecution::Thread, [this, StateSnapshot, Depth]()
    {
        // Solve in background
        FMinimaxAction Action = MinimaxEngine::Solve(StateSnapshot, Depth);

        // Return to game thread to apply the move
        AsyncTask(ENamedThreads::GameThread, [this, Action]()
        {
            ExecuteAction(Action);
            CurrentPlayerTurn = 1;
            SelectedPawn = GetPawnForPlayer(1);
            bMinimaxInProgress = false;

            UE_LOG(LogTemp, Warning, TEXT("=> Engine chose: %s (score=%d)"),
                Action.bIsWall ?
                    *FString::Printf(TEXT("Wall @(%d,%d) %s"), Action.SlotX, Action.SlotY, Action.bHorizontal ? TEXT("H") : TEXT("V")) :
                    *FString::Printf(TEXT("Move to (%d,%d)"), Action.MoveX, Action.MoveY),
                Action.Score);
        });
    });
}

void AMinimaxBoardAI::ExecuteAction(const FMinimaxAction& Act)
{
    if (Act.bIsWall)
    {
        // Find the correct wall slot based on coordinates and orientation
        EWallOrientation Orientation = Act.bHorizontal ? EWallOrientation::Horizontal : EWallOrientation::Vertical;
        AWallSlot* SlotToUse = FindWallSlotAt(Act.SlotX, Act.SlotY, Orientation);

        if (SlotToUse)
        {
            // Set parameters for wall placement
            PendingWallLength = Act.WallLength;
            PendingWallOrientation = Orientation;

            // Try placing the wall
            TryPlaceWall(SlotToUse, Act.WallLength);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("ExecuteAction: Failed to find wall slot at (%d,%d) [%s]"),
                Act.SlotX, Act.SlotY, *UEnum::GetValueAsString(Orientation));
        }
    }
    else
    {
        // Handle pawn move
        if (Act.MoveY >= 0 && Act.MoveY < Tiles.Num() &&
            Act.MoveX >= 0 && Act.MoveX < Tiles[Act.MoveY].Num())
        {
            ATile* TargetTile = Tiles[Act.MoveY][Act.MoveX];
            if (TargetTile)
            {
                HandleTileClick(TargetTile);
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("ExecuteAction: Target tile at (%d,%d) is null"), Act.MoveX, Act.MoveY);
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("ExecuteAction: Move position out of bounds (%d,%d)"), Act.MoveX, Act.MoveY);
        }
    }
}



