// QuoridorPawn.cpp
#include "QuoridorPawn.h"
#include "Quoridor/Tile/Tile.h"
#include "Quoridor/Board/QuoridorBoard.h"
#include "Components/BoxComponent.h"

AQuoridorPawn::AQuoridorPawn()
{
	PrimaryActorTick.bCanEverTick = false;

	PawnMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PawnMesh"));
	RootComponent = PawnMesh;

	SelectionCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("SelectionCollision"));
	SelectionCollision->SetupAttachment(RootComponent);
	SelectionCollision->SetBoxExtent(FVector(50, 50, 100));

	GridX = 0;
	GridY = 0;
	PlayerNumber = 1;
}

void AQuoridorPawn::MoveToTile(ATile* NewTile)
{
	if (NewTile && CanMoveToTile(NewTile))
	{
		if (CurrentTile) 
		{
			CurrentTile->SetPawnOnTile(nullptr);
		}
        
		CurrentTile = NewTile;
		CurrentTile->SetPawnOnTile(this);
        
		// Get tile location with vertical offset
		const FVector NewLocation = CurrentTile->GetActorLocation() + FVector(0, 0, 50);
		SetActorLocation(NewLocation);
        
		UE_LOG(LogTemp, Warning, TEXT("Moved pawn to: %s"), *NewLocation.ToString());
	}
}

bool AQuoridorPawn::CanMoveToTile(const ATile* TargetTile) const
{
	return TargetTile && 
		  !TargetTile->IsOccupied() && 
		  CurrentTile->IsAdjacent(TargetTile);
}

void AQuoridorPawn::InitializePawn(ATile* StartTile, AQuoridorBoard* Board)
{
	if (StartTile)
	{
		CurrentTile = StartTile;
		BoardReference = Board;  // Store the board reference
		SetActorLocation(StartTile->GetActorLocation() + FVector(0, 0, 50));
		StartTile->SetPawnOnTile(this);
	}
}

bool AQuoridorPawn::HasWallOfLength(int32 Length) const
{
	for (const FWallDefinition& Wall : PlayerWalls)
	{
		if (Wall.Length == Length)
			return true;
	}
	return false;
}


int32 AQuoridorPawn::GetWallCountOfLength(int32 Length) const
{
	int32 Count = 0;
	for (const FWallDefinition& Wall : PlayerWalls)
	{
		if (Wall.Length == Length)
		{
			Count++;
		}
	}
	return Count;
}


bool AQuoridorPawn::RemoveWallOfLength(int32 Length)
{
	for (int32 i = 0; i < PlayerWalls.Num(); ++i)
	{
		if (PlayerWalls[i].Length == Length)
		{
			PlayerWalls.RemoveAt(i);
			return true;
		}
	}
	return false; // Tidak ada wall yang cocok
}

void AQuoridorPawn::SetGridPosition(int32 X, int32 Y)
{
	GridX = X;
	GridY = Y;
}
void AQuoridorPawn::MovePawn(int32 NewX, int32 NewY)
{
    if (!BoardReference)
    {
        UE_LOG(LogTemp, Warning, TEXT("MovePawn Failed: BoardReference is nullptr for Player %d"), PlayerNumber);
        return;
    }

    // Periksa apakah giliran saat ini sesuai dengan nomor pemain
    int32 CurrentTurn = BoardReference->CurrentPlayerTurn;
    if (CurrentTurn != PlayerNumber)
    {
        UE_LOG(LogTemp, Warning, TEXT("MovePawn Failed: Not Player %d's turn (Current turn: Player %d)"),
            PlayerNumber, CurrentTurn);
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red,
            FString::Printf(TEXT("Not Player %d's turn! Current turn: Player %d"),
                PlayerNumber, CurrentTurn));
        return;
    }

    // Validasi posisi baru
    if (NewX < 0 || NewX >= BoardReference->GridSize || NewY < 0 || NewY >= BoardReference->GridSize)
    {
        UE_LOG(LogTemp, Warning, TEXT("MovePawn Failed: Position (%d, %d) out of bounds for Player %d"),
            NewX, NewY, PlayerNumber);
        return;
    }

    // Periksa apakah ada tembok yang menghalangi (opsional)
    // Anda bisa menambahkan logika ini nanti jika diperlukan

    // Perbarui posisi pion
    GridX = NewX;
    GridY = NewY;
    if (BoardReference->Tiles.IsValidIndex(NewY) && BoardReference->Tiles[NewY].IsValidIndex(NewX) && BoardReference->Tiles[NewY][NewX])
    {
        SetActorLocation(BoardReference->Tiles[NewY][NewX]->GetActorLocation());
        UE_LOG(LogTemp, Warning, TEXT("MovePawn Success: Player %d moved to (%d, %d)"), PlayerNumber, NewX, NewY);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("MovePawn Failed: Invalid tile at position (%d, %d)"), NewX, NewY);
        return;
    }

    // Ganti giliran setelah pergerakan
    BoardReference->CurrentPlayerTurn = BoardReference->CurrentPlayerTurn == 1 ? 2 : 1;
    UE_LOG(LogTemp, Warning, TEXT("MovePawn: Switched turn to Player %d"), BoardReference->CurrentPlayerTurn);

    // Bersihkan SelectedPawn setelah pergerakan
    BoardReference->SelectedPawn = nullptr;
}





