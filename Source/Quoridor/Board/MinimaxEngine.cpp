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
// FMinimaxState::FromBoard
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
// ComputePathToGoal (A* with Jumps)
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



bool MinimaxEngine::FindPathForPawn(const FMinimaxState& S, int32 PlayerNum, TArray<FIntPoint>& OutPath)
{
    OutPath.Empty();

    // 1) Validate PlayerNum
    if (PlayerNum < 1 || PlayerNum > 2)
    {
        return false;
    }

    // 2) Read the pawn’s starting tile coordinates from the state
    int32 StartX = S.PawnX[PlayerNum - 1];
    int32 StartY = S.PawnY[PlayerNum - 1];

    // Ensure they’re in bounds [0..8]
    if (StartX < 0 || StartX > 8 || StartY < 0 || StartY > 8)
    {
        return false; 
    }

    // 3) Determine the target row (Player 1 needs to reach Y=8, Player 2 needs to reach Y=0)
    const int32 TargetRow = (PlayerNum == 1) ? 8 : 0;

    // 4) A* node struct
    struct FNode
    {
        int32 X;
        int32 Y;
        int32 GCost;
        int32 HCost;
        FNode* Parent;

        FNode(int32 InX, int32 InY, int32 InG, int32 InH, FNode* InParent = nullptr)
            : X(InX), Y(InY), GCost(InG), HCost(InH), Parent(InParent)
        {}

        FORCEINLINE int32 FCost() const
        {
            return GCost + HCost;
        }
    };

    // 5) Containers for A*
    TArray<FNode*>    OpenSet;
    TSet<FIntPoint>   ClosedSet;
    TArray<FNode*>    AllNodes; // for cleanup

    // 6) Heuristic = vertical distance to target row
    auto Heuristic = [TargetRow](int32 X, int32 Y) -> int32
    {
        return FMath::Abs(TargetRow - Y);
    };

    // 7) Create and push the start node
    {
        int32 H0 = Heuristic(StartX, StartY);
        FNode* StartNode = new FNode(StartX, StartY, /*G=*/0, /*H=*/H0, /*Parent=*/nullptr);
        OpenSet.Add(StartNode);
        AllNodes.Add(StartNode);
    }

    // 8) Read opponent pawn position (treated as blocked)
    const int32 OpponentNum = 3 - PlayerNum;
    const int32 OppX = S.PawnX[OpponentNum - 1];
    const int32 OppY = S.PawnY[OpponentNum - 1];

    // 9) A* main loop
    while (OpenSet.Num() > 0)
    {
        // 9a) Pick node in OpenSet with lowest FCost (tie‐breaker: smaller HCost)
        FNode* Current = OpenSet[0];
        for (FNode* NodePtr : OpenSet)
        {
            if (NodePtr->FCost() < Current->FCost() ||
                (NodePtr->FCost() == Current->FCost() && NodePtr->HCost < Current->HCost))
            {
                Current = NodePtr;
            }
        }

        // 9b) Remove Current from OpenSet, mark visited
        OpenSet.Remove(Current);
        ClosedSet.Add(FIntPoint(Current->X, Current->Y));

        // 9c) If we've reached the target row, reconstruct path and return true
        if (Current->Y == TargetRow)
        {
            // Walk back up via Parent pointers, collecting X,Y in reverse
            TArray<FIntPoint> Reversed;
            FNode* Walk = Current;

            while (Walk != nullptr)
            {
                Reversed.Add(FIntPoint(Walk->X, Walk->Y));
                Walk = Walk->Parent;
            }

            // Reverse so that OutPath goes from start → goal
            for (int32 i = Reversed.Num() - 1; i >= 0; --i)
            {
                OutPath.Add(Reversed[i]);
            }

            // Cleanup all nodes before returning
            for (FNode* N : AllNodes)
            {
                delete N;
            }
            AllNodes.Empty();
            OpenSet.Empty();
            return true;
        }

        // 9d) Otherwise, expand up to 4 neighbors (N, S, E, W)
        const int32 CX = Current->X;
        const int32 CY = Current->Y;
        const int32 NextG = Current->GCost + 1;

        // Local helper to add a neighbor if valid
        auto TryAddNeighbor = [&](int32 NX, int32 NY)
        {
            // 1) Bounds check
            if (NX < 0 || NX > 8 || NY < 0 || NY > 8)
            {
                return;
            }

            // 2) Already visited?
            if (ClosedSet.Contains(FIntPoint(NX, NY)))
            {
                return;
            }

            // 3) Occupied by opponent pawn?
            if (NX == OppX && NY == OppY)
            {
                return;
            }

            // 4) If already in OpenSet, skip
            for (FNode* NodePtr : OpenSet)
            {
                if (NodePtr->X == NX && NodePtr->Y == NY)
                {
                    return;
                }
            }

            // 5) Everything good → create new node and push
            int32 H = Heuristic(NX, NY);
            FNode* NewNode = new FNode(NX, NY, NextG, H, Current);
            OpenSet.Add(NewNode);
            AllNodes.Add(NewNode);
        };

        // ─── North (Y+1) ───
        if (CY < 8)
        {
            // A horizontal wall at row CY, column CX would block northward movement
            if (!S.HorizontalBlocked[CY][CX])
            {
                TryAddNeighbor(CX, CY + 1);
            }
        }

        // ─── South (Y-1) ───
        if (CY > 0)
        {
            // A horizontal wall at row CY-1, column CX blocks southward.
            if (!S.HorizontalBlocked[CY - 1][CX])
            {
                TryAddNeighbor(CX, CY - 1);
            }
        }

        // ─── East (X+1) ───
        if (CX < 8)
        {
            // A vertical wall at row CY, column CX blocks eastward
            if (!S.VerticalBlocked[CY][CX])
            {
                TryAddNeighbor(CX + 1, CY);
            }
        }

        // ─── West (X-1) ───
        if (CX > 0)
        {
            // A vertical wall at row CY, column CX-1 blocks westward
            if (!S.VerticalBlocked[CY][CX - 1])
            {
                TryAddNeighbor(CX - 1, CY);
            }
        }

        // (Don’t delete Current here; we still need it if its children lead to the goal later.)
    }

    // 10) If we exit the loop, no path exists: clean up and return false
    for (FNode* N : AllNodes)
    {
        delete N;
    }
    AllNodes.Empty();
    return false;
}

