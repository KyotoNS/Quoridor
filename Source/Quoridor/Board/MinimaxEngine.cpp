// MinimaxEngine.cpp
#include "MinimaxEngine.h"
#include <queue>
#include <vector>
#include "Quoridor/Board/QuoridorBoard.h"
#include "Quoridor/Wall/WallSlot.h"
#include "Quoridor/Pawn/QuoridorPawn.h"
#include "Math/UnrealMathUtility.h"

//-----------------------------------------------------------------------------
// FMinimaxState::FromBoard
//-----------------------------------------------------------------------------
FMinimaxState FMinimaxState::FromBoard(AQuoridorBoard* Board)
{
   
    FMinimaxState S;

    // Initialize blocked arrays
    for (int y = 0; y < 9; ++y)
        for (int x = 0; x < 8; ++x)
            S.HorizontalBlocked[y][x] = false;

    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 9; ++x)
            S.VerticalBlocked[y][x] = false;

    // Get pawns
    AQuoridorPawn* P1 = Board->GetPawnForPlayer(1);
    AQuoridorPawn* P2 = Board->GetPawnForPlayer(2);

    if (P1 && P1->GetTile())
    {
        S.PawnX[0] = P1->GetTile()->GridX;
        S.PawnY[0] = P1->GetTile()->GridY;
    }
    if (P2 && P2->GetTile())
    {
        S.PawnX[1] = P2->GetTile()->GridX;
        S.PawnY[1] = P2->GetTile()->GridY;
    }

    // Walls remaining
    S.WallsRemaining[0] = P1 ? P1->PlayerWalls.Num() : 0;
    S.WallsRemaining[1] = P2 ? P2->PlayerWalls.Num() : 0;
    

    // === Horizontal Wall Segments (with bIsOccupied printout) ===
    for (AWallSlot* Slot : Board->HorizontalWallSlots)
    {
        if (Slot && Slot->bIsOccupied)
        {
            int x = Slot->GridX;
            int y = Slot->GridY;

            UE_LOG(LogTemp, Warning, TEXT("Occupied HWall at (%d, %d)"), x, y);

            if (x >= 0 && x < 8 && y >= 0 && y < 9)
            {
                S.HorizontalBlocked[y][x] = true;
            }
        }
    }

    // === Vertical Wall Segments (with bIsOccupied printout) ===
    for (AWallSlot* Slot : Board->VerticalWallSlots)
    {
        if (Slot && Slot->bIsOccupied)
        {
            int x = Slot->GridX;
            int y = Slot->GridY;

            UE_LOG(LogTemp, Warning, TEXT("Occupied VWall at (%d, %d)"), x, y);

            if (x >= 0 && x < 9 && y >= 0 && y < 8)
            {
                S.VerticalBlocked[y][x] = true;
            }
        }
    }

    if (P1)
    {
        for (int len = 1; len <= 3; ++len)
        {
            if (P1->GetWallCountOfLength(len) > 0)
            {
                S.AvailableWallLengthsForPlayer[0].Add(len);
            }
        }
    }

    if (P2)
    {
        for (int len = 1; len <= 3; ++len)
        {
            if (P2->GetWallCountOfLength(len) > 0)
            {
                S.AvailableWallLengthsForPlayer[1].Add(len);
            }
        }
    }



    return S;
}


