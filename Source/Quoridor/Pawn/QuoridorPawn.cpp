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

FWallDefinition AQuoridorPawn::GetNextWall()
{
	if (PlayerWalls.Num() > 0)
	{
		FWallDefinition Wall = PlayerWalls[0];
		PlayerWalls.RemoveAt(0);
		return Wall;
	}
	
	// Return default wall jika kosong (harus dijaga validasinya)
	return FWallDefinition();
}

bool AQuoridorPawn::HasWallOfLength(int32 Length) const
{
	return PlayerWalls.ContainsByPredicate([Length](const FWallDefinition& Wall)
	{
		return Wall.Length == Length;
	});
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


FWallDefinition AQuoridorPawn::TakeWallOfLength(int32 Length)
{
	for (int32 i = 0; i < PlayerWalls.Num(); ++i)
	{
		if (PlayerWalls[i].Length == Length)
		{
			FWallDefinition Wall = PlayerWalls[i];
			PlayerWalls.RemoveAt(i);
			return Wall;
		}
	}
	return FWallDefinition(); // Return default if not found
}




