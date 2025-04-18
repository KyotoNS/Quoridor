﻿
#include "QuoridorBoard.h"

#include "EngineUtils.h"
#include "Quoridor/Pawn/QuoridorPawn.h"
#include "Quoridor/Wall/WallSlot.h"
#include "Quoridor/Wall/WallDefinition.h"
#include "Quoridor/Tile/Tile.h"
#include "Engine/World.h"
#include "Quoridor/Wall/WallPreview.h"

class AWallPreview;

AQuoridorBoard::AQuoridorBoard()
{
	PrimaryActorTick.bCanEverTick = false;
	CurrentPlayerTurn = 1;
}

void AQuoridorBoard::BeginPlay()
{
	Super::BeginPlay();

	// Center the board at (0,0,0)
	const float BoardHalfLength = (GridSize * TileSize) / 2.0f;
	const float HalfTileSize = TileSize / 2.0f;
	const FVector BoardCenter = GetActorLocation();

	// North Wall
	SpawnWall(BoardCenter + FVector(0.0f, BoardHalfLength + HalfTileSize, 0.0f), FRotator::ZeroRotator, FVector(GridSize, 1, 1));

	// South Wall
	SpawnWall(BoardCenter + FVector(0.0f, -BoardHalfLength - HalfTileSize, 0.0f), FRotator::ZeroRotator, FVector(GridSize, 1, 1));

	// East Wall
	SpawnWall(BoardCenter + FVector(BoardHalfLength + HalfTileSize, 0.0f, 0.0f), FRotator(0.0f, 90.0f, 0.0f), FVector(GridSize + 1.32, 1, 1));

	// West Wall
	SpawnWall(BoardCenter + FVector(-BoardHalfLength - HalfTileSize, 0.0f, 0.0f), FRotator(0.0f, 90.0f, 0.0f), FVector(GridSize + 1.32, 1, 1));
    
	// Initialize tiles
	Tiles.SetNum(GridSize);
	for(int32 y = 0; y < GridSize; y++)
	{
		Tiles[y].SetNum(GridSize);
		for(int32 x = 0; x < GridSize; x++)
		{
			// Calculate position relative to board center
			const FVector TileLocation = BoardCenter + FVector(
				(x - GridSize/2) * TileSize,
				(y - GridSize/2) * TileSize,
				0
			);

			ATile* NewTile = GetWorld()->SpawnActor<ATile>(TileClass, TileLocation, FRotator::ZeroRotator);
			NewTile->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
			NewTile->SetGridPosition(x, y);
			Tiles[y][x] = NewTile;
		}
	}

	// Spawn pawns after board is fully initialized
	FTimerHandle SpawnDelayHandle;
	GetWorldTimerManager().SetTimer(SpawnDelayHandle, [this]()
	{
		SpawnPawn(FIntPoint(4, 0), 1); // Player 1
		SpawnPawn(FIntPoint(4, 8), 2); // Player 2
	}, 0.1f, false);

	for (int32 y = 0; y < GridSize - 1; ++y)
{
    for (int32 x = 0; x < GridSize; ++x)
    {
        const FVector Loc = Tiles[y][x]->GetActorLocation() + FVector(0, TileSize / 2, 0);
        AWallSlot* Slot = GetWorld()->SpawnActor<AWallSlot>(WallSlotClass, Loc, FRotator::ZeroRotator);
        Slot->Orientation = EWallOrientation::Horizontal;
        Slot->SetGridPosition(x, y);
        WallSlots.Add(Slot);
    }
}
	// Spawn wall slots (horizontal)
	for (int32 y = 0; y < GridSize - 1; ++y)
	{
		for (int32 x = 0; x < GridSize; ++x)
		{
			const FVector Loc = Tiles[y][x]->GetActorLocation() + FVector(0, TileSize / 2, 0);
			AWallSlot* Slot = GetWorld()->SpawnActor<AWallSlot>(WallSlotClass, Loc, FRotator::ZeroRotator);
			Slot->Orientation = EWallOrientation::Horizontal;
			Slot->SetGridPosition(x, y);
			Slot->Board = this;
			WallSlots.Add(Slot);
		}
	}

	// Spawn wall slots (vertical)
	for (int32 y = 0; y < GridSize; ++y)
	{
		for (int32 x = 0; x < GridSize - 1; ++x)
		{
			const FVector Loc = Tiles[y][x]->GetActorLocation() + FVector(TileSize / 2, 0, 0);
			AWallSlot* Slot = GetWorld()->SpawnActor<AWallSlot>(WallSlotClass, Loc, FRotator(0, 90, 0));
			Slot->Orientation = EWallOrientation::Vertical;
			Slot->SetGridPosition(x, y);
			Slot->Board = this;
			WallSlots.Add(Slot);
		}
	}
}

