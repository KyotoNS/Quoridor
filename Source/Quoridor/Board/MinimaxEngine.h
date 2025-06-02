#pragma once

#include "CoreMinimal.h"
#include "Math/UnrealMathUtility.h" // For FIntPoint
#include "Containers/Array.h"       // For TArray

// Forward declare the AQuoridorBoard class to avoid circular dependencies
class AQuoridorBoard;

//-----------------------------------------------------------------------------
// FWallData - Represents a potential or placed wall
//-----------------------------------------------------------------------------
struct FWallData
{
    int32 X;          // Top-left X coordinate
    int32 Y;          // Top-left Y coordinate
    int32 Length;     // Length of the wall (1, 2, or 3)
    bool bHorizontal; // True if horizontal, false if vertical
};

//-----------------------------------------------------------------------------
// FMinimaxAction - Represents a possible move (pawn or wall)
//-----------------------------------------------------------------------------
struct FMinimaxAction
{
    bool bIsWall = false;
    int32 MoveX = -1;
    int32 MoveY = -1;
    int32 SlotX = -1;
    int32 SlotY = -1;
    int32 WallLength = 0; // Default remains 0
    bool bHorizontal = false;
    int32 Score = INT_MIN;

    // --- ADD THESE CONSTRUCTORS ---

    // Default constructor (needed if you have others)
    FMinimaxAction() {}

    // Pawn Move Constructor
    FMinimaxAction(int32 x, int32 y)
        : bIsWall(false), MoveX(x), MoveY(y), Score(INT_MIN) {}

    // Wall Move Constructor
    FMinimaxAction(int32 x, int32 y, int32 len, bool isH)
        : bIsWall(true), SlotX(x), SlotY(y), WallLength(len), bHorizontal(isH), Score(INT_MIN)
    {
        // Add a check right inside the constructor for safety!
        check(bIsWall == false || (WallLength > 0 && WallLength <= 3));
    }

    // --- END ADDED CONSTRUCTORS ---
};

//-----------------------------------------------------------------------------
// FMinimaxState - A lightweight representation of the board state
//-----------------------------------------------------------------------------
struct FMinimaxState
{
    int32 PawnX[2];
    int32 PawnY[2];

    // Stores the count of walls for each player and each length.
    // Index: [PlayerIndex][WallLength - 1]
    int32 WallCounts[2][3];
    int32 WallsRemaining[2];
    FIntPoint LastPawnPos[2]; // Tracks previous tile position for each player
    FIntPoint SecondLastPawnPos[2];
    bool HorizontalBlocked[8][9]; // [Y][X] - 9 rows, 8 slots per row
    bool VerticalBlocked[9][8];   // [Y][X] - 8 rows, 9 slots per row

    /** Creates a MinimaxState from the current game board */
    static FMinimaxState FromBoard(AQuoridorBoard* Board);
};

//-----------------------------------------------------------------------------
// MinimaxEngine - Contains the AI logic and search algorithms
//-----------------------------------------------------------------------------
class MinimaxEngine // Or your project's API macro, or remove if not needed
{
public:

    // --- Core AI Solvers ---

    /** Solves the current state using Minimax with Alpha-Beta Pruning (Recommended) */
    static FMinimaxAction SolveAlphaBeta(const FMinimaxState& Initial, int32 Depth, int32 RootPlayer);
    static FMinimaxAction SolveParallelAlphaBeta(const FMinimaxState& Initial, int32 Depth, int32 RootPlayer);

    /** Solves the current state using Plain Minimax (Very Slow) */
    static FMinimaxAction Solve(const FMinimaxState& Initial, int32 Depth, int32 RootPlayer); // Can wrap Minimax or AB

    /** Solves the current state using Plain Minimax in Parallel (Still Slow) */
    static FMinimaxAction SolveParallel(const FMinimaxState& Initial, int32 Depth, int32 RootPlayer);


    // --- Pathfinding & Evaluation ---

    /** Calculates the shortest path using A* (includes jumps) */
    static TArray<FIntPoint> ComputePathToGoal(const FMinimaxState& S, int32 PlayerNum, int32* OutLength = nullptr);

    /** Evaluates the board state from the perspective of RootPlayer */
    static int32 Evaluate(const FMinimaxState& S, int32 RootPlayer);
    static bool FindPathForPawn(const FMinimaxState& S, int32 PlayerNum, TArray<FIntPoint>& OutPath);

    // --- Move Generation ---

    /** Gets all valid pawn moves (including jumps) */
    static TArray<FIntPoint> GetPawnMoves(const FMinimaxState& S, int32 PlayerNum);

    /** Gets a scored/filtered list of useful wall placements */
    static TArray<FWallData> GetAllUsefulWallPlacements(const FMinimaxState& S, int32 PlayerNum);
    
    static TArray<FIntPoint> RecentMoves;


private: // These are primarily internal helpers - could be in .cpp as static

    /** Applies a pawn move to a state */
    static void ApplyPawnMove(FMinimaxState& S, int32 PlayerNum, int32 X, int32 Y);

    /** Applies a wall placement to a state (handles length & counts) */
    static void ApplyWall(FMinimaxState& S, int32 PlayerNum, const FWallData& W);

    /** The recursive Plain Minimax algorithm */
    static int32 Minimax(FMinimaxState S, int32 Depth, int32 RootPlayer, int32 CurrentPlayer);

    /** The recursive Minimax algorithm with Alpha-Beta Pruning */
    static int32 MinimaxAlphaBeta(FMinimaxState S, int32 Depth, int32 RootPlayer, int32 CurrentPlayer, int32 Alpha, int32 Beta);

    /** Checks if a wall placement is strictly legal (bounds, overlaps, basic intersections) */
    static bool IsWallPlacementStrictlyLegal(const FMinimaxState& S, const FWallData& W);

    /** Checks if applying a wall would completely block either player */
    static bool DoesWallBlockPlayer(FMinimaxState& TempState);
    static void PrintBlockedWalls(const FMinimaxState& S, const FString& Context);

    /** Helper to estimate board control/mobility using BFS */
    static void ComputeBoardControl(const FMinimaxState& S, int32& MyControl, int32& OppControl, int32 RootPlayer);
    static void PrintInventory(const FMinimaxState& S, const FString& Context);

    // (a) helper to compute path‐net‐gain
    static int32 ComputePathNetGain(
        const FMinimaxState& BeforeState,
        const FMinimaxState& AfterState,
        int32 RootPlayer,
        int32 Opponent,
        int32 BeforeAILen,
        int32 BeforeOppLen,
        int32 Multiplier);

    // (b) helper for walls
    static int32 CalculateWallScore(
        const FMinimaxState& Initial,
        const FMinimaxState& AfterState,
        int32 RootPlayer,
        int32 Opponent,
        int32 InitialAILen,
        int32 InitialOppLen);

    // (c) helper for pawn‐moves (we’ll adjust this later)
    static int32 CalculatePawnScore(
        const FMinimaxState& Initial,
        const FMinimaxState& AfterState,
        int32 RootPlayer,
        int32 Opponent,
        int32 InitialAILength,
        const TArray<FIntPoint>& AIPath,
        const FMinimaxAction& Act);
    
    
};