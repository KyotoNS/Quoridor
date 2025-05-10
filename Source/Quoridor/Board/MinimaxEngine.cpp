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
    for(int y=0;y<9;++y)
      for(int x=0;x<8;++x)
        S.HorizontalBlocked[y][x] = false;
    for(int y=0;y<8;++y)
      for(int x=0;x<9;++x)
        S.VerticalBlocked[y][x]   = false;

    // Pawns
    AQuoridorPawn* P1 = Board->GetPawnForPlayer(1);
    AQuoridorPawn* P2 = Board->GetPawnForPlayer(2);
    S.PawnX[0] = P1->GridX;
    S.PawnY[0] = P1->GridY;
    S.PawnX[1] = P2->GridX;
    S.PawnY[1] = P2->GridY;

    // Walls remaining
    S.WallsRemaining[0] = P1->PlayerWalls.Num();
    S.WallsRemaining[1] = P2->PlayerWalls.Num();

    // Mark actual walls
    for(AWallSlot* Slot: Board->HorizontalWallSlots)
        S.HorizontalBlocked[Slot->GridY][Slot->GridX] = Slot->bIsOccupied;
    for(AWallSlot* Slot: Board->VerticalWallSlots)
        S.VerticalBlocked[Slot->GridY][Slot->GridX]   = Slot->bIsOccupied;

    return S;
}

//-----------------------------------------------------------------------------
// A* helper for distance-to-goal (100=no path)
//-----------------------------------------------------------------------------
static int32 ComputeDistanceToGoal(const FMinimaxState& S, int32 PlayerNum)
{
    int goalY = (PlayerNum==1 ? 8 : 0);
    int idx = PlayerNum-1;
    int sx = S.PawnX[idx], sy = S.PawnY[idx];
    struct Node { int f,g,x,y; };
    struct Cmp { bool operator()(const Node&a,const Node&b)const{return a.f>b.f;} };
    std::priority_queue<Node, std::vector<Node>, Cmp> open;
    bool closed[9][9] = {};
    int  gCost[9][9]; for(int y=0;y<9;y++) for(int x=0;x<9;x++) gCost[y][x]=INT_MAX;
    auto h=[&](int x,int y){ return FMath::Abs(goalY-y); };
    gCost[sy][sx]=0;
    open.push({h(sx,sy),0,sx,sy});
    const FIntPoint dirs[4]={{1,0},{-1,0},{0,1},{0,-1}};
    while(!open.empty()){
        Node n = open.top(); open.pop();
        if(closed[n.y][n.x]) continue;
        closed[n.y][n.x]=true;
        if(n.y==goalY){ UE_LOG(LogTemp,Log, TEXT("A* P%d goal in %d"), PlayerNum,n.g); return n.g; }
        for(auto& d:dirs){
            int nx=n.x+d.X, ny=n.y+d.Y;
            if(nx<0||nx>8||ny<0||ny>8) continue;
            if(d.X==1 && S.VerticalBlocked[n.y][n.x]) continue;
            if(d.X==-1&&S.VerticalBlocked[n.y][nx]) continue;
            if(d.Y==1 && S.HorizontalBlocked[n.y][n.x]) continue;
            if(d.Y==-1&&S.HorizontalBlocked[ny][n.x]) continue;
            if(closed[ny][nx]) continue;
            int ng=n.g+1;
            if(ng<gCost[ny][nx]){ gCost[ny][nx]=ng; open.push({ng+h(nx,ny),ng,nx,ny}); }
        }
    }
    UE_LOG(LogTemp,Warning, TEXT("A* fail P%d"), PlayerNum);
    return 100;
}

int32 MinimaxEngine::Evaluate(const FMinimaxState& S)
{
    int32 d1=ComputeDistanceToGoal(S,1);
    int32 d2=ComputeDistanceToGoal(S,2);
    int32 wd=S.WallsRemaining[1]-S.WallsRemaining[0];
    return (d1-d2)*10 + wd*2;
}

TArray<FIntPoint> MinimaxEngine::GetPawnMoves(const FMinimaxState& S, int32 PlayerNum)
{
    TArray<FIntPoint> Out;
    int idx=PlayerNum-1, x=S.PawnX[idx], y=S.PawnY[idx];
    auto CanStep=[&](int ax,int ay,int bx,int by){
        if(bx<0||bx>8||by<0||by>8) return false;
        if(bx==ax+1&&S.VerticalBlocked[ay][ax]) return false;
        if(bx==ax-1&&S.VerticalBlocked[ay][bx]) return false;
        if(by==ay+1&&S.HorizontalBlocked[ay][ax]) return false;
        if(by==ay-1&&S.HorizontalBlocked[by][ax]) return false;
        return true;
    };
    const FIntPoint dirs[4]={{1,0},{-1,0},{0,1},{0,-1}};
    for(auto& d:dirs){ int nx=x+d.X, ny=y+d.Y;
        bool occ=((S.PawnX[0]==nx&&S.PawnY[0]==ny)||(S.PawnX[1]==nx&&S.PawnY[1]==ny));
        if(!occ && CanStep(x,y,nx,ny)) Out.Add({nx,ny});
        else if(occ && CanStep(x,y,nx,ny)){
            int jx=nx+d.X, jy=ny+d.Y;
            if(CanStep(nx,ny,jx,jy)&& !((S.PawnX[0]==jx&&S.PawnY[0]==jy)||(S.PawnX[1]==jx&&S.PawnY[1]==jy)))
                Out.Add({jx,jy});
            else{
                FIntPoint perp[2]={{d.Y,d.X},{-d.Y,-d.X}};
                for(auto& p:perp){ int sx=nx+p.X, sy=ny+p.Y;
                    if(CanStep(nx,ny,sx,sy)&& !((S.PawnX[0]==sx&&S.PawnY[0]==sy)||(S.PawnX[1]==sx&&S.PawnY[1]==sy)))
                        Out.Add({sx,sy});
                }
            }
        }
    }
    return Out;
}

