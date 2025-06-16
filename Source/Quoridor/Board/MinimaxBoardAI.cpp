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
    bool bAI1IsPlayer1 = 1;

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
    if (bDelayPassed && CurrentPlayerTurn == AI1Player && !bIsAITurnRunning)
    {
        AQuoridorPawn* P = GetPawnForPlayer(CurrentPlayerTurn);
        if (P && P->GetTile())
        {
            bIsAITurnRunning = true;
            RunMinimax(AI1Player, 3);
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
            RunMinimax(AI2Player, 4);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("AI pawn not ready yet"));
        }
    }
    // if (bDelayPassed && CurrentPlayerTurn == AI2Player && !bIsAITurnRunning)
    // {
    //     AQuoridorPawn* P = GetPawnForPlayer(CurrentPlayerTurn);
    //     if (P && P->GetTile())
    //     {
    //         bIsAITurnRunning = true;
    //         RunMinimaxForAlphaBeta(AI2Player);
    //     }
    //     else
    //     {
    //         UE_LOG(LogTemp, Warning, TEXT("AI pawn not ready yet"));
    //     }
    // }
}

// Parallel
void AMinimaxBoardAI::RunMinimax(int32 Player, int32 algo)
{
    FString SelectedAIType;
    switch (algo)
    {
    case 1:
        SelectedAIType = TEXT("Plain Minimax");
        break;
    case 2:
        SelectedAIType = TEXT("Minimax Parallel");
        break;
    case 3:
        SelectedAIType = TEXT("Minimax Alpha Beta");
        break;  
    case 4:
        SelectedAIType = TEXT("Parallel Minimax Alpha-Beta");
        break;
    default:
        SelectedAIType = TEXT("Unknown AI");
        break;
    }

    if (Player == 1)
    {
        this->AITypeName1 = SelectedAIType;
        UE_LOG(LogTemp, Log, TEXT("Player 1 AI type set to: %s"), *this->AITypeName1);
    }
    else if (Player == 2)
    {
        this->AITypeName2 = SelectedAIType;
        UE_LOG(LogTemp, Log, TEXT("Player 2 AI type set to: %s"), *this->AITypeName2);
    }
    if (bMinimaxInProgress)
        return;

    const int32 AIPlayer = Player;
    const int32 Choice = algo;
    bMinimaxInProgress = true;

    // Create a timer handle so we can schedule a Delay
    FTimerHandle DelayHandle;
    // Schedule the next block to run in 2.0 seconds instead of next tick
    GetWorld()->GetTimerManager().SetTimer(
        DelayHandle,
        [this, AIPlayer, Choice]()
        {
            if (AIPlayer == 1)
                ThinkingStartTimeP1 = FPlatformTime::Seconds();
            else if (AIPlayer == 2)
                ThinkingStartTimeP2 = FPlatformTime::Seconds();
            
            FMinimaxState StateSnapshot = FMinimaxState::FromBoard(this);
            int32 Depth = 4;

            // Run the actual minimax on a background thread
            Async(EAsyncExecution::Thread, [this, StateSnapshot, Depth, AIPlayer,Choice]()
            {
                FMinimaxResult Action = MinimaxEngine::RunSelectedAlgorithm(StateSnapshot,Depth,AIPlayer,Choice);

                // Once SolveParallel finishes, come back to GameThread to execute the move
                AsyncTask(ENamedThreads::GameThread, [this, Action, AIPlayer]()
                {
                    double EndTime = FPlatformTime::Seconds();
                    double Elapsed = 0.0;

                    if (AIPlayer == 1)
                    {
                        Elapsed = EndTime - ThinkingStartTimeP1;
                        TotalThinkingTimeP1 += Elapsed;
                        UE_LOG(LogTemp, Warning, TEXT("[AI P1] Thinking Time: %.4f s | Total: %.4f s"), Elapsed, TotalThinkingTimeP1);
                    }
                    else if (AIPlayer == 2)
                    {
                        
                        Elapsed = EndTime - ThinkingStartTimeP2;
                        TotalThinkingTimeP2 += Elapsed;
                        UE_LOG(LogTemp, Warning, TEXT("[AI P2] Thinking Time: %.4f s | Total: %.4f s"), Elapsed, TotalThinkingTimeP2);
                    }
                    const FMinimaxAction& BestAct = Action.BestAction;
                    ExecuteAction(BestAct);

                    // Swap turn after action is done
                    // CurrentPlayerTurn = (AIPlayer == 1) ? 2 : 1;
                    // SelectedPawn = GetPawnForPlayer(CurrentPlayerTurn);
                    bMinimaxInProgress = false;

                    // 4) Log detail aksi dan skor:
                    if (BestAct.bIsWall)
                    {
                        UE_LOG(LogTemp, Warning,
                            TEXT("=> Engine chose: Wall @(%d,%d) %s (score=%d)"),
                            BestAct.SlotX,
                            BestAct.SlotY,
                            BestAct.bHorizontal ? TEXT("H") : TEXT("V"),
                            Action.BestValue);
                    }
                    else
                    {
                        UE_LOG(LogTemp, Warning,
                            TEXT("=> Engine chose: Move to (%d,%d) (score=%d)"),
                            BestAct.MoveX,
                            BestAct.MoveY,
                            Action.BestValue);
                    }
                });
            });
        },
        2.0f,    // Delay in seconds
        false    // Do not loop
    );
}

float AMinimaxBoardAI::GetTotalThinkingTimeForPlayer(int32 PlayerNum) const
{
    if (PlayerNum == 1)
        return TotalThinkingTimeP1;
    else if (PlayerNum == 2)
        return TotalThinkingTimeP2;

    return 0.0f;
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
            ? FVector(i * TileSize, 0, 50)
            : FVector(0, i * TileSize, 50));

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
}

