// MinimaxEngine.cpp
#include "MinimaxEngine.h"
#include <queue>
#include <vector>
#include <limits.h>
#include <functional>
#include <algorithm> // Required for std::sort
#include "Quoridor/Board/QuoridorBoard.h"
#include "Quoridor/Wall/WallSlot.h"
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
    int32 idxAI = RootPlayer - 1;
    int32 idxOpponent = 1 - idxAI;
    int32 OpponentPlayerNum = 3 - RootPlayer;

    int32 AILen = 100, OppLen = 100;
    TArray<FIntPoint> AIPath = ComputePathToGoal(S, RootPlayer, &AILen);
    TArray<FIntPoint> OppPath = ComputePathToGoal(S, OpponentPlayerNum, &OppLen);

    if (AILen == 0) return 50000;
    if (OppLen == 0) return -50000;
    if (AILen >= 100) return -49000;
    if (OppLen >= 100) return 49000;

    const int32 W_PathDiff      = 150;
    const int32 W_WallCount     = 8;
    const int32 W_BoardControl  = 3;
    const int32 W_StrategicWall = 15;
    const int32 W_PathDefense   = -7;

    int32 Score = (OppLen - AILen) * W_PathDiff;
    Score += (S.WallsRemaining[idxAI] - S.WallsRemaining[idxOpponent]) * W_WallCount;

    int32 MyControl = 0, OppControl = 0;
    ComputeBoardControl(S, MyControl, OppControl, RootPlayer);
    Score += (MyControl - OppControl) * W_BoardControl;

    // Strategic wall score for blocking finish row
    int oppBlockY = (RootPlayer == 1) ? 0 : 8;
    for (int x = 0; x < 8; ++x)
    {
        if (S.HorizontalBlocked[oppBlockY][x]) Score += W_StrategicWall;
    }

    // Opponent near my path penalty (optional)
    int OppDistToMyPath = 100;
    int OppX = S.PawnX[idxOpponent], OppY = S.PawnY[idxOpponent];
    for (const FIntPoint& p : AIPath)
    {
        OppDistToMyPath = FMath::Min(OppDistToMyPath, FMath::Abs(p.X - OppX) + FMath::Abs(p.Y - OppY));
    }
    if (OppDistToMyPath <= 2)
    {
        Score += (OppDistToMyPath - 3) * W_PathDefense;
    }

    // Position and movement info
    const FIntPoint Curr(S.PawnX[idxAI], S.PawnY[idxAI]);
    const FIntPoint& Prev = S.LastPawnPos[idxAI];
    const FIntPoint& Prev2 = S.SecondLastPawnPos[idxAI];

    // === Movement Evaluation Fixes ===

    // 1. Reward if pawn moved to follow the path
    if (AIPath.Num() > 1 && Curr == AIPath[1])
    {
        Score += 120; // Great move — exact path step
    }
    else if (AIPath.Num() > 2 && Curr == AIPath[2])
    {
        Score += 55; // Slight detour but still on path
    }
    else
    {
        Score -= 100; // Off-path
    }

    // 2. Reward heading toward finish
    Score += (8 - FMath::Abs(Curr.Y - (RootPlayer == 1 ? 8 : 0))) * 2;

    // 3. Penalize backtracking
    if (Curr == Prev)    Score -= 3000;
    if (Curr == Prev2)   Score -= 3000;

    // 4. Reward forward movement
    if (AIPath.Num() > 1)
    {
        const FIntPoint& Next = AIPath[1];
        int dx = FMath::Abs(Next.X - Curr.X);
        int dy = Next.Y - Curr.Y;

        const int GoalDir = (RootPlayer == 1 ? 1 : -1);
        if (dx == 0 && dy == GoalDir)
        {
            Score += 40; // Vertical forward
        }
        else if (dx == 0 && dy == -GoalDir)
        {
            Score -= 80; // Vertical backward
        }
    }

    // 5. Jump move bonus (if large step)
    if (FMath::Abs(Curr.X - Prev.X) > 1 || FMath::Abs(Curr.Y - Prev.Y) > 1)
    {
        Score += 50;
    }

    // 6. Minor bonus if standing on opponent’s path (disruption)
    for (const FIntPoint& t : OppPath)
    {
        if (FMath::Abs(t.X - Curr.X) + FMath::Abs(t.Y - Curr.Y) <= 1)
        {
            Score += 20;
            break;
        }
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

//-----------------------------------------------------------------------------
// Solve Parallel (Using Plain Minimax)
//-----------------------------------------------------------------------------
FMinimaxAction MinimaxEngine::SolveParallel(const FMinimaxState& Initial, int32 Depth, int32 RootPlayer)
{
    UE_LOG(LogTemp, Warning, TEXT("=== SolveParallel (Plain Minimax) | Root=%d | Depth=%d ==="), RootPlayer, Depth);
    PrintInventory(Initial, TEXT("Start SolveParallel"));
    PrintBlockedWalls(Initial, TEXT("Start SolveParallel"));

    const int idx = RootPlayer - 1;
    const int Opponent = 3 - RootPlayer;
    int32 InitialAILength = 0, InitialOppLength = 0; // Renamed to avoid confusion
    TArray<FIntPoint> AIPath = ComputePathToGoal(Initial, RootPlayer, &InitialAILength);
    TArray<FIntPoint> OppPath = ComputePathToGoal(Initial, Opponent, &InitialOppLength);
    UE_LOG(LogTemp, Warning, TEXT("Initial Paths: AI=%d | Opp=%d"), InitialAILength, InitialOppLength);
    for (int i = 0; i < AIPath.Num(); ++i)
        UE_LOG(LogTemp, Warning, TEXT("  AI[%d] = (%d,%d)"), i, AIPath[i].X, AIPath[i].Y);
    TArray<FMinimaxAction> Candidates;

    // === Generate Candidates (Using Constructors) ===
    TArray<FIntPoint> PawnMoves = GetPawnMoves(Initial, RootPlayer);
    for (const auto& mv : PawnMoves) {
        Candidates.Add(FMinimaxAction(mv.X, mv.Y));
    }
    if (Initial.WallsRemaining[idx] > 0) {
        TArray<FWallData> WallMoves = GetAllUsefulWallPlacements(Initial, RootPlayer);
        for (const auto& w : WallMoves) {
            Candidates.Add(FMinimaxAction(w.X, w.Y, w.Length, w.bHorizontal));
        }
    }

    if (Candidates.Num() == 0) {
        UE_LOG(LogTemp, Error, TEXT("SolveParallel: No candidates found!"));
        return FMinimaxAction();
    }

    // === Parallel Scoring ===
    TArray<int32> Scores;
    Scores.SetNum(Candidates.Num());

    ParallelFor(Candidates.Num(), [&](int32 i)
    {
        const FMinimaxAction& act = Candidates[i];
        FMinimaxState SS = Initial;
        int32 score = INT_MIN;

        if (act.bIsWall) {
            FWallData w{ act.SlotX, act.SlotY, act.WallLength, act.bHorizontal };
            FMinimaxState TempCheck = Initial; // Check on a fresh copy of Initial
            ApplyWall(TempCheck, RootPlayer, w);
            if(DoesWallBlockPlayer(TempCheck)) {
                 Scores[i] = INT_MIN; // Illegal blocking wall
                 return; // Exit lambda for this index
            }
            ApplyWall(SS, RootPlayer, w); // Apply to the state for Minimax
        } else {
            ApplyPawnMove(SS, RootPlayer, act.MoveX, act.MoveY);
        }

        // Call the PLAIN Minimax function
        score = Minimax(SS, Depth - 1, RootPlayer, Opponent);

        // Optional: Add immediate heuristic bonus (like original code)
        int32 AfterAILen = 0, AfterOppLen = 0;
        ComputePathToGoal(SS, RootPlayer, &AfterAILen);
        ComputePathToGoal(SS, Opponent, &AfterOppLen);

        // Calculate NetGain relative to the state *after* this move (SS)
        // compared to the paths of the initial state (Initial)
        if (InitialAILength < 100 && InitialOppLength < 100 && AfterAILen < 100 && AfterOppLen < 100) {
             int32 NetGain = (AfterOppLen - AfterAILen) - (InitialOppLength - InitialAILength);
             score += NetGain * 10; // Add path difference gain
        }


        // *** ADD ANTI-OSCILLATION PENALTY for pawn moves ***
        if (!act.bIsWall) {
            // This assumes 'RecentMoves' is accessible and is for the current RootPlayer
            if (RecentMoves.Num() > 0) {
                // Check against the very last move (the one that led to 'Initial' state)
                if (Initial.PawnX[idx] == act.MoveX && Initial.PawnY[idx] == RecentMoves.Last().Y && RecentMoves.Last().X == act.MoveX) {
                    // This logic is a bit tricky: we are evaluating a move *from* Initial.
                    // So, if act.MoveX, act.MoveY takes us to where we were 2 moves ago
                    // (i.e., Initial.PawnX[idx], Initial.PawnY[idx] was the result of moving from act.MoveX, act.MoveY)
                    // We need to compare act.MoveX, act.MoveY with RecentMoves.Last() if RecentMoves stores previous *target* locations.
                    // Let's assume RecentMoves stores where the pawn *landed*.
                    // If moving to (act.MoveX, act.MoveY) is the same as the *grandparent* position
                }
                // Simpler logic matching SolveAlphaBeta:
                // Penalize moves that would take us back to where we were previously
                FIntPoint CurrentPawnPos = FIntPoint(Initial.PawnX[idx], Initial.PawnY[idx]); // Where AI is currently
                if (RecentMoves.Num() > 0) {
                    if (act.MoveX == CurrentPawnPos.X && act.MoveY == CurrentPawnPos.Y) {
                        // This check is wrong: act is a *target*. CurrentPawnPos is start.
                        // We need to check if act.MoveX, act.MoveY matches a previous position
                    }
                    // Corrected logic: act.MoveX, act.MoveY is the *target* of the current candidate move.
                    // RecentMoves.Last() is where the pawn was *before* its current position (Initial.PawnX[idx], Initial.PawnY[idx]).
                    if (RecentMoves.Num() >= 1 && act.MoveX == RecentMoves.Last().X && act.MoveY == RecentMoves.Last().Y) {
                         score -= 1000; // Heavy penalty for immediate reversal to the previous square
                         UE_LOG(LogTemp, Warning, TEXT("SolveParallel: Penalizing immediate reversal to (%d,%d) for P%d"), act.MoveX, act.MoveY, RootPlayer);
                    }
                }
                if (RecentMoves.Num() >= 2) { // Need at least two past moves to check the one before last
                    if (act.MoveX == RecentMoves[RecentMoves.Num()-2].X && act.MoveY == RecentMoves[RecentMoves.Num()-2].Y) {
                        score -= 500;   // Penalty for returning to 2-moves-ago position
                        UE_LOG(LogTemp, Warning, TEXT("SolveParallel: Penalizing reversal to 2-moves-ago (%d,%d) for P%d"), act.MoveX, act.MoveY, RootPlayer);
                    }
                }
            }
        }
        // *** END ANTI-OSCILLATION PENALTY ***

        Scores[i] = score;
    });

    // === Select Best Move (After Parallel Execution) ===
    FMinimaxAction BestAct;
    BestAct.Score = INT_MIN;

    for (int32 i = 0; i < Candidates.Num(); ++i)
    {
        if (Scores[i] > BestAct.Score)
        {
            BestAct = Candidates[i];
            BestAct.Score = Scores[i];
        }
    }

    // *** ADD UPDATE MOVE HISTORY (for RootPlayer) ***
    if (!BestAct.bIsWall && BestAct.Score > INT_MIN) { // Ensure BestAct is valid
        // This assumes 'RecentMoves' is the correct list for the current RootPlayer
        RecentMoves.Add(FIntPoint(BestAct.MoveX, BestAct.MoveY));
        if (RecentMoves.Num() > 4) {  // Keep only last N moves (e.g., 4 for 2-move repetition)
            RecentMoves.RemoveAt(0);
        }
        UE_LOG(LogTemp, Log, TEXT("SolveParallel: P%d RecentMoves updated. Newest: (%d,%d). Count: %d"), RootPlayer, BestAct.MoveX, BestAct.MoveY, RecentMoves.Num());
    }
    // *** END UPDATE MOVE HISTORY ***


     UE_LOG(LogTemp, Warning, TEXT("=> SolveParallel selected: %s (Score=%d)"),
        BestAct.bIsWall ?
            *FString::Printf(TEXT("Wall L%d @(%d,%d) %s"), BestAct.WallLength, BestAct.SlotX, BestAct.SlotY, BestAct.bHorizontal ? TEXT("H") : TEXT("V")) :
            *FString::Printf(TEXT("Move to (%d,%d)"), BestAct.MoveX, BestAct.MoveY),
        BestAct.Score);

    if (BestAct.Score == INT_MIN && Candidates.Num() > 0) {
       UE_LOG(LogTemp, Error, TEXT("No best move found > INT_MIN, picking first candidate!"));
       return Candidates[0];
    }

    return BestAct;
}

//-----------------------------------------------------------------------------
// Solve Alpha-Beta (Main Entry Point)
//-----------------------------------------------------------------------------
FMinimaxAction MinimaxEngine::SolveAlphaBeta(const FMinimaxState& Initial, int32 Depth, int32 RootPlayer)
{
    UE_LOG(LogTemp, Warning, TEXT("=== SolveAlphaBeta | Root=%d | Depth=%d ==="), RootPlayer, Depth);
    PrintInventory(Initial, TEXT("Start SolveAlphaBeta"));
    PrintBlockedWalls(Initial, TEXT("Start SolveAlphaBeta"));
    
    const int idx = RootPlayer - 1;
    const int Opponent = 3 - RootPlayer;
    int32 AILength = 0, OppLength = 0;
    TArray<FIntPoint> AIPath = ComputePathToGoal(Initial, RootPlayer, &AILength);
    TArray<FIntPoint> OppPath = ComputePathToGoal(Initial, Opponent, &OppLength);
    UE_LOG(LogTemp, Warning, TEXT("Initial Paths: AI=%d | Opp=%d"), AILength, OppLength);
    for (int i = 0; i < AIPath.Num(); ++i)
        UE_LOG(LogTemp, Warning, TEXT("  AI[%d] = (%d,%d)"), i, AIPath[i].X, AIPath[i].Y);
    
    TArray<FMinimaxAction> Candidates;

    // === Generate Pawn Moves (Using Pawn Constructor) ===
    TArray<FIntPoint> PawnMoves = GetPawnMoves(Initial, RootPlayer);
    for (const auto& mv : PawnMoves) {
        Candidates.Add(FMinimaxAction(mv.X, mv.Y));
    }

    // === Generate Wall Moves (Using Wall Constructor) ===
    if (Initial.WallsRemaining[idx] > 0) {
        TArray<FWallData> WallMoves = GetAllUsefulWallPlacements(Initial, RootPlayer);
        for (const auto& w : WallMoves) {
            Candidates.Add(FMinimaxAction(w.X, w.Y, w.Length, w.bHorizontal));
        }
    }

    if (Candidates.Num() == 0) {
        UE_LOG(LogTemp, Error, TEXT("SolveAlphaBeta: No candidates found!"));
        return FMinimaxAction();
    }

    // === Score & Sort Candidates for Move Ordering ===
    int32 CurrentX = Initial.PawnX[idx];
    int32 CurrentY = Initial.PawnY[idx];
    
    for (FMinimaxAction& act : Candidates) {
         FMinimaxState SS = Initial;
         if(act.bIsWall) {
             ApplyWall(SS, RootPlayer, {act.SlotX, act.SlotY, act.WallLength, act.bHorizontal});
         } else {
             ApplyPawnMove(SS, RootPlayer, act.MoveX, act.MoveY);
             
             // ANTI-OSCILLATION: Penalize moves that go back to recent positions
             // This helps prevent the back-and-forth movement
             if (RecentMoves.Num() > 0) {
                 FIntPoint LastMove = RecentMoves.Last();
                 if (act.MoveX == LastMove.X && act.MoveY == LastMove.Y) {
                     act.Score -= 1000;  // Heavy penalty for immediate reversal
                 }
             }
             if (RecentMoves.Num() > 1) {
                 FIntPoint SecondLastMove = RecentMoves[RecentMoves.Num()-2];
                 if (act.MoveX == SecondLastMove.X && act.MoveY == SecondLastMove.Y) {
                     act.Score -= 500;   // Penalty for returning to 2-moves-ago position
                 }
             }
         }
         
         int32 TempAILen = 100, TempOppLen = 100;
         ComputePathToGoal(SS, RootPlayer, &TempAILen);
         ComputePathToGoal(SS, Opponent, &TempOppLen);
         act.Score += (TempOppLen - TempAILen) - (OppLength - AILength);
    }
    
    Candidates.Sort([](const FMinimaxAction& A, const FMinimaxAction& B) {
        return A.Score > B.Score;
    });

    // === Run Minimax on Ordered Candidates ===
    FMinimaxAction BestAct;
    BestAct.Score = INT_MIN;
    int32 Alpha = INT_MIN;
    int32 Beta = INT_MAX;

    for (const auto& act : Candidates)
    {
        FMinimaxState SS = Initial;
        int32 CurrentScore = INT_MIN;

        if (act.bIsWall) {
            FWallData w{ act.SlotX, act.SlotY, act.WallLength, act.bHorizontal };
            FMinimaxState TempCheck = Initial;
            ApplyWall(TempCheck, RootPlayer, w);
            if(DoesWallBlockPlayer(TempCheck)) continue;
            ApplyWall(SS, RootPlayer, w);
        } else {
            ApplyPawnMove(SS, RootPlayer, act.MoveX, act.MoveY);
        }

        CurrentScore = MinimaxAlphaBeta(SS, Depth - 1, RootPlayer, Opponent, Alpha, Beta);

        if (CurrentScore > BestAct.Score) {
            BestAct = act;
            BestAct.Score = CurrentScore;
            Alpha = FMath::Max(Alpha, CurrentScore);
        }
    }

    // === Update move history to prevent oscillation ===
    if (!BestAct.bIsWall) {
        RecentMoves.Add(FIntPoint(BestAct.MoveX, BestAct.MoveY));
        if (RecentMoves.Num() > 4) {  // Keep only last 4 moves
            RecentMoves.RemoveAt(0);
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("=> SolveAlphaBeta selected: %s (Score=%d)"),
        BestAct.bIsWall ?
            *FString::Printf(TEXT("Wall L%d @(%d,%d) %s"), BestAct.WallLength, BestAct.SlotX, BestAct.SlotY, BestAct.bHorizontal ? TEXT("H") : TEXT("V")) :
            *FString::Printf(TEXT("Move to (%d,%d)"), BestAct.MoveX, BestAct.MoveY),
        BestAct.Score);

    if (BestAct.Score == INT_MIN && Candidates.Num() > 0) {
       UE_LOG(LogTemp, Error, TEXT("No best move found > INT_MIN, picking first candidate!"));
       return Candidates[0];
    }

    return BestAct;
}

FMinimaxAction MinimaxEngine::SolveParallelAlphaBeta(const FMinimaxState& Initial, int32 Depth, int32 RootPlayer)
{
    UE_LOG(LogTemp, Warning, TEXT("=== SolveParallelAlphaBeta | Root=%d | Depth=%d ==="), RootPlayer, Depth);

    // Using the single static MinimaxEngine::RecentMoves
    // WARNING: This shared list will cause issues if two AIs use this Solve function simultaneously
    // or in sequence without clearing/managing the history per player turn.

    PrintInventory(Initial, TEXT("Start SolveParallelAlphaBeta"));
    PrintBlockedWalls(Initial, TEXT("Start SolveParallelAlphaBeta"));

    const int idx = RootPlayer - 1;
    const int Opponent = 3 - RootPlayer;
    int32 InitialAILength = 0, InitialOppLength = 0;
    TArray<FIntPoint> AIPath = ComputePathToGoal(Initial, RootPlayer, &InitialAILength);
    TArray<FIntPoint> OppPath = ComputePathToGoal(Initial, Opponent, &InitialOppLength);
    UE_LOG(LogTemp, Warning, TEXT("Initial Paths: AI=%d | Opp=%d"), InitialAILength, InitialOppLength);
    for (int i = 0; i < AIPath.Num(); ++i)
        UE_LOG(LogTemp, Warning, TEXT("  AI[%d] = (%d,%d)"), i, AIPath[i].X, AIPath[i].Y);
    
    TArray<FMinimaxAction> Candidates;

    // === Generate Candidate Moves ===
    TArray<FIntPoint> PawnMoves = GetPawnMoves(Initial, RootPlayer);
    for (const auto& mv : PawnMoves) {
        Candidates.Add(FMinimaxAction(mv.X, mv.Y));
    }

    if (Initial.WallsRemaining[idx] > 0) {
        TArray<FWallData> WallMoves = GetAllUsefulWallPlacements(Initial, RootPlayer);
        for (const auto& w : WallMoves) {
            Candidates.Add(FMinimaxAction(w.X, w.Y, w.Length, w.bHorizontal));
        }
    }

    if (Candidates.Num() == 0) {
        UE_LOG(LogTemp, Error, TEXT("SolveParallelAlphaBeta: No candidates found!"));
        return FMinimaxAction();
    }

    TArray<int32> Scores;
    Scores.SetNum(Candidates.Num());

    ParallelFor(Candidates.Num(), [&](int32 i)
    {
        const FMinimaxAction& act = Candidates[i];
        FMinimaxState SS = Initial;
        int32 score = INT_MIN;

        if (act.bIsWall) {
            FWallData w{ act.SlotX, act.SlotY, act.WallLength, act.bHorizontal };
            FMinimaxState TempCheck = Initial;
            ApplyWall(TempCheck, RootPlayer, w);
            if(DoesWallBlockPlayer(TempCheck)) {
                 Scores[i] = INT_MIN;
                 return;
            }
            ApplyWall(SS, RootPlayer, w);
        } else {
            ApplyPawnMove(SS, RootPlayer, act.MoveX, act.MoveY);
        }

        score = MinimaxAlphaBeta(SS, Depth - 1, RootPlayer, Opponent, INT_MIN, INT_MAX);

        if (InitialAILength < 100 && InitialOppLength < 100 && SS.PawnX[idx] >=0 /*Ensure SS paths can be computed*/) {
            int32 AfterAILen = 0, AfterOppLen = 0;
            ComputePathToGoal(SS, RootPlayer, &AfterAILen);
            ComputePathToGoal(SS, Opponent, &AfterOppLen);
            if (AfterAILen < 100 && AfterOppLen < 100) {
                 int32 NetGain = (AfterOppLen - AfterAILen) - (InitialOppLength - InitialAILength);
                 score += NetGain * 10;
            }
        }

        if (!act.bIsWall) {
            // Using the single static MinimaxEngine::RecentMoves
            if (MinimaxEngine::RecentMoves.Num() >= 1) {
                if (act.MoveX == MinimaxEngine::RecentMoves.Last().X && act.MoveY == MinimaxEngine::RecentMoves.Last().Y) {
                    score -= 1000;
                    UE_LOG(LogTemp, Log, TEXT("SolveParallelAlphaBeta: Penalizing immediate reversal to (%d,%d) for P%d"), act.MoveX, act.MoveY, RootPlayer);
                }
            }
            if (MinimaxEngine::RecentMoves.Num() >= 2) {
                if (act.MoveX == MinimaxEngine::RecentMoves[MinimaxEngine::RecentMoves.Num()-2].X && act.MoveY == MinimaxEngine::RecentMoves[MinimaxEngine::RecentMoves.Num()-2].Y) {
                    score -= 500;
                    UE_LOG(LogTemp, Log, TEXT("SolveParallelAlphaBeta: Penalizing reversal to 2-moves-ago (%d,%d) for P%d"), act.MoveX, act.MoveY, RootPlayer);
                }
            }
        }
        Scores[i] = score;
    });

    FMinimaxAction BestAct;
    BestAct.Score = INT_MIN;

    for (int32 i = 0; i < Candidates.Num(); ++i)
    {
        Candidates[i].Score = Scores[i];
        if (Scores[i] > BestAct.Score)
        {
            BestAct = Candidates[i];
            // BestAct.Score = Scores[i]; // Already done by assigning Candidates[i] which has .Score
        }
    }

    if (!BestAct.bIsWall && BestAct.Score > INT_MIN) {
        MinimaxEngine::RecentMoves.Add(FIntPoint(BestAct.MoveX, BestAct.MoveY));
        if (MinimaxEngine::RecentMoves.Num() > 4) {
            MinimaxEngine::RecentMoves.RemoveAt(0);
        }
        UE_LOG(LogTemp, Log, TEXT("SolveParallelAlphaBeta: P%d RecentMoves updated. Newest: (%d,%d). Count: %d"), RootPlayer, BestAct.MoveX, BestAct.MoveY, MinimaxEngine::RecentMoves.Num());
    }

    UE_LOG(LogTemp, Warning, TEXT("=> SolveParallelAlphaBeta selected: %s (Score=%d)"),
        BestAct.bIsWall ?
            *FString::Printf(TEXT("Wall L%d @(%d,%d) %s"), BestAct.WallLength, BestAct.SlotX, BestAct.SlotY, BestAct.bHorizontal ? TEXT("H") : TEXT("V")) :
            *FString::Printf(TEXT("Move to (%d,%d)"), BestAct.MoveX, BestAct.MoveY),
        BestAct.Score);

    if (BestAct.Score == INT_MIN && Candidates.Num() > 0) {
       UE_LOG(LogTemp, Error, TEXT("SolveParallelAlphaBeta: No best move found > INT_MIN, picking first valid candidate if any!"));
       for(int32 i=0; i < Candidates.Num(); ++i) {
           if(Scores[i] > INT_MIN) {
               return Candidates[i];
           }
       }
       return Candidates[0];
    }
    return BestAct;
}