//-----------------------------------------------------------------------------
// Wall Legality Check
//-----------------------------------------------------------------------------
bool MinimaxEngine::IsWallPlacementStrictlyLegal(const FMinimaxState& S,const FWallData& W)
{
    // ------------------------------------------------------------
    // 1) Bounds check for the entire wall
    // ------------------------------------------------------------
    if (W.bHorizontal)
    {
        // Horizontal wall of length L sits at Y = W.Y, X ∈ [W.X .. W.X+L-1].
        // Valid X range for a horizontal segment is [0..8], valid Y is [0..7].
        // So we require:
        //    0 ≤ W.Y < 8
        //    0 ≤ W.X ≤  8   AND   (W.X + W.Length - 1) ≤ 8
        if (W.Y < 0 || W.Y >= 8 ||
            W.X < 0 || W.X + W.Length - 1 > 8)
        {
            return false;
        }
    }
    else
    {
        // Vertical wall of length L sits at X = W.X, Y ∈ [W.Y .. W.Y+L-1].
        // Valid Y range for a vertical segment is [0..8], valid X is [0..7].
        // So we require:
        //    0 ≤ W.X < 8
        //    0 ≤ W.Y ≤  8   AND   (W.Y + W.Length - 1) ≤ 8
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
            // 2a) Overlap: check if this horizontal segment is already blocked
            //    HorizontalBlocked has 8 rows × 9 columns, so valid indices are:
            //      0 ≤ cy < 8  and  0 ≤ cx < 9
            if (S.HorizontalBlocked[cy][cx])
            {
                // This exact segment is already occupied by another horizontal wall.
                return false;
            }

            // 2b) Intersection (cross): does a vertical wall “cross” here?
            //    A length‐2 vertical slot at X=cx, Y=cy would mark:
            //      VerticalBlocked[cy][cx]   and  
            //      VerticalBlocked[cy+1][cx]
            //    So we must check if both those are already true. But only if (cy+1) ≤ 8.
            if (cy + 1 <= 8)
            {
                if (S.VerticalBlocked[cy][cx] &&
                    S.VerticalBlocked[cy + 1][cx])
                {
                    // A vertical slot occupies exactly those two segments,
                    // so a new horizontal segment here would cross it at the T‐junction.
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
// Check if wall blocks a player
//-----------------------------------------------------------------------------
bool MinimaxEngine::DoesWallBlockPlayer(FMinimaxState& TempState)
{
    int32 TestLen1 = 100, TestLen2 = 100;
    ComputePathToGoal(TempState, 1, &TestLen1);
    ComputePathToGoal(TempState, 2, &TestLen2);
    return (TestLen1 >= 100 || TestLen2 >= 100);
}

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
// Get All Useful Wall Placements (New)
//-----------------------------------------------------------------------------
TArray<FWallData> MinimaxEngine::GetAllUsefulWallPlacements(const FMinimaxState& S, int32 PlayerNum)
{
    TArray<FWallData> FinalCandidates;
    int idx = PlayerNum - 1;

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

    AllLegalWalls.Sort([](const ScoredWall& A, const ScoredWall& B) {
        return A.Score > B.Score;
    });

    const int MaxWallCandidates = 25;
    for (int i = 0; i < AllLegalWalls.Num() && i < MaxWallCandidates; ++i) {
        FinalCandidates.Add(AllLegalWalls[i].Wall);
    }
    
    return FinalCandidates;
}




//-----------------------------------------------------------------------------
// Get Pawn Moves (Unchanged, but ensure it's robust)
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
// Board Control Heuristic
//-----------------------------------------------------------------------------
void MinimaxEngine::ComputeBoardControl(const FMinimaxState& S, int32& MyControl, int32& OppControl, int32 RootPlayer)
{
    MyControl = 0; OppControl = 0;
    int visited[9][9] = {};
    std::queue<FIntPoint> q1, q2;

    int idxAI = RootPlayer - 1;
    int idxOpponent = 1 - idxAI;

    int ax = S.PawnX[idxAI], ay = S.PawnY[idxAI];
    int bx = S.PawnX[idxOpponent], by = S.PawnY[idxOpponent];

    if (ay >= 0 && ax >= 0) { visited[ay][ax] = RootPlayer; q1.push({ax, ay}); MyControl++; }
    if (by >= 0 && bx >= 0) { visited[by][bx] = 3 - RootPlayer; q2.push({bx, by}); OppControl++; }

    auto CanMove = [&](int ax, int ay, int bx, int by) -> bool {
        if (bx < 0 || bx > 8 || by < 0 || by > 8) return false;
        if (bx == ax + 1 && ax >= 0 && ax < 8 && S.VerticalBlocked[ay][ax]) return false;
        if (bx == ax - 1 && bx >= 0 && bx < 8 && S.VerticalBlocked[ay][bx]) return false;
        if (by == ay + 1 && ay >= 0 && ay < 8 && S.HorizontalBlocked[ay][ax]) return false;
        if (by == ay - 1 && by >= 0 && by < 8 && S.HorizontalBlocked[by][ax]) return false;
        return true;
    };

    const FIntPoint dirs[4] = {{1,0}, {-1,0}, {0,1}, {0,-1}};

    while (!q1.empty() || !q2.empty()) {
        int q1_size = q1.size();
        for (int i = 0; i < q1_size; ++i) {
            FIntPoint curr = q1.front(); q1.pop();
            for (const auto& d : dirs) {
                int nx = curr.X + d.X;
                int ny = curr.Y + d.Y;
                if (nx >= 0 && nx < 9 && ny >= 0 && ny < 9 &&
                    CanMove(curr.X, curr.Y, nx, ny) && visited[ny][nx] == 0) {
                    visited[ny][nx] = RootPlayer; MyControl++;
                    q1.push({nx, ny});
                }
            }
        }

        int q2_size = q2.size();
        for (int i = 0; i < q2_size; ++i) {
            FIntPoint curr = q2.front(); q2.pop();
            for (const auto& d : dirs) {
                int nx = curr.X + d.X;
                int ny = curr.Y + d.Y;
                if (nx >= 0 && nx < 9 && ny >= 0 && ny < 9 &&
                    CanMove(curr.X, curr.Y, nx, ny) && visited[ny][nx] == 0) {
                    visited[ny][nx] = 3 - RootPlayer; OppControl++;
                    q2.push({nx, ny});
                }
            }
        }
    }
}

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

//-----------------------------------------------------------------------------
// Evaluate (Pro-Inspired)
//-----------------------------------------------------------------------------
int32 MinimaxEngine::Evaluate(const FMinimaxState& S, int32 RootPlayer)
{
    int32 idxAI       = RootPlayer - 1;
    int32 idxOpp      = 1 - idxAI;
    int32 OpponentNum = 3 - RootPlayer;

    // 1) Recompute A* path lengths for both players
    int32 AILen = 100, OppLen = 100;
    TArray<FIntPoint> AIPath  = ComputePathToGoal(S, RootPlayer,   &AILen);
    TArray<FIntPoint> OppPath = ComputePathToGoal(S, OpponentNum, &OppLen);

    // 2) Terminal conditions (no‐path for AI only)
    //    If AI cannot reach its goal, that’s catastrophic; give a large negative.
    if (AILen >= 100)   
        return -49000;

    //    We do NOT immediately return +49000 for OppLen >= 100 here.
    //    (Blocking the opponent via pawn alone should not reward +49000.)

    // 3) Gather positional info
    FIntPoint CurrAI(S.PawnX[idxAI], S.PawnY[idxAI]);
    const FIntPoint& PrevAI  = S.LastPawnPos[idxAI];
    const FIntPoint& Prev2AI = S.SecondLastPawnPos[idxAI];

    // 4) Begin accumulating Score
    int32 Score = 0;

    // 4a) Path‐difference term: reward if AI’s path is shorter than opponent’s
    //     Weight: 150 per step of difference
    const int32 W_PathDiff = 150;
    Score += (OppLen - AILen) * W_PathDiff;

    // 4b) Wall‐inventory term: prefer having more walls in hand than opponent
    //     Weight: 8 per extra wall
    const int32 W_WallCount = 8;
    Score += (S.WallsRemaining[idxAI] - S.WallsRemaining[idxOpp]) * W_WallCount;

    // 4c) Board‐control term: prefer controlling more board squares
    //     Weight: 3 per extra controlled square
    const int32 W_BoardControl = 3;
    int32 MyControl = 0, OppControl = 0;
    ComputeBoardControl(S, MyControl, OppControl, RootPlayer);
    Score += (MyControl - OppControl) * W_BoardControl;

    // 4d) “Strategic wall” bonus if opponent is within 3 moves of winning
    //     Each horizontal wall on their finish row yields +15
    const int32 W_StrategicWall = 15;
    if (OppLen <= 3)
    {
        int32 oppGoalRow = (RootPlayer == 1 ? 0 : 8);
        for (int x = 0; x < 8; ++x)
        {
            if (S.HorizontalBlocked[oppGoalRow][x])
            {
                Score += W_StrategicWall;
            }
        }
    }

    // 4e) “Opponent near‐path” bonus up to +21
    //     If the opponent’s pawn is within 2 Manhattan steps of any tile on AIPath,
    //     we add (OppDistToPath – 3) * (–7), which for distances 0,1,2 gives +21, +14, +7.
    const int32 W_PathDefense = -7;
    int32 OppDistToPath = INT_MAX;
    int32 OppX = S.PawnX[idxOpp], OppY = S.PawnY[idxOpp];
    for (const FIntPoint& p : AIPath)
    {
        int32 d = FMath::Abs(p.X - OppX) + FMath::Abs(p.Y - OppY);
        OppDistToPath = FMath::Min(OppDistToPath, d);
    }
    if (OppDistToPath <= 2)
    {
        Score += (OppDistToPath - 3) * W_PathDefense;  // 0→+21, 1→+14, 2→+7
    }

    // 4f) Penalize backtracking: if AI’s current position equals its last two positions
    if (CurrAI == PrevAI)   Score -= 2000;
    if (CurrAI == Prev2AI)  Score -= 1000;

    // 4g) Tiny forward‐Y bonus (tiebreaker): encourage moving toward the finish row
    {
        int32 goalY = (RootPlayer == 1 ? 8 : 0);
        int32 dy = FMath::Abs(CurrAI.Y - goalY);
        Score += (8 - dy) * 2;
    }

    if (AIPath.Num() > 1 && CurrAI == AIPath[1])
    {
        Score += 50;  // Strong reward for staying on the very next step
    }

    return Score;
}




//-----------------------------------------------------------------------------
// Apply Pawn Move
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
// Apply Wall (Updated for WallCounts & Length)
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
// Minimax (Plain - No Pruning)
//-----------------------------------------------------------------------------
int32 MinimaxEngine::Minimax(FMinimaxState S, int32 Depth, int32 RootPlayer, int32 CurrentPlayer)
{
    int32 AILenCheck = 100, OppLenCheck = 100;
    ComputePathToGoal(S, RootPlayer, &AILenCheck);
    ComputePathToGoal(S, 3 - RootPlayer, &OppLenCheck);

    // Terminal/Quiescence Check
    if (Depth <= 0 || AILenCheck == 0 || OppLenCheck == 0)
        return Evaluate(S, RootPlayer);

    const bool maximize = (CurrentPlayer == RootPlayer);
    int opponent = 3 - CurrentPlayer;
    int idx = CurrentPlayer - 1;

    int32 bestValue = maximize ? INT_MIN : INT_MAX;

    // === Pawn Moves ===
    TArray<FIntPoint> PawnMoves = GetPawnMoves(S, CurrentPlayer);
    for (const auto& mv : PawnMoves) {
        FMinimaxState SS = S;
        ApplyPawnMove(SS, CurrentPlayer, mv.X, mv.Y);
        int32 v = Minimax(SS, Depth - 1, RootPlayer, opponent); // Recursive call

        if (maximize) {
            bestValue = FMath::Max(bestValue, v);
        } else {
            bestValue = FMath::Min(bestValue, v);
        }
    }

    // === Wall Moves ===
    if (S.WallsRemaining[idx] > 0) {
        TArray<FWallData> WallMoves = GetAllUsefulWallPlacements(S, CurrentPlayer);
        for (const auto& w : WallMoves) {
            FMinimaxState SS = S;
            ApplyWall(SS, CurrentPlayer, w);
            int32 v = Minimax(SS, Depth - 1, RootPlayer, opponent); // Recursive call

            if (maximize) {
                bestValue = FMath::Max(bestValue, v);
            } else {
                bestValue = FMath::Min(bestValue, v);
            }
        }
    }

    // Handle case where no moves are possible (shouldn't happen in Quoridor usually)
    if (bestValue == INT_MIN || bestValue == INT_MAX) {
        return Evaluate(S, RootPlayer);
    }

    return bestValue;
}

//-----------------------------------------------------------------------------
// Minimax Alpha-Beta
//-----------------------------------------------------------------------------
int32 MinimaxEngine::MinimaxAlphaBeta(FMinimaxState S, int32 Depth, int32 RootPlayer, int32 CurrentPlayer, int32 Alpha, int32 Beta)
{
    int32 AILenCheck = 100, OppLenCheck = 100;
    ComputePathToGoal(S, RootPlayer, &AILenCheck);
    ComputePathToGoal(S, 3 - RootPlayer, &OppLenCheck);

    // Terminal/Quiescence Check
    if (Depth <= 0 || AILenCheck == 0 || OppLenCheck == 0)
        return Evaluate(S, RootPlayer);

    const bool maximize = (CurrentPlayer == RootPlayer);
    int opponent = 3 - CurrentPlayer;
    int idx = CurrentPlayer - 1;

    // === Pawn Moves ===
    TArray<FIntPoint> PawnMoves = GetPawnMoves(S, CurrentPlayer);
    
    for (const auto& mv : PawnMoves) {
        FMinimaxState SS = S;
        ApplyPawnMove(SS, CurrentPlayer, mv.X, mv.Y);
        int32 v = MinimaxAlphaBeta(SS, Depth - 1, RootPlayer, opponent, Alpha, Beta);

        if (maximize) {
            Alpha = FMath::Max(Alpha, v);
            if (Beta <= Alpha) return Beta; // Prune
        } else {
            Beta = FMath::Min(Beta, v);
            if (Beta <= Alpha) return Alpha; // Prune
        }
    }

    // === Wall Moves ===
    if (S.WallsRemaining[idx] > 0) {
        TArray<FWallData> WallMoves = GetAllUsefulWallPlacements(S, CurrentPlayer);
        for (const auto& w : WallMoves) {
            FMinimaxState SS = S;
            ApplyWall(SS, CurrentPlayer, w);
            int32 v = MinimaxAlphaBeta(SS, Depth - 1, RootPlayer, opponent, Alpha, Beta);

            if (maximize) {
                Alpha = FMath::Max(Alpha, v);
                if (Beta <= Alpha) return Beta; // Prune
            } else {
                Beta = FMath::Min(Beta, v);
                if (Beta <= Alpha) return Alpha; // Prune
            }
        }
    }

    return maximize ? Alpha : Beta;
}



// (1) Definition of ComputePathNetGain as a private static method:
int32 MinimaxEngine::ComputePathNetGain(const FMinimaxState& BeforeState,const FMinimaxState& AfterState,int32 RootPlayer,int32 Opponent,int32 BeforeAILen,int32 BeforeOppLen,int32 Multiplier)
{
    // If either “before” length was “infinite,” skip.
    if (BeforeAILen >= 100 || BeforeOppLen >= 100)
        return 0;

    int32 AfterAILen = 0, AfterOppLen = 0;

    // NOTE: We qualify ComputePathToGoal with MinimaxEngine:: if it is a static member.
    ComputePathToGoal(AfterState, RootPlayer, &AfterAILen);
    ComputePathToGoal(AfterState, Opponent, &AfterOppLen);

    if (AfterAILen >= 100 || AfterOppLen >= 100)
        return 0;

    int32 NetGain = (AfterOppLen - AfterAILen) - (BeforeOppLen - BeforeAILen);
    return NetGain * Multiplier;
}

// (2) Definition of CalculateWallScore as a private static method:
int32 MinimaxEngine::CalculateWallScore(const FMinimaxState& Initial,const FMinimaxState& AfterState,int32 RootPlayer,int32 Opponent,int32 InitialAILen,int32 InitialOppLen)
{
    constexpr int32 WallNetGainMultiplier = 20;
    return ComputePathNetGain(
        Initial, AfterState,
        RootPlayer, Opponent,
        InitialAILen, InitialOppLen,
        WallNetGainMultiplier
    );
}

// (3) Definition of CalculatePawnScore as a private static method,
//     using the user’s custom “forward/path/shorter‐path/anti‐oscillation” logic:
int32 MinimaxEngine::CalculatePawnScore(const FMinimaxState& Initial,const FMinimaxState& AfterState,int32 RootPlayer,int32 Opponent,int32 InitialAILength,const TArray<FIntPoint>& /*AIPath*/,const FMinimaxAction& Act)
{
    int32 score = 0;
    const int32 idx = RootPlayer - 1;

    // 1) Compute the new A* path for AI after applying this pawn move:
    int32 AfterAILen = 100;
    TArray<FIntPoint> NewPath =ComputePathToGoal(AfterState, RootPlayer, &AfterAILen);

    // If there's no path to the goal, heavy penalty and bail out:
    if (AfterAILen >= 100) {
        return -2000;
    }

    // 2) If pawn is only one move away from finishing (AfterAILen == 1),
    //    give a very large bonus so that moving wins over any wall placement.
    if (AfterAILen == 0) {
        return 100000; 
    }

    // 3) If this move follows the exact next step on the new shortest path (NewPath[1]), big bonus:
    if (NewPath.Num() > 1 &&
        Act.MoveX == NewPath[1].X &&
        Act.MoveY == NewPath[1].Y)
    {
        score += 1000;
    }
    else {
        // Not on the exact next step → small penalty
        score -= 1000;
    }

    // 4) Reward any reduction in A* path length:
    int32 deltaLen = InitialAILength - AfterAILen;
    if (deltaLen > 0) {
        // Each step shortened gives +20 points
        score += deltaLen * 20;
    }
    else if (deltaLen < 0) {
        // If the path got longer, penalize by −50 per extra step
        score += deltaLen * 50;  // deltaLen is negative here
    }

    // 5) Anti‐oscillation: discourage reversing recent moves
    if (RecentMoves.Num() >= 1) {
        const FIntPoint& Last = RecentMoves.Last();
        if (Act.MoveX == Last.X && Act.MoveY == Last.Y) {
            score -= 1000;
        }
    }
    if (RecentMoves.Num() >= 2) {
        const FIntPoint& TwoAgo =
            RecentMoves[RecentMoves.Num() - 2];
        if (Act.MoveX == TwoAgo.X && Act.MoveY == TwoAgo.Y) {
            score -= 500;
        }
    }

    return score;
}



//-----------------------------------------------------------------------------
// Solve Parallel (Using Plain Minimax)
//-----------------------------------------------------------------------------
FMinimaxAction MinimaxEngine::SolveParallel(const FMinimaxState& Initial,int32 Depth,int32 RootPlayer)
{
    UE_LOG(LogTemp, Warning, TEXT(
        "=== SolveParallel (Plain Minimax) | Root=%d | Depth=%d ==="),
        RootPlayer, Depth);

    PrintInventory(Initial, TEXT("Start SolveParallel"));
    PrintBlockedWalls(Initial, TEXT("Start SolveParallel"));

    const int idx = RootPlayer - 1;
    const int Opponent = 3 - RootPlayer;

    // 1) Compute “before” shortest-path lengths and store AIPath
    int32 InitialAILength = 0, InitialOppLength = 0;
    TArray<FIntPoint> AIPath =
        ComputePathToGoal(Initial, RootPlayer, &InitialAILength);
    TArray<FIntPoint> OppPath =
        ComputePathToGoal(Initial, Opponent, &InitialOppLength);

    UE_LOG(LogTemp, Warning, TEXT(
        "Initial Paths: AI=%d | Opp=%d"),
        InitialAILength, InitialOppLength);
    for (int i = 0; i < AIPath.Num(); ++i) {
        UE_LOG(LogTemp, Warning, TEXT(
            "  AI[%d] = (%d,%d)"),
            i, AIPath[i].X, AIPath[i].Y);
    }

    // 2) Generate candidate pawn-moves and wall-placements
    TArray<FMinimaxAction> Candidates;
    TArray<FIntPoint> PawnMoves = GetPawnMoves(Initial, RootPlayer);
    for (const auto& mv : PawnMoves) {
        Candidates.Add(FMinimaxAction(mv.X, mv.Y));
    }
    if (Initial.WallsRemaining[idx] > 0) {
        TArray<FWallData> WallMoves =
            GetAllUsefulWallPlacements(Initial, RootPlayer);
        for (const auto& w : WallMoves) {
            Candidates.Add(
                FMinimaxAction(w.X, w.Y, w.Length, w.bHorizontal));
        }
    }

    if (Candidates.Num() == 0) {
        UE_LOG(LogTemp, Error, TEXT(
            "SolveParallel: No candidates found!"));
        return FMinimaxAction();
    }

    // 3) Prepare a score array
    TArray<int32> Scores;
    Scores.SetNum(Candidates.Num());

    // 4) Parallel evaluation
    ParallelFor(Candidates.Num(), [&](int32 i)
    {
        const FMinimaxAction& act = Candidates[i];
        FMinimaxState SS = Initial;
        int32 baseScore = INT_MIN;

        // 4a) Apply the action (wall or pawn move)
        if (act.bIsWall) {
            FWallData w{ act.SlotX, act.SlotY, act.WallLength, act.bHorizontal };
            FMinimaxState TempCheck = Initial;
            ApplyWall(TempCheck, RootPlayer, w);

            if (DoesWallBlockPlayer(TempCheck)) {
                Scores[i] = INT_MIN;
                return;
            }
            ApplyWall(SS, RootPlayer, w);
        }
        else {
            ApplyPawnMove(SS, RootPlayer, act.MoveX, act.MoveY);
        }

        // 4b) Compute the plain Minimax value on SS:
        //      (next player’s turn is 'Opponent')
        baseScore = Minimax(
            SS,
            Depth - 1,
            RootPlayer,
            Opponent
        );

        // 4c) Add either the wall‐score or pawn‐score helper
        int32 bonus = 0;
        if (act.bIsWall) {
            bonus = CalculateWallScore(
                Initial,
                SS,
                RootPlayer,
                Opponent,
                InitialAILength,
                InitialOppLength
            );
        }
        else {
            bonus = CalculatePawnScore(
                Initial,
                SS,
                RootPlayer,
                Opponent,
                InitialAILength,
                AIPath,
                act
            );
        }

        Scores[i] = baseScore + bonus;
    });

    // 5) Choose the best action
    FMinimaxAction BestAct;
    BestAct.Score = INT_MIN;
    for (int32 i = 0; i < Candidates.Num(); ++i) {
        if (Scores[i] > BestAct.Score) {
            BestAct = Candidates[i];
            BestAct.Score = Scores[i];
        }
    }

    // 6) Update RecentMoves if it's a pawn move
    if (!BestAct.bIsWall && BestAct.Score > INT_MIN) {
        RecentMoves.Add(
            FIntPoint(BestAct.MoveX, BestAct.MoveY));
        if (RecentMoves.Num() > 4) {
            RecentMoves.RemoveAt(0);
        }
        UE_LOG(LogTemp, Log, TEXT(
            "SolveParallel: P%d RecentMoves updated. Newest: (%d,%d). Count: %d"),
            RootPlayer,
            BestAct.MoveX, BestAct.MoveY,
            MinimaxEngine::RecentMoves.Num());
    }

    // 7) Logging and fallback
    UE_LOG(LogTemp, Warning, TEXT(
        "=> SolveParallel selected: %s (Score=%d)"),
        BestAct.bIsWall
            ? *FString::Printf(TEXT("Wall L%d @(%d,%d) %s"),
                               BestAct.WallLength,
                               BestAct.SlotX,
                               BestAct.SlotY,
                               BestAct.bHorizontal ? TEXT("H") : TEXT("V"))
            : *FString::Printf(TEXT("Move to (%d,%d)"),
                               BestAct.MoveX,
                               BestAct.MoveY),
        BestAct.Score);

    if (BestAct.Score == INT_MIN && Candidates.Num() > 0) {
        UE_LOG(LogTemp, Error, TEXT(
            "SolveParallel: No best move found > INT_MIN, picking first candidate!"));
        return Candidates[0];
    }

    return BestAct;
}

//-----------------------------------------------------------------------------
// Solve Alpha-Beta (Main Entry Point)
//-----------------------------------------------------------------------------
FMinimaxAction MinimaxEngine::SolveAlphaBeta(const FMinimaxState& Initial,int32 Depth,int32 RootPlayer)
{
    UE_LOG(LogTemp, Warning, TEXT(
        "=== SolveAlphaBeta | Root=%d | Depth=%d ==="),
        RootPlayer, Depth);
    PrintInventory(Initial, TEXT("Start SolveAlphaBeta"));
    PrintBlockedWalls(Initial, TEXT("Start SolveAlphaBeta"));

    const int idx = RootPlayer - 1;
    const int Opponent = 3 - RootPlayer;

    // 1) Compute “before” path lengths & store AIPath
    int32 InitialAILength = 0, InitialOppLength = 0;
    TArray<FIntPoint> AIPath =
        ComputePathToGoal(Initial, RootPlayer, &InitialAILength);
    TArray<FIntPoint> OppPath =
        ComputePathToGoal(Initial, Opponent, &InitialOppLength);

    UE_LOG(LogTemp, Warning, TEXT(
        "Initial Paths: AI=%d | Opp=%d"),
        InitialAILength, InitialOppLength);
    for (int i = 0; i < AIPath.Num(); ++i) {
        UE_LOG(LogTemp, Warning, TEXT(
            "  AI[%d] = (%d,%d)"),
            i, AIPath[i].X, AIPath[i].Y);
    }

    // 2) Generate Candidate Moves
    TArray<FMinimaxAction> Candidates;

    // 2a) Pawn moves
    TArray<FIntPoint> PawnMoves = GetPawnMoves(Initial, RootPlayer);
    for (const auto& mv : PawnMoves) {
        Candidates.Add(FMinimaxAction(mv.X, mv.Y));
    }

    // 2b) Wall placements (if walls remain)
    if (Initial.WallsRemaining[idx] > 0) {
        TArray<FWallData> WallMoves =
            GetAllUsefulWallPlacements(Initial, RootPlayer);
        for (const auto& w : WallMoves) {
            Candidates.Add(
                FMinimaxAction(w.X, w.Y, w.Length, w.bHorizontal));
        }
    }

    if (Candidates.Num() == 0) {
        UE_LOG(LogTemp, Error, TEXT(
            "SolveAlphaBeta: No candidates found!"));
        return FMinimaxAction();
    }
    

    // 3) Heuristic‐score & sort for move ordering
    //    We’ll temporarily store each candidate’s “ordering score” in act.Score.
    for (FMinimaxAction& act : Candidates) {
        // Rebuild a temporary state SS
        FMinimaxState SS = Initial;
        if (act.bIsWall) {
            // 3a) Wall: apply and compute purely heuristic score
            FWallData w{ act.SlotX, act.SlotY, act.WallLength, act.bHorizontal };
            FMinimaxState TempCheck = Initial;
            ApplyWall(TempCheck, RootPlayer, w);
            if (DoesWallBlockPlayer(TempCheck)) {
                // Illegal wall: give it a very low ordering score
                act.Score = INT_MIN;
                continue;
            }
            ApplyWall(SS, RootPlayer, w);

            // Heuristic = CalculateWallScore(...)
            act.Score = CalculateWallScore(
                Initial,       // before state
                SS,            // after state
                RootPlayer,
                Opponent,
                InitialAILength,
                InitialOppLength
            );
        }
        else {
            // 3b) Pawn move: apply and compute heuristic via CalculatePawnScore
            ApplyPawnMove(SS, RootPlayer, act.MoveX, act.MoveY);

            // We do NOT re‐compute AILength/OppLength here; CalculatePawnScore does it internally
            act.Score = CalculatePawnScore(
                Initial,       // before state
                SS,            // after state
                RootPlayer,
                Opponent,
                InitialAILength,
                AIPath,        // “before” path for path‐following bonus
                act
            );
        }
    }

    // Sort descending by act.Score so the highest heuristics are first
    Candidates.Sort([](const FMinimaxAction& A, const FMinimaxAction& B) {
        return A.Score > B.Score;
    });

    // 4) Run full AlphaBeta on ordered candidates
    FMinimaxAction BestAct;
    BestAct.Score = INT_MIN;
    int32 Alpha = INT_MIN;
    int32 Beta = INT_MAX;

    for (const auto& act : Candidates) {
        // If heuristic was INT_MIN (illegal), skip
        if (act.Score == INT_MIN) {
            continue;
        }

        FMinimaxState SS = Initial;
        int32 CurrentScore = INT_MIN;

        if (act.bIsWall) {
            FWallData w{ act.SlotX, act.SlotY, act.WallLength, act.bHorizontal };
            FMinimaxState TempCheck = Initial;
            ApplyWall(TempCheck, RootPlayer, w);
            if (DoesWallBlockPlayer(TempCheck)) {
                continue;
            }
            ApplyWall(SS, RootPlayer, w);
        }
        else {
            ApplyPawnMove(SS, RootPlayer, act.MoveX, act.MoveY);
        }

        // Evaluate full α/β from the child state
        CurrentScore = MinimaxAlphaBeta(
            SS,
            Depth - 1,
            RootPlayer,
            Opponent,
            Alpha,
            Beta
        );

        if (CurrentScore > BestAct.Score) {
            BestAct = act;
            BestAct.Score = CurrentScore;
            Alpha = FMath::Max(Alpha, CurrentScore);
            if (Alpha >= Beta) {
                // Beta cutoff
                break;
            }
        }
    }

    // 5) Update move history (to prevent oscillation) if best is a pawn move
    if (!BestAct.bIsWall) {
        RecentMoves.Add(FIntPoint(BestAct.MoveX, BestAct.MoveY));
        if (RecentMoves.Num() > 4) {
            RecentMoves.RemoveAt(0);
        }
    }

    UE_LOG(LogTemp, Warning, TEXT(
        "=> SolveAlphaBeta selected: %s (Score=%d)"),
        BestAct.bIsWall
            ? *FString::Printf(TEXT("Wall L%d @(%d,%d) %s"),
                               BestAct.WallLength,
                               BestAct.SlotX,
                               BestAct.SlotY,
                               BestAct.bHorizontal ? TEXT("H") : TEXT("V"))
            : *FString::Printf(TEXT("Move to (%d,%d)"),
                               BestAct.MoveX,
                               BestAct.MoveY),
        BestAct.Score);

    // 6) Fallback if no candidate scored > INT_MIN
    if (BestAct.Score == INT_MIN && Candidates.Num() > 0) {
        UE_LOG(LogTemp, Error, TEXT(
            "SolveAlphaBeta: No best move found > INT_MIN, picking first candidate!"));
        return Candidates[0];
    }

    return BestAct;
}


FMinimaxAction MinimaxEngine::SolveParallelAlphaBeta(const FMinimaxState& Initial,int32 Depth,int32 RootPlayer)
{
    UE_LOG(LogTemp, Warning, TEXT(
        "=== SolveParallelAlphaBeta | Root=%d | Depth=%d ==="),
        RootPlayer, Depth);
    PrintInventory(Initial, TEXT("Start SolveParallelAlphaBeta"));
    PrintBlockedWalls(Initial, TEXT("Start SolveParallelAlphaBeta"));

    const int idx = RootPlayer - 1;
    const int Opponent = 3 - RootPlayer;

    // 1) Compute “before” path lengths & store AIPath
    int32 InitialAILength = 0, InitialOppLength = 0;
    TArray<FIntPoint> AIPath =
        ComputePathToGoal(Initial, RootPlayer, &InitialAILength);
    TArray<FIntPoint> OppPath =
        ComputePathToGoal(Initial, Opponent, &InitialOppLength);

    UE_LOG(LogTemp, Warning, TEXT(
        "Initial Paths: AI=%d | Opp=%d"),
        InitialAILength, InitialOppLength);
    for (int i = 0; i < AIPath.Num(); ++i) {
        UE_LOG(LogTemp, Warning, TEXT(
            "  AI[%d] = (%d,%d)"),
            i, AIPath[i].X, AIPath[i].Y);
    }

    // 2) Generate Candidate Moves
    TArray<FMinimaxAction> Candidates;

    // 2a) Pawn moves
    TArray<FIntPoint> PawnMoves = GetPawnMoves(Initial, RootPlayer);
    for (const auto& mv : PawnMoves) {
        Candidates.Add(FMinimaxAction(mv.X, mv.Y));
    }

    // 2b) Wall placements (if walls remain)
    if (Initial.WallsRemaining[idx] > 0) {
        TArray<FWallData> WallMoves =
            GetAllUsefulWallPlacements(Initial, RootPlayer);
        for (const auto& w : WallMoves) {
            Candidates.Add(
                FMinimaxAction(w.X, w.Y, w.Length, w.bHorizontal));
        }
    }

    if (Candidates.Num() == 0) {
        UE_LOG(LogTemp, Error, TEXT(
            "SolveAlphaBeta: No candidates found!"));
        return FMinimaxAction();
    }

    // 3) Heuristic‐score & sort for move ordering
    //    Compute act.Score = CalculateX(...) in parallel
    TArray<int32> HeuristicScores;
    HeuristicScores.SetNum(Candidates.Num());

    ParallelFor(Candidates.Num(), [&](int32 i)
    {
        const FMinimaxAction& act = Candidates[i];
        FMinimaxState SS = Initial;
        bool illegal = false;

        if (act.bIsWall) {
            // Apply and legality check
            FWallData w{ act.SlotX, act.SlotY, act.WallLength, act.bHorizontal };
            FMinimaxState TempCheck = Initial;
            ApplyWall(TempCheck, RootPlayer, w);
            if (DoesWallBlockPlayer(TempCheck)) {
                illegal = true;
            }
            else {
                ApplyWall(SS, RootPlayer, w);
            }
        }
        else {
            ApplyPawnMove(SS, RootPlayer, act.MoveX, act.MoveY);
        }

        if (illegal) {
            HeuristicScores[i] = INT_MIN;
        }
        else {
            // Compute action‐only heuristic (no α/β here)
            if (act.bIsWall) {
                HeuristicScores[i] = CalculateWallScore(
                    Initial,
                    SS,
                    RootPlayer,
                    Opponent,
                    InitialAILength,
                    InitialOppLength
                );
            }
            else {
                HeuristicScores[i] = CalculatePawnScore(
                    Initial,
                    SS,
                    RootPlayer,
                    Opponent,
                    InitialAILength,
                    AIPath,
                    act
                );
            }
        }
    });

    // Copy those scores into act.Score so we can sort
    for (int32 i = 0; i < Candidates.Num(); ++i) {
        Candidates[i].Score = HeuristicScores[i];
    }

    // Sort descending by act.Score so the highest heuristics are first
    Candidates.Sort([](const FMinimaxAction& A, const FMinimaxAction& B) {
        return A.Score > B.Score;
    });

    {
        // Prepare an array to hold each candidate’s final currentScore
        TArray<int32> FinalScores;
        FinalScores.SetNum(Candidates.Num());

        // Shared alpha for sibling pruning (initially −∞)
        TAtomic<int32> SharedAlpha = INT_MIN;

        ParallelFor(Candidates.Num(), [&](int32 i)
        {
            const FMinimaxAction& act = Candidates[i];

            // If we already marked this candidate illegal (Score == INT_MIN), skip it
            if (act.Score == INT_MIN) {
                FinalScores[i] = INT_MIN;
                return;
            }

            // Rebuild SS = Initial and apply this candidate
            FMinimaxState SS = Initial;
            bool illegal = false;
            if (act.bIsWall) {
                FWallData w{ act.SlotX, act.SlotY, act.WallLength, act.bHorizontal };
                FMinimaxState TempCheck = Initial;
                ApplyWall(TempCheck, RootPlayer, w);
                if (DoesWallBlockPlayer(TempCheck)) {
                    illegal = true;
                } else {
                    ApplyWall(SS, RootPlayer, w);
                }
            }
            else {
                ApplyPawnMove(SS, RootPlayer, act.MoveX, act.MoveY);
            }
            if (illegal) {
                FinalScores[i] = INT_MIN;
                return;
            }

            // Each thread fetches the current shared alpha
            int32 localAlpha = SharedAlpha.Load();

            // Run α/β from this child with (alpha = localAlpha, beta = +∞)
            int32 childScore = MinimaxAlphaBeta(
                SS,
                Depth - 1,
                RootPlayer,
                Opponent,
                localAlpha,
                INT_MAX
            );

            // Store the raw α/β result (no per‐node bonus here)
            FinalScores[i] = childScore;

            // Atomically update sharedAlpha if this childScore is higher
            int32 oldA = SharedAlpha.Load();
            while (childScore > oldA && 
                   !SharedAlpha.CompareExchange(oldA, childScore))
            {
                // On failure, CompareExchange updates oldA to current SharedAlpha; retry if childScore still higher
            }
        });

        // Now pick the best candidate in a quick serial pass:
        FMinimaxAction BestAct;
        BestAct.Score = INT_MIN;
        for (int32 i = 0; i < Candidates.Num(); ++i) {
            int32 score = FinalScores[i];
            if (score == INT_MIN) continue;
            if (score > BestAct.Score) {
                BestAct = Candidates[i];
                BestAct.Score = score;
            }
        }

        // 5) Update move history if BestAct is a pawn move
        if (!BestAct.bIsWall && BestAct.Score > INT_MIN) {
            RecentMoves.Add(FIntPoint(BestAct.MoveX, BestAct.MoveY));
            if (RecentMoves.Num() > 4) {
                RecentMoves.RemoveAt(0);
            }
            UE_LOG(LogTemp, Log, TEXT(
                "SolveAlphaBeta (parallel) P%d RecentMoves updated. Newest: (%d,%d). Count: %d"),
                RootPlayer,
                BestAct.MoveX, BestAct.MoveY,
                RecentMoves.Num());
        }

        UE_LOG(LogTemp, Warning, TEXT(
            "=> SolveAlphaBeta (parallel) selected: %s (Score=%d)"),
            BestAct.bIsWall
                ? *FString::Printf(TEXT("Wall L%d @(%d,%d) %s"),
                                   BestAct.WallLength,
                                   BestAct.SlotX,
                                   BestAct.SlotY,
                                   BestAct.bHorizontal ? TEXT("H") : TEXT("V"))
                : *FString::Printf(TEXT("Move to (%d,%d)"),
                                   BestAct.MoveX,
                                   BestAct.MoveY),
            BestAct.Score);

        // 6) Fallback if nothing scored above INT_MIN
        if (BestAct.Score == INT_MIN && Candidates.Num() > 0) {
            for (int32 i = 0; i < Candidates.Num(); ++i) {
                if (FinalScores[i] > INT_MIN) {
                    return Candidates[i];
                }
            }
            return Candidates[0];
        }

        return BestAct;
    }
}










