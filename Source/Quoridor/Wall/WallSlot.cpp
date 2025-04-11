#include "WallSlot.h"

AWallSlot::AWallSlot()
{
	PrimaryActorTick.bCanEverTick = false;

	UStaticMeshComponent* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SlotMesh"));
	RootComponent = Mesh;
}
