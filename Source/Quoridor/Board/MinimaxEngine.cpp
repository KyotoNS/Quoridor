// MinimaxEngine.cpp
#include "MinimaxEngine.h"
#include <queue>
#include <vector>
#include <limits.h>
#include <functional>
#include <algorithm> // Required for std::sort
#include "Quoridor/Board/QuoridorBoard.h"
#include "Quoridor/Wall/WallSlot.h"
#include "Async/ParallelFor.h"
#include "Quoridor/Pawn/QuoridorPawn.h"
#include "Math/UnrealMathUtility.h"
TArray<FIntPoint> MinimaxEngine::RecentMoves;


//-----------------------------------------------------------------------------
// FMinimaxState::FromBoard (bener)
//-----------------------------------------------------------------------------
FMinimaxState FMinimaxState::FromBoard(AQuoridorBoard* Board)
{
    FMinimaxState S;

    // 1) Zero‐out all blocked‐edge arrays
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 9; ++x)
            S.HorizontalBlocked[y][x] = false;

    for (int y = 0; y < 9; ++y)
        for (int x = 0; x < 8; ++x)
            S.VerticalBlocked[y][x] = false;

    // 2) Copy pawn positions + wall counts
    for (int player = 1; player <= 2; ++player)
    {
        AQuoridorPawn* P = Board->GetPawnForPlayer(player);
        int idx = player - 1;

        if (P && P->GetTile())
        {
            S.PawnX[idx] = P->GetTile()->GridX;
            S.PawnY[idx] = P->GetTile()->GridY;
        }
        else
        {
            S.PawnX[idx] = -1;
            S.PawnY[idx] = -1;
            UE_LOG(LogTemp, Error, TEXT("FromBoard: Player %d pawn missing"), player);
        }

        if (P)
        {
            S.WallCounts[idx][0]  = P->GetWallCountOfLength(1);
            S.WallCounts[idx][1]  = P->GetWallCountOfLength(2);
            S.WallCounts[idx][2]  = P->GetWallCountOfLength(3);
            S.WallsRemaining[idx] = S.WallCounts[idx][0]
                                   + S.WallCounts[idx][1]
                                   + S.WallCounts[idx][2];
        }
        else
        {
            S.WallCounts[idx][0]  = 0;
            S.WallCounts[idx][1]  = 0;
            S.WallCounts[idx][2]  = 0;
            S.WallsRemaining[idx] = 0;
        }
    }

    // 3) Horizontal slots → HorizontalBlocked
    for (AWallSlot* Slot : Board->HorizontalWallSlots)
    {
        if (!Slot || !Slot->bIsOccupied)
            continue;

        int slotX = Slot->GridX;  // expected 0..8
        int slotY = Slot->GridY;  // expected 0..7

        if (slotX < 0 || slotX > 8 || slotY < 0 || slotY > 7)
            continue;

        // Block two adjacent horizontal edges
        S.HorizontalBlocked[slotY][slotX] = true;

        UE_LOG(LogTemp, Warning,
            TEXT("FromBoard: Mapped H‐Wall @ (X=%d, Y=%d)"),
            slotX, slotY);
    }


    // 4) Vertical slots → VerticalBlocked
    for (AWallSlot* Slot : Board->VerticalWallSlots)
    {
        if (!Slot || !Slot->bIsOccupied)
            continue;

        int slotX = Slot->GridX;  // expected 0..7
        int slotY = Slot->GridY;  // expected 0..8

        if (slotX < 0 || slotX > 7 || slotY < 0 || slotY > 8)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("FromBoard: Skipping V‐Wall @ (X=%d, Y=%d) because out of range"), 
                slotX, slotY);
            continue;
        }

        // Block two adjacent vertical edges
        S.VerticalBlocked[slotY][slotX] = true;

        UE_LOG(LogTemp, Warning,
            TEXT("FromBoard: Mapped V‐Wall @ (X=%d, Y=%d)"),
            slotX, slotY);
    }


    return S;
}

//-----------------------------------------------------------------------------
// ComputePathToGoal (A* with Jumps)(bener, kayaknya )
//-----------------------------------------------------------------------------
TArray<FIntPoint> MinimaxEngine::ComputePathToGoal(const FMinimaxState& S,int32 PlayerNum,int32* OutLength)
{
    const int goalY = (PlayerNum == 1 ? 8 : 0);
    const int idx   = PlayerNum - 1;

    int sx = S.PawnX[idx];
    int sy = S.PawnY[idx];
    if (sx < 0 || sx > 8 || sy < 0 || sy > 8)
    {
        UE_LOG(LogTemp, Error,
            TEXT("ComputePathToGoal: Invalid pawn position for Player %d => (%d,%d)"),
            PlayerNum, sx, sy);
        if (OutLength) *OutLength = 100;
        return {};
    }

    bool closed[9][9] = {};
    int gCost[9][9];
    FIntPoint cameFrom[9][9];

    for (int y = 0; y < 9; ++y)
    {
        for (int x = 0; x < 9; ++x)
        {
            gCost[y][x]    = INT_MAX;
            cameFrom[y][x] = FIntPoint(-1, -1);
        }
    }

    auto Heuristic = [&](int x, int y) {
        return FMath::Abs(goalY - y);
    };

    struct Node { int f, g, x, y; };
    struct Cmp { bool operator()(Node const &A, Node const &B) const { return A.f > B.f; } };
    std::priority_queue<Node, std::vector<Node>, Cmp> open;

    gCost[sy][sx] = 0;
    open.push({ Heuristic(sx, sy), 0, sx, sy });

    const FIntPoint dirs[4] = {
        FIntPoint(+1,  0),
        FIntPoint(-1,  0),
        FIntPoint( 0, +1),
        FIntPoint( 0, -1)
    };

    int OpponentIdx = 1 - idx;

    while (!open.empty())
    {
        Node n = open.top(); open.pop();
        if (closed[n.y][n.x]) continue;
        closed[n.y][n.x] = true;

        if (n.y == goalY)
        {
            if (OutLength) *OutLength = n.g;
            TArray<FIntPoint> Path;
            for (int cx = n.x, cy = n.y; cx != -1 && cy != -1; ) {
                Path.Insert(FIntPoint(cx, cy), 0);
                FIntPoint p = cameFrom[cy][cx];
                cx = p.X; cy = p.Y;
            }
            return Path;
        }

        for (const FIntPoint& d : dirs)
        {
            int nx = n.x + d.X;
            int ny = n.y + d.Y;
            if (nx < 0 || nx > 8 || ny < 0 || ny > 8) continue;

            bool blocked = false;
            if (d.X == 1 && S.VerticalBlocked[n.y][n.x]) blocked = true;
            else if (d.X == -1 && S.VerticalBlocked[n.y][n.x - 1]) blocked = true;
            else if (d.Y == 1 && S.HorizontalBlocked[n.y][n.x]) blocked = true;
            else if (d.Y == -1 && S.HorizontalBlocked[n.y - 1][n.x]) blocked = true;

            if (blocked) continue;

            bool isJump = (nx == S.PawnX[OpponentIdx] && ny == S.PawnY[OpponentIdx]);
            if (isJump)
            {
                int jx = nx + d.X;
                int jy = ny + d.Y;

                if (jx >= 0 && jx <= 8 && jy >= 0 && jy <= 8)
                {
                    bool jumpBlocked = false;
                    if (d.X == 1 && S.VerticalBlocked[ny][nx]) jumpBlocked = true;
                    else if (d.X == -1 && S.VerticalBlocked[ny][nx - 1]) jumpBlocked = true;
                    else if (d.Y == 1 && S.HorizontalBlocked[ny][nx]) jumpBlocked = true;
                    else if (d.Y == -1 && S.HorizontalBlocked[ny - 1][nx]) jumpBlocked = true;

                    if (!jumpBlocked && !closed[jy][jx])
                    {
                        int ng = n.g + 2;
                        if (ng < gCost[jy][jx])
                        {
                            gCost[jy][jx] = ng;
                            cameFrom[jy][jx] = FIntPoint(n.x, n.y);
                            int h = Heuristic(jx, jy);
                            open.push({ ng + h, ng, jx, jy });
                        }
                    }
                }
                else
                {
                    for (const FIntPoint& side : { FIntPoint(d.Y, d.X), FIntPoint(-d.Y, -d.X) })
                    {
                        int sideX = nx + side.X;
                        int sideY = ny + side.Y;
                        if (sideX >= 0 && sideX <= 8 && sideY >= 0 && sideY <= 8 && !closed[sideY][sideX])
                        {
                            int ng = n.g + 2;
                            if (ng < gCost[sideY][sideX])
                            {
                                gCost[sideY][sideX] = ng;
                                cameFrom[sideY][sideX] = FIntPoint(n.x, n.y);
                                int h = Heuristic(sideX, sideY);
                                open.push({ ng + h, ng, sideX, sideY });
                            }
                        }
                    }
                }
            }
            else if (!closed[ny][nx])
            {
                int ng = n.g + 1;
                if (ng < gCost[ny][nx])
                {
                    gCost[ny][nx] = ng;
                    cameFrom[ny][nx] = FIntPoint(n.x, n.y);
                    int h = Heuristic(nx, ny);
                    open.push({ ng + h, ng, nx, ny });
                }
            }
        }
    }

    if (OutLength) *OutLength = 100;
    return {};
}

