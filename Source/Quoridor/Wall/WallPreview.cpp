#include "WallPreview.h"
#include "Components/StaticMeshComponent.h"

AWallPreview::AWallPreview()
{
	PrimaryActorTick.bCanEverTick = false;

	PreviewMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PreviewMesh"));
	SetRootComponent(PreviewMesh);

	// Disable collision for preview
	PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AWallPreview::SetPreviewTransform(FVector Location, FRotator Rotation, int32 Length)
{
	SetActorLocation(Location);
	SetActorRotation(Rotation);

	// Scale preview mesh based on length
	SetActorScale3D(FVector(Length, 1.0f, 1.0f));
}
