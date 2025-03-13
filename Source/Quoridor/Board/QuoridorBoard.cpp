#include "QuoridorBoard.h"
#include "Quoridor/Tile/Tile.h"
#include "Quoridor/Pawn/QuoridorPawn.h"
#include "Quoridor/Wall/Wall.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Containers/Queue.h"

AQuoridorBoard::AQuoridorBoard() {
    PrimaryActorTick.bCanEverTick = false;
    GridSize = 9;
    TileSize = 100.f;
    CurrentPlayerTurn = 1;
}

void AQuoridorBoard::BeginPlay() {
    Super::BeginPlay();
    
    // Initialize tiles
    const FVector BoardCenter = GetActorLocation();
    Tiles.SetNum(GridSize);
    
    for(int32 y = 0; y < GridSize; y++) {
        Tiles[y].SetNum(GridSize);
        for(int32 x = 0; x < GridSize; x++) {
            const FVector Location = BoardCenter + FVector(
                (x - GridSize/2) * TileSize,
                (y - GridSize/2) * TileSize,
                0
            );
            
            ATile* NewTile = GetWorld()->SpawnActor<ATile>(TileClass, Location, FRotator::ZeroRotator);
            NewTile->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
            NewTile->SetGridPosition(x, y);
            Tiles[y][x] = NewTile;
        }
    }

    // Initialize wall grid (8x8 for both orientations)
    WallGrid.SetNum(8);
    for(int x=0; x<8; x++) {
        WallGrid[x].SetNum(8);
        for(int y=0; y<8; y++) {
            WallGrid[x][y].SetNum(2);
            WallGrid[x][y][0] = false; // Horizontal
            WallGrid[x][y][1] = false; // Vertical
        }
    }

    // Spawn pawns
    SpawnPawn(FIntPoint(4, 0), 1);
    SpawnPawn(FIntPoint(4, 8), 2);
}

void AQuoridorBoard::SpawnPawn(FIntPoint GridPosition, int32 PlayerNumber) {
    if(!Tiles.IsValidIndex(GridPosition.Y) || !Tiles[GridPosition.Y].IsValidIndex(GridPosition.X)) return;

    ATile* StartTile = Tiles[GridPosition.Y][GridPosition.X];
    const FVector SpawnLocation = StartTile->GetActorLocation() + FVector(0,0,50);
    
    AQuoridorPawn* NewPawn = GetWorld()->SpawnActor<AQuoridorPawn>(PawnClass, SpawnLocation, FRotator::ZeroRotator);
    NewPawn->CurrentTile = StartTile;
    NewPawn->PlayerNumber = PlayerNumber;
    StartTile->SetPawnOnTile(NewPawn);
}

// Pathfinding Node Structure
struct FPathNode {
    FIntPoint Position;
    int32 GCost;
    int32 HCost;
    FPathNode* Parent;

    FPathNode(FIntPoint Pos, int32 G, int32 H, FPathNode* ParentNode) 
        : Position(Pos), GCost(G), HCost(H), Parent(ParentNode) {}

    int32 FCost() const { return GCost + HCost; }
};

TArray<FIntPoint> AQuoridorBoard::FindPath(int32 StartX, int32 StartY, int32 TargetRow) {
    TArray<FPathNode*> OpenSet;
    TMap<FIntPoint, FPathNode*> AllNodes;

    // Initialize start node
    FPathNode* StartNode = new FPathNode(FIntPoint(StartX, StartY), 0, FMath::Abs(StartY - TargetRow), nullptr);
    OpenSet.Add(StartNode);
    AllNodes.Add(StartNode->Position, StartNode);

    while(OpenSet.Num() > 0) {
        // Get lowest F-cost node
        int32 MinIndex = 0;
        for(int32 i=1; i<OpenSet.Num(); i++) {
            if(OpenSet[i]->FCost() < OpenSet[MinIndex]->FCost() || 
              (OpenSet[i]->FCost() == OpenSet[MinIndex]->FCost() && 
               OpenSet[i]->HCost < OpenSet[MinIndex]->HCost)) {
                MinIndex = i;
            }
        }
        FPathNode* Current = OpenSet[MinIndex];

        // Check if reached target row
        if(Current->Position.Y == TargetRow) {
            TArray<FIntPoint> Path;
            FPathNode* Node = Current;
            while(Node) {
                Path.Insert(Node->Position, 0);
                Node = Node->Parent;
            }
            // Cleanup
            for(auto& Elem : AllNodes) delete Elem.Value;
            return Path;
        }

        OpenSet.RemoveAt(MinIndex);
        
        // Check neighbors
        const TArray<FIntPoint> Directions = {{1,0}, {-1,0}, {0,1}, {0,-1}};
        for(const FIntPoint& Dir : Directions) {
            FIntPoint NeighborPos = Current->Position + Dir;
            
            if(!IsValidTile(NeighborPos.X, NeighborPos.Y)) continue;
            
            // Wall checks
            bool bBlocked = false;
            if(Dir.X == 1) bBlocked = HasWallBetween(Current->Position.X, Current->Position.Y, EDirection::East);
            if(Dir.X == -1) bBlocked = HasWallBetween(NeighborPos.X, NeighborPos.Y, EDirection::East);
            if(Dir.Y == 1) bBlocked = HasWallBetween(Current->Position.X, Current->Position.Y, EDirection::North);
            if(Dir.Y == -1) bBlocked = HasWallBetween(NeighborPos.X, NeighborPos.Y, EDirection::North);
            if(bBlocked) continue;

            int32 GCost = Current->GCost + 1;
            int32 HCost = FMath::Abs(NeighborPos.Y - TargetRow);
            
            FPathNode** ExistingNode = AllNodes.Find(NeighborPos);
            if(!ExistingNode) {
                FPathNode* NewNode = new FPathNode(NeighborPos, GCost, HCost, Current);
                OpenSet.Add(NewNode);
                AllNodes.Add(NeighborPos, NewNode);
            } else if(GCost < (*ExistingNode)->GCost) {
                (*ExistingNode)->GCost = GCost;
                (*ExistingNode)->Parent = Current;
            }
        }
    }

    // Cleanup if no path
    for(auto& Elem : AllNodes) delete Elem.Value;
    return TArray<FIntPoint>();
}