//-----------------------------------------------------------------------------
// Wall Legality Check (bener)
//-----------------------------------------------------------------------------
bool MinimaxEngine::IsWallPlacementStrictlyLegal(const FMinimaxState& S,const FWallData& W)
{
    // ------------------------------------------------------------
    // 1) Bounds check for the entire wall
    // ------------------------------------------------------------
    if (W.bHorizontal)
    {
        if (W.Y < 0 || W.Y >= 8 ||
            W.X < 0 || W.X + W.Length - 1 > 8)
        {
            return false;
        }
    }
    else
    {
        if (W.X < 0 || W.X >= 8 ||
            W.Y < 0 || W.Y + W.Length - 1 > 8)
        {
            return false;
        }
    }

    for (int i = 0; i < W.Length; ++i)
    {
        int cx = W.bHorizontal ? (W.X + i) : W.X;
        int cy = W.bHorizontal ? W.Y       : (W.Y + i);

        if (W.bHorizontal)
        {
            if (S.HorizontalBlocked[cy][cx])
            {
                return false;
            }
            
            if (cy + 1 <= 8)
            {
                if (S.VerticalBlocked[cy][cx] &&
                    S.VerticalBlocked[cy + 1][cx])
                {
                    return false;
                }
            }
        }
        else
        {
            // 2c) Overlap: check if this vertical segment is already blocked
            //    VerticalBlocked has 9 rows × 8 columns, so valid indices are:
            //      0 ≤ cy < 9  and  0 ≤ cx < 8
            if (S.VerticalBlocked[cy][cx])
            {
                // This exact segment is already occupied by another vertical wall.
                return false;
            }

            // 2d) Intersection (cross): does a horizontal wall “cross” here?
            //    A length‐2 horizontal slot at X=cx, Y=cy would mark:
            //      HorizontalBlocked[cy][cx]   and  
            //      HorizontalBlocked[cy][cx+1]
            //    So check if both those are already true—but only if (cx+1) ≤ 8.
            if (cx + 1 <= 8)
            {
                if (S.HorizontalBlocked[cy][cx] &&
                    S.HorizontalBlocked[cy][cx + 1])
                {
                    // A horizontal slot occupies those segments, so a new vertical
                    // segment at (cx, cy) would cross it at the T‐junction.
                    return false;
                }
            }
        }
    }

    // ------------------------------------------------------------
    // 3) If we reach here, there’s no overlap or “simple cross.” It’s strictly legal.
    // ------------------------------------------------------------
    return true;
}

//-----------------------------------------------------------------------------
// Check if wall blocks a player(bener)
//-----------------------------------------------------------------------------
bool MinimaxEngine::DoesWallBlockPlayer(FMinimaxState& TempState)
{
    int32 TestLen1 = 100, TestLen2 = 100;
    ComputePathToGoal(TempState, 1, &TestLen1);
    ComputePathToGoal(TempState, 2, &TestLen2);
    return (TestLen1 >= 100 || TestLen2 >= 100);
}

//-----------------------------------------------------------------------------
// print wall wall yang udah di taro ( bener)
//-----------------------------------------------------------------------------
void MinimaxEngine::PrintBlockedWalls(const FMinimaxState& S, const FString& Context)
{
    UE_LOG(LogTemp, Warning, TEXT("--- Blocked Walls (%s) ---"), *Context);

    int32 HorizontalWallCount = 0;
    int32 VerticalWallCount   = 0;

    // --- Print every blocked‐edge in HorizontalBlocked[y][x] (9 rows × 8 columns) ---
    for (int y = 0; y < 8; ++y)
    {
        for (int x = 0; x < 9; ++x)
        {
            if (S.HorizontalBlocked[y][x])
            {
                UE_LOG(LogTemp, Warning, TEXT("  H‐Blocked @ (X=%d, Y=%d)"), x, y);
                HorizontalWallCount++;
            }
        }
    }

    // --- Print every blocked‐edge in VerticalBlocked[y][x] (8 rows × 9 columns) ---
    for (int y = 0; y < 9; ++y)
    {
        for (int x = 0; x < 8; ++x)   // x < 9, not x <= 9
        {
            if (S.VerticalBlocked[y][x])
            {
                UE_LOG(LogTemp, Warning, TEXT("  V‐Blocked @ (X=%d, Y=%d)"), x, y);
                VerticalWallCount++;
            }
        }
    }

    // --- Totals ---
    if (HorizontalWallCount == 0 && VerticalWallCount == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("  No walls found on board."));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("  Total H‐edges blocked: %d"), HorizontalWallCount);
        UE_LOG(LogTemp, Warning, TEXT("  Total V‐edges blocked: %d"), VerticalWallCount);
        UE_LOG(LogTemp, Warning, TEXT("  Overall blocked‐edges:  %d"), HorizontalWallCount + VerticalWallCount);
    }

    UE_LOG(LogTemp, Warning, TEXT("--------------------------"));
}

