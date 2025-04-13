//QuoridorPawn.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Quoridor/Wall/WallDefinition.h"
#include "QuoridorPawn.generated.h"

class ATile;

UCLASS()
class QUORIDOR_API AQuoridorPawn : public APawn
{
	GENERATED_BODY()

public:
	AQuoridorPawn();
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	AQuoridorBoard* BoardReference;

	UFUNCTION(BlueprintCallable)
	void MoveToTile(ATile* NewTile);

	UFUNCTION(BlueprintCallable)
	bool CanMoveToTile(const ATile* TargetTile) const;

	UFUNCTION(BlueprintCallable)
	bool RemoveWallOfLength(int32 INT32);

	UFUNCTION(BlueprintCallable)
	bool HasWallOfLength(int32 Length) const;
	

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	ATile* CurrentTile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Player")
	int32 PlayerNumber;

	UPROPERTY(BlueprintReadOnly)
	TArray<FWallDefinition> PlayerWalls;

	UFUNCTION(BlueprintCallable)
	bool HasRemainingWalls() const { return PlayerWalls.Num() > 0; }
	
	UFUNCTION(BlueprintCallable)
	void InitializePawn(ATile* StartTile, AQuoridorBoard* Board);
	
	UFUNCTION(BlueprintCallable)
	int32 GetWallCountOfLength(int32 Length) const;

	UFUNCTION(BlueprintCallable)
	FWallDefinition TakeWallOfLength(int32 Length);


protected:
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly, Category = "Pawn")
	UStaticMeshComponent* PawnMesh;

	UPROPERTY(VisibleAnywhere)
	class UBoxComponent* SelectionCollision;
};