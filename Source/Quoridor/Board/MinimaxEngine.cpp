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

    for (int y = 0; y < 9; ++y) for (int x = 0; x < 8; ++x) S.HorizontalBlocked[y][x] = false;
    for (int y = 0; y < 8; ++y) for (int x = 0; x < 9; ++x) S.VerticalBlocked[y][x] = false;

    for (int player = 1; player <= 2; ++player)
    {
        AQuoridorPawn* P = Board->GetPawnForPlayer(player);
        int idx = player - 1;

        if (P && P->GetTile()) {
            S.PawnX[idx] = P->GetTile()->GridX; S.PawnY[idx] = P->GetTile()->GridY;
        } else {
            S.PawnX[idx] = -1; S.PawnY[idx] = -1;
            UE_LOG(LogTemp, Error, TEXT("Player %d pawn missing"), player);
        }

        if (P) {
            S.WallCounts[idx][0] = P->GetWallCountOfLength(1);
            S.WallCounts[idx][1] = P->GetWallCountOfLength(2);
            S.WallCounts[idx][2] = P->GetWallCountOfLength(3);
            S.WallsRemaining[idx] = S.WallCounts[idx][0] + S.WallCounts[idx][1] + S.WallCounts[idx][2];
        } else {
            S.WallsRemaining[idx] = 0;
            S.WallCounts[idx][0] = 0; S.WallCounts[idx][1] = 0; S.WallCounts[idx][2] = 0;
        }
    }

    for (AWallSlot* Slot : Board->HorizontalWallSlots) {
        if (Slot && Slot->bIsOccupied) {
            int x = Slot->GridX; int y = Slot->GridY;
            if (x >= 0 && x < 8 && y >= 0 && y < 9) S.HorizontalBlocked[y][x] = true;
        }
    }
    for (AWallSlot* Slot : Board->VerticalWallSlots) {
        if (Slot && Slot->bIsOccupied) {
            int x = Slot->GridX; int y = Slot->GridY;
            if (x >= 0 && x < 9 && y >= 0 && y < 8) S.VerticalBlocked[y][x] = true;
        }
    }
    return S;
}

//-----------------------------------------------------------------------------
// ComputePathToGoal (A* with Jumps)
//-----------------------------------------------------------------------------
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