//-----------------------------------------------------------------------------
// Get All Useful Wall Placements (bener)
//-----------------------------------------------------------------------------
TArray<FWallData> MinimaxEngine::GetAllUsefulWallPlacements(const FMinimaxState& S, int32 PlayerNum)
{
    TArray<FWallData> FinalCandidates;
    int32 idx      = PlayerNum - 1;

    if (S.WallsRemaining[idx] <= 0) return FinalCandidates;

    TArray<int32> AvailableLengths;
    if (S.WallCounts[idx][0] > 0) AvailableLengths.Add(1);
    if (S.WallCounts[idx][1] > 0) AvailableLengths.Add(2);
    if (S.WallCounts[idx][2] > 0) AvailableLengths.Add(3);

    if (AvailableLengths.Num() == 0) return FinalCandidates;

    const int32 Opponent = 3 - PlayerNum;
    int32 MyPathLen = 0, OppPathLen = 0;
    ComputePathToGoal(S, PlayerNum, &MyPathLen);
    ComputePathToGoal(S, Opponent, &OppPathLen);

    if (MyPathLen >= 100 || OppPathLen >= 100) return FinalCandidates;

    struct ScoredWall { FWallData Wall; int32 Score; };
    TArray<ScoredWall> AllLegalWalls;

    int BlockedCount = 0;
    int OverlapCount = 0;
    int LowScoreCount = 0;

    for (int length : AvailableLengths)
    {
        // Horizontal Walls
        for (int y = 0; y < 8; ++y) {
            for (int x = 0; x <= 8 - length; ++x) {
                FWallData W{ x, y, length, true };
                if (!IsWallPlacementStrictlyLegal(S, W)) {
                    OverlapCount++;
                    continue;
                }

                FMinimaxState SimulatedState = S;
                ApplyWall(SimulatedState, PlayerNum, W);

                if (DoesWallBlockPlayer(SimulatedState)) {
                    BlockedCount++;
                    continue;
                }

                int32 NewMyPathLen = 100, NewOppPathLen = 100;
                ComputePathToGoal(SimulatedState, PlayerNum, &NewMyPathLen);
                ComputePathToGoal(SimulatedState, Opponent, &NewOppPathLen);
                int32 OppDelta = NewOppPathLen - OppPathLen;
                int32 MyDelta = NewMyPathLen - MyPathLen;
                int32 WallScore = (OppDelta * 10) - (MyDelta * 15);

                if (WallScore < -10) {
                    LowScoreCount++;
                    continue;
                }

                check(W.Length > 0 && W.Length <= 3);
                AllLegalWalls.Add({W, WallScore});
            }
        }

        // Vertical Walls
        for (int y = 0; y <= 8 - length; ++y) {
            for (int x = 0; x < 8; ++x) {
                FWallData W{ x, y, length, false };
                if (!IsWallPlacementStrictlyLegal(S, W)) {
                    OverlapCount++;
                    continue;
                }

                FMinimaxState SimulatedState = S;
                ApplyWall(SimulatedState, PlayerNum, W);

                if (DoesWallBlockPlayer(SimulatedState)) {
                    BlockedCount++;
                    continue;
                }

                int32 NewMyPathLen = 100, NewOppPathLen = 100;
                ComputePathToGoal(SimulatedState, PlayerNum, &NewMyPathLen);
                ComputePathToGoal(SimulatedState, Opponent, &NewOppPathLen);
                int32 OppDelta = NewOppPathLen - OppPathLen;
                int32 MyDelta = NewMyPathLen - MyPathLen;
                int32 WallScore = (OppDelta * 10) - (MyDelta * 15);

                if (WallScore < -10) {
                    LowScoreCount++;
                    continue;
                }

                check(W.Length > 0 && W.Length <= 3);
                AllLegalWalls.Add({W, WallScore});
            }
        }
    }

    // Sort by score descending (highest first)
    AllLegalWalls.Sort([](const ScoredWall& A, const ScoredWall& B) {
        return A.Score > B.Score;
    });

    const int MaxWallCandidates = 25;
    int32 NumToTake = FMath::Min(AllLegalWalls.Num(), MaxWallCandidates);

    // Collect top‐scoring walls into FinalCandidates
    for (int32 i = 0; i < NumToTake; ++i) {
        FinalCandidates.Add(AllLegalWalls[i].Wall);
    }

    return FinalCandidates;
}


//-----------------------------------------------------------------------------
// Get Pawn Moves (sejauh ini bener)
//-----------------------------------------------------------------------------
TArray<FIntPoint> MinimaxEngine::GetPawnMoves(const FMinimaxState& S, int32 PlayerNum)
{
    TArray<FIntPoint> Out;
    Out.Reserve(8);

    int idx = PlayerNum - 1;
    int ax  = S.PawnX[idx];
    int ay  = S.PawnY[idx];

    // Quick check in case the pawn isn't on board:
    if (ax < 0 || ax > 8 || ay < 0 || ay > 8)
        return Out;

    // Helper: true if either pawn currently occupies (tx,ty).
    auto IsOccupied = [&](int tx, int ty) -> bool
    {
        return ((S.PawnX[0] == tx && S.PawnY[0] == ty) ||
                (S.PawnX[1] == tx && S.PawnY[1] == ty));
    };

    // Helper: returns true if the edge between (fromX,fromY) → (toX,toY) is blocked.
    // Also returns true if (toX,toY) is off‐board.
    auto IsBlocked = [&](int fromX, int fromY, int toX, int toY) -> bool
    {
        // 1) Must stay on the 9×9 board.
        if (toX < 0 || toX > 8 || toY < 0 || toY > 8)
            return true;

        // 2) Moving Right?
        if (toX == fromX + 1 && toY == fromY)
        {
            // Check VerticalBlocked[fromY][fromX], valid whenever
            // fromY ∈ [0..8] and fromX ∈ [0..7].
            if (fromY >= 0 && fromY < 9 && fromX >= 0 && fromX < 8)
            {
                if (S.VerticalBlocked[fromY][fromX])
                    return true;
            }
        }

        // 3) Moving Left?
        if (toX == fromX - 1 && toY == fromY)
        {
            // Check VerticalBlocked[fromY][toX], valid whenever
            // fromY ∈ [0..8] and toX ∈ [0..7].
            if (fromY >= 0 && fromY < 9 && toX >= 0 && toX < 8)
            {
                if (S.VerticalBlocked[fromY][toX])
                    return true;
            }
        }

        // 4) Moving Down?
        if (toX == fromX && toY == fromY + 1)
        {
            // Check HorizontalBlocked[fromY][fromX], valid whenever
            // fromY ∈ [0..7] and fromX ∈ [0..8].
            if (fromY >= 0 && fromY < 8 && fromX >= 0 && fromX < 9)
            {
                if (S.HorizontalBlocked[fromY][fromX])
                    return true;
            }
        }

        // 5) Moving Up?
        if (toX == fromX && toY == fromY - 1)
        {
            // Check HorizontalBlocked[toY][fromX], valid whenever
            // toY ∈ [0..7] and fromX ∈ [0..8].
            if (toY >= 0 && toY < 8 && fromX >= 0 && fromX < 9)
            {
                if (S.HorizontalBlocked[toY][fromX])
                    return true;
            }
        }

        // Otherwise, no wall is blocking that edge
        return false;
    };

    // Four cardinal directions
    const FIntPoint dirs[4] = { { 1,  0},  // right
                                { -1, 0},  // left
                                { 0,  1},  // down
                                { 0, -1} };// up

    for (const auto& d : dirs)
    {
        int nx = ax + d.X;
        int ny = ay + d.Y;

        // (1) If the step from (ax,ay) to (nx,ny) is blocked, skip.
        if (IsBlocked(ax, ay, nx, ny))
            continue;

        // (2) If (nx,ny) is unoccupied, that’s a valid “step” move.
        if (!IsOccupied(nx, ny))
        {
            Out.Add({ nx, ny });
            continue;
        }

        // (3) Otherwise, the adjacent square (nx,ny) is occupied by the other pawn.
        //     Try jumping straight over it:
        int jx = nx + d.X;
        int jy = ny + d.Y;

        if (!IsBlocked(nx, ny, jx, jy) && !IsOccupied(jx, jy))
        {
            Out.Add({ jx, jy });
            continue;
        }

        // (4) If the straight‐ahead jump is blocked by a wall or goes off‐board,
        //     then we attempt the two “diagonal” sidestep moves around (nx,ny).
        FIntPoint perp[2] = { { d.Y,  d.X},   // 90° rotated
                             {-d.Y, -d.X} }; // -90° rotated

        for (const auto& p : perp)
        {
            int sx = nx + p.X;
            int sy = ny + p.Y;

            // Check if stepping from (nx,ny) to (sx,sy) is legal (not blocked) and unoccupied:
            if (!IsBlocked(nx, ny, sx, sy) && !IsOccupied(sx, sy))
            {
                Out.Add({ sx, sy });
            }
        }
    }

    return Out;
}