//-----------------------------------------------------------------------------
// A* helper for distance-to-goal (100=no path)
//-----------------------------------------------------------------------------
// Replaces ComputeDistanceToGoal: Returns shortest path as tile list and optional length
TArray<FIntPoint> MinimaxEngine::ComputePathToGoal(const FMinimaxState& S, int32 PlayerNum, int32* OutLength)
{
    int goalY = (PlayerNum == 1 ? 8 : 0);
    int idx = PlayerNum - 1;
    int sx = S.PawnX[idx], sy = S.PawnY[idx];

    struct Node { int f, g, x, y; FIntPoint parent; };
    struct Cmp { bool operator()(const Node& a, const Node& b) const { return a.f > b.f; } };

    std::priority_queue<Node, std::vector<Node>, Cmp> open;
    bool closed[9][9] = {};
    int gCost[9][9];
    FIntPoint cameFrom[9][9];
    for (int y = 0; y < 9; ++y)
        for (int x = 0; x < 9; ++x) {
            gCost[y][x] = INT_MAX;
            cameFrom[y][x] = FIntPoint(-1, -1);
        }

    auto Heuristic = [&](int x, int y) { return FMath::Abs(goalY - y); };

    gCost[sy][sx] = 0;
    open.push({ Heuristic(sx, sy), 0, sx, sy, FIntPoint(-1, -1) });

    const FIntPoint dirs[4] = { {1,0}, {-1,0}, {0,1}, {0,-1} };

    while (!open.empty()) {
        Node n = open.top(); open.pop();

        if (closed[n.y][n.x]) continue;
        closed[n.y][n.x] = true;
        cameFrom[n.y][n.x] = n.parent;

        if (n.y == goalY) {
            if (OutLength) *OutLength = n.g;

            // Reconstruct path
            TArray<FIntPoint> Path;
            int cx = n.x, cy = n.y;
            while (cx != -1 && cy != -1) {
                Path.Insert(FIntPoint(cx, cy), 0);
                FIntPoint p = cameFrom[cy][cx];
                cx = p.X; cy = p.Y;
            }
            return Path;
        }

        for (const auto& d : dirs) {
            int nx = n.x + d.X;
            int ny = n.y + d.Y;
            if (nx < 0 || nx > 8 || ny < 0 || ny > 8) continue;
            if (d.X == 1 && S.VerticalBlocked[n.y][n.x]) continue;
            if (d.X == -1 && S.VerticalBlocked[n.y][nx]) continue;
            if (d.Y == 1 && S.HorizontalBlocked[n.y][n.x]) continue;
            if (d.Y == -1 && S.HorizontalBlocked[ny][n.x]) continue;

            int ng = n.g + 1;
            if (ng < gCost[ny][nx]) {
                gCost[ny][nx] = ng;
                open.push({ ng + Heuristic(nx, ny), ng, nx, ny, FIntPoint(n.x, n.y) });
            }
        }
    }

    if (OutLength) *OutLength = 100;
    return {}; // No path found
}
// Returns true if the wall (w) intersects or is adjacent to any tile in path.
bool MinimaxEngine::WallTouchesPath(const FWallData& w, const TArray<FIntPoint>& Path)
{
    for (const FIntPoint& tile : Path)
    {
        int tx = tile.X;
        int ty = tile.Y;

        // Horizontal wall spans (X,Y) to (X+length-1,Y), blocks between (Y,Y+1)
        if (w.bHorizontal)
        {
            for (int i = 0; i < w.Length; ++i)
            {
                if ((w.X + i == tx || w.X + i == tx - 1) && (w.Y == ty || w.Y == ty - 1))
                    return true;
            }
        }
        else // Vertical wall spans (X,Y) to (X,Y+length-1), blocks between (X,X+1)
        {
            for (int i = 0; i < w.Length; ++i)
            {
                if ((w.Y + i == ty || w.Y + i == ty - 1) && (w.X == tx || w.X == tx - 1))
                    return true;
            }
        }
    }
    return false;
}




int32 MinimaxEngine::Evaluate(const FMinimaxState& S)
{
    int32 P1Len = 0;
    int32 P2Len = 0;

    // Compute actual shortest paths and lengths
    ComputePathToGoal(S, 1, &P1Len); // Human
    ComputePathToGoal(S, 2, &P2Len); // AI

    int32 Score = (P1Len - P2Len) * 10;

    // Bonus: Walls near P1 goal row (row 8)
    for (int x = 0; x < 8; ++x)
    {
        if (S.HorizontalBlocked[7][x]) Score += 4;
    }

    for (int x = 0; x < 9; ++x)
    {
        if (S.VerticalBlocked[7][x]) Score += 2;
    }

    // Bonus: Fewer walls left for opponent
    Score += (S.WallsRemaining[1] - S.WallsRemaining[0]) * 2;

    return Score;
}