void AQuoridorBoard::SpawnPawn(FIntPoint GridPosition, int32 PlayerNumber)
{
	if (Tiles.IsValidIndex(GridPosition.Y) && Tiles[GridPosition.Y].IsValidIndex(GridPosition.X))
	{
		ATile* StartTile = Tiles[GridPosition.Y][GridPosition.X];
		if (StartTile && PawnClass)
		{
			// Spawn parameters
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			const FVector SpawnLocation = StartTile->GetActorLocation() + FVector(0, 0, 50);
			AQuoridorPawn* NewPawn = GetWorld()->SpawnActor<AQuoridorPawn>(PawnClass, SpawnLocation, FRotator::ZeroRotator, SpawnParams);
			
			if (NewPawn)
			{
				NewPawn->CurrentTile = StartTile;
				NewPawn->PlayerNumber = PlayerNumber;
				StartTile->SetPawnOnTile(NewPawn);

				// Tambah random wall
				TArray<FWallDefinition> RandomWalls;
				for (int32 i = 0; i < 10; ++i)
				{
					FWallDefinition NewWall;
					NewWall.Length = FMath::RandRange(1, 3); // panjang: 1 - 3
					NewWall.Orientation = FMath::RandBool() ? EWallOrientation::Horizontal : EWallOrientation::Vertical;
					RandomWalls.Add(NewWall);
				}

				NewPawn->PlayerWalls = RandomWalls;
				// Initialize orientation in PlayerOrientations
				PlayerOrientations.Add(NewPawn, EWallOrientation::Horizontal);
			}
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("Spawning pawn at X:%d Y:%d"), GridPosition.X, GridPosition.Y);
}

void AQuoridorBoard::HandlePawnClick(AQuoridorPawn* ClickedPawn)
{
	if (!ClickedPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("HandlePawnClick: ClickedPawn is nullptr"));
		return;
	}

	// Periksa apakah giliran saat ini sesuai dengan nomor pemain pion yang diklik
	if (ClickedPawn->PlayerNumber != CurrentPlayerTurn)
	{
		UE_LOG(LogTemp, Warning, TEXT("HandlePawnClick Failed: Not Player %d's turn (Current turn: Player %d)"),
			ClickedPawn->PlayerNumber, CurrentPlayerTurn);
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red,
			FString::Printf(TEXT("Not Player %d's turn! Current turn: Player %d"),
				ClickedPawn->PlayerNumber, CurrentPlayerTurn));
		SelectedPawn = nullptr; // Pastikan SelectedPawn diatur ulang
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("HandlePawnClick: Pawn clicked for Player %d"), ClickedPawn->PlayerNumber);

	// Pilih pion untuk digerakkan atau untuk menempatkan tembok
	SelectedPawn = ClickedPawn;
}

void AQuoridorBoard::HandleTileClick(ATile* ClickedTile)
{
	if (SelectedPawn && ClickedTile)
	{
		if (SelectedPawn->CanMoveToTile(ClickedTile))
		{
			SelectedPawn->MoveToTile(ClickedTile);
			CurrentPlayerTurn = CurrentPlayerTurn == 1 ? 2 : 1;
		}
		ClearSelection();
	}
}

void AQuoridorBoard::ClearSelection()
{
	SelectedPawn = nullptr;
	// Unhighlight all tiles
}