//-----------------------------------------------------------------------------
// print inventory ai , wall apa aja yand dipunya
//-----------------------------------------------------------------------------
void MinimaxEngine::PrintInventory(const FMinimaxState& S, const FString& Context)
{
    UE_LOG(LogTemp, Warning, TEXT("--- Wall Inventory (%s) ---"), *Context);
    for (int i = 0; i < 2; ++i)
    {
        UE_LOG(LogTemp, Warning, TEXT("  Player %d: L1=%d, L2=%d, L3=%d | Total = %d"),
               i + 1,               // Player Number (1 or 2)
               S.WallCounts[i][0],  // Count for Length 1
               S.WallCounts[i][1],  // Count for Length 2
               S.WallCounts[i][2],  // Count for Length 3
               S.WallsRemaining[i]);// Total Walls
    }
    UE_LOG(LogTemp, Warning, TEXT("--------------------------------"));
}

int32 MinimaxEngine::EvaluatePawnMove(const FMinimaxState& AfterState,int32 RootPlayer,int32 BeforeAILength,const TArray<FIntPoint>& BeforeAIPath,const FIntPoint& PrevPos,const FIntPoint& Prev2Pos)
{
    // 1) Determine the AI’s new pawn position
    const int32 idxAI = RootPlayer - 1;
    FIntPoint CurrPos(AfterState.PawnX[idxAI], AfterState.PawnY[idxAI]);

    int32 score = 0;

    // 2) Path‐following bonus: if there is a “next step” in BeforeAIPath and
    //    the pawn landed exactly on it, give a large bonus.
    if (BeforeAIPath.Num() > 1 && CurrPos == BeforeAIPath[1]) {
        score += 1000;  // tune this as needed
    }

    // 3) Goal‐row bonus: if CurrPos.Y is the goal row, add a small bonus.
    //    For Player 1, goal row is y=8; for Player 2, goal row is y=0.
    int32 goalY = (RootPlayer == 1 ? 8 : 0);
    if (CurrPos.Y == goalY) {
        score += 20;
    }

    return score;
    
}

int32 MinimaxEngine::EvaluateWallPlacement(const FMinimaxState& BeforeState,const FMinimaxState& AfterState,int32 RootPlayer,int32 BeforeAILength,int32 BeforeOppLength)
{
    const int idxAI       = RootPlayer - 1;
    const int idxOpp      = 1 - idxAI;
    const int OpponentNum = 3 - RootPlayer;

    // 1) Recompute A* path lengths after placing the wall
    int32 AfterAILength = 100;
    int32 AfterOppLength = 100;
    ComputePathToGoal(AfterState, RootPlayer,   &AfterAILength);
    ComputePathToGoal(AfterState, OpponentNum,  &AfterOppLength);
    

    // 3) Compute “net‐gain” in path lengths (Opponent’s path – AI’s path),
    //    compared to before the wall, and multiply by a weight
    constexpr int32 WallNetGainMultiplier = 20;
    int32 netGain = (AfterOppLength - AfterAILength)
                  - (BeforeOppLength - BeforeAILength);
    int32 Score = netGain * WallNetGainMultiplier;

    // 4) Strategic bonus: if this new wall lies on the opponent’s goal row,
    //    add a small extra reward for hindering their final crossing.
    int32 oppGoalRow = (RootPlayer == 1 ? 0 : 8);
    for (int x = 0; x < 8; ++x)
    {
        if (AfterState.HorizontalBlocked[oppGoalRow][x])
        {
            Score += 300;  // tune this value as needed
        }
    }

    return Score;
}



//-----------------------------------------------------------------------------
// Evaluate (sudah)
//-----------------------------------------------------------------------------
int32 MinimaxEngine::Evaluate(const FMinimaxState& S, int32 RootPlayer)
{
    // 1 ) declare score
    int32 Score = 0;
    // 2 ) index player dan musuh ( 0 and 1 )
    int32 idxAI       = RootPlayer - 1;
    int32 idxOpp      = 1 - idxAI;
    // 3 ) turn player dan musuh ( 1 and 2 ) -- rootplayer = curr turn -- oppnum = opp turn
    int32 OpponentNum = 3 - RootPlayer;

    // 4) Recompute A* path lengths for both players
    int32 AILen = 0, OppLen = 0;
    TArray<FIntPoint> AIPath  = ComputePathToGoal(S, RootPlayer,   &AILen);
    TArray<FIntPoint> OppPath = ComputePathToGoal(S, OpponentNum, &OppLen);

    if (AILen == 0)
    {
        Score += 10000;
    }
    if (OppLen == 0)
    {
        Score -= 10000;
    }
    // 5 ) kalau len Aipath tidak 0 maka 
    if (AIPath.Num()!= 0 )
    {
        // Tambahan pengecekan untuk menghindari pembagian dengan nol jika AILen bisa 0
        Score += static_cast<int32>(std::round((100.0f / AILen) * 10000.0f) / 10000.0f);
        
    }
    else
    {
        Score += 1000;
    }

    // 6 ) kalau len Opp tidak 0 maka 
    if (OppPath.Num()!= 0 )
    {
        // Tambahan pengecekan untuk menghindari pembagian dengan nol jika AILen bisa 0
        Score -= static_cast<int32>(std::round((50.0f / OppLen) * 10000.0f) / 10000.0f);
    }
    else
    {
        Score -= 500;
    }

    // 7 ) cek apakah kita punya wall lebih banyak atau ga , (kalau lebih banyak berarti advantage )
    Score += (S.WallsRemaining[idxAI] - S.WallsRemaining[idxOpp]) * 10;
    // 8 ) untuk Menghindari Perilaku Pasif atau "Menimbun" Dinding
    Score -= S.WallsRemaining[idxAI] * 5;

    return Score;
}


//-----------------------------------------------------------------------------
// Apply Pawn Move ( harus nya bener )
//-----------------------------------------------------------------------------
void MinimaxEngine::ApplyPawnMove(FMinimaxState& S, int32 PlayerNum, int32 X, int32 Y)
{
    int idx = PlayerNum - 1;

    // Shift last position history
    S.SecondLastPawnPos[idx] = S.LastPawnPos[idx];
    S.LastPawnPos[idx] = FIntPoint(S.PawnX[idx], S.PawnY[idx]);

    // Update current position
    S.PawnX[idx] = X;
    S.PawnY[idx] = Y;
}


//-----------------------------------------------------------------------------
// Apply Wall (Updated for WallCounts & Length) -- ( harusnya bener _
//-----------------------------------------------------------------------------
void MinimaxEngine::ApplyWall(FMinimaxState& S, int32 PlayerNum, const FWallData& W)
{
    int idx = PlayerNum - 1;

    // *** Sanity check length at the top ***
    if (W.Length <= 0 || W.Length > 3)
    {
        UE_LOG(LogTemp, Error,
            TEXT("ApplyWall: Player %d called with INVALID wall length %d! Aborting."),
            PlayerNum, W.Length);
        return;
    }
    // *** End check ***

    int lenIdx = W.Length - 1; // maps 1→0, 2→1, 3→2

    // --- Apply each segment of the wall ---
    for (int i = 0; i < W.Length; ++i)
    {
        int cx = W.bHorizontal ? (W.X + i) : W.X;
        int cy = W.bHorizontal ? W.Y       : (W.Y + i);

        if (W.bHorizontal)
        {
            // HorizontalBlocked is [8 rows][9 columns], so valid indices are:
            //    0 ≤ cy < 8   and   0 ≤ cx < 9
            if (cy >= 0 && cy < 8 && cx >= 0 && cx < 9)
            {
                S.HorizontalBlocked[cy][cx] = true;
            }
            else
            {
                UE_LOG(LogTemp, Error,
                    TEXT("ApplyWall: H‐Wall segment out of bounds at (%d,%d). "
                         "Original anchor=(%d,%d), length=%d."),
                    cx, cy, W.X, W.Y, W.Length);
            }
        }
        else
        {
            // VerticalBlocked is [9 rows][8 columns], so valid indices are:
            //    0 ≤ cy < 9   and   0 ≤ cx < 8
            if (cy >= 0 && cy < 9 && cx >= 0 && cx < 8)
            {
                S.VerticalBlocked[cy][cx] = true;
            }
            else
            {
                UE_LOG(LogTemp, Error,
                    TEXT("ApplyWall: V‐Wall segment out of bounds at (%d,%d). "
                         "Original anchor=(%d,%d), length=%d."),
                    cx, cy, W.X, W.Y, W.Length);
            }
        }
    }

    // --- Update counts (we know lenIdx is 0,1,2) ---
    if (S.WallCounts[idx][lenIdx] > 0)
    {
        S.WallCounts[idx][lenIdx]--;
        S.WallsRemaining[idx]--;
    }
    else
    {
        UE_LOG(LogTemp, Error,
            TEXT("ApplyWall: Player %d tried to use wall length %d but has none left!"),
            PlayerNum, W.Length);
    }
}



