#include "MinimaxBoardAI.h"
#include "MinimaxEngine.h"
#include "Quoridor/Board/QuoridorBoard.h"
#include "Quoridor/Pawn/QuoridorPawn.h"
#include "Quoridor/tile/Tile.h"
#include "Quoridor/Wall/WallSlot.h"
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
    //         RunMinimaxForParallelAlphaBeta(AI1Player);
    //     }
    //     else
    //     {
    //         UE_LOG(LogTemp, Warning, TEXT("AI pawn not ready yet"));
    //     }
    // }
    if (bDelayBeforeNextAI)
    {
        AITurnDelayTimer += DeltaTime;
        if (AITurnDelayTimer < AITurnDelayDuration)
        {
            // Still waiting—do not start any AI this frame
            return;
        }

        // We have waited long enough—clear the flag and reset timer.
        bDelayBeforeNextAI = false;
        AITurnDelayTimer   = 2.0f;

        // Return immediately so that on the *next* Tick() call, the new AI can run.
        return;
    }
    if (bDelayPassed && CurrentPlayerTurn == AI1Player && !bIsAITurnRunning)
    {
        AQuoridorPawn* P = GetPawnForPlayer(CurrentPlayerTurn);
        if (P && P->GetTile())
        {
            bIsAITurnRunning = true;
            RunMinimaxForParallel(AI1Player);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("AI pawn not ready yet"));
        }
    }
    if (bDelayPassed && CurrentPlayerTurn == AI2Player && !bIsAITurnRunning)
    {
        AQuoridorPawn* P = GetPawnForPlayer(CurrentPlayerTurn);
        if (P && P->GetTile())
        {
            bIsAITurnRunning = true;
            RunMinimaxForAlphaBeta(AI2Player);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("AI pawn not ready yet"));
        }
    }
}

// Parallel
void AMinimaxBoardAI::RunMinimaxForParallel(int32 Player)
{
    if (bMinimaxInProgress)
        return;

    const int32 AIPlayer = Player;  // Determine which player is AI right now

    bMinimaxInProgress = true;

    // Create a timer handle so we can schedule a Delay
    FTimerHandle DelayHandle;
    // Schedule the next block to run in 2.0 seconds instead of next tick
    GetWorld()->GetTimerManager().SetTimer(
        DelayHandle,
        [this, AIPlayer]()
        {
            FMinimaxState StateSnapshot = FMinimaxState::FromBoard(this);
            int32 Depth = 5;

            // Run the actual minimax on a background thread
            Async(EAsyncExecution::Thread, [this, StateSnapshot, Depth, AIPlayer]()
            {
                FMinimaxAction Action = MinimaxEngine::SolveParallel(StateSnapshot, Depth, AIPlayer);

                // Once SolveParallel finishes, come back to GameThread to execute the move
                AsyncTask(ENamedThreads::GameThread, [this, Action, AIPlayer]()
                {
                    ExecuteAction(Action);

                    // Swap turn after action is done
                    CurrentPlayerTurn = (AIPlayer == 1) ? 2 : 1;
                    SelectedPawn = GetPawnForPlayer(CurrentPlayerTurn);
                    bMinimaxInProgress = false;

                    UE_LOG(LogTemp, Warning, TEXT("=> Engine chose: %s (score=%d)"),
                        Action.bIsWall
                            ? *FString::Printf(TEXT("Wall @(%d,%d) %s"),
                                Action.SlotX, Action.SlotY, Action.bHorizontal ? TEXT("H") : TEXT("V"))
                            : *FString::Printf(TEXT("Move to (%d,%d)"),
                                Action.MoveX, Action.MoveY),
                        Action.Score);
                });
            });
        },
        0.5f,    // Delay in seconds
        false    // Do not loop
    );
}


