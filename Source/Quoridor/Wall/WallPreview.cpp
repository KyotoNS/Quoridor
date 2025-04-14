#include "WallPreview.h"
#include "Components/StaticMeshComponent.h"

AWallPreview::AWallPreview()
{
	PrimaryActorTick.bCanEverTick = false;

	PreviewMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PreviewMesh"));
	SetRootComponent(PreviewMesh);

	PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Ensure the mesh is 1 tile long (120 units) in the X-axis
	// If your mesh isn't 120 units, scale it here to match TileSize
	float BaseMeshLength = 120.0f; // Adjust based on your mesh's actual size
	float TileSize = 120.0f; // Match the board's TileSize
	float ScaleFactor = TileSize / BaseMeshLength;
	PreviewMesh->SetRelativeScale3D(FVector(ScaleFactor, 0.1f, 1.0f)); // X (length), Y (thickness), Z (height)

	if (PreviewMesh->GetStaticMesh())
	{
		FBoxSphereBounds Bounds = PreviewMesh->GetStaticMesh()->GetBounds();
		UE_LOG(LogTemp, Warning, TEXT("PreviewMesh bounds: %s"), *Bounds.BoxExtent.ToString());
	}
}

void AWallPreview::SetPreviewTransform(FVector Location, FRotator Rotation, int32 Length, float InTileSize, EWallOrientation Orientation)
{
	SetActorLocation(Location);
	SetActorRotation(Rotation);
	// No scaling here; each preview segment is 1 tile long
}