#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WallPreview.generated.h"

UCLASS()
class QUORIDOR_API AWallPreview : public AActor
{
	GENERATED_BODY()
    
public:    
	AWallPreview();

	UFUNCTION(BlueprintCallable)
	void SetPreviewTransform(FVector Location, FRotator Rotation, int32 Length, float InTileSize, EWallOrientation Orientation);

protected:
	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* PreviewMesh;
};