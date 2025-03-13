#include "Wall.h"
#include "Components/StaticMeshComponent.h"

AWall::AWall() {
	PrimaryActorTick.bCanEverTick = false;
	WallMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WallMesh"));
	RootComponent = WallMesh;
}

void AWall::Initialize(FIntPoint GridPos, EWallOrientation InOrientation) {
	GridPosition = GridPos;
	Orientation = InOrientation;
}