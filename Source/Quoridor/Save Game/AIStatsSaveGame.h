// AIStatsSaveGame.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "AIStatsSaveGame.generated.h"

UCLASS()
class QUORIDOR_API UAIStatsSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, Category="AI Stats")
	int32 TotalTurns;

	UPROPERTY(VisibleAnywhere, Category="AI Stats")
	float TotalThinkingTime;

	UPROPERTY(VisibleAnywhere, Category="AI Stats")
	int32 AIPlayerNumber;

	UPROPERTY(VisibleAnywhere, Category="AI Stats")
	TArray<int32> WallInventoryAtStart;  // Index 0 = L1, 1 = L2, 2 = L3

	UPROPERTY(VisibleAnywhere, Category="AI Stats")
	FString AIType;

	UPROPERTY(VisibleAnywhere, Category="AI Stats")
	FDateTime Timestamp;
};
