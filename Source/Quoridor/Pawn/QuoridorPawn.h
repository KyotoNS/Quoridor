// QuoridorPawn.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	ATile* CurrentTile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Player")
	int32 PlayerNumber;
	
	UFUNCTION(BlueprintCallable)
	void InitializePawn(ATile* StartTile, AQuoridorBoard* Board);

protected:
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly, Category = "Pawn")
	UStaticMeshComponent* PawnMesh;

	UPROPERTY(VisibleAnywhere)
	class UBoxComponent* SelectionCollision;
};