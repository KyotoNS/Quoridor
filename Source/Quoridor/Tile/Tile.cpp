#include "Tile.h"
#include "Components/StaticMeshComponent.h"

// Constructor: Initializes the tile, sets up the mesh, and ensures it's visible in the game.
ATile::ATile()
{
	PrimaryActorTick.bCanEverTick = false;

	// Create a static mesh component for visualization
	TileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TileMesh"));
	RootComponent = TileMesh;

	// Ensure the tile is not hidden in the game
	if (TileMesh)
	{
		TileMesh->SetHiddenInGame(false);
	}
}

// Checks if another tile is adjacent to this one (horizontally or vertically)
bool ATile::IsAdjacent(const ATile* OtherTile) const
{
	if (!OtherTile) return false;

	// A tile is adjacent if it differs by exactly 1 in X or Y
	return (FMath::Abs(GridX - OtherTile->GridX) + 
		   FMath::Abs(GridY - OtherTile->GridY)) == 1;
}
