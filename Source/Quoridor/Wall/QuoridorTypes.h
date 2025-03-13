#pragma once
#include "CoreMinimal.h"
#include "QuoridorTypes.generated.h"

UENUM(BlueprintType)
enum class EDirection : uint8 {
	North UMETA(DisplayName = "North"),
	East  UMETA(DisplayName = "East"),
	South UMETA(DisplayName = "South"),
	West  UMETA(DisplayName = "West"),
	None  UMETA(DisplayName = "None")
};

UENUM(BlueprintType)
enum class EWallOrientation : uint8 {
	Horizontal UMETA(DisplayName = "Horizontal"),
	Vertical   UMETA(DisplayName = "Vertical")
};