bool AQuoridorBoard::ValidatePathExists(int32 PlayerNumber) {
    int32 TargetRow = (PlayerNumber == 1) ? 8 : 0;
    AQuoridorPawn* Pawn = nullptr;
    
    // Find pawn for player
    TArray<AActor*> Pawns;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AQuoridorPawn::StaticClass(), Pawns);
    for(AActor* Actor : Pawns) {
        AQuoridorPawn* P = Cast<AQuoridorPawn>(Actor);
        if(P && P->PlayerNumber == PlayerNumber) {
            Pawn = P;
            break;
        }
    }
    if(!Pawn || !Pawn->CurrentTile) return false;

    TArray<FIntPoint> Path = FindPath(
        Pawn->CurrentTile->GridX,
        Pawn->CurrentTile->GridY,
        TargetRow
    );
    
    return Path.Num() > 0;
}

bool AQuoridorBoard::TryPlaceWall(FIntPoint GridPos, EWallOrientation Orientation, int32 PlayerNumber) {
    if(PlayerNumber < 1 || PlayerNumber > 2) return false;
    if(PlayerWalls[PlayerNumber-1] <= 0) return false;
    if(!IsWallPlacementValid(GridPos, Orientation)) return false;

    // Temporarily place wall
    int32 OrientIndex = static_cast<int32>(Orientation);
    WallGrid[GridPos.X][GridPos.Y][OrientIndex] = true;

    // Validate both players can still reach goals
    bool bValid = ValidatePathExists(1) && ValidatePathExists(2);
    if(!bValid) {
        WallGrid[GridPos.X][GridPos.Y][OrientIndex] = false;
        return false;
    }

    // Spawn wall actor
    FVector Location = Tiles[GridPos.Y][GridPos.X]->GetActorLocation();
    if(Orientation == EWallOrientation::Horizontal) {
        Location.Y += TileSize/2;
    } else {
        Location.X += TileSize/2;
    }
    
    AWall* NewWall = GetWorld()->SpawnActor<AWall>(WallClass, Location, FRotator::ZeroRotator);
    NewWall->Initialize(GridPos, Orientation);
    PlacedWalls.Add(NewWall);

    PlayerWalls[PlayerNumber-1]--;
    return true;
}

bool AQuoridorBoard::IsWallPlacementValid(FIntPoint GridPos, EWallOrientation Orientation) {
    if(GridPos.X < 0 || GridPos.X >= 8 || GridPos.Y < 0 || GridPos.Y >= 8) return false;
    
    // Check existing walls
    int32 OrientIndex = static_cast<int32>(Orientation);
    if(WallGrid[GridPos.X][GridPos.Y][OrientIndex]) return false;

    // Check adjacent walls don't overlap
    if(Orientation == EWallOrientation::Horizontal) {
        if((GridPos.X > 0 && WallGrid[GridPos.X-1][GridPos.Y][OrientIndex]) ||
           (GridPos.X < 7 && WallGrid[GridPos.X+1][GridPos.Y][OrientIndex])) {
            return false;
        }
    } else {
        if((GridPos.Y > 0 && WallGrid[GridPos.X][GridPos.Y-1][OrientIndex]) ||
           (GridPos.Y < 7 && WallGrid[GridPos.X][GridPos.Y+1][OrientIndex])) {
            return false;
        }
    }

    return true;
}

bool AQuoridorBoard::HasWallBetween(int32 TileX, int32 TileY, EDirection Direction) const {
    if(Direction == EDirection::North) {
        return WallGrid.IsValidIndex(TileX) && 
               WallGrid[TileX].IsValidIndex(TileY) && 
               WallGrid[TileX][TileY][static_cast<int32>(EWallOrientation::Horizontal)];
    }
    if(Direction == EDirection::East) {
        return WallGrid.IsValidIndex(TileX) && 
               WallGrid[TileX].IsValidIndex(TileY) && 
               WallGrid[TileX][TileY][static_cast<int32>(EWallOrientation::Vertical)];
    }
    return false;
}

bool AQuoridorBoard::IsValidTile(int32 X, int32 Y) const {
    return X >= 0 && X < GridSize && Y >= 0 && Y < GridSize;
}

void AQuoridorBoard::HandlePawnClick(AQuoridorPawn* ClickedPawn) {
    if(ClickedPawn && ClickedPawn->PlayerNumber == CurrentPlayerTurn) {
        ClearSelection();
        SelectedPawn = ClickedPawn;
        // Add your highlight logic here
    }
}

void AQuoridorBoard::HandleTileClick(ATile* ClickedTile) {
    if(SelectedPawn && ClickedTile) {
        if(SelectedPawn->CanMoveToTile(ClickedTile)) {
            SelectedPawn->MoveToTile(ClickedTile);
            CurrentPlayerTurn = CurrentPlayerTurn == 1 ? 2 : 1;
        }
        ClearSelection();
    }
}

void AQuoridorBoard::ClearSelection() {
    SelectedPawn = nullptr;
    // Add your deselection logic here
}