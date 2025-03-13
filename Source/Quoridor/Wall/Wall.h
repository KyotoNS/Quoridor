#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "QuoridorTypes.h"
#include "Wall.generated.h"

UCLASS()
class QUORIDOR_API AWall : public AActor {
	GENERATED_BODY()
    
public:    
	AWall();
    
	void Initialize(FIntPoint GridPosition, EWallOrientation Orientation);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	EWallOrientation Orientation;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* WallMesh;

private:
	FIntPoint GridPosition;
};