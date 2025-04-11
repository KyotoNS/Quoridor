
#include "QuoridorBoard.h"
#include "Quoridor/Pawn/QuoridorPawn.h"
#include "Quoridor/Wall/WallSlot.h"
#include "Quoridor/Tile/Tile.h"
#include "Engine/World.h"

AQuoridorBoard::AQuoridorBoard()
{
	PrimaryActorTick.bCanEverTick = false;
	CurrentPlayerTurn = 1;
}

void AQuoridorBoard::BeginPlay()
{
	Super::BeginPlay();

	// Center the board at (0,0,0)
	const float BoardHalfLength = (GridSize * TileSize) / 2.0f;
	const float HalfTileSize = TileSize / 2.0f;
	const FVector BoardCenter = GetActorLocation();

	// North Wall
	SpawnWall(BoardCenter + FVector(0.0f, BoardHalfLength + HalfTileSize, 0.0f), FRotator::ZeroRotator, FVector(GridSize, 1, 1));

	// South Wall
	SpawnWall(BoardCenter + FVector(0.0f, -BoardHalfLength - HalfTileSize, 0.0f), FRotator::ZeroRotator, FVector(GridSize, 1, 1));

	// East Wall
	SpawnWall(BoardCenter + FVector(BoardHalfLength + HalfTileSize, 0.0f, 0.0f), FRotator(0.0f, 90.0f, 0.0f), FVector(GridSize + 1.32, 1, 1));

	// West Wall
	SpawnWall(BoardCenter + FVector(-BoardHalfLength - HalfTileSize, 0.0f, 0.0f), FRotator(0.0f, 90.0f, 0.0f), FVector(GridSize + 1.32, 1, 1));
    
	// Initialize tiles
	Tiles.SetNum(GridSize);
	for(int32 y = 0; y < GridSize; y++)
	{
		Tiles[y].SetNum(GridSize);
		for(int32 x = 0; x < GridSize; x++)
		{
			// Calculate position relative to board center
			const FVector TileLocation = BoardCenter + FVector(
				(x - GridSize/2) * TileSize,
				(y - GridSize/2) * TileSize,
				0
			);

			ATile* NewTile = GetWorld()->SpawnActor<ATile>(TileClass, TileLocation, FRotator::ZeroRotator);
			NewTile->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
			NewTile->SetGridPosition(x, y);
			Tiles[y][x] = NewTile;
		}
	}

	// Spawn pawns after board is fully initialized
	FTimerHandle SpawnDelayHandle;
	GetWorldTimerManager().SetTimer(SpawnDelayHandle, [this]()
	{
		SpawnPawn(FIntPoint(4, 0), 1); // Player 1
		SpawnPawn(FIntPoint(4, 8), 2); // Player 2
	}, 0.1f, false);

	// Spawn wall slots (horizontal)
	for (int32 y = 0; y < GridSize - 1; ++y)
	{
		for (int32 x = 0; x < GridSize; ++x)
		{
			const FVector Loc = Tiles[y][x]->GetActorLocation() + FVector(0, TileSize / 2, 0);
			AWallSlot* Slot = GetWorld()->SpawnActor<AWallSlot>(WallSlotClass, Loc, FRotator::ZeroRotator);
			Slot->Orientation = EWallOrientation::Horizontal;
			WallSlots.Add(Slot);
		}
	}

	// Spawn wall slots (vertical)
	for (int32 y = 0; y < GridSize; ++y)
	{
		for (int32 x = 0; x < GridSize - 1; ++x)
		{
			const FVector Loc = Tiles[y][x]->GetActorLocation() + FVector(TileSize / 2, 0, 0);
			AWallSlot* Slot = GetWorld()->SpawnActor<AWallSlot>(WallSlotClass, Loc, FRotator(0, 90, 0));
			Slot->Orientation = EWallOrientation::Vertical;
			WallSlots.Add(Slot);
		}
	}
	
}

void AQuoridorBoard::SpawnPawn(FIntPoint GridPosition, int32 PlayerNumber)
{
	if (Tiles.IsValidIndex(GridPosition.Y) && Tiles[GridPosition.Y].IsValidIndex(GridPosition.X))
	{
		ATile* StartTile = Tiles[GridPosition.Y][GridPosition.X];
		if (StartTile && PawnClass)
		{
			// Spawn parameters
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            
			// Spawn pawn at tile location
			const FVector SpawnLocation = StartTile->GetActorLocation() + FVector(0, 0, 50);
			AQuoridorPawn* NewPawn = GetWorld()->SpawnActor<AQuoridorPawn>(PawnClass, SpawnLocation, FRotator::ZeroRotator, SpawnParams);
            
			// Initialize pawn
			NewPawn->CurrentTile = StartTile;
			NewPawn->PlayerNumber = PlayerNumber;
			StartTile->SetPawnOnTile(NewPawn);
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("Spawning pawn at X:%d Y:%d"), GridPosition.X, GridPosition.Y);
}
void AQuoridorBoard::HandlePawnClick(AQuoridorPawn* ClickedPawn)
{
	if (ClickedPawn && ClickedPawn->PlayerNumber == CurrentPlayerTurn)
	{
		SelectedPawn = ClickedPawn;
		// Highlight possible moves
	}
}

void AQuoridorBoard::HandleTileClick(ATile* ClickedTile)
{
	if (SelectedPawn && ClickedTile)
	{
		if (SelectedPawn->CanMoveToTile(ClickedTile))
		{
			SelectedPawn->MoveToTile(ClickedTile);
			CurrentPlayerTurn = CurrentPlayerTurn == 1 ? 2 : 1;
		}
		ClearSelection();
	}
}

void AQuoridorBoard::ClearSelection()
{
	SelectedPawn = nullptr;
	// Unhighlight all tiles
}
void AQuoridorBoard::SpawnWall(FVector Location, FRotator Rotation, FVector Scale)
{
	if (WallAroundClass)
	{
		AActor* Wall = GetWorld()->SpawnActor<AActor>(WallAroundClass, Location, Rotation);
		Wall->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
		Wall->SetActorScale3D(Scale);
	}
}

bool AQuoridorBoard::TryPlaceWall(AWallSlot* TargetSlot)
{
	if (TargetSlot && TargetSlot->CanPlaceWall())
	{
		TargetSlot->SetOccupied(true);

		// Visual wall
		SpawnWall(TargetSlot->GetActorLocation(),
			TargetSlot->Orientation == EWallOrientation::Horizontal
				? FRotator::ZeroRotator
				: FRotator(0, 90, 0),
			FVector(1, 1, 1)); // Bisa diatur lebih spesifik ukuran wall-nya

		CurrentPlayerTurn = CurrentPlayerTurn == 1 ? 2 : 1;
		return true;
	}
	return false;
}
