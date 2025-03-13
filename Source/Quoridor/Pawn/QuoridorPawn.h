#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Quoridor/Wall/QuoridorTypes.h"
#include "QuoridorPawn.generated.h"

class ATile;

UCLASS()
class QUORIDOR_API AQuoridorPawn : public APawn {
	GENERATED_BODY()
    
public:    
	AQuoridorPawn();

	UFUNCTION(BlueprintCallable)
	void MoveToTile(ATile* NewTile);

	UFUNCTION(BlueprintCallable)
	bool CanMoveToTile(const ATile* TargetTile) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	ATile* CurrentTile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 PlayerNumber;

protected:
	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* PawnMesh;

	UPROPERTY(VisibleAnywhere)
	class UBoxComponent* SelectionCollision;

private:
	EDirection GetDirectionToTile(const ATile* TargetTile) const;
};