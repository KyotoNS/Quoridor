#include "Tile.h"
#include "Components/StaticMeshComponent.h"
#include "Quoridor/Board/QuoridorBoard.h"
#include "Kismet/GameplayStatics.h"

ATile::ATile() {
	PrimaryActorTick.bCanEverTick = false;
	TileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TileMesh"));
	RootComponent = TileMesh;
}

void ATile::SetGridPosition(int32 X, int32 Y) {
	GridX = X;
	GridY = Y;
}

bool ATile::HasWall(EDirection Direction) const {
	TArray<AActor*> Boards;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AQuoridorBoard::StaticClass(), Boards);
	return Boards.Num() > 0 ? Cast<AQuoridorBoard>(Boards[0])->HasWallBetween(GridX, GridY, Direction) : false;
}