// Alpha Beta
void AMinimaxBoardAI::RunMinimaxForAlphaBeta(int32 Player)
{
    if (bMinimaxInProgress)
        return;

    const int32 AIPlayer = Player;  // Determine which player is AI right now

    bMinimaxInProgress = true;

    // Create a timer handle so we can schedule a Delay
    FTimerHandle DelayHandle;
    // Schedule the next block to run in 2.0 seconds instead of next tick
    GetWorld()->GetTimerManager().SetTimer(
        DelayHandle,
        [this, AIPlayer]()
        {
            FMinimaxState StateSnapshot = FMinimaxState::FromBoard(this);
            int32 Depth = 5;

            // Run the actual minimax on a background thread
            Async(EAsyncExecution::Thread, [this, StateSnapshot, Depth, AIPlayer]()
            {
                FMinimaxAction Action = MinimaxEngine::SolveAlphaBeta(StateSnapshot, Depth, AIPlayer);

                // Once SolveParallel finishes, come back to GameThread to execute the move
                AsyncTask(ENamedThreads::GameThread, [this, Action, AIPlayer]()
                {
                    ExecuteAction(Action);

                    // Swap turn after action is done
                    CurrentPlayerTurn = (AIPlayer == 1) ? 2 : 1;
                    SelectedPawn = GetPawnForPlayer(CurrentPlayerTurn);
                    bMinimaxInProgress = false;

                    UE_LOG(LogTemp, Warning, TEXT("=> Engine chose: %s (score=%d)"),
                        Action.bIsWall
                            ? *FString::Printf(TEXT("Wall @(%d,%d) %s"),
                                Action.SlotX, Action.SlotY, Action.bHorizontal ? TEXT("H") : TEXT("V"))
                            : *FString::Printf(TEXT("Move to (%d,%d)"),
                                Action.MoveX, Action.MoveY),
                        Action.Score);
                });
            });
        },
        0.5f,    // Delay in seconds
        false    // Do not loop
    );
}

// solve
void AMinimaxBoardAI::RunMinimaxForParallelAlphaBeta(int32 Player)
{
    if (bMinimaxInProgress)
        return;

    const int32 AIPlayer = Player;  // Determine which player is AI right now

    bMinimaxInProgress = true;

    // Create a timer handle so we can schedule a Delay
    FTimerHandle DelayHandle;
    // Schedule the next block to run in 2.0 seconds instead of next tick
    GetWorld()->GetTimerManager().SetTimer(
        DelayHandle,
        [this, AIPlayer]()
        {
            FMinimaxState StateSnapshot = FMinimaxState::FromBoard(this);
            int32 Depth = 5;

            // Run the actual minimax on a background thread
            Async(EAsyncExecution::Thread, [this, StateSnapshot, Depth, AIPlayer]()
            {
                FMinimaxAction Action = MinimaxEngine::SolveParallelAlphaBeta(StateSnapshot, Depth, AIPlayer);

                // Once SolveParallel finishes, come back to GameThread to execute the move
                AsyncTask(ENamedThreads::GameThread, [this, Action, AIPlayer]()
                {
                    ExecuteAction(Action);

                    // Swap turn after action is done
                    CurrentPlayerTurn = (AIPlayer == 1) ? 2 : 1;
                    SelectedPawn = GetPawnForPlayer(CurrentPlayerTurn);
                    bMinimaxInProgress = false;

                    UE_LOG(LogTemp, Warning, TEXT("=> Engine chose: %s (score=%d)"),
                        Action.bIsWall
                            ? *FString::Printf(TEXT("Wall @(%d,%d) %s"),
                                Action.SlotX, Action.SlotY, Action.bHorizontal ? TEXT("H") : TEXT("V"))
                            : *FString::Printf(TEXT("Move to (%d,%d)"),
                                Action.MoveX, Action.MoveY),
                        Action.Score);
                });
            });
        },
        0.5f,    // Delay in seconds
        false    // Do not loop
    );
}
bool AMinimaxBoardAI::ForcePlaceWallForAI(int32 SlotX, int32 SlotY, int32 Length, bool bHorizontal)
{
    EWallOrientation Orientation = bHorizontal ? EWallOrientation::Horizontal : EWallOrientation::Vertical;
    AWallSlot* StartSlot = FindWallSlotAt(SlotX, SlotY, Orientation);
    
    if (!StartSlot || Length <= 0)
    {
        UE_LOG(LogTemp, Error, TEXT("[ForcePlaceWallForAI] Invalid StartSlot or Length=0"));
        return false;
    }

    PendingWallLength = Length;
    PendingWallOrientation = Orientation;

    // Gather affected slots
    TArray<AWallSlot*> AffectedSlots;
    AffectedSlots.Add(StartSlot);

    for (int i = 1; i < Length; ++i)
    {
        int32 NextX = SlotX + (bHorizontal ? i : 0);
        int32 NextY = SlotY + (bHorizontal ? 0 : i);
        AWallSlot* NextSlot = FindWallSlotAt(NextX, NextY, Orientation);

        if (!NextSlot)
        {
            UE_LOG(LogTemp, Error, TEXT("[ForcePlaceWallForAI] Slot (%d,%d) [%s] not found"),
                NextX, NextY, *UEnum::GetValueAsString(Orientation));
            return false;
        }

        AffectedSlots.Add(NextSlot);
    }

    // Mark slots as occupied
    for (AWallSlot* Slot : AffectedSlots)
    {
        Slot->SetOccupied(true);
        UE_LOG(LogTemp, Warning, TEXT("Wall segment marked occupied at (%d,%d) [%s]"),
            Slot->GridX, Slot->GridY, *UEnum::GetValueAsString(Orientation));
    }

    // Spawn visual wall mesh segments
    FVector BaseLocation = StartSlot->GetActorLocation();
    FRotator WallRotation = bHorizontal ? FRotator::ZeroRotator : FRotator(0, 90, 0);

    for (int i = 0; i < Length; ++i)
    {
        FVector SegmentLocation = BaseLocation + (bHorizontal
            ? FVector(i * TileSize, 0, 100)
            : FVector(0, i * TileSize, 100));

        AActor* NewWall = GetWorld()->SpawnActor<AActor>(WallPlacementClass, SegmentLocation, WallRotation);
        if (NewWall)
        {
            NewWall->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
        }
    }

    // Remove AI's wall resource
    AQuoridorPawn* AIPawn = GetPawnForPlayer(CurrentPlayerTurn);
    if (AIPawn)
    {
        AIPawn->RemoveWallOfLength(Length);
    }

    // Advance turn
    CurrentPlayerTurn = (CurrentPlayerTurn == 1) ? 2 : 1;
    SelectedPawn = GetPawnForPlayer(CurrentPlayerTurn);

    UE_LOG(LogTemp, Warning, TEXT("[ForcePlaceWallForAI] Wall placed (%d,%d) [%s] L=%d | Turn -> Player %d"),
        SlotX, SlotY, *UEnum::GetValueAsString(Orientation), Length, CurrentPlayerTurn);

    return true;
}