//-----------------------------------------------------------------------------
// Max_Minimax
//-----------------------------------------------------------------------------
FMinimaxResult MinimaxEngine::Max_Minimax(const FMinimaxState& S,int32 Depth,int32 RootPlayer)
{
    const int idxAI       = RootPlayer - 1;
    const int idxOpp      = 2 - RootPlayer;
    const int OpponentNum = 3 - RootPlayer;

    // Atomic untuk menyimpan bestValue, mulai dengan nilai terendah
    TAtomic<int32> bestValue;
    bestValue = INT_MIN;

    // Akan di‐protect saat update
    FCriticalSection Mutex;
    // Simpan action yang menghasilkan bestValue
    FMinimaxAction bestAction; 
    TArray<TPair<FMinimaxAction,int32>> BestHistory;
    TArray<FMinimaxAction> Candidates;

    // 1) Cek terminal (path length atau depth)
    int32 AILenCheck = 100;
    int32 OppLenCheck = 100;
    TArray<FIntPoint> AIPath = ComputePathToGoal(S, RootPlayer,  &AILenCheck);
    TArray<FIntPoint> OppPath = ComputePathToGoal(S, OpponentNum, &OppLenCheck);
    UE_LOG(LogTemp, Warning, TEXT(
         "Initial Paths: AI=%d | Opp=%d"),
         AILenCheck, OppLenCheck);
     for (int i = 0; i < AIPath.Num(); ++i) {
         UE_LOG(LogTemp, Warning, TEXT(
             "  AI[%d] = (%d,%d)"),
             i, AIPath[i].X, AIPath[i].Y);
     }

    if (Depth == 0 || AILenCheck <= 0 || OppLenCheck <= 0)
    {
        // Kembalikan result: action kosong saja, value dari Evaluate
        UE_LOG(LogTemp, Warning, TEXT("IN Depth 0 Max_Minimax"));
        int32 eval = Evaluate(S, RootPlayer);
        UE_LOG(LogTemp, Warning, TEXT("Out Max_Minimax:, Return Evaluate = %d"), eval);
        return FMinimaxResult(FMinimaxAction(), eval);
    }

    // 2) Generate seluruh Pawn‐move untuk CurrentPlayer (RootPlayer di level pertama)
    {
        TArray<FIntPoint> PawnMoves = GetPawnMoves(S, RootPlayer);
        for (const auto& mv : PawnMoves)
        {
            UE_LOG(LogTemp, Warning, TEXT("Pawn‐move candidate: (%d, %d)"), mv.X, mv.Y);
            Candidates.Add(FMinimaxAction(mv.X, mv.Y));
        }
    }

    // // 3) Jika masih punya wall, generate wall candidates
    // if (S.WallsRemaining[idxAI] > 0)
    // {
    //     // Hanya ketika giliran RootPlayer (AI), karena RootPlayer adalah si yang maksimasi
    //     TArray<FWallData> WallMoves = GetAllUsefulWallPlacements(S, RootPlayer);
    //     UE_LOG(LogTemp, Warning, TEXT("=== Wall Candidates for Player %d ==="), RootPlayer);
    //
    //     for (const auto& w : WallMoves)
    //     {
    //         UE_LOG(
    //             LogTemp, Warning,
    //             TEXT("  Candidate Wall @ (%d, %d)  Length=%d  Horizontal=%s"),
    //             w.X, w.Y, w.Length,
    //             w.bHorizontal ? TEXT("true") : TEXT("false")
    //         );
    //         Candidates.Add(FMinimaxAction(w.X, w.Y, w.Length, w.bHorizontal));
    //     }
    // }

    //4) ParallelFor: evaluasi setiap candidate
    // ParallelFor(Candidates.Num(), [&](int32 i)
    // {
    //     const FMinimaxAction& act = Candidates[i];
    //     FMinimaxState SS = S;
    //
    //     // 4.a) Terapkan aksi
    //     if (act.bIsWall)
    //     {
    //         FWallData w{ act.SlotX, act.SlotY, act.WallLength, act.bHorizontal };
    //         FMinimaxState TempCheck = S;
    //         ApplyWall(TempCheck, RootPlayer, w);
    //
    //         // Kalau illegal (memblokir semua path), skip
    //         if (DoesWallBlockPlayer(TempCheck))
    //         {
    //             return;
    //         }
    //
    //         ApplyWall(SS, RootPlayer, w);
    //     }
    //     else
    //     {
    //         ApplyPawnMove(SS, RootPlayer, act.MoveX, act.MoveY);
    //     }
    //     
    //     // Panggil Min_ParallelMinimax (karena selanjutnya kita cari nilai minimum)
    //     FMinimaxResult subResult = Min_ParallelMinimax(SS, Depth - 1, OpponentNum);
    //     int32 v = subResult.BestValue;
    //
    //     // 4.c) Update bestValue & bestAction secara thread‐safe
    //     {
    //         FScopeLock Lock(&Mutex);
    //         if (v > bestValue)
    //         {
    //             bestValue = v;
    //             bestAction = act;
    //             BestHistory.Add(TPair<FMinimaxAction,int32>(act, v));
    //         }
    //     }
    // });
    for (int32 i = 0; i < Candidates.Num(); ++i)
    {
        UE_LOG(LogTemp, Warning, TEXT("IN Max_Minimax"));
        const FMinimaxAction& act = Candidates[i];
        FMinimaxState SS = S;
    
        if (act.bIsWall)
        {
            FWallData w{ act.SlotX, act.SlotY, act.WallLength, act.bHorizontal };
            FMinimaxState TempCheck = S;
            ApplyWall(TempCheck, RootPlayer, w);
    
            if (DoesWallBlockPlayer(TempCheck))
            {
                continue; // Ganti return; menjadi continue; pada loop biasa
            }
            ApplyWall(SS, RootPlayer, w);
        }
        else
        {
            ApplyPawnMove(SS, RootPlayer, act.MoveX, act.MoveY);
        }
    
        FMinimaxResult subResult = Min_Minimax(SS, Depth - 1, OpponentNum);
        int32 v = subResult.BestValue;
    
        // 4.c) Update bestValue (cari nilai tertinggi)
        if (v > bestValue)  
        {
            bestValue = v;
            bestAction = act;
        }
        BestHistory.Add(TPair<FMinimaxAction,int32>(act, v));
        UE_LOG(LogTemp, Warning, TEXT("out Max_Minimax"));
    }

    for (const auto& Pair : BestHistory)
    {
        const FMinimaxAction& Act       = Pair.Key;
        const int32         Value       = Pair.Value;
        FString Description;

        if (Act.bIsWall)
        {
            Description = FString::Printf(
                TEXT("Wall@(%d,%d) %s"),
                Act.SlotX, Act.SlotY,
                Act.bHorizontal ? TEXT("H") : TEXT("V")
            );
        }
        else
        {
            Description = FString::Printf(
                TEXT("Move(%d,%d)"),
                Act.MoveX, Act.MoveY
            );
        }

        UE_LOG(
            LogTemp, Warning,
            TEXT("[History] Candidate MAX: %s  → Value = %d"),
            *Description,
            Value
        );
    }   

    if (bestAction.bIsWall)
    {
        UE_LOG(LogTemp, Warning, TEXT("BestAction: Wall @(%d,%d) %s"),
            bestAction.SlotX, bestAction.SlotY,
            bestAction.bHorizontal ? TEXT("H") : TEXT("V"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("BestAction: Move to (%d,%d)"),
            bestAction.MoveX, bestAction.MoveY);
    }

    // 5) Setelah parallel selesai, kembalikan action + value
    return FMinimaxResult(bestAction, bestValue);
}

//-----------------------------------------------------------------------------
// Min_Minimax
//-----------------------------------------------------------------------------
FMinimaxResult MinimaxEngine::Min_Minimax(const FMinimaxState& S,int32 Depth,int32 RootPlayer)
{
    const int idxAI       = RootPlayer - 1;
    const int idxOpp      = 2 - RootPlayer;
    const int OpponentNum = 3 - RootPlayer;
    
    TAtomic<int32> bestValue;
    bestValue = INT_MAX;

    FCriticalSection Mutex;
    FMinimaxAction bestAction;

    TArray<FMinimaxAction> Candidates;
    TArray<TPair<FMinimaxAction,int32>> BestHistory;

    // 1) Cek terminal
    int32 AILenCheck = 100;
    int32 OppLenCheck = 100;
    ComputePathToGoal(S, RootPlayer,  &AILenCheck);
    ComputePathToGoal(S, OpponentNum, &OppLenCheck);

    if (Depth <= 0 || AILenCheck == 0 || OppLenCheck == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("IN Depth 0 Min_Minimax"));
        int32 eval = Evaluate(S, RootPlayer);
        UE_LOG(LogTemp, Warning, TEXT("Out Min_Minimax:, Return Evaluate = %d"), eval);
        return FMinimaxResult(FMinimaxAction(), eval);
        
    }

    // 2) Generate PawnMoves untuk CurrentPlayer (lawan RootPlayer di level Min)
    {
        TArray<FIntPoint> PawnMoves = GetPawnMoves(S, RootPlayer);
        for (const auto& mv : PawnMoves)
        {
            UE_LOG(LogTemp, Warning, TEXT("Pawn‐move candidate (Min): (%d, %d)"), mv.X, mv.Y);
            Candidates.Add(FMinimaxAction(mv.X, mv.Y));
        }
    }

    // 3) Generate WallMoves jika CurrentPlayer (lawan) masih punya wall
    // if (S.WallsRemaining[idxAI] > 0)
    // {
    //     TArray<FWallData> WallMoves = GetAllUsefulWallPlacements(S, RootPlayer);
    //     UE_LOG(LogTemp, Warning, TEXT("=== Wall Candidates for Player %d (Min) ==="), RootPlayer);
    //
    //     for (const auto& w : WallMoves)
    //     {
    //         UE_LOG(
    //             LogTemp, Warning,
    //             TEXT("  Candidate Wall @ (%d, %d)  Length=%d  Horizontal=%s"),
    //             w.X, w.Y, w.Length,
    //             w.bHorizontal ? TEXT("true") : TEXT("false")
    //         );
    //         Candidates.Add(FMinimaxAction(w.X, w.Y, w.Length, w.bHorizontal));
    //     }
    // }

    // 4) ParallelFor: evaluasi setiap candidate
    // ParallelFor(Candidates.Num(), [&](int32 i)
    // {
    //     UE_LOG(LogTemp, Warning, TEXT("IN Min_ParallelMinimax"));
    //     const FMinimaxAction& act = Candidates[i];
    //     FMinimaxState SS = S;
    //
    //     if (act.bIsWall)
    //     {
    //         FWallData w{ act.SlotX, act.SlotY, act.WallLength, act.bHorizontal };
    //         FMinimaxState TempCheck = S;
    //         ApplyWall(TempCheck, RootPlayer, w);
    //
    //         if (DoesWallBlockPlayer(TempCheck))
    //         {
    //             return;
    //         }
    //         ApplyWall(SS, RootPlayer, w);
    //     }
    //     else
    //     {
    //         ApplyPawnMove(SS, RootPlayer, act.MoveX, act.MoveY);
    //     }
    //     
    //
    //     FMinimaxResult subResult = Max_ParallelMinimax(SS, Depth - 1, OpponentNum);
    //     int32 v = subResult.BestValue;
    //
    //     // 4.c) Update bestValue (cari nilai terendah) secara thread‐safe
    //     {
    //         FScopeLock Lock(&Mutex);
    //         if (v < bestValue)  
    //         {
    //             bestValue = v;
    //             bestAction = act;
    //             BestHistory.Add(TPair<FMinimaxAction,int32>(act, v));
    //         }
    //         
    //     }
    // });
    for (int32 i = 0; i < Candidates.Num(); ++i)
    {
        UE_LOG(LogTemp, Warning, TEXT("IN Min_Minimax"));
        const FMinimaxAction& act = Candidates[i];
        FMinimaxState SS = S;

        if (act.bIsWall)
        {
            FWallData w{ act.SlotX, act.SlotY, act.WallLength, act.bHorizontal };
            FMinimaxState TempCheck = S;
            ApplyWall(TempCheck, RootPlayer, w);

            if (DoesWallBlockPlayer(TempCheck))
            {
                continue; // Ganti return; menjadi continue; pada loop biasa
            }
            ApplyWall(SS, RootPlayer, w);
        }
        else
        {
            ApplyPawnMove(SS, RootPlayer, act.MoveX, act.MoveY);
        }

        FMinimaxResult subResult = Max_Minimax(SS, Depth - 1, OpponentNum);
        int32 v = subResult.BestValue;

        // 4.c) Update bestValue (cari nilai terendah)
        if (v < bestValue)  
        {
            bestValue = v;
            bestAction = act;
        }
        BestHistory.Add(TPair<FMinimaxAction,int32>(act, v));
        UE_LOG(LogTemp, Warning, TEXT("out Min_Minimax"));
    }

    for (const auto& Pair : BestHistory)
    {
        const FMinimaxAction& Act       = Pair.Key;
        const int32         Value       = Pair.Value;
        FString Description;

        if (Act.bIsWall)
        {
            Description = FString::Printf(
                TEXT("Wall@(%d,%d) %s"),
                Act.SlotX, Act.SlotY,
                Act.bHorizontal ? TEXT("H") : TEXT("V")
            );
        }
        else
        {
            Description = FString::Printf(
                TEXT("Move(%d,%d)"),
                Act.MoveX, Act.MoveY
            );
        }

        UE_LOG(
            LogTemp, Warning,
            TEXT("[History] Candidate MIN: %s  → Value = %d"),
            *Description,
            Value
        );
    }

    return FMinimaxResult(bestAction, bestValue);
}

