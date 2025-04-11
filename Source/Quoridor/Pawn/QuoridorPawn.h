
#pragma once  // Prevents multiple inclusions of this header file

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "QuoridorPawn.generated.h"

// Forward declaration of ATile class
class ATile;

// Declares the AQuoridorPawn class, which represents a player's pawn
UCLASS()
class QUORIDOR_API AQuoridorPawn : public APawn
{
	GENERATED_BODY()

public:
	// Constructor
	AQuoridorPawn();
	
	// Reference to the game board (used for move validation, turn handling, etc.)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	AQuoridorBoard* BoardReference;

	// Moves the pawn to a new tile if valid
	UFUNCTION(BlueprintCallable)
	void MoveToTile(ATile* NewTile);

	// Checks if the pawn can move to a given tile
	UFUNCTION(BlueprintCallable)
	bool CanMoveToTile(const ATile* TargetTile) const;

	// Pointer to the tile where the pawn is currently located
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	ATile* CurrentTile;

	// The number assigned to the player controlling this pawn (1 or 2)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player")
	int32 PlayerNumber;
	
	// Initializes the pawn's position and assigns it to a tile and board
	UFUNCTION(BlueprintCallable)
	void InitializePawn(ATile* StartTile, AQuoridorBoard* Board);

protected:
	// Visual representation of the pawn
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pawn")
	UStaticMeshComponent* PawnMesh;

	// Collision box for detecting pawn selection via mouse click
	UPROPERTY(VisibleAnywhere)
	class UBoxComponent* SelectionCollision;
};