TArray<FIntPoint> MinimaxEngine::GetPawnMoves(const FMinimaxState& S, int32 PlayerNum)
{
    TArray<FIntPoint> Out;
    int idx = PlayerNum - 1;
    int x = S.PawnX[idx];
    int y = S.PawnY[idx];

    auto CanStep = [&](int ax, int ay, int bx, int by)
    {
        if (bx < 0 || bx > 8 || by < 0 || by > 8)
            return false;

        if (bx == ax + 1 && S.VerticalBlocked[ay][ax]) return false;
        if (bx == ax - 1 && S.VerticalBlocked[ay][bx]) return false;
        if (by == ay + 1 && S.HorizontalBlocked[ay][ax]) return false;
        if (by == ay - 1 && S.HorizontalBlocked[by][ax]) return false;

        return true;
    };

    const FIntPoint dirs[4] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1} };

    for (const auto& d : dirs)
    {
        int nx = x + d.X;
        int ny = y + d.Y;

        // Skip if blocked or out of bounds
        if (!CanStep(x, y, nx, ny))
            continue;

        bool occupied = (S.PawnX[0] == nx && S.PawnY[0] == ny) || (S.PawnX[1] == nx && S.PawnY[1] == ny);
        if (!occupied)
        {
            Out.Add({ nx, ny }); // Normal move
        }
        else
        {
            // Attempt jump straight
            int jx = nx + d.X;
            int jy = ny + d.Y;

            if (CanStep(nx, ny, jx, jy) &&
                !(S.PawnX[0] == jx && S.PawnY[0] == jy) &&
                !(S.PawnX[1] == jx && S.PawnY[1] == jy))
            {
                Out.Add({ jx, jy }); // Straight jump
            }
            else
            {
                // Try side steps if blocked behind
                FIntPoint perp[2] = { { d.Y, d.X }, { -d.Y, -d.X } };
                for (const auto& p : perp)
                {
                    int sx = nx + p.X;
                    int sy = ny + p.Y;

                    if (CanStep(nx, ny, sx, sy) &&
                        !(S.PawnX[0] == sx && S.PawnY[0] == sy) &&
                        !(S.PawnX[1] == sx && S.PawnY[1] == sy))
                    {
                        Out.Add({ sx, sy }); // Side step
                    }
                }
            }
        }
    }

    return Out;
}