//-----------------------------------------------------------------------------
// Max_ParallelMinimax
//-----------------------------------------------------------------------------
FMinimaxResult MinimaxEngine::Max_ParallelMinimax(const FMinimaxState& S,int32 Depth,int32 RootPlayer)
{
    const int idxAI       = RootPlayer - 1;
    const int idxOpp      = 2 - RootPlayer;
    const int OpponentNum = 3 - RootPlayer;

    // Atomic untuk menyimpan bestValue, mulai dengan nilai terendah
    TAtomic<int32> bestValue;
    bestValue = INT_MIN;

    // Akan di‐protect saat update
    FCriticalSection Mutex;
    // Simpan action yang menghasilkan bestValue
    FMinimaxAction bestAction; 
    TArray<TPair<FMinimaxAction,int32>> BestHistory;
    TArray<FMinimaxAction> Candidates;

    // 1) Cek terminal (path length atau depth)
    int32 AILenCheck = 100;
    int32 OppLenCheck = 100;
    TArray<FIntPoint> AIPath = ComputePathToGoal(S, RootPlayer,  &AILenCheck);
    TArray<FIntPoint> OppPath = ComputePathToGoal(S, OpponentNum, &OppLenCheck);
    UE_LOG(LogTemp, Warning, TEXT(
         "Initial Paths: AI=%d | Opp=%d"),
         AILenCheck, OppLenCheck);
     for (int i = 0; i < AIPath.Num(); ++i) {
         UE_LOG(LogTemp, Warning, TEXT(
             "  AI[%d] = (%d,%d)"),
             i, AIPath[i].X, AIPath[i].Y);
     }

    if (Depth == 0 || AILenCheck <= 0 || OppLenCheck <= 0)
    {
        // Kembalikan result: action kosong saja, value dari Evaluate
        UE_LOG(LogTemp, Warning, TEXT("IN Depth 0 Max_ParallelMinimax"));
        int32 eval = Evaluate(S, RootPlayer);
        UE_LOG(LogTemp, Warning, TEXT("Out Max_ParallelMinimax:, Return Evaluate = %d"), eval);
        return FMinimaxResult(FMinimaxAction(), eval);
    }

    // 2) Generate seluruh Pawn‐move untuk CurrentPlayer (RootPlayer di level pertama)
    {
        TArray<FIntPoint> PawnMoves = GetPawnMoves(S, RootPlayer);
        for (const auto& mv : PawnMoves)
        {
            UE_LOG(LogTemp, Warning, TEXT("Pawn‐move candidate: (%d, %d)"), mv.X, mv.Y);
            Candidates.Add(FMinimaxAction(mv.X, mv.Y));
        }
    }

    // // 3) Jika masih punya wall, generate wall candidates
    // if (S.WallsRemaining[idxAI] > 0)
    // {
    //     // Hanya ketika giliran RootPlayer (AI), karena RootPlayer adalah si yang maksimasi
    //     TArray<FWallData> WallMoves = GetAllUsefulWallPlacements(S, RootPlayer);
    //     UE_LOG(LogTemp, Warning, TEXT("=== Wall Candidates for Player %d ==="), RootPlayer);
    //
    //     for (const auto& w : WallMoves)
    //     {
    //         UE_LOG(
    //             LogTemp, Warning,
    //             TEXT("  Candidate Wall @ (%d, %d)  Length=%d  Horizontal=%s"),
    //             w.X, w.Y, w.Length,
    //             w.bHorizontal ? TEXT("true") : TEXT("false")
    //         );
    //         Candidates.Add(FMinimaxAction(w.X, w.Y, w.Length, w.bHorizontal));
    //     }
    // }

    //4) ParallelFor: evaluasi setiap candidate
    ParallelFor(Candidates.Num(), [&](int32 i)
    {
        UE_LOG(LogTemp, Warning, TEXT("IN Max_ParallelMinimax"));
        const FMinimaxAction& act = Candidates[i];
        FMinimaxState SS = S;
    
        // 4.a) Terapkan aksi
        if (act.bIsWall)
        {
            FWallData w{ act.SlotX, act.SlotY, act.WallLength, act.bHorizontal };
            FMinimaxState TempCheck = S;
            ApplyWall(TempCheck, RootPlayer, w);
    
            // Kalau illegal (memblokir semua path), skip
            if (DoesWallBlockPlayer(TempCheck))
            {
                return;
            }
    
            ApplyWall(SS, RootPlayer, w);
        }
        else
        {
            ApplyPawnMove(SS, RootPlayer, act.MoveX, act.MoveY);
        }
        
        // Panggil Min_ParallelMinimax (karena selanjutnya kita cari nilai minimum)
        FMinimaxResult subResult = Min_ParallelMinimax(SS, Depth - 1, OpponentNum);
        int32 v = subResult.BestValue;
    
        // 4.c) Update bestValue & bestAction secara thread‐safe
        {
            FScopeLock Lock(&Mutex);
            if (v > bestValue)
            {
                bestValue = v;
                bestAction = act;
            }
            BestHistory.Add(TPair<FMinimaxAction,int32>(act, v));
            UE_LOG(LogTemp, Warning, TEXT("out Max_ParallelMinimax"));
        }
    });
    // for (int32 i = 0; i < Candidates.Num(); ++i)
    // {
    //     UE_LOG(LogTemp, Warning, TEXT("IN Max_Minimax"));
    //     const FMinimaxAction& act = Candidates[i];
    //     FMinimaxState SS = S;
    //
    //     if (act.bIsWall)
    //     {
    //         FWallData w{ act.SlotX, act.SlotY, act.WallLength, act.bHorizontal };
    //         FMinimaxState TempCheck = S;
    //         ApplyWall(TempCheck, RootPlayer, w);
    //
    //         if (DoesWallBlockPlayer(TempCheck))
    //         {
    //             continue; // Ganti return; menjadi continue; pada loop biasa
    //         }
    //         ApplyWall(SS, RootPlayer, w);
    //     }
    //     else
    //     {
    //         ApplyPawnMove(SS, RootPlayer, act.MoveX, act.MoveY);
    //     }
    //
    //     FMinimaxResult subResult = Min_Minimax(SS, Depth - 1, OpponentNum);
    //     int32 v = subResult.BestValue;
    //
    //     // 4.c) Update bestValue (cari nilai tertinggi)
    //     if (v > bestValue)  
    //     {
    //         bestValue = v;
    //         bestAction = act;
    //     }
    //     BestHistory.Add(TPair<FMinimaxAction,int32>(act, v));
    //     UE_LOG(LogTemp, Warning, TEXT("out Max_Minimax"));
    // }

    for (const auto& Pair : BestHistory)
    {
        const FMinimaxAction& Act       = Pair.Key;
        const int32         Value       = Pair.Value;
        FString Description;

        if (Act.bIsWall)
        {
            Description = FString::Printf(
                TEXT("Wall@(%d,%d) %s"),
                Act.SlotX, Act.SlotY,
                Act.bHorizontal ? TEXT("H") : TEXT("V")
            );
        }
        else
        {
            Description = FString::Printf(
                TEXT("Move(%d,%d)"),
                Act.MoveX, Act.MoveY
            );
        }

        UE_LOG(
            LogTemp, Warning,
            TEXT("[History] Candidate MAX: %s  → Value = %d"),
            *Description,
            Value
        );
    }   

    if (bestAction.bIsWall)
    {
        UE_LOG(LogTemp, Warning, TEXT("BestAction: Wall @(%d,%d) %s"),
            bestAction.SlotX, bestAction.SlotY,
            bestAction.bHorizontal ? TEXT("H") : TEXT("V"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("BestAction: Move to (%d,%d)"),
            bestAction.MoveX, bestAction.MoveY);
    }

    // 5) Setelah parallel selesai, kembalikan action + value
    return FMinimaxResult(bestAction, bestValue);
}

//-----------------------------------------------------------------------------
// Min_ParallelMinimax
//-----------------------------------------------------------------------------
FMinimaxResult MinimaxEngine::Min_ParallelMinimax(const FMinimaxState& S,int32 Depth,int32 RootPlayer)
{
    const int idxAI       = RootPlayer - 1;
    const int idxOpp      = 2 - RootPlayer;
    const int OpponentNum = 3 - RootPlayer;
    
    TAtomic<int32> bestValue;
    bestValue = INT_MAX;

    FCriticalSection Mutex;
    FMinimaxAction bestAction;

    TArray<FMinimaxAction> Candidates;
    TArray<TPair<FMinimaxAction,int32>> BestHistory;

    // 1) Cek terminal
    int32 AILenCheck = 100;
    int32 OppLenCheck = 100;
    ComputePathToGoal(S, RootPlayer,  &AILenCheck);
    ComputePathToGoal(S, OpponentNum, &OppLenCheck);

    if (Depth <= 0 || AILenCheck == 0 || OppLenCheck == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("IN Depth 0 Min_ParallelMinimax"));
        int32 eval = Evaluate(S, RootPlayer);
        UE_LOG(LogTemp, Warning, TEXT("Out Min_Minimax:, Return Evaluate = %d"), eval);
        return FMinimaxResult(FMinimaxAction(), eval);
        
    }

    // 2) Generate PawnMoves untuk CurrentPlayer (lawan RootPlayer di level Min)
    {
        TArray<FIntPoint> PawnMoves = GetPawnMoves(S, RootPlayer);
        for (const auto& mv : PawnMoves)
        {
            UE_LOG(LogTemp, Warning, TEXT("Pawn‐move candidate (Min): (%d, %d)"), mv.X, mv.Y);
            Candidates.Add(FMinimaxAction(mv.X, mv.Y));
        }
    }

    // 3) Generate WallMoves jika CurrentPlayer (lawan) masih punya wall
    // if (S.WallsRemaining[idxAI] > 0)
    // {
    //     TArray<FWallData> WallMoves = GetAllUsefulWallPlacements(S, RootPlayer);
    //     UE_LOG(LogTemp, Warning, TEXT("=== Wall Candidates for Player %d (Min) ==="), RootPlayer);
    //
    //     for (const auto& w : WallMoves)
    //     {
    //         UE_LOG(
    //             LogTemp, Warning,
    //             TEXT("  Candidate Wall @ (%d, %d)  Length=%d  Horizontal=%s"),
    //             w.X, w.Y, w.Length,
    //             w.bHorizontal ? TEXT("true") : TEXT("false")
    //         );
    //         Candidates.Add(FMinimaxAction(w.X, w.Y, w.Length, w.bHorizontal));
    //     }
    // }

    // 4) ParallelFor: evaluasi setiap candidate
    ParallelFor(Candidates.Num(), [&](int32 i)
    {
        UE_LOG(LogTemp, Warning, TEXT("IN Min_ParallelMinimax"));
        const FMinimaxAction& act = Candidates[i];
        FMinimaxState SS = S;
    
        if (act.bIsWall)
        {
            FWallData w{ act.SlotX, act.SlotY, act.WallLength, act.bHorizontal };
            FMinimaxState TempCheck = S;
            ApplyWall(TempCheck, RootPlayer, w);
    
            if (DoesWallBlockPlayer(TempCheck))
            {
                return;
            }
            ApplyWall(SS, RootPlayer, w);
        }
        else
        {
            ApplyPawnMove(SS, RootPlayer, act.MoveX, act.MoveY);
        }
        
    
        FMinimaxResult subResult = Max_ParallelMinimax(SS, Depth - 1, OpponentNum);
        int32 v = subResult.BestValue;
    
        // 4.c) Update bestValue (cari nilai terendah) secara thread‐safe
        {
            FScopeLock Lock(&Mutex);
            if (v < bestValue)  
            {
                bestValue = v;
                bestAction = act;
                
            }
            BestHistory.Add(TPair<FMinimaxAction,int32>(act, v));
            UE_LOG(LogTemp, Warning, TEXT("out Min_ParallelMinimax"));
        }
    });
    // for (int32 i = 0; i < Candidates.Num(); ++i)
    // {
    //     UE_LOG(LogTemp, Warning, TEXT("IN Min_Minimax"));
    //     const FMinimaxAction& act = Candidates[i];
    //     FMinimaxState SS = S;
    //
    //     if (act.bIsWall)
    //     {
    //         FWallData w{ act.SlotX, act.SlotY, act.WallLength, act.bHorizontal };
    //         FMinimaxState TempCheck = S;
    //         ApplyWall(TempCheck, RootPlayer, w);
    //
    //         if (DoesWallBlockPlayer(TempCheck))
    //         {
    //             continue; // Ganti return; menjadi continue; pada loop biasa
    //         }
    //         ApplyWall(SS, RootPlayer, w);
    //     }
    //     else
    //     {
    //         ApplyPawnMove(SS, RootPlayer, act.MoveX, act.MoveY);
    //     }
    //
    //     FMinimaxResult subResult = Max_Minimax(SS, Depth - 1, OpponentNum);
    //     int32 v = subResult.BestValue;
    //
    //     // 4.c) Update bestValue (cari nilai terendah)
    //     if (v < bestValue)  
    //     {
    //         bestValue = v;
    //         bestAction = act;
    //     }
    //     BestHistory.Add(TPair<FMinimaxAction,int32>(act, v));
    //     UE_LOG(LogTemp, Warning, TEXT("out Min_Minimax"));
    // }

    for (const auto& Pair : BestHistory)
    {
        const FMinimaxAction& Act       = Pair.Key;
        const int32         Value       = Pair.Value;
        FString Description;

        if (Act.bIsWall)
        {
            Description = FString::Printf(
                TEXT("Wall@(%d,%d) %s"),
                Act.SlotX, Act.SlotY,
                Act.bHorizontal ? TEXT("H") : TEXT("V")
            );
        }
        else
        {
            Description = FString::Printf(
                TEXT("Move(%d,%d)"),
                Act.MoveX, Act.MoveY
            );
        }

        UE_LOG(
            LogTemp, Warning,
            TEXT("[History] Candidate MIN: %s  → Value = %d"),
            *Description,
            Value
        );
    }

    return FMinimaxResult(bestAction, bestValue);
}