TArray<FWallData> MinimaxEngine::GetWallPlacements(const FMinimaxState& S, int32 PlayerNum)
{
    TArray<FWallData> Walls;
    const int MaxLengths[3] = {1, 2, 3}; // AI has walls of length 1, 2, and 3
    
    // Horizontal walls: 9 rows × (8,7,6) columns depending on length
    for (int length : MaxLengths)
    {
        for (int y = 0; y < 9; ++y)
        {
            for (int x = 0; x <= 8 - length; ++x)
            {
                FWallData W;
                W.X = x;
                W.Y = y;
                W.Length = length;
                W.bHorizontal = true;

                // simulate
                FMinimaxState SS = S;
                // block each segment
                for (int offset = 0; offset < length; ++offset)
                {
                    SS.HorizontalBlocked[y][x + offset] = true;
                }
                SS.WallsRemaining[PlayerNum - 1] -= 1;

                // keep if both players still have a path
                if (ComputeDistanceToGoal(SS, 1) < 100 && ComputeDistanceToGoal(SS, 2) < 100)
                {
                    Walls.Add(W);
                }
            }
        }
    }

    // Vertical walls: (8,7,6) rows × 9 columns depending on length
    for (int length : MaxLengths)
    {
        for (int y = 0; y <= 8 - length; ++y)
        {
            for (int x = 0; x < 9; ++x)
            {
                FWallData W;
                W.X = x;
                W.Y = y;
                W.Length = length;
                W.bHorizontal = false;

                // simulate
                FMinimaxState SS = S;
                // block each segment
                for (int offset = 0; offset < length; ++offset)
                {
                    SS.VerticalBlocked[y + offset][x] = true;
                }
                SS.WallsRemaining[PlayerNum - 1] -= 1;

                // keep if both players still have a path
                if (ComputeDistanceToGoal(SS, 1) < 100 && ComputeDistanceToGoal(SS, 2) < 100)
                {
                    Walls.Add(W);
                }
            }
        }
    }

    return Walls;
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

int32 MinimaxEngine::Minimax(FMinimaxState& S,int32 Depth,bool bMax)
{
    if(Depth<=0) return Evaluate(S);
    bool maximize=bMax;
    int32 best = maximize?INT_MIN:INT_MAX;
    int player = maximize?2:1;
    for(auto mv:GetPawnMoves(S,player)){
        FMinimaxState SS=S; ApplyPawnMove(SS,player,mv.X,mv.Y);
        int32 v=Minimax(SS,Depth-1,!bMax);
        best = maximize? FMath::Max(best,v) : FMath::Min(best,v);
    }
    int idx=player-1; if(S.WallsRemaining[idx]>0)
        for(auto w:GetWallPlacements(S,player)){
            FMinimaxState SS=S; ApplyWall(SS,player,w);
            int32 v=Minimax(SS,Depth-1,!bMax);
            best = maximize? FMath::Max(best,v) : FMath::Min(best,v);
        }
    return best;
}

FMinimaxAction MinimaxEngine::Solve(const FMinimaxState& Initial,int32 Depth)
{
    UE_LOG(LogTemp,Warning,TEXT("=== MinimaxEngine::Solve depth=%d ==="),Depth);
    FMinimaxAction bestAct; bestAct.Score=INT_MIN;
    int root=2, idx=root-1;
    for(auto mv:GetPawnMoves(const_cast<FMinimaxState&>(Initial),root)){
        
        FMinimaxState SS=Initial; ApplyPawnMove(SS,root,mv.X,mv.Y);
        
        int32 d1=ComputeDistanceToGoal(SS,1), d2=ComputeDistanceToGoal(SS,2);
        
        int32 leaf=(d1-d2)*10 + (SS.WallsRemaining[1]-SS.WallsRemaining[0])*2;
        
        UE_LOG(LogTemp,Warning,TEXT("[CAND] Move(%d,%d): d1=%d d2=%d leaf=%d"),mv.X,mv.Y,d1,d2,leaf);
        
        int32 v=Minimax(SS,Depth,false);
        
        UE_LOG(LogTemp,Warning,TEXT("       => MinimaxScore=%d"),v);
        if(v>bestAct.Score){ bestAct.bIsWall=false; bestAct.MoveX=mv.X; bestAct.MoveY=mv.Y; bestAct.Score=v;}    }
        
    if(Initial.WallsRemaining[idx]>0)
   
        for(auto w:GetWallPlacements(const_cast<FMinimaxState&>(Initial),root)){
            
            UE_LOG(LogTemp,Warning,TEXT("[CAND] Wall@(%d,%d)%s"),w.X,w.Y,w.bHorizontal?TEXT("H"):TEXT("V"));
            
            FMinimaxState SS=Initial; ApplyWall(SS,root,w);
            int32 v=Minimax(SS,Depth,false);
            
            UE_LOG(LogTemp,Warning,TEXT("       => WallScore=%d"),v);
            
            if(v>bestAct.Score){ bestAct.bIsWall=true; bestAct.SlotX=w.X; bestAct.SlotY=w.Y;
                bestAct.WallLength=w.Length; bestAct.bHorizontal=w.bHorizontal; bestAct.Score=v;}    }
    
    return bestAct;
}

