
#include "QuoridorBoard.h"
#include "Tile.h"
#include "Engine/World.h"

AQuoridorBoard::AQuoridorBoard()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AQuoridorBoard::BeginPlay()
{
	Super::BeginPlay();

	// Initialize 9x9 grid
	Tiles.SetNum(GridSize);
	for(int32 y = 0; y < GridSize; y++)
	{
		Tiles[y].SetNum(GridSize);
		for(int32 x = 0; x < GridSize; x++)
		{
			// Calculate position
			const FVector Location = FVector(
				x * TileSize - (GridSize-1)*TileSize/2, 
				y * TileSize - (GridSize-1)*TileSize/2, 
				5
			);

			// Spawn tile
			ATile* NewTile = GetWorld()->SpawnActor<ATile>(TileClass, Location, FRotator::ZeroRotator);
			NewTile->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
			NewTile->SetGridPosition(x, y);
            
			Tiles[y][x] = NewTile;
		}
	}
}