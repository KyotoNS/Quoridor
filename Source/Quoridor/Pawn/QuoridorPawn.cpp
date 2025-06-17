// QuoridorPawn.cpp
#include "QuoridorPawn.h"
#include "Quoridor/Tile/Tile.h"
#include "Quoridor/Board/QuoridorBoard.h"
#include "Quoridor/Board/MinimaxBoardAI.h"
#include "Components/BoxComponent.h"

AQuoridorPawn::AQuoridorPawn()
{
	PrimaryActorTick.bCanEverTick = false;

	PawnMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PawnMesh"));
	RootComponent = PawnMesh;

	SelectionCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("SelectionCollision"));
	SelectionCollision->SetupAttachment(RootComponent);
	SelectionCollision->SetBoxExtent(FVector(50, 50, 50));

	GridX = 0;
	GridY = 0;
	PlayerNumber = 1;
}
AQuoridorPawn* ATile::GetOccupyingPawn() const
{
	TArray<AActor*> OverlappingActors;
	GetOverlappingActors(OverlappingActors, AQuoridorPawn::StaticClass());

	for (AActor* Actor : OverlappingActors)
	{
		if (AQuoridorPawn* Pawn = Cast<AQuoridorPawn>(Actor))
		{
			return Pawn;
		}
	}

	return nullptr;
}

void AQuoridorPawn::MoveToTile(ATile* NewTile, bool bForceMove)
{
	if (NewTile && (bForceMove || CanMoveToTile(NewTile)))
	{
		if (CurrentTile) 
		{
			CurrentTile->SetPawnOnTile(nullptr);
		}

		CurrentTile = NewTile;
		CurrentTile->SetPawnOnTile(this);
		SetActorLocation(CurrentTile->GetActorLocation() + FVector(0, 0, 50));

		UE_LOG(LogTemp, Warning, TEXT("Moved pawn to tile (%d, %d)"), CurrentTile->GridX, CurrentTile->GridY);

		// Cek kemenangan
		if (BoardReference)
		{
			int32 FinalRow = (PlayerNumber == 1) ? BoardReference->GridSize - 1 : 0;
			if (CurrentTile->GridY == FinalRow)
			{
				BoardReference->HandleWin(PlayerNumber);
			}
		}
	}
}


// Di dalam file QuoridorPawn.cpp

bool AQuoridorPawn::CanMoveToTile(const ATile* TargetTile) const
{
	// Langkah 1: Lakukan pengecekan dasar (selalu merupakan ide bagus)
	if (!TargetTile || !BoardReference)
	{
		UE_LOG(LogTemp, Error, TEXT("CanMoveToTile: TargetTile or BoardReference is null!"));
		return false;
	}

	// Langkah 2: Buat "snapshot" dari kondisi papan permainan saat ini
	// Fungsi ini harus ada di struct FMinimaxState Anda untuk mengonversi board nyata ke state AI.
	const FMinimaxState CurrentState = FMinimaxState::FromBoard(BoardReference);

	// Langkah 3: Dapatkan SEMUA kemungkinan gerakan yang sah untuk pion ini dari MinimaxEngine
	// Engine akan menangani semua aturan rumit (lompatan, blokade tembok, dll.)
	const TArray<FIntPoint> PossibleMoves = MinimaxEngine::GetPawnMoves(CurrentState, this->PlayerNumber);

	// Langkah 4: Buat FIntPoint untuk tile yang ingin kita tuju
	const FIntPoint TargetCoords(TargetTile->GridX, TargetTile->GridY);

	// Langkah 5: Periksa apakah koordinat target ada di dalam daftar gerakan yang memungkinkan
	// Fungsi TArray::Contains() akan melakukan semua pekerjaan untuk kita.
	const bool bIsMoveValid = PossibleMoves.Contains(TargetCoords);

	// (Opsional tapi membantu) Tambahkan log untuk melihat hasilnya
	if (bIsMoveValid)
	{
		UE_LOG(LogTemp, Log, TEXT("CanMoveToTile: Move to (%d, %d) is VALID based on GetPawnMoves result."), TargetCoords.X, TargetCoords.Y);
	}
	else
	{
		// Log ini bisa dinonaktifkan jika terlalu ramai di console
		// UE_LOG(LogTemp, Warning, TEXT("CanMoveToTile: Move to (%d, %d) is INVALID."), TargetCoords.X, TargetCoords.Y);
	}

	return bIsMoveValid;
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

            UE_LOG(LogTemp, Warning, TEXT("MovePawn Success: Player %d moved to (%d, %d)"), PlayerNumber, NewX, NewY)

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