TArray<FWallData> MinimaxEngine::GetWallPlacements(const FMinimaxState& S, int32 PlayerNum, const TArray<int32>& AvailableLengths)
{
    TArray<FWallData> Walls;

    if (AvailableLengths.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("GetWallPlacements: No available wall lengths for Player %d"), PlayerNum);
        return Walls;
    }

    int oppX = S.PawnX[0];
    int oppY = S.PawnY[0];

    int32 beforeDist = 0;
    TArray<FIntPoint> OpponentPath = ComputePathToGoal(S, 1, &beforeDist);

    auto IsWallLegal = [](const FMinimaxState& State, const FWallData& W)
    {
        for (int offset = 0; offset < W.Length; ++offset)
        {
            int x = W.bHorizontal ? W.X + offset : W.X;
            int y = W.bHorizontal ? W.Y : W.Y + offset;

            if (x < 0 || x >= 9 || y < 0 || y >= 9)
            {
                UE_LOG(LogTemp, Warning, TEXT("[Illegal] Out of bounds: Wall@(%d,%d)L%d %s"),
                    W.X, W.Y, W.Length, W.bHorizontal ? TEXT("H") : TEXT("V"));
                return false;
            }

            if (W.bHorizontal && (x >= 8 || State.HorizontalBlocked[y][x]))
            {
                UE_LOG(LogTemp, Warning, TEXT("[Illegal] HWall blocked: (%d,%d)"), x, y);
                return false;
            }

            if (!W.bHorizontal && (y >= 8 || State.VerticalBlocked[y][x]))
            {
                UE_LOG(LogTemp, Warning, TEXT("[Illegal] VWall blocked: (%d,%d)"), x, y);
                return false;
            }
        }

        return true;
    };

    auto SimulateWall = [](FMinimaxState& State, const FWallData& W)
    {
        for (int offset = 0; offset < W.Length; ++offset)
        {
            int x = W.bHorizontal ? W.X + offset : W.X;
            int y = W.bHorizontal ? W.Y : W.Y + offset;

            if (x >= 9 || y >= 9)
            {
                UE_LOG(LogTemp, Warning, TEXT("[Simulate FAIL] Wall@(%d,%d)L%d %s — out of bounds"),
                    W.X, W.Y, W.Length, W.bHorizontal ? TEXT("H") : TEXT("V"));
                return false;
            }

            if (W.bHorizontal)
                State.HorizontalBlocked[y][x] = true;
            else
                State.VerticalBlocked[y][x] = true;
        }

        if (ComputePathToGoal(State, 1).Num() == 0 || ComputePathToGoal(State, 2).Num() == 0)
        {
            UE_LOG(LogTemp, Warning, TEXT("[Simulate FAIL] Wall@(%d,%d)L%d %s — path blocked"),
                W.X, W.Y, W.Length, W.bHorizontal ? TEXT("H") : TEXT("V"));
            return false;
        }

        return true;
    };

    auto IsWallUseful = [&](const FMinimaxState& SimState, const FWallData& W)
    {
        int32 afterDist = 0;
        ComputePathToGoal(SimState, 1, &afterDist);
        int delta = afterDist - beforeDist;

        int distToOpponent = FMath::Abs(W.X - oppX) + FMath::Abs(W.Y - oppY);
        bool bTouchesPath = WallTouchesPath(W, OpponentPath);
        bool bIsNearPawn = (distToOpponent <= 2);

        if (!bTouchesPath && !bIsNearPawn)
        {
            UE_LOG(LogTemp, Warning, TEXT("[Not Useful] Wall@(%d,%d)L%d %s — not near pawn or path"),
                W.X, W.Y, W.Length, W.bHorizontal ? TEXT("H") : TEXT("V"));
            return false;
        }

        if (delta <= 0 && distToOpponent > 2)
        {
            UE_LOG(LogTemp, Warning, TEXT("[Not Useful] Wall@(%d,%d)L%d %s — no delay"),
                W.X, W.Y, W.Length, W.bHorizontal ? TEXT("H") : TEXT("V"));
            return false;
        }

        UE_LOG(LogTemp, Warning, TEXT("[Useful] Wall@(%d,%d)L%d %s => Δ=%d, dist=%d"),
            W.X, W.Y, W.Length, W.bHorizontal ? TEXT("H") : TEXT("V"), delta, distToOpponent);
        return true;
    };

    // Horizontal Walls
    for (int length : AvailableLengths)
    {
        for (int y = 0; y < 9; ++y)
        {
            for (int x = 0; x <= 8 - length; ++x)
            {
                FWallData W{ x, y, length, true };
                if (!IsWallLegal(S, W)) continue;

                FMinimaxState SS = S;
                SS.WallsRemaining[PlayerNum - 1]--;

                if (!SimulateWall(SS, W)) continue;

                if (IsWallUseful(SS, W)) {
                    UE_LOG(LogTemp, Warning, TEXT("[CAND] Wall(%d,%d)H len=%d — Accepted"), x, y, length);
                    Walls.Add(W);
                }
            }
        }
    }

    // Vertical Walls
    for (int length : AvailableLengths)
    {
        for (int y = 0; y <= 8 - length; ++y)
        {
            for (int x = 0; x < 9; ++x)
            {
                FWallData W{ x, y, length, false };
                if (!IsWallLegal(S, W)) continue;

                FMinimaxState SS = S;
                SS.WallsRemaining[PlayerNum - 1]--;

                if (!SimulateWall(SS, W)) continue;

                if (IsWallUseful(SS, W)) {
                    UE_LOG(LogTemp, Warning, TEXT("[CAND] Wall(%d,%d)V len=%d — Accepted"), x, y, length);
                    Walls.Add(W);
                }
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("GetWallPlacements: Returning %d candidate walls"), Walls.Num());
    return Walls;
}


// Suggests smart wall placements around the enemy pawn
TArray<FWallData> MinimaxEngine::GetTargetedWallPlacements(const FMinimaxState& S, int32 PlayerNum, const TArray<int32>& AvailableLengths)
{
    TArray<FWallData> Results;
    const int32 Opponent = 3 - PlayerNum;
    const int oppX = S.PawnX[Opponent - 1];
    const int oppY = S.PawnY[Opponent - 1];

    UE_LOG(LogTemp, Warning, TEXT("Targeting walls around opponent at (%d, %d)"), oppX, oppY);

    int32 beforeLen = 0;
    TArray<FIntPoint> pathBefore = ComputePathToGoal(S, Opponent, &beforeLen);

    struct ScoredWall {
        FWallData Wall;
        int Delta;
    };

    TArray<ScoredWall> Candidates;

    auto SimulateAndScore = [&](FWallData W) -> bool
    {
        if (!IsWallLegal(S, W))
            return false;

        FMinimaxState SS = S;

        for (int i = 0; i < W.Length; ++i)
        {
            if (W.bHorizontal)
            {
                int x = W.X + i;
                if (x >= 9 || W.Y >= 9) return false;
                SS.HorizontalBlocked[W.Y][x] = true;
            }
            else
            {
                int y = W.Y + i;
                if (y >= 9 || W.X >= 9) return false;
                SS.VerticalBlocked[y][W.X] = true;
            }
        }

        if (ComputePathToGoal(SS, PlayerNum).Num() == 0) return false;
        int32 lenAfter = 0;
        TArray<FIntPoint> afterPath = ComputePathToGoal(SS, Opponent, &lenAfter);
        if (afterPath.Num() == 0) return false;

        int delta = lenAfter - beforeLen;
        if (delta <= 0) return false;

        Candidates.Add({W, delta});
        return true;
    };

    if (AvailableLengths.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("Player %d has no wall lengths available."), PlayerNum);
        return Results;
    }

    for (int len : AvailableLengths)
    {
        for (int dy = -2; dy <= 2; ++dy)
        {
            for (int dx = -2; dx <= 2; ++dx)
            {
                int cx = oppX + dx;
                int cy = oppY + dy;

                if (cx < 0 || cy < 0 || cx >= 9 || cy >= 9)
                    continue;

                if (cx < 2 && oppX >= 3)
                    continue;

                if (cx + len - 1 < 9)
                {
                    FWallData W = {cx, cy, len, true};
                    SimulateAndScore(W);
                }

                if (cy + len - 1 < 9)
                {
                    FWallData W = {cx, cy, len, false};
                    SimulateAndScore(W);
                }
            }
        }
    }

    Candidates.Sort([](const ScoredWall& A, const ScoredWall& B) {
        return A.Delta > B.Delta;
    });

    for (const auto& Entry : Candidates)
    {
        Results.Add(Entry.Wall);
    }

    return Results;
}




