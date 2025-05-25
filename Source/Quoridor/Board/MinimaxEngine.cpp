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

            UE_LOG(LogTemp, Warning, TEXT("Player %d Pawn at (%d, %d)"), player, S.PawnX[idx], S.PawnY[idx]);
        }
        else
        {
            S.PawnX[idx] = -1;
            S.PawnY[idx] = -1;
            UE_LOG(LogTemp, Error, TEXT("Player %d pawn is missing or not on tile!"), player);
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

            int32 L1 = P->GetWallCountOfLength(1);
            int32 L2 = P->GetWallCountOfLength(2);
            int32 L3 = P->GetWallCountOfLength(3);
            int32 Total = L1 + L2 + L3;

            UE_LOG(LogTemp, Warning, TEXT("Player %d Wall Inventory: L1=%d, L2=%d, L3=%d (Total=%d)"),
                player, L1, L2, L3, Total);
        }
        else
        {
            S.WallsRemaining[idx] = 0;
        }
    }

    // === Horizontal Walls ===
    for (AWallSlot* Slot : Board->HorizontalWallSlots)
    {
        if (Slot && Slot->bIsOccupied)
        {
            int x = Slot->GridX;
            int y = Slot->GridY;
            if (x >= 0 && x <= 8 && y >= 0 && y < 9)
            {
                S.HorizontalBlocked[y][x] = true;
                UE_LOG(LogTemp, Warning, TEXT("Occupied HWall at (%d, %d)"), x, y);
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
            if (x >= 0 && x < 9 && y >= 0 && y <= 8)
            {
                S.VerticalBlocked[y][x] = true;
                UE_LOG(LogTemp, Warning, TEXT("Occupied VWall at (%d, %d)"), x, y);
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
    const int goalY = (PlayerNum == 1 ? 8 : 0);
    const int idx = PlayerNum - 1;

    int sx = S.PawnX[idx];
    int sy = S.PawnY[idx];

    // === Validate pawn position ===
    if (sx < 0 || sx > 8 || sy < 0 || sy > 8)
    {
        UE_LOG(LogTemp, Error, TEXT("ComputePathToGoal: Invalid pawn position for Player %d => (%d, %d)"),
               PlayerNum, sx, sy);
        if (OutLength) *OutLength = 100;
        return {};
    }

    struct Node { int f, g, x, y; FIntPoint parent; };
    struct Cmp { bool operator()(const Node& a, const Node& b) const { return a.f > b.f; } };

    std::priority_queue<Node, std::vector<Node>, Cmp> open;
    bool closed[9][9] = {};
    int gCost[9][9];
    FIntPoint cameFrom[9][9];

    for (int y = 0; y < 9; ++y)
    {
        for (int x = 0; x < 9; ++x)
        {
            gCost[y][x] = INT_MAX;
            cameFrom[y][x] = FIntPoint(-1, -1);
        }
    }

    auto Heuristic = [&](int x, int y) { return FMath::Abs(goalY - y); };

    gCost[sy][sx] = 0;
    open.push({ Heuristic(sx, sy), 0, sx, sy, FIntPoint(-1, -1) });

    const FIntPoint dirs[4] = { {1,0}, {-1,0}, {0,1}, {0,-1} };

    while (!open.empty())
    {
        Node n = open.top(); open.pop();
        if (closed[n.y][n.x]) continue;
        closed[n.y][n.x] = true;
        cameFrom[n.y][n.x] = n.parent;

        if (n.y == goalY)
        {
            if (OutLength) *OutLength = n.g;

            // Reconstruct path
            TArray<FIntPoint> Path;
            int cx = n.x, cy = n.y;
            while (cx != -1 && cy != -1)
            {
                Path.Insert(FIntPoint(cx, cy), 0);
                FIntPoint p = cameFrom[cy][cx];
                cx = p.X;
                cy = p.Y;
            }
            return Path;
        }

        for (const auto& d : dirs)
        {
            int nx = n.x + d.X;
            int ny = n.y + d.Y;
            if (nx < 0 || nx > 8 || ny < 0 || ny > 8) continue;

            // Check walls
            if (d.X == 1 && S.VerticalBlocked[n.y][n.x]) continue;
            if (d.X == -1 && S.VerticalBlocked[n.y][nx]) continue;
            if (d.Y == 1 && S.HorizontalBlocked[n.y][n.x]) continue;
            if (d.Y == -1 && S.HorizontalBlocked[ny][n.x]) continue;

            int ng = n.g + 1;
            if (ng < gCost[ny][nx])
            {
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



int32 MinimaxEngine::Evaluate(const FMinimaxState& S, int32 RootPlayer)
{
    int32 idxAI = RootPlayer - 1;
    int32 idxOpponent = 1 - idxAI; // 0 or 1

    int32 AILen = 0;
    int32 OppLen = 0;

    ComputePathToGoal(S, RootPlayer, &AILen);
    ComputePathToGoal(S, 3 - RootPlayer, &OppLen);

    int32 Score = (OppLen - AILen) * 10;

    // Bonus: Walls near opponent's goal row
    int oppGoalRow = (RootPlayer == 1) ? 8 : 0;
    int wallRowIdx = FMath::Clamp(oppGoalRow - (RootPlayer == 1 ? 1 : 0), 0, 8);

    for (int x = 0; x < 8; ++x)
    {
        if (S.HorizontalBlocked[wallRowIdx][x]) Score += 4;
    }

    for (int x = 0; x < 9; ++x)
    {
        if (S.VerticalBlocked[wallRowIdx][x]) Score += 2;
    }

    Score += (S.WallsRemaining[idxAI] - S.WallsRemaining[idxOpponent]) * 2;

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
        }
        else
        {
            int jx = nx + d.X;
            int jy = ny + d.Y;

            if (CanStep(nx, ny, jx, jy) && !IsOccupied(jx, jy))
            {
                Out.Add({ jx, jy });
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

        for (int PlayerNum = 1; PlayerNum <= 2; ++PlayerNum)
        {
            if (ComputePathToGoal(State, PlayerNum).Num() == 0)
                return false;
        }
        return true;

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

int32 MinimaxEngine::Minimax(FMinimaxState& S, int32 Depth, int32 RootPlayer, int32 CurrentPlayer)
{
    if (Depth <= 0)
        return Evaluate(S, RootPlayer);

    const bool maximize = (CurrentPlayer == RootPlayer);

    int32 best = maximize ? INT_MIN : INT_MAX;
    int opponent = 3 - CurrentPlayer;
    int idx = CurrentPlayer - 1;

    // --- Pawn moves ---
    for (auto mv : GetPawnMoves(S, CurrentPlayer))
    {
        FMinimaxState SS = S;
        ApplyPawnMove(SS, CurrentPlayer, mv.X, mv.Y);
        int32 v = Minimax(SS, Depth - 1, RootPlayer, 3 - CurrentPlayer);
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

            for (const auto& w : GetWallPlacements(S, CurrentPlayer, AvailableLengths))
            {
                if (!WallTouchesPath(w, beforePath))
                    continue;

                FMinimaxState SS = S;
                ApplyWall(SS, CurrentPlayer, w);

                int32 afterLen = 0;
                ComputePathToGoal(SS, opponent, &afterLen);
                int delta = afterLen - beforeLen;

                int distToOpponent = FMath::Abs(w.X - oppX) + FMath::Abs(w.Y - oppY);
                if (delta <= 0 && distToOpponent > 2)
                    continue;

                int32 v = Minimax(SS, Depth - 1, RootPlayer, 3 - CurrentPlayer);
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

    int32 AILength = 0, OppLength = 0;
    ComputePathToGoal(Initial, RootPlayer, &AILength);
    ComputePathToGoal(Initial, Opponent, &OppLength);

    UE_LOG(LogTemp, Warning, TEXT("AI Path: %d | Opponent Path: %d"), AILength, OppLength);

    // === PAWN MOVES ===
    int32 beforeLen = AILength;
    for (const auto& mv : GetPawnMoves(Initial, RootPlayer))
    {
        FMinimaxState SS = Initial;
        ApplyPawnMove(SS, RootPlayer, mv.X, mv.Y);

        int32 afterLen = 0;
        ComputePathToGoal(SS, RootPlayer, &afterLen);
        int delta = beforeLen - afterLen;

        bool bIsJump = false;
        int dx = FMath::Abs(mv.X - Initial.PawnX[idx]);
        int dy = FMath::Abs(mv.Y - Initial.PawnY[idx]);
        if ((dx == 2 && dy == 0) || (dy == 2 && dx == 0))
        {
            bIsJump = true;
        }

        int32 v = Minimax(SS, Depth, RootPlayer, Opponent);
        v += delta * 15;
        if (bIsJump) v += 10;

        UE_LOG(LogTemp, Warning, TEXT("[CAND] Move to (%d,%d) => Score=%d (Δ=%d)%s"),
            mv.X, mv.Y, v, delta, bIsJump ? TEXT(" [JUMP]") : TEXT(""));

        if (v > bestAct.Score ||
            (v == bestAct.Score && mv.Y > bestAct.MoveY) ||
            (v == bestAct.Score && mv.Y == bestAct.MoveY && mv.X < bestAct.MoveX))
        {
            bestAct = FMinimaxAction{ false, mv.X, mv.Y, 0, false, v };
        }
    }

    // === WALL MOVES ===
    if (Initial.WallsRemaining[idx] > 0)
    {
        const TArray<int32>& AvailableLengths = Initial.AvailableWallLengthsForPlayer[idx];
        TArray<FWallData> WallCandidates = GetTargetedWallPlacements(Initial, RootPlayer, AvailableLengths);

        for (const auto& w : WallCandidates)
        {
            if (!IsWallLegal(Initial, w)) continue;

            FMinimaxState SS = Initial;
            ApplyWall(SS, RootPlayer, w);

            int32 OppBefore = 0, OppAfter = 0;
            int32 SelfBefore = 0, SelfAfter = 0;

            ComputePathToGoal(Initial, Opponent, &OppBefore);
            ComputePathToGoal(Initial, RootPlayer, &SelfBefore);

            ComputePathToGoal(SS, Opponent, &OppAfter);
            ComputePathToGoal(SS, RootPlayer, &SelfAfter);

            if (OppAfter >= 100) continue;

            int32 OppDelta = OppAfter - OppBefore;
            int32 SelfDelta = SelfAfter - SelfBefore;
            int32 NetGain = OppDelta - SelfDelta;

            int32 v = Minimax(SS, Depth, RootPlayer, Opponent);
            v += NetGain * 12; // Strategic scoring based on path impact

            // Positional bonuses
            if (!w.bHorizontal && w.Y >= 7) v += 5;
            if (w.bHorizontal && w.Y >= 7) v += 8;

            UE_LOG(LogTemp, Warning, TEXT("[CAND] Wall@(%d,%d)%s L=%d => Score=%d (NetGain=%d)"),
                w.X, w.Y, w.bHorizontal ? TEXT("H") : TEXT("V"), w.Length, v, NetGain);

            if (v > bestAct.Score)
            {
                bestAct = FMinimaxAction{ true, w.X, w.Y, w.Length, w.bHorizontal, v };
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

    int32 AILength = 0, OppLength = 0;
    ComputePathToGoal(Initial, RootPlayer, &AILength);
    ComputePathToGoal(Initial, Opponent, &OppLength);

    UE_LOG(LogTemp, Warning, TEXT("AI Path: %d | Opponent Path: %d"), AILength, OppLength);

    // === Generate pawn move candidates ===
    for (const auto& mv : GetPawnMoves(Initial, RootPlayer))
    {
        Candidates.Add(FMinimaxAction{
            .bIsWall = false,
            .MoveX = mv.X,
            .MoveY = mv.Y
        });
    }

    // === Generate wall move candidates ===
    if (Initial.WallsRemaining[idx] > 0)
    {
        const TArray<int32>& AvailableLengths = Initial.AvailableWallLengthsForPlayer[idx];
        TArray<FWallData> WallMoves = GetTargetedWallPlacements(Initial, RootPlayer, AvailableLengths);
        for (const auto& w : WallMoves)
        {
            Candidates.Add(FMinimaxAction{
                .bIsWall = true,
                .SlotX = w.X,
                .SlotY = w.Y,
                .WallLength = w.Length,
                .bHorizontal = w.bHorizontal
            });
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
            if (!IsWallLegal(Initial, w)) return;

            ApplyWall(SS, RootPlayer, w);

            int32 OppBefore = 0, OppAfter = 0;
            int32 SelfBefore = 0, SelfAfter = 0;
            ComputePathToGoal(Initial, Opponent, &OppBefore);
            ComputePathToGoal(Initial, RootPlayer, &SelfBefore);
            ComputePathToGoal(SS, Opponent, &OppAfter);
            ComputePathToGoal(SS, RootPlayer, &SelfAfter);

            if (OppAfter >= 100) return;

            int OppDelta = OppAfter - OppBefore;
            int SelfDelta = SelfAfter - SelfBefore;
            int NetGain = OppDelta - SelfDelta;

            score = Minimax(SS, Depth, RootPlayer, Opponent);
            score += NetGain * 12;

            // Positional bonus (like in Solve)
            if (!w.bHorizontal && w.Y >= 7) score += 5;
            if (w.bHorizontal && w.Y >= 7) score += 8;
        }
        else
        {
            ApplyPawnMove(SS, RootPlayer, act.MoveX, act.MoveY);

            int32 afterLen = 0;
            ComputePathToGoal(SS, RootPlayer, &afterLen);
            int delta = AILength - afterLen;

            bool bIsJump = false;
            int dx = FMath::Abs(act.MoveX - Initial.PawnX[idx]);
            int dy = FMath::Abs(act.MoveY - Initial.PawnY[idx]);
            if ((dx == 2 && dy == 0) || (dy == 2 && dx == 0))
                bIsJump = true;

            score = Minimax(SS, Depth, RootPlayer, Opponent);
            score += delta * 15;
            if (bIsJump) score += 10;
        }

        Scores[i] = score;
    });

    // === Select best move ===
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

    // === Final legality check for wall ===
    if (BestAct.bIsWall)
    {
        FWallData FinalWall{ BestAct.SlotX, BestAct.SlotY, BestAct.WallLength, BestAct.bHorizontal };
        if (!IsWallLegal(Initial, FinalWall))
        {
            UE_LOG(LogTemp, Warning, TEXT("[SolveParallel] Best wall (%d,%d)%s is now illegal — falling back"),
                BestAct.SlotX, BestAct.SlotY, BestAct.bHorizontal ? TEXT("H") : TEXT("V"));

            // Optional fallback: choose best pawn move instead
            BestAct = FMinimaxAction{};
            BestAct.Score = INT_MIN;

            for (int32 i = 0; i < Candidates.Num(); ++i)
            {
                if (!Candidates[i].bIsWall && Scores[i] > BestAct.Score)
                {
                    BestAct = Candidates[i];
                    BestAct.Score = Scores[i];
                }
            }
        }
    }

    return BestAct;
}



















