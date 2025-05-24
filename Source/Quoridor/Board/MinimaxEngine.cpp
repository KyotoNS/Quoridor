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

    // Clear wall blocks
    for (int y = 0; y < 9; ++y)
        for (int x = 0; x < 8; ++x)
            S.HorizontalBlocked[y][x] = false;

    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 9; ++x)
            S.VerticalBlocked[y][x] = false;

    // === P1 & P2 Setup ===
    for (int player = 1; player <= 2; ++player)
    {
        AQuoridorPawn* P = Board->GetPawnForPlayer(player);
        int idx = player - 1;

        if (P && P->GetTile())
        {
            S.PawnX[idx] = P->GetTile()->GridX;
            S.PawnY[idx] = P->GetTile()->GridY;
        }

        if (P)
        {
            S.WallsRemaining[idx] = P->PlayerWalls.Num();

            for (int len = 1; len <= 3; ++len)
            {
                if (P->GetWallCountOfLength(len) > 0)
                {
                    S.AvailableWallLengthsForPlayer[idx].Add(len);
                }
            }

            // Log wall inventory for each player
            int32 L1 = P->GetWallCountOfLength(1);
            int32 L2 = P->GetWallCountOfLength(2);
            int32 L3 = P->GetWallCountOfLength(3);
            int32 Total = L1 + L2 + L3;

            UE_LOG(LogTemp, Warning, TEXT("Player %d Wall Inventory: L1=%d, L2=%d, L3=%d (Total=%d)"),
                player, L1, L2, L3, Total);
        }
    }

    // === Horizontal Walls ===
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

    // === Vertical Walls ===
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

    auto IsOccupied = [&](int tx, int ty) -> bool
    {
        return (S.PawnX[0] == tx && S.PawnY[0] == ty) || (S.PawnX[1] == tx && S.PawnY[1] == ty);
    };

    auto CanStep = [&](int ax, int ay, int bx, int by)
    {
        if (bx < 0 || bx > 8 || by < 0 || by > 8)
            return false;

        if (bx == ax + 1 && S.VerticalBlocked[ay][ax]) return false;
        if (bx == ax - 1 && ax - 1 >= 0 && S.VerticalBlocked[ay][ax - 1]) return false;
        if (by == ay + 1 && S.HorizontalBlocked[ay][ax]) return false;
        if (by == ay - 1 && ay - 1 >= 0 && S.HorizontalBlocked[ay - 1][ax]) return false;

        return true;
    };

    const FIntPoint dirs[4] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1} };

    for (const auto& d : dirs)
    {
        int nx = x + d.X;
        int ny = y + d.Y;

        if (!CanStep(x, y, nx, ny))
            continue;

        if (!IsOccupied(nx, ny))
        {
            Out.Add({ nx, ny });
            UE_LOG(LogTemp, Warning, TEXT("Generated Move: Normal (%d,%d)"), nx, ny);
        }
        else
        {
            int jx = nx + d.X;
            int jy = ny + d.Y;

            if (CanStep(nx, ny, jx, jy) && !IsOccupied(jx, jy))
            {
                Out.Add({ jx, jy });
                UE_LOG(LogTemp, Warning, TEXT("Generated Move: Jump (%d,%d)"), jx, jy);
            }
            else
            {
                FIntPoint perp[2] = { { d.Y, d.X }, { -d.Y, -d.X } };

                for (const auto& p : perp)
                {
                    int sx = nx + p.X;
                    int sy = ny + p.Y;

                    if (CanStep(nx, ny, sx, sy) && !IsOccupied(sx, sy))
                    {
                        Out.Add({ sx, sy });
                        UE_LOG(LogTemp, Warning, TEXT("Generated Move: Side (%d,%d)"), sx, sy);
                    }
                }
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("Total pawn moves generated for Player %d: %d"), PlayerNum, Out.Num());
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

    const int32 Opponent = 3 - PlayerNum;
    const int oppX = S.PawnX[Opponent - 1];
    const int oppY = S.PawnY[Opponent - 1];

    int32 beforeDist = 0;
    TArray<FIntPoint> OpponentPath = ComputePathToGoal(S, Opponent, &beforeDist);

    auto IsWallLegal = [](const FMinimaxState& State, const FWallData& W)
    {
        for (int offset = 0; offset < W.Length; ++offset)
        {
            int x = W.bHorizontal ? W.X + offset : W.X;
            int y = W.bHorizontal ? W.Y : W.Y + offset;

            if (x < 0 || x >= 9 || y < 0 || y >= 9)
                return false;

            if (W.bHorizontal && (x >= 8 || State.HorizontalBlocked[y][x]))
                return false;

            if (!W.bHorizontal && (y >= 8 || State.VerticalBlocked[y][x]))
                return false;
        }
        return true;
    };

    auto SimulateWall = [](FMinimaxState& State, const FWallData& W) -> bool
    {
        for (int offset = 0; offset < W.Length; ++offset)
        {
            int x = W.bHorizontal ? W.X + offset : W.X;
            int y = W.bHorizontal ? W.Y : W.Y + offset;

            if (x >= 9 || y >= 9)
                return false;

            if (W.bHorizontal)
                State.HorizontalBlocked[y][x] = true;
            else
                State.VerticalBlocked[y][x] = true;
        }

        return ComputePathToGoal(State, 1).Num() > 0 && ComputePathToGoal(State, 2).Num() > 0;
    };

    auto IsWallUseful = [&](const FMinimaxState& SimState, const FWallData& W)
    {
        int32 afterDist = 0;
        ComputePathToGoal(SimState, Opponent, &afterDist);
        int delta = afterDist - beforeDist;

        int distToOpponent = FMath::Abs(W.X - oppX) + FMath::Abs(W.Y - oppY);
        bool bTouchesPath = WallTouchesPath(W, OpponentPath);
        bool bIsNearPawn = (distToOpponent <= 2);

        if (!bTouchesPath && !bIsNearPawn)
            return false;

        if (delta <= 0 && distToOpponent > 2)
            return false;

        return true;
    };

    // === Horizontal Walls ===
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
                if (!IsWallUseful(SS, W)) continue;

                Walls.Add(W);
            }
        }
    }

    // === Vertical Walls ===
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
                if (!IsWallUseful(SS, W)) continue;

                Walls.Add(W);
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

    if (AvailableLengths.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("Player %d has no available wall lengths. Returning empty."), PlayerNum);
        return Results;
    }

    int32 beforeLen = 0;
    TArray<FIntPoint> pathBefore = ComputePathToGoal(S, Opponent, &beforeLen);

    struct ScoredWall
    {
        FWallData Wall;
        int Delta;
    };
    TArray<ScoredWall> Candidates;

    auto SimulateAndScore = [&](const FWallData& W) -> bool
    {
        if (!IsWallLegal(S, W))
            return false;

        FMinimaxState Simulated = S;

        for (int i = 0; i < W.Length; ++i)
        {
            int x = W.bHorizontal ? W.X + i : W.X;
            int y = W.bHorizontal ? W.Y : W.Y + i;

            if (x >= 9 || y >= 9) return false;

            if (W.bHorizontal)
                Simulated.HorizontalBlocked[y][x] = true;
            else
                Simulated.VerticalBlocked[y][x] = true;
        }

        if (ComputePathToGoal(Simulated, PlayerNum).Num() == 0)
            return false;

        int32 afterLen = 0;
        TArray<FIntPoint> afterPath = ComputePathToGoal(Simulated, Opponent, &afterLen);
        if (afterPath.Num() == 0) return false;

        int delta = afterLen - beforeLen;
        if (delta <= 0) return false;

        Candidates.Add({W, delta});
        return true;
    };

    // Try placing walls of each allowed length near the opponent's pawn
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

                // Try horizontal
                if (cx + len - 1 < 9)
                {
                    FWallData W = {cx, cy, len, true};
                    SimulateAndScore(W);
                }

                // Try vertical
                if (cy + len - 1 < 9)
                {
                    FWallData W = {cx, cy, len, false};
                    SimulateAndScore(W);
                }
            }
        }
    }

    // Sort by highest delta first (more delay = better)
    Candidates.Sort([](const ScoredWall& A, const ScoredWall& B)
    {
        return A.Delta > B.Delta;
    });

    for (const auto& Entry : Candidates)
    {
        Results.Add(Entry.Wall);
    }

    UE_LOG(LogTemp, Warning, TEXT("GetTargetedWallPlacements: Returning %d walls"), Results.Num());
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