void MinimaxEngine::ApplyPawnMove(FMinimaxState& S, int32 PlayerNum,int32 X,int32 Y)
{
    S.PawnX[PlayerNum-1]=X; S.PawnY[PlayerNum-1]=Y;
}

void MinimaxEngine::ApplyWall(FMinimaxState& S,int32 PlayerNum,const FWallData& W)
{
    int idx=PlayerNum-1;
    if(W.bHorizontal) S.HorizontalBlocked[W.Y][W.X]=true; else S.VerticalBlocked[W.Y][W.X]=true;
    S.WallsRemaining[idx]--;
}

bool MinimaxEngine::IsWallLegal(const FMinimaxState& S, const FWallData& W)
{
    for (int offset = 0; offset < W.Length; ++offset)
    {
        int x = W.bHorizontal ? W.X + offset : W.X;
        int y = W.bHorizontal ? W.Y : W.Y + offset;

        if (x < 0 || x >= 9 || y < 0 || y >= 9)
            return false;

        if (W.bHorizontal)
        {
            if (x >= 8 || S.HorizontalBlocked[y][x])
                return false;
        }
        else
        {
            if (y >= 8 || S.VerticalBlocked[y][x])
                return false;
        }
    }
    return true;
}

int32 MinimaxEngine::Minimax(FMinimaxState& S, int32 Depth, bool bMax)
{
    if (Depth <= 0)
        return Evaluate(S);

    bool maximize = bMax;
    int32 best = maximize ? INT_MIN : INT_MAX;
    int player = maximize ? 2 : 1;
    int opponent = 3 - player;
    int idx = player - 1;

    // --- Pawn moves ---
    for (auto mv : GetPawnMoves(S, player))
    {
        FMinimaxState SS = S;
        ApplyPawnMove(SS, player, mv.X, mv.Y);
        int32 v = Minimax(SS, Depth - 1, !bMax);
        best = maximize ? FMath::Max(best, v) : FMath::Min(best, v);
    }

    // --- Wall moves ---
    if (S.WallsRemaining[idx] > 0)
    {
        const TArray<int32>& AvailableLengths = S.AvailableWallLengthsForPlayer[idx];

        if (AvailableLengths.Num() > 0)
        {
            int oppX = S.PawnX[opponent - 1];
            int oppY = S.PawnY[opponent - 1];

            int32 beforeLen = 0;
            TArray<FIntPoint> beforePath = ComputePathToGoal(S, opponent, &beforeLen);

            for (const auto& w : GetWallPlacements(S, player, AvailableLengths))
            {
                if (!AvailableLengths.Contains(w.Length))
                {
                    UE_LOG(LogTemp, Warning, TEXT("[Minimax SKIP] Wall@(%d,%d)L%d %s — not owned"),
                        w.X, w.Y, w.Length, w.bHorizontal ? TEXT("H") : TEXT("V"));
                    continue;
                }

                if (!WallTouchesPath(w, beforePath))
                    continue;

                FMinimaxState SS = S;
                ApplyWall(SS, player, w);

                int32 afterLen = 0;
                ComputePathToGoal(SS, opponent, &afterLen);
                int32 delta = afterLen - beforeLen;

                int distToOpponent = FMath::Abs(w.X - oppX) + FMath::Abs(w.Y - oppY);

                if (delta <= 0 && distToOpponent > 2)
                    continue;

                int32 v = Minimax(SS, Depth - 1, !bMax);
                v += delta * 10;
                v += FMath::Clamp(10 - distToOpponent, 0, 10);

                best = maximize ? FMath::Max(best, v) : FMath::Min(best, v);
            }
        }
    }

    return best;
}

        

