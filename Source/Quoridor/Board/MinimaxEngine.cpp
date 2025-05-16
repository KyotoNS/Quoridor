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
    AQuoridorPawn* P1 = Board->GetPawnForPlayer(1); // Player
    AQuoridorPawn* P2 = Board->GetPawnForPlayer(2); // AI

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

    // === Mark Horizontal Wall Segments ===
    for (AWallSlot* Slot : Board->HorizontalWallSlots)
    {
        if (Slot && Slot->bIsOccupied)
        {
            int x = Slot->GridX;
            int y = Slot->GridY;

            if (x >= 0 && x < 8 && y >= 0 && y < 9)
            {
                S.HorizontalBlocked[y][x] = true;
            }
        }
    }

    // === Mark Vertical Wall Segments ===
    for (AWallSlot* Slot : Board->VerticalWallSlots)
    {
        if (Slot && Slot->bIsOccupied)
        {
            int x = Slot->GridX;
            int y = Slot->GridY;

            if (x >= 0 && x < 9 && y >= 0 && y < 8)
            {
                S.VerticalBlocked[y][x] = true;
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


TArray<FWallData> MinimaxEngine::GetWallPlacements(const FMinimaxState& S, int32 PlayerNum)
{
    TArray<FWallData> Walls;
    const int MaxLengths[3] = {1, 2, 3}; // AI wall lengths

    int oppX = S.PawnX[0]; // Player 1
    int oppY = S.PawnY[0];

    int32 beforeDist = 0;
    TArray<FIntPoint> OpponentPath = ComputePathToGoal(S, 1, &beforeDist);

    auto SimulateWall = [](FMinimaxState& State, const FWallData& W) -> bool
    {
        for (int offset = 0; offset < W.Length; ++offset)
        {
            if (W.bHorizontal)
            {
                int x = W.X + offset;
                if (x >= 9 || W.Y >= 9) return false;
                State.HorizontalBlocked[W.Y][x] = true;
            }
            else
            {
                int y = W.Y + offset;
                if (y >= 9 || W.X >= 9) return false;
                State.VerticalBlocked[y][W.X] = true;
            }
        }

        // Ensure both players still have a path
        return ComputePathToGoal(State, 1).Num() > 0 && ComputePathToGoal(State, 2).Num() > 0;
    };

    auto IsWallUseful = [&](FMinimaxState& SimState, const FWallData& W) -> bool
    {
        int32 afterDist = 0;
        ComputePathToGoal(SimState, 1, &afterDist);
        int delta = afterDist - beforeDist;

        int distToOpponent = FMath::Abs(W.X - oppX) + FMath::Abs(W.Y - oppY);
        bool bTouchesPath = WallTouchesPath(W, OpponentPath);
        bool bIsNearPawn = (distToOpponent <= 1);

        if (!bTouchesPath && !bIsNearPawn)
            return false;

        if (delta <= 0 && distToOpponent > 2)
            return false;

        return true;
    };

    // === Horizontal Walls ===
    for (int length : MaxLengths)
    {
        for (int y = 0; y < 9; ++y)
        {
            for (int x = 0; x <= 8 - length; ++x)
            {
                FWallData W{ x, y, length, true };
                FMinimaxState SS = S;
                SS.WallsRemaining[PlayerNum - 1]--;

                if (!SimulateWall(SS, W)) continue;

                if (IsWallUseful(SS, W))
                    Walls.Add(W);
            }
        }
    }

    // === Vertical Walls ===
    for (int length : MaxLengths)
    {
        for (int y = 0; y <= 8 - length; ++y)
        {
            for (int x = 0; x < 9; ++x)
            {
                FWallData W{ x, y, length, false };
                FMinimaxState SS = S;
                SS.WallsRemaining[PlayerNum - 1]--;

                if (!SimulateWall(SS, W)) continue;

                if (IsWallUseful(SS, W))
                    Walls.Add(W);
            }
        }
    }

    return Walls;
}


// Suggests smart wall placements around the enemy pawn
TArray<FWallData> MinimaxEngine::GetTargetedWallPlacements(const FMinimaxState& S, int32 PlayerNum)
{
    TArray<FWallData> Results;
    const int32 Opponent = 3 - PlayerNum;
    const int oppX = S.PawnX[Opponent - 1];
    const int oppY = S.PawnY[Opponent - 1];

    UE_LOG(LogTemp, Warning, TEXT("Targeting walls around opponent at (%d, %d)"), oppX, oppY);

    // Precompute path length before any wall
    int32 beforeLen = 0;
    TArray<FIntPoint> pathBefore = ComputePathToGoal(S, Opponent, &beforeLen);

    struct ScoredWall {
        FWallData Wall;
        int Delta;
    };

    TArray<ScoredWall> Candidates;

    auto SimulateAndScore = [&](FWallData W) -> bool
    {
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

    const int MaxLengths[3] = {1, 2, 3};
    for (int len : MaxLengths)
    {
        for (int dy = -2; dy <= 2; ++dy)
        {
            for (int dx = -2; dx <= 2; ++dx)
            {
                int cx = oppX + dx;
                int cy = oppY + dy;

                if (cx < 0 || cy < 0 || cx >= 9 || cy >= 9)
                    continue;

                // Skip far-left wall slots unless the pawn is also on left
                if (cx < 2 && oppX >= 3)
                    continue;

                // Horizontal
                if (cx + len - 1 < 9)
                {
                    FWallData W = {cx, cy, len, true};
                    SimulateAndScore(W);
                }

                // Vertical
                if (cy + len - 1 < 9)
                {
                    FWallData W = {cx, cy, len, false};
                    SimulateAndScore(W);
                }
            }
        }
    }

    // Sort by delta descending
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

int32 MinimaxEngine::Minimax(FMinimaxState& S, int32 Depth, bool bMax)
{
    if (Depth <= 0)
        return Evaluate(S);

    bool maximize = bMax;
    int32 best = maximize ? INT_MIN : INT_MAX;
    int player = maximize ? 2 : 1;
    int opponent = 3 - player;

    // --- Pawn moves ---
    for (auto mv : GetPawnMoves(S, player))
    {
        FMinimaxState SS = S;
        ApplyPawnMove(SS, player, mv.X, mv.Y);
        int32 v = Minimax(SS, Depth - 1, !bMax);
        best = maximize ? FMath::Max(best, v) : FMath::Min(best, v);
    }

    // --- Wall moves (filtered) ---
    int idx = player - 1;
    if (S.WallsRemaining[idx] > 0)
    {
        int oppX = S.PawnX[opponent - 1];
        int oppY = S.PawnY[opponent - 1];

        int32 beforeLen = 0;
        TArray<FIntPoint> beforePath = ComputePathToGoal(S, opponent, &beforeLen);

        for (auto w : GetWallPlacements(S, player))
        {
            // Reject walls that don't touch the path
            if (!WallTouchesPath(w, beforePath))
                continue;

            FMinimaxState SS = S;
            ApplyWall(SS, player, w);

            int32 afterLen = 0;
            ComputePathToGoal(SS, opponent, &afterLen);
            int32 delta = afterLen - beforeLen;

            int distToOpponent = FMath::Abs(w.X - oppX) + FMath::Abs(w.Y - oppY);

            // Skip walls that do nothing and aren't very close
            if (delta <= 0 && distToOpponent > 2)
                continue;

            int32 v = Minimax(SS, Depth - 1, !bMax);
            v += delta * 10;
            v += FMath::Clamp(10 - distToOpponent, 0, 10);

            best = maximize ? FMath::Max(best, v) : FMath::Min(best, v);
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

    // === PAWN MOVES ===
    for (auto mv : GetPawnMoves(const_cast<FMinimaxState&>(Initial), root))
    {
        FMinimaxState SS = Initial;
        ApplyPawnMove(SS, root, mv.X, mv.Y);

        int32 d1 = 0, d2 = 0;
        ComputePathToGoal(SS, 1, &d1);
        ComputePathToGoal(SS, 2, &d2);

        int32 v = Minimax(SS, Depth, false);

        UE_LOG(LogTemp, Warning, TEXT("[CAND] Move(%d,%d): d1=%d d2=%d => MinimaxScore=%d"),
            mv.X, mv.Y, d1, d2, v);

        if (v > bestAct.Score)
        {
            bestAct.bIsWall = false;
            bestAct.MoveX = mv.X;
            bestAct.MoveY = mv.Y;
            bestAct.Score = v;
        }
    }

    // === WALL MOVES (targeted near opponent) ===
    if (Initial.WallsRemaining[idx] > 0)
    {
        for (auto w : GetTargetedWallPlacements(Initial, root))
        {
            FMinimaxState SS = Initial;
            ApplyWall(SS, root, w);

            int32 beforeLen = 0, afterLen = 0;
            ComputePathToGoal(Initial, Opponent, &beforeLen);
            ComputePathToGoal(SS, Opponent, &afterLen);

            int delta = afterLen - beforeLen;
            int v = Minimax(SS, Depth, false);

            v += delta * 10;
            if (!w.bHorizontal && w.Y >= 7) v += 5;
            if ( w.bHorizontal && w.Y >= 7) v += 8;

            UE_LOG(LogTemp, Warning, TEXT("[CAND] Wall@(%d,%d)%s => Score=%d (Δ=%d)"),
                w.X, w.Y, w.bHorizontal ? TEXT("H") : TEXT("V"), v, delta);

            if (v > bestAct.Score)
            {
                bestAct.bIsWall = true;
                bestAct.SlotX = w.X;
                bestAct.SlotY = w.Y;
                bestAct.WallLength = w.Length;
                bestAct.bHorizontal = w.bHorizontal;
                bestAct.Score = v;
            }
        }
    }

    return bestAct;
}




