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
void AMinimaxBoardAI::RunMinimaxForPlayer2()
{
    FMinimaxState State = FMinimaxState::FromBoard(this);
    UE_LOG(LogTemp,Warning,TEXT(">> SNAP: P1=(%d,%d) P2=(%d,%d) Walls=[%d,%d]"),
        State.PawnX[0],State.PawnY[0],State.PawnX[1],State.PawnY[1],
        State.WallsRemaining[0],State.WallsRemaining[1]);

    FMinimaxAction Act = MinimaxEngine::Solve(State,1);

    if (Act.bIsWall)
    {
        // Place wall based on the chosen action
        AWallSlot* SlotToUse = nullptr;
        if (Act.bHorizontal)
        {
            // Search horizontal slots
            for (AWallSlot* Slot : HorizontalWallSlots)
            {
                if (Slot->GridX == Act.SlotX && Slot->GridY == Act.SlotY)
                {
                    SlotToUse = Slot;
                    break;
                }
            }
        }
        else
        {
            // Search vertical slots
            for (AWallSlot* Slot : VerticalWallSlots)
            {
                if (Slot->GridX == Act.SlotX && Slot->GridY == Act.SlotY)
                {
                    SlotToUse = Slot;
                    break;
                }
            }
        }
    
        if (SlotToUse)
        {
            // Configure pending wall parameters
            PendingWallLength = Act.WallLength;
            PendingWallOrientation = Act.bHorizontal
                ? EWallOrientation::Horizontal
                : EWallOrientation::Vertical;

            // Execute wall placement
            TryPlaceWall(SlotToUse, Act.WallLength);
        }
    }
    else
    {
        // Move pawn to the chosen tile
        if (Act.MoveY >= 0 && Act.MoveY < Tiles.Num())
        {
            ATile* Dest = Tiles[Act.MoveY][Act.MoveX];
            if (Dest)
            {
                HandleTileClick(Dest);
            }
        }
    }

    CurrentPlayerTurn = 1;
    SelectedPawn      = GetPawnForPlayer(1);

    UE_LOG(LogTemp,Warning,TEXT("=> Engine chose: %s (score=%d)"),
        Act.bIsWall?
           *FString::Printf(TEXT("Wall @(%d,%d) %s"),Act.SlotX,Act.SlotY,Act.bHorizontal?TEXT("H"):TEXT("V")):
           *FString::Printf(TEXT("Move to (%d,%d)"),Act.MoveX,Act.MoveY),
        Act.Score);
}