FMinimaxAction MinimaxEngine::Solve(const FMinimaxState& Initial, int32 Depth)
{
    UE_LOG(LogTemp, Warning, TEXT("=== MinimaxEngine::Solve depth=%d ==="), Depth);
    
    FMinimaxAction bestAct;
    bestAct.Score = INT_MIN;

    const int root = 2;         // AI player
    const int idx = root - 1;
    const int Opponent = 1;

    // === WALL MOVES ===
    if (Initial.WallsRemaining[idx] > 0)
    {
        const TArray<int32>& AvailableLengths = Initial.AvailableWallLengthsForPlayer[idx];
        TArray<FWallData> WallCandidates = GetTargetedWallPlacements(Initial, root, AvailableLengths);
        
        UE_LOG(LogTemp, Warning, TEXT("=== Raw Wall Candidates (%d total) ==="), WallCandidates.Num());
        for (const auto& w : WallCandidates)
        {
            UE_LOG(LogTemp, Warning, TEXT("Candidate Wall: (%d,%d) %s L=%d"),
                w.X, w.Y, w.bHorizontal ? TEXT("H") : TEXT("V"), w.Length);
        }

        if (WallCandidates.Num() == 0)
        {
            UE_LOG(LogTemp, Warning, TEXT("No wall candidates generated. Trying fallback wall."));

            FWallData Fallback;
            Fallback.X = Initial.PawnX[Opponent - 1];
            Fallback.Y = Initial.PawnY[Opponent - 1];
            Fallback.Length = 1;
            Fallback.bHorizontal = true;

            if (AvailableLengths.Contains(Fallback.Length))
            {
                WallCandidates.Add(Fallback);
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("Fallback wall length L=1 not available — skipped"));
            }
        }

        for (const auto& w : WallCandidates)
        {
            if (!AvailableLengths.Contains(w.Length))
            {
                UE_LOG(LogTemp, Warning, TEXT("[SKIP] Wall@(%d,%d)L%d %s — not in inventory"),
                    w.X, w.Y, w.Length, w.bHorizontal ? TEXT("H") : TEXT("V"));
                continue;
            }

            if (!IsWallLegal(Initial, w))
            {
                UE_LOG(LogTemp, Warning, TEXT("[SKIP] Illegal wall candidate (%d,%d)L%d %s"),
                    w.X, w.Y, w.Length, w.bHorizontal ? TEXT("H") : TEXT("V"));
                continue;
            }

            FMinimaxState SS = Initial;
            ApplyWall(SS, root, w);

            int32 beforeLen = 0, afterLen = 0;
            ComputePathToGoal(Initial, Opponent, &beforeLen);
            TArray<FIntPoint> path = ComputePathToGoal(SS, Opponent, &afterLen);

            if (afterLen >= 100)
            {
                UE_LOG(LogTemp, Warning, TEXT("[SKIP] Wall@(%d,%d)%s L=%d — would block path"),
                    w.X, w.Y, w.bHorizontal ? TEXT("H") : TEXT("V"), w.Length);
                continue;
            }

            int delta = afterLen - beforeLen;

            int32 v = Minimax(SS, Depth, false);
            v += delta * 10;

            // Additional heuristics
            if (!w.bHorizontal && w.Y >= 7) v += 5;
            if ( w.bHorizontal && w.Y >= 7) v += 8;

            UE_LOG(LogTemp, Warning, TEXT("[CAND] Wall@(%d,%d)%s L=%d => Score=%d (Δ=%d)"),
                w.X, w.Y, w.bHorizontal ? TEXT("H") : TEXT("V"), w.Length, v, delta);

            if (v > bestAct.Score)
            {
                bestAct = FMinimaxAction{
                    .bIsWall = true,
                    .SlotX = w.X,
                    .SlotY = w.Y,
                    .WallLength = w.Length,
                    .bHorizontal = w.bHorizontal,
                    .Score = v
                };
            }
        }
    }

    return bestAct;
}









