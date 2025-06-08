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
	SelectionCollision->SetBoxExtent(FVector(50, 50, 100));

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


bool AQuoridorPawn::CanMoveToTile(const ATile* TargetTile) const
{
    if (!TargetTile || !CurrentTile || !BoardReference)
    {
        UE_LOG(LogTemp, Warning, TEXT("CanMoveToTile: Invalid input (TargetTile, CurrentTile, or BoardReference is nullptr)"));
        return false;
    }

    if (TargetTile->IsOccupied())
    {
        UE_LOG(LogTemp, Warning, TEXT("CanMoveToTile: TargetTile (%d, %d) is occupied"), TargetTile->GridX, TargetTile->GridY);
        return false;
    }

    // Step 1: Periksa langkah normal (ke petak tetangga yang terhubung)
    if (CurrentTile->ConnectedTiles.Contains(TargetTile))
    {
        UE_LOG(LogTemp, Log, TEXT("CanMoveToTile: Normal move to (%d, %d) is valid"), TargetTile->GridX, TargetTile->GridY);
        return true;
    }

    // Step 2: Periksa lompatan melewati pion lawan
    for (ATile* Neighbor : CurrentTile->ConnectedTiles)
    {
        if (!Neighbor || !Neighbor->IsOccupied())
            continue;

        AQuoridorPawn* OtherPawn = Neighbor->PawnOnTile;
        if (!OtherPawn || OtherPawn == this)
            continue;

        // Pion lawan ditemukan di petak tetangga
        int32 NDeltaX = Neighbor->GridX - CurrentTile->GridX;
        int32 NDeltaY = Neighbor->GridY - CurrentTile->GridY;

        // Tentukan petak di belakang pion lawan (lompatan lurus)
        int32 BehindX = Neighbor->GridX + NDeltaX;
        int32 BehindY = Neighbor->GridY + NDeltaY;
        bool bJumpPossible = false;

        // Periksa apakah petak di belakang valid
        if (BehindX >= 0 && BehindX < BoardReference->GridSize && BehindY >= 0 && BehindY < BoardReference->GridSize)
        {
            ATile* BehindTile = BoardReference->Tiles[BehindY][BehindX];
            if (BehindTile && !BehindTile->IsOccupied() && Neighbor->ConnectedTiles.Contains(BehindTile))
            {
                bJumpPossible = true;
                // Jika TargetTile adalah petak di belakang, lompatan lurus valid
                if (TargetTile == BehindTile)
                {
                    UE_LOG(LogTemp, Log, TEXT("CanMoveToTile: Straight jump to (%d, %d) is valid"), TargetTile->GridX, TargetTile->GridY);
                    return true;
                }
            }
        }

        // Step 3: Periksa side step jika lompatan lurus tidak memungkinkan
        if (!bJumpPossible)
        {
            for (ATile* SideTile : Neighbor->ConnectedTiles)
            {
                if (!SideTile || SideTile == CurrentTile || SideTile->IsOccupied())
                    continue;

                // Pastikan side step adalah kiri/kanan relatif terhadap arah lompatan
                bool bIsSideStep = false;
                if (NDeltaX != 0) // Lompatan horizontal, side step adalah vertikal (kiri/kanan)
                {
                    bIsSideStep = (SideTile->GridX == Neighbor->GridX && SideTile->GridY != Neighbor->GridY);
                }
                else if (NDeltaY != 0) // Lompatan vertikal, side step adalah horizontal (kiri/kanan)
                {
                    bIsSideStep = (SideTile->GridY == Neighbor->GridY && SideTile->GridX != Neighbor->GridX);
                }

                if (bIsSideStep && SideTile == TargetTile)
                {
                    UE_LOG(LogTemp, Log, TEXT("CanMoveToTile: Side step to (%d, %d) is valid"), SideTile->GridX, SideTile->GridY);
                    return true;
                }
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("CanMoveToTile: No valid move to (%d, %d)"), TargetTile->GridX, TargetTile->GridY);
    return false;
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





