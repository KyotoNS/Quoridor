#include "QuoridorPawn.h"
#include "Quoridor/Tile/Tile.h"
#include "Components/BoxComponent.h"

AQuoridorPawn::AQuoridorPawn() {
	PrimaryActorTick.bCanEverTick = false;
	PawnMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PawnMesh"));
	RootComponent = PawnMesh;
	SelectionCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("SelectionCollision"));
	SelectionCollision->SetupAttachment(RootComponent);
}

void AQuoridorPawn::MoveToTile(ATile* NewTile) {
	if(NewTile && CanMoveToTile(NewTile)) {
		CurrentTile = NewTile;
		SetActorLocation(NewTile->GetActorLocation() + FVector(0,0,50));
	}
}

bool AQuoridorPawn::CanMoveToTile(const ATile* TargetTile) const {
	return TargetTile && 
		  !TargetTile->IsOccupied() && 
		  CurrentTile->IsAdjacent(TargetTile) &&
		  !CurrentTile->HasWall(GetDirectionToTile(TargetTile));
}

EDirection AQuoridorPawn::GetDirectionToTile(const ATile* TargetTile) const {
	const int32 DX = TargetTile->GridX - CurrentTile->GridX;
	const int32 DY = TargetTile->GridY - CurrentTile->GridY;
    
	if(DX == 1) return EDirection::East;
	if(DX == -1) return EDirection::West;
	if(DY == 1) return EDirection::North;
	if(DY == -1) return EDirection::South;
    
	return EDirection::None;
}