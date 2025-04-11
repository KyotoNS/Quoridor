#include "QuoridorBoard.h"
#include "Quoridor/Pawn/QuoridorPawn.h"
#include "Quoridor/Tile/Tile.h"
#include "Engine/World.h"

// Constructor: Initializes the board and sets the default player turn
AQuoridorBoard::AQuoridorBoard()
{
	PrimaryActorTick.bCanEverTick = false;  // Board does not update every frame
	CurrentPlayerTurn = 1;  // Player 1 starts first
}

// Called when the game starts or when spawned
void AQuoridorBoard::BeginPlay()
{
	Super::BeginPlay();

	// Position the board at the world origin
	const FVector BoardCenter = GetActorLocation();
    
	// Initialize a 9x9 grid of tiles
	Tiles.SetNum(GridSize);
	for(int32 y = 0; y < GridSize; y++)
	{
		Tiles[y].SetNum(GridSize);
		for(int32 x = 0; x < GridSize; x++)
		{
			// Calculate the tile's position relative to the board center
			const FVector TileLocation = BoardCenter + FVector(
				(x - GridSize / 2) * TileSize,
				(y - GridSize / 2) * TileSize,
				0
			);

			// Spawn a new tile at the calculated position
			ATile* NewTile = GetWorld()->SpawnActor<ATile>(TileClass, TileLocation, FRotator::ZeroRotator);
			NewTile->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
			NewTile->SetGridPosition(x, y);  // Assign X and Y coordinates
			Tiles[y][x] = NewTile;
		}
	}

	// Spawn the player pawns after a small delay to ensure the board is initialized
	FTimerHandle SpawnDelayHandle;
	GetWorldTimerManager().SetTimer(SpawnDelayHandle, [this]()
	{
		SpawnPawn(FIntPoint(4, 0), 1); // Player 1 (starts at row 0)
		SpawnPawn(FIntPoint(4, 8), 2); // Player 2 (starts at row 8)
	}, 0.1f, false);
}

// Spawns a pawn for a specific player at a given grid position
void AQuoridorBoard::SpawnPawn(FIntPoint GridPosition, int32 PlayerNumber)
{
	// Ensure the tile exists in the grid
	if (Tiles.IsValidIndex(GridPosition.Y) && Tiles[GridPosition.Y].IsValidIndex(GridPosition.X))
	{
		ATile* StartTile = Tiles[GridPosition.Y][GridPosition.X];
		if (StartTile && PawnClass)
		{
			// Spawn parameters to ensure the pawn is always created
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            
			// Adjust the pawn's position to be slightly above the tile
			const FVector SpawnLocation = StartTile->GetActorLocation() + FVector(0, 0, 50);
			AQuoridorPawn* NewPawn = GetWorld()->SpawnActor<AQuoridorPawn>(PawnClass, SpawnLocation, FRotator::ZeroRotator, SpawnParams);
            
			// Assign the pawn to the tile
			NewPawn->CurrentTile = StartTile;
			NewPawn->PlayerNumber = PlayerNumber;
			StartTile->SetPawnOnTile(NewPawn);
		}
	}
	
	// Log pawn spawn position for debugging
	UE_LOG(LogTemp, Warning, TEXT("Spawning pawn at X:%d Y:%d"), GridPosition.X, GridPosition.Y);
}

// Handles when a pawn is clicked
void AQuoridorBoard::HandlePawnClick(AQuoridorPawn* ClickedPawn)
{
	// Check if the clicked pawn belongs to the current player
	if (ClickedPawn && ClickedPawn->PlayerNumber == CurrentPlayerTurn)
	{
		SelectedPawn = ClickedPawn;  // Set the selected pawn
		// (Optional: Highlight possible moves here)
	}
}

// Handles when a tile is clicked
void AQuoridorBoard::HandleTileClick(ATile* ClickedTile)
{
	// Ensure a pawn is selected and a valid tile was clicked
	if (SelectedPawn && ClickedTile)
	{
		// Check if the pawn can move to the selected tile
		if (SelectedPawn->CanMoveToTile(ClickedTile))
		{
			SelectedPawn->MoveToTile(ClickedTile);  // Move the pawn to the new tile

			// Switch turns: If it was Player 1, switch to Player 2 and vice versa
			CurrentPlayerTurn = (CurrentPlayerTurn == 1) ? 2 : 1;
		}

		// Clear the selection state after the move
		ClearSelection();
	}
}

// Clears the current selection (removes pawn selection)
void AQuoridorBoard::ClearSelection()
{
	SelectedPawn = nullptr;
	// (Optional: Unhighlight all tiles here)
}