void AMinimaxBoardAI::ExecuteAction(const FMinimaxAction& Act)
{
    const int32 ActingPlayer = CurrentPlayerTurn;

    if (Act.bIsWall)
    {
        // Determine orientation
        EWallOrientation Orientation = Act.bHorizontal ? EWallOrientation::Horizontal : EWallOrientation::Vertical;

        UE_LOG(LogTemp, Warning, TEXT("ExecuteAction: Player %d placing wall at (%d, %d) [%s]"),
            ActingPlayer, Act.SlotX, Act.SlotY,
            *UEnum::GetValueAsString(Orientation));

        // Use custom AI wall placement
        bool bSuccess = ForcePlaceWallForAI(Act.SlotX, Act.SlotY, Act.WallLength, Act.bHorizontal);
        if (!bSuccess)
        {
            UE_LOG(LogTemp, Error, TEXT("ExecuteAction: ForcePlaceWallForAI failed at (%d, %d) [%s]"),
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
        // Pawn move
        if (Act.MoveY >= 0 && Act.MoveY < Tiles.Num() &&
            Act.MoveX >= 0 && Act.MoveX < Tiles[Act.MoveY].Num())
        {
            ATile* TargetTile = Tiles[Act.MoveY][Act.MoveX];
            AQuoridorPawn* Pawn = GetPawnForPlayer(ActingPlayer);

            if (Pawn && TargetTile)
            {
                UE_LOG(LogTemp, Warning, TEXT("ExecuteAction: Player %d moving to tile (%d, %d)"), ActingPlayer, Act.MoveX, Act.MoveY);
                Pawn->MoveToTile(TargetTile, true);  // Skip CanMoveToTile check for AI
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("ExecuteAction: Invalid pawn or target tile for player %d"), ActingPlayer);
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("ExecuteAction: Invalid move coordinates (%d, %d)"), Act.MoveX, Act.MoveY);
        }
    }

    // Advance turn to the other player
    CurrentPlayerTurn = (ActingPlayer == 1) ? 2 : 1;
    SelectedPawn = GetPawnForPlayer(CurrentPlayerTurn);
    TurnCount++;
    bIsAITurnRunning = false;
    bDelayBeforeNextAI = true;
    AITurnDelayTimer = 2.0f;
}

