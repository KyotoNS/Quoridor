// QuoridorPawn.cpp
#include "QuoridorPawn.h"
#include "Quoridor/Tile/Tile.h"
#include "Components/BoxComponent.h"

AQuoridorPawn::AQuoridorPawn()
{
	PrimaryActorTick.bCanEverTick = false;

	PawnMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PawnMesh"));
	RootComponent = PawnMesh;

	SelectionCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("SelectionCollision"));
	SelectionCollision->SetupAttachment(RootComponent);
	SelectionCollision->SetBoxExtent(FVector(50, 50, 100));
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


