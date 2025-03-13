#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Quoridor/Wall/QuoridorTypes.h"
#include "Tile.generated.h"

UCLASS()
class QUORIDOR_API ATile : public AActor {
	GENERATED_BODY()
    
public:    
	ATile();

	UFUNCTION(BlueprintCallable)
	void SetGridPosition(int32 X, int32 Y);

	UFUNCTION(BlueprintCallable)
	bool HasWall(EDirection Direction) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 GridX;
    
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 GridY;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* TileMesh;

private:
	virtual void BeginPlay() override;
};