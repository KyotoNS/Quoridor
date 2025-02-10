
#pragma once

#include "CoreMinimal.h"
#include "Tile.h"
#include "GameFramework/Actor.h"
#include "QuoridorBoard.generated.h"

class ATile;

UCLASS()
class QUORIDOR_API AQuoridorBoard : public AActor
{
	GENERATED_BODY()
    
public:    
	AQuoridorBoard();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, Category = "Board")
	TSubclassOf<ATile> TileClass;
	
	TArray<TArray<ATile*>> Tiles;

	UPROPERTY(EditDefaultsOnly, Category = "Board")
	float TileSize = 110.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Board")
	int32 GridSize = 9;
};