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
		SetActorLocation(CurrentTile->GetActorLocation() + FVector(0, 0, 50));

		UE_LOG(LogTemp, Warning, TEXT("Moved pawn to tile (%d, %d)"), GridX, GridY);

		// Cek kemenangan
		if (BoardReference)
		{
			int32 FinalRow = (PlayerNumber == 1) ? BoardReference->GridSize - 1 : 0;
			if (CurrentTile && CurrentTile->GridY == FinalRow)
			{
				BoardReference->HandleWin(PlayerNumber);
			}
		}
	}
}



bool AQuoridorPawn::CanMoveToTile(const ATile* TargetTile) const
{
    if (!TargetTile || !CurrentTile)
    {
        UE_LOG(LogTemp, Warning, TEXT("CanMoveToTile Failed: Invalid TargetTile or CurrentTile"));
        return false;
    }

    if (!BoardReference)
    {
        UE_LOG(LogTemp, Error, TEXT("CanMoveToTile Failed: BoardReference is nullptr for Player %d"), PlayerNumber);
        return false;
    }

    if (TargetTile->IsOccupied())
    {
        UE_LOG(LogTemp, Warning, TEXT("CanMoveToTile Failed: TargetTile (%d, %d) is occupied"), TargetTile->GridX, TargetTile->GridY);
        return false;
    }

    if (!CurrentTile->IsAdjacent(TargetTile))
    {
        UE_LOG(LogTemp, Warning, TEXT("CanMoveToTile Failed: TargetTile (%d, %d) is not adjacent to CurrentTile (%d, %d)"),
            TargetTile->GridX, TargetTile->GridY, CurrentTile->GridX, CurrentTile->GridY);
        return false;
    }

    // Tentukan arah pergerakan
    int32 DeltaX = TargetTile->GridX - CurrentTile->GridX;
    int32 DeltaY = TargetTile->GridY - CurrentTile->GridY;
    AWallSlot* BlockingWallSlot = nullptr;

    // Periksa tembok berdasarkan arah pergerakan
    if (DeltaX == 1) // Ke kanan
    {
        BlockingWallSlot = BoardReference->FindWallSlotAt(CurrentTile->GridX, CurrentTile->GridY, EWallOrientation::Vertical);
        UE_LOG(LogTemp, Warning, TEXT("CanMoveToTile: Checking vertical wall slot at (%d, %d)"), CurrentTile->GridX, CurrentTile->GridY);
    }
    else if (DeltaX == -1) // Ke kiri
    {
        BlockingWallSlot = BoardReference->FindWallSlotAt(CurrentTile->GridX - 1, CurrentTile->GridY, EWallOrientation::Vertical);
        UE_LOG(LogTemp, Warning, TEXT("CanMoveToTile: Checking vertical wall slot at (%d, %d)"), CurrentTile->GridX - 1, CurrentTile->GridY);
    }
    else if (DeltaY == 1) // Ke atas
    {
        BlockingWallSlot = BoardReference->FindWallSlotAt(CurrentTile->GridX, CurrentTile->GridY, EWallOrientation::Horizontal);
        UE_LOG(LogTemp, Warning, TEXT("CanMoveToTile: Checking horizontal wall slot at (%d, %d)"), CurrentTile->GridX, CurrentTile->GridY);
    }
    else if (DeltaY == -1) // Ke bawah
    {
        BlockingWallSlot = BoardReference->FindWallSlotAt(CurrentTile->GridX, CurrentTile->GridY - 1, EWallOrientation::Horizontal);
        UE_LOG(LogTemp, Warning, TEXT("CanMoveToTile: Checking horizontal wall slot at (%d, %d)"), CurrentTile->GridX, CurrentTile->GridY - 1);
    }

    if (BlockingWallSlot)
    {
        if (BlockingWallSlot->bIsOccupied)
        {
            UE_LOG(LogTemp, Warning, TEXT("CanMoveToTile Failed: Wall slot at (%d, %d, %s) is occupied, blocking path from (%d, %d) to (%d, %d)"),
                BlockingWallSlot->GridX, BlockingWallSlot->GridY,
                BlockingWallSlot->Orientation == EWallOrientation::Horizontal ? TEXT("Horizontal") : TEXT("Vertical"),
                CurrentTile->GridX, CurrentTile->GridY, TargetTile->GridX, TargetTile->GridY);
            return false;
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("CanMoveToTile: Wall slot at (%d, %d, %s) is not occupied"),
                BlockingWallSlot->GridX, BlockingWallSlot->GridY,
                BlockingWallSlot->Orientation == EWallOrientation::Horizontal ? TEXT("Horizontal") : TEXT("Vertical"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("CanMoveToTile: No wall slot found for path from (%d, %d) to (%d, %d)"),
            CurrentTile->GridX, CurrentTile->GridY, TargetTile->GridX, TargetTile->GridY);
    }

    UE_LOG(LogTemp, Warning, TEXT("CanMoveToTile Success: Valid move from (%d, %d) to (%d, %d)"),
        CurrentTile->GridX, CurrentTile->GridY, TargetTile->GridX, TargetTile->GridY);
    return true;
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
	UE_LOG(LogTemp, Warning, TEXT("InitializePawn Success: Player %d initialized at tile (%d, %d) with BoardReference set"),
		PlayerNumber, StartTile->GridX, StartTile->GridY);
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

    if (NewX < 0 || NewX >= BoardReference->GridSize || NewY < 0 || NewY >= BoardReference->GridSize)
    {
        UE_LOG(LogTemp, Warning, TEXT("MovePawn Failed: Position (%d, %d) out of bounds for Player %d"),
            NewX, NewY, PlayerNumber);
        return;
    }

    // Validasi pergerakan menggunakan CanMoveToTile
    if (BoardReference->Tiles.IsValidIndex(NewY) && BoardReference->Tiles[NewY].IsValidIndex(NewX))
    {
        ATile* TargetTile = BoardReference->Tiles[NewY][NewX];
        if (CanMoveToTile(TargetTile))
        {
            if (CurrentTile)
            {
                CurrentTile->SetPawnOnTile(nullptr);
            }

            GridX = NewX;
            GridY = NewY;
            CurrentTile = TargetTile;
            CurrentTile->SetPawnOnTile(this);
            SetActorLocation(CurrentTile->GetActorLocation() + FVector(0, 0, 50));

            UE_LOG(LogTemp, Warning, TEXT("MovePawn Success: Player %d moved to (%d, %d)"), PlayerNumber, NewX, NewY);

            BoardReference->CurrentPlayerTurn = BoardReference->CurrentPlayerTurn == 1 ? 2 : 1;
            UE_LOG(LogTemp, Warning, TEXT("MovePawn: Switched turn to Player %d"), BoardReference->CurrentPlayerTurn);
            BoardReference->SelectedPawn = nullptr;
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("MovePawn Failed: Invalid move to (%d, %d) for Player %d"), NewX, NewY, PlayerNumber);
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("MovePawn Failed: Invalid tile at position (%d, %d)"), NewX, NewY);
    }
}





