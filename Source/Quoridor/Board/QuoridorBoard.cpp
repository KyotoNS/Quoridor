
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
	PrimaryActorTick.bCanEverTick = true;
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

	// Spawn wall slots (horizontal)
	for (int32 y = 0; y < GridSize - 1; ++y)
	{
		for (int32 x = 0; x < GridSize; ++x)
		{
			const FVector Loc = Tiles[y][x]->GetActorLocation() + FVector(0, TileSize / 2, 0);
			AWallSlot* Slot = GetWorld()->SpawnActor<AWallSlot>(WallSlotClass, Loc, FRotator::ZeroRotator);
			Slot->Orientation = EWallOrientation::Horizontal;
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
			}
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("Spawning pawn at X:%d Y:%d"), GridPosition.X, GridPosition.Y);
}

void AQuoridorBoard::HandlePawnClick(AQuoridorPawn* ClickedPawn)
{
	if (ClickedPawn && ClickedPawn->PlayerNumber == CurrentPlayerTurn)
	{
		SelectedPawn = ClickedPawn;
		// Highlight possible moves
	}
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
	}
}

bool AQuoridorBoard::TryPlaceWall(AWallSlot* StartSlot, int32 WallLength)
{
	if (!StartSlot || !StartSlot->CanPlaceWall()) return false;

	int32 StartX = StartSlot->GridX;
	int32 StartY = StartSlot->GridY;
	EWallOrientation Orientation = StartSlot->Orientation;

	TArray<AWallSlot*> AffectedSlots;
	AffectedSlots.Add(StartSlot);

	// Cek semua slot yang akan dipakai oleh wall
	for (int32 i = 1; i < WallLength; ++i)
	{
		int32 NextX = StartX;
		int32 NextY = StartY;

		if (Orientation == EWallOrientation::Horizontal)
			NextY += i;
		else
			NextX += i;

		AWallSlot* NextSlot = FindWallSlotAt(NextX, NextY, Orientation);
		if (!NextSlot || NextSlot->bIsOccupied)
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed: Wall slot (%d, %d) is invalid."), NextX, NextY);
			return false;
		}
		AffectedSlots.Add(NextSlot);
	}

	// Tandai semua slot sebagai occupied
	for (AWallSlot* Slot : AffectedSlots)
	{
		Slot->SetOccupied(true);
	}

	// Hitung lokasi tengah
	FVector StartLocation = StartSlot->GetActorLocation();
	FVector EndLocation = AffectedSlots.Last()->GetActorLocation();
	FVector MiddleLocation = (StartLocation + EndLocation) / 2;

	FRotator WallRotation = Orientation == EWallOrientation::Horizontal
		? FRotator::ZeroRotator
		: FRotator(0, 90, 0);

	// Sesuaikan skala wall
	FVector Scale = FVector(WallLength, 1, 1); // Kalau perlu stretch

	// Spawn wall
	AActor* NewWall = GetWorld()->SpawnActor<AActor>(WallPlacementClass, MiddleLocation, WallRotation);
	if (NewWall)
	{
		NewWall->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
		NewWall->SetActorScale3D(Scale);
	}

	// Kurangi wall dari pemain aktif (jika ada sistemnya)
	if (SelectedPawn)
	{
		SelectedPawn->RemoveWallOfLength(WallLength); // opsional
	}

	// Log sukses
	UE_LOG(LogTemp, Warning, TEXT("Wall placed by Player %d at (%d, %d), length %d"), CurrentPlayerTurn, StartX, StartY, WallLength);

	// Ganti giliran
	CurrentPlayerTurn = CurrentPlayerTurn == 1 ? 2 : 1;

	return true;
}


AWallSlot* AQuoridorBoard::FindWallSlotAt(int32 X, int32 Y, EWallOrientation Orientation)
{
	for (AWallSlot* Slot : WallSlots)
	{
		if (Slot && Slot->GridX == X && Slot->GridY == Y && Slot->Orientation == Orientation)
		{
			return Slot;
		}
	}
	return nullptr;
}

void AQuoridorBoard::HandleWallSlotClick(AWallSlot* ClickedSlot)
{
	if (!bIsPlacingWall || !ClickedSlot || !SelectedPawn) return;

	if (TryPlaceWall(ClickedSlot, PendingWallLength))
	{
		SelectedPawn->TakeWallOfLength(PendingWallLength);

		bIsPlacingWall = false;
		PendingWallLength = 0;

		UE_LOG(LogTemp, Warning, TEXT("Wall placed successfully."));
		
		if (WallPreviewActor)
		{
			WallPreviewActor->Destroy();
			WallPreviewActor = nullptr;
		}

	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to place wall."));
	}
}


void AQuoridorBoard::StartWallPlacement(int32 WallLength, EWallOrientation Orientation)
{
	if (!SelectedPawn || !SelectedPawn->HasWallOfLength(WallLength))
		return;

	PendingWallLength = WallLength;
	PendingWallOrientation = Orientation;
	bIsPlacingWall = true;

	if (!WallPreviewActor && WallPreviewClass)
	{
		WallPreviewActor = GetWorld()->SpawnActor<AActor>(WallPreviewClass);
	}
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

void AQuoridorBoard::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bIsPlacingWall || !WallPreviewActor)
		return;

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC) return;

	FHitResult Hit;
	if (PC->GetHitResultUnderCursor(ECC_Visibility, false, Hit))
	{
		AWallSlot* HoveredSlot = Cast<AWallSlot>(Hit.GetActor());

		if (HoveredSlot)
		{
			// ✅ Log saat hover slot, walaupun belum pilih wall
			UE_LOG(LogTemp, Warning, TEXT("Hovering WallSlot at (%d, %d), Orientation: %s"),
				HoveredSlot->GridX,
				HoveredSlot->GridY,
				HoveredSlot->Orientation == EWallOrientation::Horizontal ? TEXT("Horizontal") : TEXT("Vertical"));
		}

		if (HoveredSlot && !HoveredSlot->bIsOccupied && HoveredSlot->Orientation == PendingWallOrientation)
		{
			// --- Update preview wall position ---
			FVector Location = HoveredSlot->GetActorLocation();
			FRotator Rotation = PendingWallOrientation == EWallOrientation::Horizontal
				? FRotator::ZeroRotator
				: FRotator(0, 90, 0);

			if (AWallPreview* Preview = Cast<AWallPreview>(WallPreviewActor))
			{
				Preview->SetPreviewTransform(Location, Rotation, PendingWallLength);
			}

			// --- Check for click to place wall ---
			if (PC->WasInputKeyJustPressed(EKeys::LeftMouseButton))
			{
				if (TryPlaceWall(HoveredSlot, PendingWallLength))
				{
					// Success, stop preview
					bIsPlacingWall = false;
					PendingWallLength = 0;

					if (WallPreviewActor)
					{
						WallPreviewActor->Destroy();
						WallPreviewActor = nullptr;
					}
				}
			}
		}
	}
}