void AQuoridorBoard::SpawnWall(FVector Location, FRotator Rotation, FVector Scale)
{
	if (WallAroundClass)
	{
		AActor* Wall = GetWorld()->SpawnActor<AActor>(WallAroundClass, Location, Rotation);
		Wall->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
		Wall->SetActorScale3D(Scale);
		BorderWalls.Add(Wall); // Store the border wall
	}
}

bool AQuoridorBoard::TryPlaceWall(AWallSlot* StartSlot, int32 WallLength)
{
    if (!StartSlot)
    {
        UE_LOG(LogTemp, Warning, TEXT("TryPlaceWall Failed: StartSlot is nullptr"));
        return false;
    }
    UE_LOG(LogTemp, Warning, TEXT("TryPlaceWall: StartSlot at X:%d Y:%d"), StartSlot->GridX, StartSlot->GridY);

    if (!bIsPlacingWall)
    {
        UE_LOG(LogTemp, Warning, TEXT("TryPlaceWall Failed: Not in wall placement mode (bIsPlacingWall is false)"));
        return false;
    }
    if (WallLength == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("TryPlaceWall Failed: WallLength is 0"));
        return false;
    }
    UE_LOG(LogTemp, Warning, TEXT("TryPlaceWall: Wall placement mode active, WallLength: %d"), WallLength);

    if (StartSlot->Orientation != PendingWallOrientation)
    {
        UE_LOG(LogTemp, Warning, TEXT("TryPlaceWall Failed: Orientation mismatch. Slot: %s, Player: %s"),
            StartSlot->Orientation == EWallOrientation::Horizontal ? TEXT("Horizontal") : TEXT("Vertical"),
            PendingWallOrientation == EWallOrientation::Horizontal ? TEXT("Horizontal") : TEXT("Vertical"));
        return false;
    }
    UE_LOG(LogTemp, Warning, TEXT("TryPlaceWall: Orientation matched. Slot: %s, Player: %s"),
        StartSlot->Orientation == EWallOrientation::Horizontal ? TEXT("Horizontal") : TEXT("Vertical"),
        PendingWallOrientation == EWallOrientation::Horizontal ? TEXT("Horizontal") : TEXT("Vertical"));

    SelectedPawn = GetPawnForPlayer(CurrentPlayerTurn);
    if (!SelectedPawn)
    {
        UE_LOG(LogTemp, Warning, TEXT("TryPlaceWall Failed: No pawn found for Player %d"), CurrentPlayerTurn);
        return false;
    }
    UE_LOG(LogTemp, Warning, TEXT("TryPlaceWall: Found pawn for Player %d"), CurrentPlayerTurn);

    if (!SelectedPawn->HasWallOfLength(WallLength))
    {
        UE_LOG(LogTemp, Warning, TEXT("TryPlaceWall Failed: Player %d does not have a wall of length %d"), CurrentPlayerTurn, WallLength);
        return false;
    }
    UE_LOG(LogTemp, Warning, TEXT("TryPlaceWall: Player %d has a wall of length %d"), CurrentPlayerTurn, WallLength);

    if (!StartSlot->CanPlaceWall())
    {
        UE_LOG(LogTemp, Warning, TEXT("TryPlaceWall Failed: StartSlot (%d, %d) is occupied"), StartSlot->GridX, StartSlot->GridY);
        return false;
    }
    UE_LOG(LogTemp, Warning, TEXT("TryPlaceWall: StartSlot (%d, %d) is available"), StartSlot->GridX, StartSlot->GridY);

    int32 StartX = StartSlot->GridX;
    int32 StartY = StartSlot->GridY;
    EWallOrientation Orientation = StartSlot->Orientation;
    TArray<AWallSlot*> AffectedSlots;
    AffectedSlots.Add(StartSlot);
    UE_LOG(LogTemp, Warning, TEXT("TryPlaceWall: Checking adjacent slots for length %d, Orientation: %s"),
        WallLength, Orientation == EWallOrientation::Horizontal ? TEXT("Horizontal") : TEXT("Vertical"));

    for (int32 i = 1; i < WallLength; ++i)
    {
        int32 NextX = StartX;
        int32 NextY = StartY;
        if (Orientation == EWallOrientation::Horizontal)
            NextX += i;
        else
            NextY += i;

        // Validasi batas koordinat
        if (Orientation == EWallOrientation::Horizontal && NextX >= GridSize)
        {
            UE_LOG(LogTemp, Warning, TEXT("TryPlaceWall Failed: Next slot (%d, %d) exceeds horizontal bounds (GridSize: %d)"),
                NextX, NextY, GridSize);
            return false;
        }
        if (Orientation == EWallOrientation::Vertical && NextY >= GridSize)
        {
            UE_LOG(LogTemp, Warning, TEXT("TryPlaceWall Failed: Next slot (%d, %d) exceeds vertical bounds (GridSize: %d)"),
                NextX, NextY, GridSize);
            return false;
        }

        AWallSlot* NextSlot = FindWallSlotAt(NextX, NextY, Orientation);
        if (!NextSlot)
        {
            UE_LOG(LogTemp, Warning, TEXT("TryPlaceWall Failed: No slot found at (%d, %d) with Orientation: %s"),
                NextX, NextY, Orientation == EWallOrientation::Horizontal ? TEXT("Horizontal") : TEXT("Vertical"));
            return false;
        }
        if (NextSlot->bIsOccupied)
        {
            UE_LOG(LogTemp, Warning, TEXT("TryPlaceWall Failed: Wall slot (%d, %d) is occupied"), NextX, NextY);
            return false;
        }
        AffectedSlots.Add(NextSlot);
        UE_LOG(LogTemp, Warning, TEXT("TryPlaceWall: Slot (%d, %d) is available"), NextX, NextY);
    }

    FVector BaseLocation = StartSlot->GetActorLocation();
    FRotator WallRotation = Orientation == EWallOrientation::Horizontal
        ? FRotator::ZeroRotator
        : FRotator(0, 90, 0);
    UE_LOG(LogTemp, Warning, TEXT("TryPlaceWall: Checking for border wall collisions"));

    for (int32 i = 0; i < WallLength; ++i)
    {
        FVector SegmentLocation = BaseLocation;
        if (Orientation == EWallOrientation::Horizontal)
            SegmentLocation.X += i * TileSize;
        else
            SegmentLocation.Y += i * TileSize;

        FBox SegmentBounds = FBox::BuildAABB(SegmentLocation, FVector(TileSize / 2, TileSize / 2, 10.0f));
        for (AActor* BorderWall : BorderWalls)
        {
            if (!BorderWall) continue;

            FVector BorderOrigin, BorderExtent;
            BorderWall->GetActorBounds(false, BorderOrigin, BorderExtent);
            FBox BorderBounds = FBox::BuildAABB(BorderOrigin, BorderExtent);

            if (SegmentBounds.Intersect(BorderBounds))
            {
                UE_LOG(LogTemp, Warning, TEXT("TryPlaceWall Failed: Wall segment at (%f, %f) overlaps with border wall"),
                    SegmentLocation.X, SegmentLocation.Y);
                return false;
            }
        }
    }
    UE_LOG(LogTemp, Warning, TEXT("TryPlaceWall: No border wall collisions detected"));

    for (AWallSlot* Slot : AffectedSlots)
    {
        Slot->SetOccupied(true);
        UE_LOG(LogTemp, Warning, TEXT("TryPlaceWall: Marked slot (%d, %d) as occupied"), Slot->GridX, Slot->GridY);
    }

    for (int32 i = 0; i < WallLength; ++i)
    {
        FVector SegmentLocation = BaseLocation;
        if (Orientation == EWallOrientation::Horizontal)
            SegmentLocation.X += i * TileSize;
        else
            SegmentLocation.Y += i * TileSize;

        AActor* NewWall = GetWorld()->SpawnActor<AActor>(WallPlacementClass, SegmentLocation, WallRotation);
        if (NewWall)
        {
            NewWall->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
            UE_LOG(LogTemp, Warning, TEXT("TryPlaceWall: Spawned wall segment at (%f, %f)"), SegmentLocation.X, SegmentLocation.Y);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("TryPlaceWall Failed: Failed to spawn wall segment at (%f, %f)"), SegmentLocation.X, SegmentLocation.Y);
        }
    }

    if (SelectedPawn)
    {
        SelectedPawn->RemoveWallOfLength(WallLength);
        UE_LOG(LogTemp, Warning, TEXT("TryPlaceWall: Removed wall of length %d from Player %d"), WallLength, CurrentPlayerTurn);
    }

    bIsPlacingWall = false;
    PendingWallLength = 0;
    HideWallPreview();
    UE_LOG(LogTemp, Warning, TEXT("TryPlaceWall: Reset wall placement state"));

    CurrentPlayerTurn = CurrentPlayerTurn == 1 ? 2 : 1;
    UE_LOG(LogTemp, Warning, TEXT("TryPlaceWall Success: Wall placed by Player %d at (%d, %d), length %d. Switched turn to Player %d"),
        CurrentPlayerTurn == 1 ? 2 : 1, StartX, StartY, WallLength, CurrentPlayerTurn);

    return true;
}