//-----------------------------------------------------------------------------
// Wall Legality Check
//-----------------------------------------------------------------------------
bool MinimaxEngine::IsWallPlacementStrictlyLegal(const FMinimaxState& S, const FWallData& W)
{
    // Check bounds
    if (W.bHorizontal) {
        if (W.X < 0 || W.X + W.Length - 1 > 7 || W.Y < 0 || W.Y > 8) return false;
    } else {
        if (W.X < 0 || W.X > 8 || W.Y < 0 || W.Y + W.Length - 1 > 7) return false;
    }

    // Check overlaps and intersections
    for (int i = 0; i < W.Length; ++i)
    {
        int cx = W.bHorizontal ? W.X + i : W.X;
        int cy = W.bHorizontal ? W.Y : W.Y + i;

        if (W.bHorizontal) {
            if (S.HorizontalBlocked[cy][cx]) return false; // Overlap H
            // Check intersection (center point 'cross')
            // If V[y-1][cx] and V[y][cx] both exist, it would cross.
            // Requires Y>0 and Y<8 for V walls. Simplified: Check if V exists *at* cx,cy.
            if (S.VerticalBlocked[cy > 0 ? cy - 1 : 0][cx] && S.VerticalBlocked[cy][cx]) return false;

        } else { // Vertical
            if (S.VerticalBlocked[cy][cx]) return false; // Overlap V
            // Check intersection: If H[cy][cx-1] and H[cy][cx] both exist.
            if (S.HorizontalBlocked[cy][cx > 0 ? cx - 1 : 0] && S.HorizontalBlocked[cy][cx]) return false;
        }
    }

    // Check intersection with other end (only needed for length > 1, but check anyway)
    // A more robust check might be needed depending on how walls intersect.
    // This basic check prevents direct overlap and simple crosses.

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

    // --- ADD COUNTERS ---
    int32 HorizontalWallCount = 0;
    int32 VerticalWallCount = 0;
    // --- END ADD COUNTERS ---

    // --- Horizontal Walls ---
    // Iterate through S.HorizontalBlocked[y][x] (9 rows, 8 slots per row)
    for (int y = 0; y < 9; ++y)
    {
        for (int x = 0; x < 8; ++x)
        {
            if (S.HorizontalBlocked[y][x])
            {
                UE_LOG(LogTemp, Warning, TEXT("  H-Wall @ (X=%d, Y=%d)"), x, y);
                HorizontalWallCount++; // Increment count
            }
        }
    }

    // --- Vertical Walls ---
    // Iterate through S.VerticalBlocked[y][x] (8 rows, 9 slots per row)
    for (int y = 0; y < 8; ++y)
    {
        for (int x = 0; x < 9; ++x)
        {
            if (S.VerticalBlocked[y][x])
            {
                UE_LOG(LogTemp, Warning, TEXT("  V-Wall @ (X=%d, Y=%d)"), x, y);
                VerticalWallCount++; // Increment count
            }
        }
    }

    // --- Print Totals ---
    if (HorizontalWallCount == 0 && VerticalWallCount == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("  No walls found on board."));
    }
    else
    {
        // Print the total counts
        UE_LOG(LogTemp, Warning, TEXT("  Total Horizontal Segments: %d"), HorizontalWallCount);
        UE_LOG(LogTemp, Warning, TEXT("  Total Vertical Segments:   %d"), VerticalWallCount);
        UE_LOG(LogTemp, Warning, TEXT("  Overall Total Segments:    %d"), HorizontalWallCount + VerticalWallCount);
    }
    // --- End Print Totals ---

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

    for (int length : AvailableLengths)
    {
        // Horizontal Walls
        for (int y = 0; y < 8; ++y) {
            for (int x = 0; x <= 8 - length; ++x) {
                FWallData W{ x, y, length, true };
                if (IsWallPlacementStrictlyLegal(S, W)) {
                    FMinimaxState SimulatedState = S;
                    ApplyWall(SimulatedState, PlayerNum, W);

                    if (!DoesWallBlockPlayer(SimulatedState)) {
                        int32 NewMyPathLen = 100, NewOppPathLen = 100;
                        ComputePathToGoal(SimulatedState, PlayerNum, &NewMyPathLen);
                        ComputePathToGoal(SimulatedState, Opponent, &NewOppPathLen);
                        int32 OppDelta = NewOppPathLen - OppPathLen;
                        int32 MyDelta = NewMyPathLen - MyPathLen;
                        int32 WallScore = (OppDelta * 10) - (MyDelta * 15);

                        // *** ADDED CHECK HERE ***
                        check(W.Length > 0 && W.Length <= 3);
                        AllLegalWalls.Add({W, WallScore});
                    }
                }
            }
        }
        // Vertical Walls
        for (int y = 0; y <= 8 - length; ++y) {
            for (int x = 0; x < 8; ++x) {
                FWallData W{ x, y, length, false };
                if (IsWallPlacementStrictlyLegal(S, W)) {
                    FMinimaxState SimulatedState = S;
                    ApplyWall(SimulatedState, PlayerNum, W);

                    if (!DoesWallBlockPlayer(SimulatedState)) {
                        int32 NewMyPathLen = 100, NewOppPathLen = 100;
                        ComputePathToGoal(SimulatedState, PlayerNum, &NewMyPathLen);
                        ComputePathToGoal(SimulatedState, Opponent, &NewOppPathLen);
                        int32 OppDelta = NewOppPathLen - OppPathLen;
                        int32 MyDelta = NewMyPathLen - MyPathLen;
                        int32 WallScore = (OppDelta * 10) - (MyDelta * 15);

                        // *** ADDED CHECK HERE ***
                        check(W.Length > 0 && W.Length <= 3);
                        AllLegalWalls.Add({W, WallScore});
                    }
                }
            }
        }
    }

    AllLegalWalls.Sort([](const ScoredWall& A, const ScoredWall& B) {
        return A.Score > B.Score;
    });

    const int MaxWallCandidates = 25;
    for (int i = 0; i < AllLegalWalls.Num() && i < MaxWallCandidates; ++i) {
        if (AllLegalWalls[i].Score >= -10) {
             FinalCandidates.Add(AllLegalWalls[i].Wall);
        }
    }

    return FinalCandidates;
}


