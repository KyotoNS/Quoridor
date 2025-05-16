#pragma once

#include "CoreMinimal.h"
#include "MinimaxEngine.generated.h"

//-----------------------------------------------------------------------------
// Encoded best action for the root of the search
//-----------------------------------------------------------------------------
USTRUCT(BlueprintType)
struct FMinimaxAction
{
    GENERATED_BODY()

    // If true, place a wall; otherwise move pawn
    UPROPERTY() bool   bIsWall     = false;
    UPROPERTY() int32  MoveX       = -1;
    UPROPERTY() int32  MoveY       = -1;

    // Wall placement
    UPROPERTY() int32  SlotX       = -1;
    UPROPERTY() int32  SlotY       = -1;
    UPROPERTY() int32  WallLength  = 1;
    UPROPERTY() bool   bHorizontal = false;

    // Score for debugging
    UPROPERTY() int32  Score       = TNumericLimits<int32>::Min();
};

//-----------------------------------------------------------------------------
// Pure data snapshot of board for Minimax
//-----------------------------------------------------------------------------
USTRUCT()
struct FMinimaxState
{
    GENERATED_BODY()

    int32 PawnX[2];               // [0]=Player1, [1]=Player2
    int32 PawnY[2];
    int32 WallsRemaining[2];      // walls left per player

    // Blocked edges by walls:
    // Horizontal[y][x]: between (x,y)<->(x,y+1)
    bool HorizontalBlocked[9][8];
    // Vertical[y][x]: between (x,y)<->(x+1,y)
    bool VerticalBlocked[8][9];

    // Snapshot from actor-based board
    static FMinimaxState FromBoard(class AQuoridorBoard* Board);
};

//-----------------------------------------------------------------------------
// Wall data for simulation
//-----------------------------------------------------------------------------
USTRUCT()
struct FWallData
{
    GENERATED_BODY()

    int32 X;
    int32 Y;
    int32 Length;
    bool  bHorizontal;
};

//-----------------------------------------------------------------------------
// Pure Minimax engine namespace
//-----------------------------------------------------------------------------
namespace MinimaxEngine
{
    int32 Evaluate(const FMinimaxState& S);
    TArray<FIntPoint> GetPawnMoves(const FMinimaxState& S, int32 PlayerNum);
    TArray<FWallData>  GetWallPlacements(const FMinimaxState& S, int32 PlayerNum);
    void ApplyPawnMove(FMinimaxState& S, int32 PlayerNum, int32 X, int32 Y);
    void ApplyWall    (FMinimaxState& S, int32 PlayerNum, const FWallData& W);
    int32 Minimax(FMinimaxState& S, int32 Depth, bool bMaximizing);
    FMinimaxAction Solve(const FMinimaxState& Initial, int32 Depth);
    static TArray<FIntPoint> ComputePathToGoal(const FMinimaxState& S, int32 PlayerNum, int32* OutLength = nullptr);
    static bool WallTouchesPath(const FWallData& w, const TArray<FIntPoint>& Path);
    TArray<FWallData>GetTargetedWallPlacements(const FMinimaxState& S, int32 PlayerNum);

}

