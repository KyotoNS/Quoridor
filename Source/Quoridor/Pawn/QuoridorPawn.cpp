#include "QuoridorPawn.h"
#include "Quoridor/Tile/Tile.h"
#include "Components/BoxComponent.h"

// Constructor: Initializes the pawn's components
AQuoridorPawn::AQuoridorPawn()
{
	PrimaryActorTick.bCanEverTick = false;  // The pawn does not update every frame

	// Creates the visual mesh of the pawn
	PawnMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PawnMesh"));
	RootComponent = PawnMesh;  // Set the mesh as the root component

	// Creates a collision box for selecting the pawn with the mouse
	SelectionCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("SelectionCollision"));
	SelectionCollision->SetupAttachment(RootComponent);  // Attach to the pawn mesh
	SelectionCollision->SetBoxExtent(FVector(50, 50, 100));  // Sets the size of the collision box
}

// Moves the pawn to a new tile if it's a valid move
void AQuoridorPawn::MoveToTile(ATile* NewTile)
{
	// Ensure the target tile exists and is a valid move
	if (NewTile && CanMoveToTile(NewTile))
	{
		// Remove the pawn from the current tile
		if (CurrentTile) 
		{
			CurrentTile->SetPawnOnTile(nullptr);
		}
        
		// Update the pawn's current tile reference
		CurrentTile = NewTile;
		CurrentTile->SetPawnOnTile(this);  // Assign the pawn to the new tile
        
		// Move the pawn slightly above the tile
		const FVector NewLocation = CurrentTile->GetActorLocation() + FVector(0, 0, 50);
		SetActorLocation(NewLocation);
        
		// Debug log to confirm the move
		UE_LOG(LogTemp, Warning, TEXT("Moved pawn to: %s"), *NewLocation.ToString());
	}
}

// Checks if the pawn can move to a given tile
bool AQuoridorPawn::CanMoveToTile(const ATile* TargetTile) const
{
	// The move is valid if:
	// 1. The target tile exists
	// 2. The target tile is NOT occupied by another pawn
	// 3. The target tile is adjacent to the current tile
	return TargetTile && 
		  !TargetTile->IsOccupied() && 
		  CurrentTile->IsAdjacent(TargetTile);
}

// Initializes the pawn's starting position and references the board
void AQuoridorPawn::InitializePawn(ATile* StartTile, AQuoridorBoard* Board)
{
	// Ensure the starting tile exists
	if (StartTile)
	{
		CurrentTile = StartTile;  // Set the pawn's starting tile
		BoardReference = Board;   // Store a reference to the game board

		// Position the pawn slightly above the tile
		SetActorLocation(StartTile->GetActorLocation() + FVector(0, 0, 50));

		// Assign the pawn to the tile
		StartTile->SetPawnOnTile(this);
	}
}
