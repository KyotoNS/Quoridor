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

    GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
    {
        // OR use a small delay like 0.1f if needed
        FMinimaxState StateSnapshot = FMinimaxState::FromBoard(this);
        int32 Depth = 1;

        Async(EAsyncExecution::Thread, [this, StateSnapshot, Depth]()
        {
            FMinimaxAction Action = MinimaxEngine::Solve(StateSnapshot, Depth);

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
    });
}

void AMinimaxBoardAI::ExecuteAction(const FMinimaxAction& Act)
{
    if (Act.bIsWall)
    {
        // Determine orientation
        EWallOrientation Orientation = Act.bHorizontal ? EWallOrientation::Horizontal : EWallOrientation::Vertical;

        // Log intent
        UE_LOG(LogTemp, Warning, TEXT("ExecuteAction: Trying to place wall at (%d, %d) [%s]"),
            Act.SlotX, Act.SlotY,
            *UEnum::GetValueAsString(Orientation));

        // Find the matching wall slot
        AWallSlot* SlotToUse = FindWallSlotAt(Act.SlotX, Act.SlotY, Orientation);
        if (SlotToUse)
        {
            // Set wall intent
            PendingWallLength = Act.WallLength;
            PendingWallOrientation = Orientation;

            // Try placing it
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
        // Validate coordinates
        if (Act.MoveY >= 0 && Act.MoveY < Tiles.Num() &&
            Act.MoveX >= 0 && Act.MoveX < Tiles[Act.MoveY].Num())
        {
            ATile* TargetTile = Tiles[Act.MoveY][Act.MoveX];
            if (TargetTile)
            {
                UE_LOG(LogTemp, Warning, TEXT("ExecuteAction: Moving to tile (%d, %d)"), Act.MoveX, Act.MoveY);
                SelectedPawn = GetPawnForPlayer(2);
                HandleTileClick(TargetTile);
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("ExecuteAction: Target tile (%d, %d) is null"), Act.MoveX, Act.MoveY);
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("ExecuteAction: Invalid move coordinates (%d, %d)"), Act.MoveX, Act.MoveY);
        }
    }
}