AWallSlot* AQuoridorBoard::FindWallSlotAt(int32 X, int32 Y, EWallOrientation Orientation)
{
	for (AWallSlot* Slot : WallSlots)
	{
		if (Slot && Slot->GridX == X && Slot->GridY == Y && Slot->Orientation == Orientation)
		{
			UE_LOG(LogTemp, Warning, TEXT("Found WallSlot at X:%d Y:%d, Orientation: %s"),
				X, Y, Orientation == EWallOrientation::Horizontal ? TEXT("Horizontal") : TEXT("Vertical"));
			return Slot;
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("No WallSlot found at X:%d Y:%d, Orientation: %s"),
		X, Y, Orientation == EWallOrientation::Horizontal ? TEXT("Horizontal") : TEXT("Vertical"));
	return nullptr;
}

void AQuoridorBoard::StartWallPlacement(int32 WallLength)
{
	SelectedPawn = GetPawnForPlayer(CurrentPlayerTurn);

	if (!SelectedPawn || !SelectedPawn->HasWallOfLength(WallLength))
	{
		UE_LOG(LogTemp, Warning, TEXT("Player %d does not have wall of length %d"), CurrentPlayerTurn, WallLength);
		return;
	}

	// Toggle cancel jika panjang sama & bIsPlacingWall aktif
	if (bIsPlacingWall && PendingWallLength == WallLength)
	{
		bIsPlacingWall = false;
		PendingWallLength = 0;

		// Reset the player's orientation to Horizontal when canceling
		if (SelectedPawn)
		{
			PlayerOrientations[SelectedPawn] = EWallOrientation::Horizontal;
			PendingWallOrientation = EWallOrientation::Horizontal;
			UE_LOG(LogTemp, Warning, TEXT("Reset Player %d orientation to Horizontal on cancel"), CurrentPlayerTurn);
		}

		// Clear any existing previews
		HideWallPreview();

		UE_LOG(LogTemp, Warning, TEXT("Wall placement canceled."));
		return;
	}

	// Set wall length baru
	PendingWallLength = WallLength;
	bIsPlacingWall = true;
	// Reset orientasi ke default (Horizontal)
	PendingWallOrientation = EWallOrientation::Horizontal;

	// Clear any existing previews when starting placement
	HideWallPreview();

	UE_LOG(LogTemp, Warning, TEXT("Wall selected: Length = %d, Orientation = %s"),
		PendingWallLength,
		PendingWallOrientation == EWallOrientation::Horizontal ? TEXT("Horizontal") : TEXT("Vertical"));
}

int32 AQuoridorBoard::GetCurrentPlayerWallCount(int32 WallLength) const
{
	for (TActorIterator<AQuoridorPawn> It(GetWorld()); It; ++It)
	{
		AQuoridorPawn* Pawn = *It;
		if (Pawn && Pawn->PlayerNumber == CurrentPlayerTurn)
		{
			return Pawn->GetWallCountOfLength(WallLength);
		}
	}
	return 0;
}

void AQuoridorBoard::ShowWallPreviewAtSlot(AWallSlot* HoveredSlot)
{
	if (!bIsPlacingWall || !HoveredSlot || !WallPreviewClass) return;

	if (HoveredSlot->bIsOccupied || HoveredSlot->Orientation != PendingWallOrientation)
		return;

	// Clear any existing previews
	HideWallPreview();

	// Calculate positions for each segment
	FVector BaseLocation = HoveredSlot->GetActorLocation();
	FRotator Rotation = PendingWallOrientation == EWallOrientation::Horizontal
		? FRotator::ZeroRotator
		: FRotator(0, 90, 0);

	for (int32 i = 0; i < PendingWallLength; ++i)
	{
		FVector SegmentLocation = BaseLocation;
		if (PendingWallOrientation == EWallOrientation::Horizontal)
		{
			// Horizontal: Offset in X direction (upwards, face to face)
			SegmentLocation.X += i * TileSize;
		}
		else
		{
			// Vertical: Offset in Y direction (sideways, face to face)
			SegmentLocation.Y += i * TileSize;
		}

		// Spawn a preview segment
		AActor* PreviewSegment = GetWorld()->SpawnActor<AActor>(WallPreviewClass, SegmentLocation, Rotation);
		if (PreviewSegment)
		{
			if (AWallPreview* Preview = Cast<AWallPreview>(PreviewSegment))
			{
				Preview->SetPreviewTransform(SegmentLocation, Rotation, 1, TileSize, PendingWallOrientation);
			}
			WallPreviewActors.Add(PreviewSegment);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("Spawned %d preview segments for length %d, Orientation: %s"),
		WallPreviewActors.Num(), PendingWallLength, PendingWallOrientation == EWallOrientation::Horizontal ? TEXT("Horizontal") : TEXT("Vertical"));
}

void AQuoridorBoard::HideWallPreview()
{
	for (AActor* Preview : WallPreviewActors)
	{
		if (Preview)
		{
			Preview->Destroy();
		}
	}
	WallPreviewActors.Empty();
}

AQuoridorPawn* AQuoridorBoard::GetPawnForPlayer(int32 PlayerNumber)
{
	for (TActorIterator<AQuoridorPawn> It(GetWorld()); It; ++It)
	{
		if (It->PlayerNumber == PlayerNumber)
			return *It;
	}
	return nullptr;
}

void AQuoridorBoard::ToggleWallOrientation()
{
	// If no pawn is selected, default to the current player's pawn
	if (!SelectedPawn)
	{
		SelectedPawn = GetPawnForPlayer(CurrentPlayerTurn);
	}

	if (!SelectedPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("ToggleWallOrientation: No pawn found for Player %d"), CurrentPlayerTurn);
		return;
	}

	// Toggle orientation
	PendingWallOrientation = (PendingWallOrientation == EWallOrientation::Horizontal)
		? EWallOrientation::Vertical
		: EWallOrientation::Horizontal;

	// Update player's orientation
	PlayerOrientations.FindOrAdd(SelectedPawn) = PendingWallOrientation;

	// Log for debugging
	LogAllPlayerOrientations();
}

void AQuoridorBoard::LogAllPlayerOrientations()
{
	UE_LOG(LogTemp, Warning, TEXT("=== Player Orientations ==="));
	for (const TPair<AQuoridorPawn*, EWallOrientation>& Pair : PlayerOrientations)
	{
		FString OrientationStr = (Pair.Value == EWallOrientation::Horizontal) ? TEXT("Horizontal") : TEXT("Vertical");
		FString PlayerName = *Pair.Key->GetName();

		UE_LOG(LogTemp, Warning, TEXT("%s orientation: %s"), *PlayerName, *OrientationStr);
	}
}

EWallOrientation AQuoridorBoard::GetPlayerOrientation(AQuoridorPawn* Pawn) const
{
	if (!Pawn) return EWallOrientation::Horizontal; // default

	if (const EWallOrientation* FoundOrientation = PlayerOrientations.Find(Pawn))
	{
		return *FoundOrientation;
	}

	return EWallOrientation::Horizontal; // default jika belum diset
}