//-----------------------------------------------------------------------------
// Get Pawn Moves (Unchanged, but ensure it's robust)
//-----------------------------------------------------------------------------
TArray<FIntPoint> MinimaxEngine::GetPawnMoves(const FMinimaxState& S, int32 PlayerNum)
{
    TArray<FIntPoint> Out;
    TArray<FIntPoint> Temp;

    int idx = PlayerNum - 1;
    int oppIdx = 1 - idx;

    int x = S.PawnX[idx];
    int y = S.PawnY[idx];
    int ox = S.PawnX[oppIdx];
    int oy = S.PawnY[oppIdx];

    auto CanMove = [&](int ax, int ay, int bx, int by) -> bool {
        if (bx < 0 || bx > 8 || by < 0 || by > 8) return false;
        if (bx == ax + 1 && S.VerticalBlocked[ay][ax]) return false;
        if (bx == ax - 1 && ax > 0 && S.VerticalBlocked[ay][ax - 1]) return false;
        if (by == ay + 1 && S.HorizontalBlocked[ay][ax]) return false;
        if (by == ay - 1 && ay > 0 && S.HorizontalBlocked[ay - 1][ax]) return false;
        return true;
    };

    const FIntPoint dirs[4] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1} };

    for (const auto& d : dirs)
    {
        int nx = x + d.X;
        int ny = y + d.Y;
        if (!CanMove(x, y, nx, ny)) continue;

        if (nx != ox || ny != oy)
        {
            Temp.Add({ nx, ny });
        }
        else
        {
            // Direct jump
            int jx = nx + d.X;
            int jy = ny + d.Y;
            if (CanMove(nx, ny, jx, jy))
            {
                Temp.Add({ jx, jy });
            }
            else
            {
                // Side jumps
                FIntPoint perp[2] = { { d.Y, d.X }, { -d.Y, -d.X } };
                for (const auto& p : perp)
                {
                    int sideX = nx + p.X;
                    int sideY = ny + p.Y;
                    if (CanMove(nx, ny, sideX, sideY))
                    {
                        Temp.Add({ sideX, sideY });
                    }
                }
            }
        }
    }

    // Sort by vertical proximity to the goal row
    const int goalY = (PlayerNum == 1 ? 8 : 0);
    Temp.Sort([&](const FIntPoint& A, const FIntPoint& B) {
        int da = FMath::Abs(goalY - A.Y);
        int db = FMath::Abs(goalY - B.Y);
        return da < db; // prioritize closer to goal row
    });

    Out = Temp;
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

    int idxAI = RootPlayer - 1; int idxOpponent = 1 - idxAI;

    q1.push({S.PawnX[idxAI], S.PawnY[idxAI]});
    q2.push({S.PawnX[idxOpponent], S.PawnY[idxOpponent]});
    if(S.PawnY[idxAI] >= 0 && S.PawnX[idxAI] >= 0) visited[S.PawnY[idxAI]][S.PawnX[idxAI]] = RootPlayer;
    if(S.PawnY[idxOpponent] >= 0 && S.PawnX[idxOpponent] >= 0) visited[S.PawnY[idxOpponent]][S.PawnX[idxOpponent]] = 3 - RootPlayer;
    MyControl++; OppControl++;

    auto CanMove = [&](int ax, int ay, int bx, int by) -> bool {
        if (bx < 0 || bx > 8 || by < 0 || by > 8) return false;
        if (bx == ax + 1 && S.VerticalBlocked[ay][ax]) return false;
        if (bx == ax - 1 && ax > 0 && S.VerticalBlocked[ay][ax - 1]) return false;
        if (by == ay + 1 && S.HorizontalBlocked[ay][ax]) return false;
        if (by == ay - 1 && ay > 0 && S.HorizontalBlocked[ay - 1][ax]) return false;
        return true;
    };
    const FIntPoint dirs[4] = {{1,0}, {-1,0}, {0,1}, {0,-1}}; // Corrected dir

    while (!q1.empty() || !q2.empty()) {
        int q1_size = q1.size();
        for (int i = 0; i < q1_size; ++i) {
            FIntPoint curr = q1.front(); q1.pop();
            for (const auto& d : dirs) {
                int nx = curr.X + d.X; int ny = curr.Y + d.Y;
                if (CanMove(curr.X, curr.Y, nx, ny) && visited[ny][nx] == 0) {
                    visited[ny][nx] = RootPlayer; MyControl++; q1.push({nx, ny});
                }
            }
        }
        int q2_size = q2.size();
        for (int i = 0; i < q2_size; ++i) {
            FIntPoint curr = q2.front(); q2.pop();
            for (const auto& d : dirs) {
                int nx = curr.X + d.X; int ny = curr.Y + d.Y;
                if (CanMove(curr.X, curr.Y, nx, ny) && visited[ny][nx] == 0) {
                    visited[ny][nx] = 3 - RootPlayer; OppControl++; q2.push({nx, ny});
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

    // --- Weights (CRITICAL - These MUST be tuned!) ---
    const int32 W_PathDiff = 150;
    const int32 W_WallCount = 8;
    const int32 W_BoardControl = 3;
    const int32 W_StrategicWall = 15;
    const int32 W_PathDefense = -7;

    int32 Score = (OppLen - AILen) * W_PathDiff;
    Score += (S.WallsRemaining[idxAI] - S.WallsRemaining[idxOpponent]) * W_WallCount;

    int32 MyControl = 0, OppControl = 0;
    ComputeBoardControl(S, MyControl, OppControl, RootPlayer);
    Score += (MyControl - OppControl) * W_BoardControl;

    int oppBlockY;
    if (RootPlayer == 1) {
        oppBlockY = 0;  // Block Player 2 near row 0 (their goal)
    } else {
        oppBlockY = 8;  // Block Player 1 near row 9 (their goal)
    }
    for (int x = 0; x < 8; ++x) {
        if (S.HorizontalBlocked[oppBlockY][x]) Score += W_StrategicWall;
    }

    int OppDistToMyPath = 100;
    int OppX = S.PawnX[idxOpponent]; int OppY = S.PawnY[idxOpponent];
    for (const FIntPoint& p : AIPath) {
        OppDistToMyPath = FMath::Min(OppDistToMyPath, FMath::Abs(p.X - OppX) + FMath::Abs(p.Y - OppY));
    }
    if (OppDistToMyPath <= 2) {
        Score += (OppDistToMyPath - 3) * W_PathDefense;
    }
    
    Score += (8 - FMath::Abs(S.PawnY[idxAI] - (RootPlayer == 1 ? 8 : 0))) * 2; // reward being closer to goal row

    return Score;
}


//-----------------------------------------------------------------------------
// Apply Pawn Move
//-----------------------------------------------------------------------------
void MinimaxEngine::ApplyPawnMove(FMinimaxState& S, int32 PlayerNum, int32 X, int32 Y)
{
    S.PawnX[PlayerNum - 1] = X;
    S.PawnY[PlayerNum - 1] = Y;
}

//-----------------------------------------------------------------------------
// Apply Wall (Updated for WallCounts & Length)
//-----------------------------------------------------------------------------
void MinimaxEngine::ApplyWall(FMinimaxState& S, int32 PlayerNum, const FWallData& W)
{
    int idx = PlayerNum - 1;

    // *** ADD THIS CHECK AT THE TOP ***
    if (W.Length <= 0 || W.Length > 3) {
        UE_LOG(LogTemp, Error, TEXT("ApplyWall: Player %d called with INVALID wall length %d! Halting this ApplyWall call."), PlayerNum, W.Length);
        // You might want to add a breakpoint here to see who called this with 0.
        return; // Stop processing this invalid wall
    }
    // *** END CHECK ***

    int lenIdx = W.Length - 1; // Now we know W.Length is 1, 2, or 3, so lenIdx is 0, 1, or 2.

    // Apply *all* segments of the wall
    for (int i = 0; i < W.Length; ++i)
    {
        int cx = W.bHorizontal ? W.X + i : W.X;
        int cy = W.bHorizontal ? W.Y : W.Y + i;

        if (W.bHorizontal) {
            // Add bounds check for safety during development
            if (cy < 9 && cx < 8) S.HorizontalBlocked[cy][cx] = true;
            else UE_LOG(LogTemp, Error, TEXT("ApplyWall: H-Wall out of bounds: (%d, %d) L%d"), W.X, W.Y, W.Length);
        } else {
            // Add bounds check for safety during development
            if (cy < 8 && cx < 9) S.VerticalBlocked[cy][cx] = true;
            else UE_LOG(LogTemp, Error, TEXT("ApplyWall: V-Wall out of bounds: (%d, %d) L%d"), W.X, W.Y, W.Length);
        }
    }

    // Update counts (Now we know lenIdx is 0, 1, or 2)
    if (S.WallCounts[idx][lenIdx] > 0)
    {
        S.WallCounts[idx][lenIdx]--;
        S.WallsRemaining[idx]--;
    } else {
        // *** FIX THE LOG MESSAGE ***
        // This log now correctly indicates a *logic error* - trying to use a wall
        // of a valid length when none are left. The W.Length=0 case is caught above.
        UE_LOG(LogTemp, Error, TEXT("ApplyWall: Player %d tried to use wall length %d but has 0 left!"),
               PlayerNum, W.Length);
    }
}

// Add empty definitions for functions not fully shown or assumed to exist elsewhere
// if they aren't part of MinimaxEngine but were referenced (like WallTouchesPath if needed)
bool MinimaxEngine::WallTouchesPath(const FWallData& w, const TArray<FIntPoint>& Path)
{
    // Basic proximity check (can be refined)
    for (const FIntPoint& tile : Path)
    {
        int tx = tile.X;
        int ty = tile.Y;

        if (w.bHorizontal) {
            for (int i = 0; i < w.Length; ++i) {
                // Check if wall segment (w.X + i, w.Y) is adjacent to tile (tx, ty)
                if (FMath::Abs(w.X + i - tx) <= 1 && FMath::Abs(w.Y - ty) <= 1) return true;
            }
        } else { // Vertical
            for (int i = 0; i < w.Length; ++i) {
                // Check if wall segment (w.X, w.Y + i) is adjacent to tile (tx, ty)
                if (FMath::Abs(w.X - tx) <= 1 && FMath::Abs(w.Y + i - ty) <= 1) return true;
            }
        }
    }
    return false;
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

    // Assuming RecentMoves is accessible (e.g., static member or member of an instantiated MinimaxEngine)
    // You'll need to select the correct RecentMoves list if you have one per player.
    // For this example, let's assume 'RecentMoves' refers to the correct player's history.
    // e.g., TArray<FIntPoint>& CurrentPlayerRecentMoves = (RootPlayer == 1) ? RecentMoves_P1 : RecentMoves_P2;
    // For simplicity, I'll just use 'RecentMoves' but you need to ensure it's the right one.

    PrintInventory(Initial, TEXT("Start SolveParallel"));
    PrintBlockedWalls(Initial, TEXT("Start SolveParallel"));

    const int idx = RootPlayer - 1;
    const int Opponent = 3 - RootPlayer;
    int32 InitialAILength = 0, InitialOppLength = 0; // Renamed to avoid confusion
    ComputePathToGoal(Initial, RootPlayer, &InitialAILength);
    ComputePathToGoal(Initial, Opponent, &InitialOppLength);
    UE_LOG(LogTemp, Warning, TEXT("Initial Paths: AI=%d | Opp=%d"), InitialAILength, InitialOppLength);

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
    ComputePathToGoal(Initial, RootPlayer, &AILength);
    ComputePathToGoal(Initial, Opponent, &OppLength);
    UE_LOG(LogTemp, Warning, TEXT("Initial Paths: AI=%d | Opp=%d"), AILength, OppLength);

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


