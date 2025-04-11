
#include "QuoridorBoard.h"
#include "Quoridor/Pawn/QuoridorPawn.h"
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
	const FVector BoardCenter = GetActorLocation();
    
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