FMinimaxAction MinimaxEngine::Solve(const FMinimaxState& Initial, int32 Depth, int32 RootPlayer)
{
    UE_LOG(LogTemp, Warning, TEXT("=== MinimaxEngine::Solve | Root=%d | Depth=%d ==="), RootPlayer, Depth);

    FMinimaxAction bestAct;
    bestAct.Score = INT_MIN;

    const int idx = RootPlayer - 1;
    const int Opponent = 3 - RootPlayer;

    // === Step 1: Check if AI is behind in path length ===
    int32 AILength = 0, OppLength = 0;
    ComputePathToGoal(Initial, RootPlayer, &AILength);
    ComputePathToGoal(Initial, Opponent, &OppLength);
    bool bBehind = (AILength > OppLength);

    UE_LOG(LogTemp, Warning, TEXT("AI Path: %d | Opponent Path: %d | Behind? %s"),
        AILength, OppLength, bBehind ? TEXT("YES") : TEXT("NO"));

    // === PAWN MOVES ===
    {
        int32 beforeLen = 0;
        ComputePathToGoal(Initial, RootPlayer, &beforeLen);

        for (const auto& mv : GetPawnMoves(Initial, RootPlayer))
        {
            FMinimaxState SS = Initial;
            ApplyPawnMove(SS, RootPlayer, mv.X, mv.Y);

            int32 afterLen = 0;
            ComputePathToGoal(SS, RootPlayer, &afterLen);

            int delta = beforeLen - afterLen;
            int32 v = Minimax(SS, Depth, false);
            v += delta * 15;

            // Step 2: Penalize pawn moves slightly if AI is behind
            if (bBehind)
            {
                v -= 5;
            }

            UE_LOG(LogTemp, Warning, TEXT("[CAND] Move to (%d,%d) => Score=%d (Δ=%d)"),
                mv.X, mv.Y, v, delta);

            if (v > bestAct.Score ||
                (v == bestAct.Score && mv.Y > bestAct.MoveY) ||
                (v == bestAct.Score && mv.Y == bestAct.MoveY && mv.X < bestAct.MoveX))
            {
                bestAct = FMinimaxAction{
                    .bIsWall = false,
                    .MoveX = mv.X,
                    .MoveY = mv.Y,
                    .Score = v
                };
            }
        }
    }

    // === WALL MOVES ===
    if (Initial.WallsRemaining[idx] > 0)
    {
        const TArray<int32>& AvailableLengths = Initial.AvailableWallLengthsForPlayer[idx];
        TArray<FWallData> WallCandidates = GetTargetedWallPlacements(Initial, RootPlayer, AvailableLengths);

        UE_LOG(LogTemp, Warning, TEXT("=== Raw Wall Candidates (%d total) ==="), WallCandidates.Num());
        for (const auto& w : WallCandidates)
        {
            UE_LOG(LogTemp, Warning, TEXT("Candidate Wall: (%d,%d) %s L=%d"),
                w.X, w.Y, w.bHorizontal ? TEXT("H") : TEXT("V"), w.Length);
        }

        if (WallCandidates.Num() == 0)
        {
            UE_LOG(LogTemp, Warning, TEXT("No wall candidates generated. Skipping wall move."));
        }

        int32 beforeLen = 0;
        ComputePathToGoal(Initial, Opponent, &beforeLen);

        for (const auto& w : WallCandidates)
        {
            if (!IsWallLegal(Initial, w))
            {
                UE_LOG(LogTemp, Warning, TEXT("[SKIP] Illegal wall candidate (%d,%d)L%d %s"),
                    w.X, w.Y, w.Length, w.bHorizontal ? TEXT("H") : TEXT("V"));
                continue;
            }

            FMinimaxState SS = Initial;
            ApplyWall(SS, RootPlayer, w);

            int32 afterLen = 0;
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

            // Step 3: Boost wall scoring if AI is behind
            if (bBehind)
            {
                v += 8;
            }

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


FMinimaxAction MinimaxEngine::SolveParallel(const FMinimaxState& Initial, int32 Depth, int32 RootPlayer)
{
    UE_LOG(LogTemp, Warning, TEXT("=== SolveParallel: Root Player = %d | Depth = %d ==="), RootPlayer, Depth);

    const int idx = RootPlayer - 1;
    const int Opponent = 3 - RootPlayer;

    TArray<FMinimaxAction> Candidates;

    // === Compute initial path lengths ===
    int32 AILength = 0, OppLength = 0;
    ComputePathToGoal(Initial, RootPlayer, &AILength);
    ComputePathToGoal(Initial, Opponent, &OppLength);
    bool bBehind = (AILength > OppLength);

    UE_LOG(LogTemp, Warning, TEXT("AI Path: %d | Opponent Path: %d | Behind? %s"),
        AILength, OppLength, bBehind ? TEXT("YES") : TEXT("NO"));

    // === Collect pawn move candidates ===
    int32 pawnBeforeLen = AILength;
    for (const auto& mv : GetPawnMoves(Initial, RootPlayer))
    {
        FMinimaxAction act;
        act.bIsWall = false;
        act.MoveX = mv.X;
        act.MoveY = mv.Y;
        Candidates.Add(act);
    }

    // === Collect wall move candidates ===
    int32 wallBeforeLen = OppLength;

    if (Initial.WallsRemaining[idx] > 0)
    {
        const TArray<int32>& AvailableLengths = Initial.AvailableWallLengthsForPlayer[idx];
        TArray<FWallData> WallMoves = GetTargetedWallPlacements(Initial, RootPlayer, AvailableLengths);

        for (const auto& w : WallMoves)
        {
            FMinimaxAction act;
            act.bIsWall = true;
            act.SlotX = w.X;
            act.SlotY = w.Y;
            act.WallLength = w.Length;
            act.bHorizontal = w.bHorizontal;
            Candidates.Add(act);
        }
    }

    // === Parallel scoring ===
    TArray<int32> Scores;
    Scores.SetNumZeroed(Candidates.Num());

    ParallelFor(Candidates.Num(), [&](int32 i)
    {
        const FMinimaxAction& act = Candidates[i];
        FMinimaxState SS = Initial;
        int32 score = INT_MIN;

        if (act.bIsWall)
        {
            FWallData w{ act.SlotX, act.SlotY, act.WallLength, act.bHorizontal };

            if (!IsWallLegal(Initial, w))
                return;

            ApplyWall(SS, RootPlayer, w);

            int32 afterLen = 0;
            ComputePathToGoal(SS, Opponent, &afterLen);
            if (afterLen >= 100)
                return;

            int delta = afterLen - wallBeforeLen;
            score = Minimax(SS, Depth, false) + delta * 10;

            // Step 3: Boost wall if AI is behind
            if (bBehind)
            {
                score += 8;
            }

            // Positional bonus
            if (!w.bHorizontal && w.Y >= 7) score += 5;
            if ( w.bHorizontal && w.Y >= 7) score += 8;
        }
        else
        {
            ApplyPawnMove(SS, RootPlayer, act.MoveX, act.MoveY);

            int32 afterLen = 0;
            ComputePathToGoal(SS, RootPlayer, &afterLen);
            int delta = pawnBeforeLen - afterLen;

            score = Minimax(SS, Depth, false) + delta * 15;

            // Step 2: Penalize pawn move if AI is behind
            if (bBehind)
            {
                score -= 5;
            }
        }

        Scores[i] = score;
    });

    // === Pick the best move ===
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

    return BestAct;
}