FMinimaxResult MinimaxEngine::RunSelectedAlgorithm(const FMinimaxState& Initial,int32 Depth,int32 PlayerTurn,int32 AlgorithmChoice)
{
    switch (AlgorithmChoice)
    {
    case 1:
        UE_LOG(LogTemp, Warning,
            TEXT("RunSelectedAlgorithm: Pilih Plain Parallel Minimax (Choice=1)"));
        return Max_Minimax(Initial, Depth, PlayerTurn);

    case 2:
        UE_LOG(LogTemp, Warning,
            TEXT("RunSelectedAlgorithm: Pilih Serial Minimax dengan Alpha-Beta (Choice=2)"));
        // Ganti dengan pemanggilan fungsi yang sesungguhnya, misalnya:
        // return Max_SerialAlphaBeta(Initial, Depth, PlayerTurn, CurrPlayerTurn);
        return Max_ParallelMinimax(Initial, Depth, PlayerTurn);

    case 3:
        UE_LOG(LogTemp, Warning,
            TEXT("RunSelectedAlgorithm: Pilih Parallel Minimax dengan Alpha-Beta (Choice=3)"));
        // Ganti dengan pemanggilan fungsi yang sesungguhnya, misalnya:
        // return Max_ParallelAlphaBeta(Initial, Depth, PlayerTurn, CurrPlayerTurn);
        return Max_Minimax(Initial, Depth, PlayerTurn);

    default:
        UE_LOG(LogTemp, Error,
            TEXT("RunSelectedAlgorithm: Choice tidak dikenali (%d), kembalikan evaluasi awal"), AlgorithmChoice);
        // Sebagai fallback, kembalikan action kosong + evaluasi posisi awal
        return FMinimaxResult(FMinimaxAction(), Evaluate(Initial, PlayerTurn));
    }
}













