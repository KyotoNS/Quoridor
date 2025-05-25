
#include "QuoridorBoard.h"

#include "EngineUtils.h"
#include "Quoridor/Pawn/QuoridorPawn.h"
#include "Quoridor/Wall/WallSlot.h"
#include "Quoridor/Wall/WallDefinition.h"
#include "Quoridor/Board/MinimaxBoardAI.h"
#include "Quoridor/Tile/Tile.h"
#include "Engine/World.h"
#include "Quoridor/Wall/WallPreview.h"
#include "Containers/Queue.h"
#include "Containers/Set.h"
#include "Algo/Reverse.h"

class AWallPreview;

AQuoridorBoard::AQuoridorBoard()
{
	PrimaryActorTick.bCanEverTick = true;
	CurrentPlayerTurn = 1;
}
void AQuoridorBoard::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	GEngine->AddOnScreenDebugMessage(1, 0.0f, FColor::Cyan,
		FString::Printf(TEXT("Turn: Player %d"), CurrentPlayerTurn));
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
			HorizontalWallSlots.Add(Slot); 
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
			VerticalWallSlots.Add(Slot); 
		}
	}

	UpdateAllTileConnections();
	
}

void AQuoridorBoard::SpawnPawn(FIntPoint GridPosition, int32 PlayerNumber)
{
    if (Tiles.IsValidIndex(GridPosition.Y) && Tiles[GridPosition.Y].IsValidIndex(GridPosition.X))
    {
        ATile* StartTile = Tiles[GridPosition.Y][GridPosition.X];
        if (StartTile && PawnClass)
        {
            FActorSpawnParameters SpawnParams;
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

            const FVector SpawnLocation = StartTile->GetActorLocation() + FVector(0, 0, 50);
            AQuoridorPawn* NewPawn = GetWorld()->SpawnActor<AQuoridorPawn>(PawnClass, SpawnLocation, FRotator::ZeroRotator, SpawnParams);
            
            if (NewPawn)
            {
                // Inisialisasi pion dengan petak awal dan referensi ke papan
                NewPawn->InitializePawn(StartTile, this);
                NewPawn->PlayerNumber = PlayerNumber;

                // Tambah tembok acak
                TArray<FWallDefinition> RandomWalls;
                for (int32 i = 0; i < 10; ++i)
                {
                    FWallDefinition NewWall;
                    NewWall.Length = FMath::RandRange(1, 3);
                    NewWall.Orientation = FMath::RandBool() ? EWallOrientation::Horizontal : EWallOrientation::Vertical;
                    RandomWalls.Add(NewWall);
                }

                NewPawn->PlayerWalls = RandomWalls;
                PlayerOrientations.Add(NewPawn, EWallOrientation::Horizontal);
            	
            }
          
        }
       
    }
   
}

void AQuoridorBoard::HandlePawnClick(AQuoridorPawn* ClickedPawn)
{

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
		bool bIsAI = (SelectedPawn == GetPawnForPlayer(2));

		if (bIsAI || SelectedPawn->CanMoveToTile(ClickedTile))
		{
			SelectedPawn->MoveToTile(ClickedTile, false);
			CurrentPlayerTurn = (CurrentPlayerTurn == 1) ? 2 : 1;
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
	if (!StartSlot || WallLength == 0)
	{
		return false;
	}

	const bool bIsHumanPlacing = bIsPlacingWall;
	const bool bIsAIPlacing = !bIsHumanPlacing && IsA(AMinimaxBoardAI::StaticClass());

	if (!bIsHumanPlacing && !bIsAIPlacing)
	{
		UE_LOG(LogTemp, Warning, TEXT("TryPlaceWall: Blocked — neither human nor AI placement active"));
		return false;
	}

	if (StartSlot->Orientation != PendingWallOrientation)
	{
		return false;
	}

	SelectedPawn = GetPawnForPlayer(CurrentPlayerTurn);
	if (!SelectedPawn || !SelectedPawn->HasWallOfLength(WallLength))
	{
		return false;
	}

	if (!StartSlot->CanPlaceWall())
	{
		return false;
	}

	// Collect all wall slots that will be affected
	int32 StartX = StartSlot->GridX;
	int32 StartY = StartSlot->GridY;
	EWallOrientation Orientation = StartSlot->Orientation;

	TArray<AWallSlot*> AffectedSlots;
	AWallSlot* FirstSlot = FindWallSlotAt(StartX, StartY, Orientation);
	if (!FirstSlot || FirstSlot->bIsOccupied)
	{
		UE_LOG(LogTemp, Warning, TEXT("TryPlaceWall Failed: First slot invalid or occupied"));
		return false;
	}
	AffectedSlots.Add(FirstSlot);

	for (int32 i = 1; i < WallLength; ++i)
	{
		int32 NextX = StartX + (Orientation == EWallOrientation::Horizontal ? i : 0);
		int32 NextY = StartY + (Orientation == EWallOrientation::Vertical ? i : 0);

		if ((Orientation == EWallOrientation::Horizontal && NextX >= GridSize) ||
			(Orientation == EWallOrientation::Vertical && NextY >= GridSize))
		{
			UE_LOG(LogTemp, Warning, TEXT("TryPlaceWall Failed: Exceeds bounds"));
			return false;
		}
      
		AWallSlot* NextSlot = FindWallSlotAt(NextX, NextY, Orientation);
		if (!NextSlot || NextSlot->bIsOccupied)
		{
			UE_LOG(LogTemp, Warning, TEXT("TryPlaceWall Failed: Next slot invalid or occupied"));
			return false;
		}
		AffectedSlots.Add(NextSlot);
	}

	// Simulate wall block
	TMap<TPair<ATile*, ATile*>, bool> RemovedConnections;
	SimulateWallBlock(AffectedSlots, RemovedConnections);

	bool bPath1 = IsPathAvailableForPawn(GetPawnForPlayer(1));
	bool bPath2 = IsPathAvailableForPawn(GetPawnForPlayer(2));

	if (!bPath1 || !bPath2)
	{
		RevertWallBlock(RemovedConnections);
		UE_LOG(LogTemp, Warning, TEXT("TryPlaceWall Failed: Path would be blocked"));
		return false;
	}

	// Mark all slots as occupied
	for (AWallSlot* Slot : AffectedSlots)
	{
		Slot->SetOccupied(true);
		UE_LOG(LogTemp, Warning, TEXT("Wall segment occupied at (%d, %d) [%s] => Ptr: %p"),
			Slot->GridX, Slot->GridY,
			*UEnum::GetValueAsString(Slot->Orientation), Slot);
	}

	// Visual wall placement
	FVector BaseLocation = StartSlot->GetActorLocation();
	FRotator WallRotation = Orientation == EWallOrientation::Horizontal ? FRotator::ZeroRotator : FRotator(0, 90, 0);

	for (int32 i = 0; i < WallLength; ++i)
	{
		FVector SegmentLocation = BaseLocation + (Orientation == EWallOrientation::Horizontal
			? FVector(i * TileSize, 0, 0)
			: FVector(0, i * TileSize, 0));

		AActor* NewWall = GetWorld()->SpawnActor<AActor>(WallPlacementClass, SegmentLocation, WallRotation);
		if (NewWall)
		{
			NewWall->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
		}
	}

	if (SelectedPawn)
	{
		SelectedPawn->RemoveWallOfLength(WallLength);
	}

	// Cleanup
	bIsPlacingWall = false;
	PendingWallLength = 0;
	HideWallPreview();

	CurrentPlayerTurn = (CurrentPlayerTurn == 1) ? 2 : 1;

	UE_LOG(LogTemp, Warning, TEXT("TryPlaceWall Success: Wall placed. Turn now: Player %d"), CurrentPlayerTurn);
	return true;
}


AWallSlot* AQuoridorBoard::FindWallSlotAt(int32 X, int32 Y, EWallOrientation Orientation)
{
	const TArray<AWallSlot*>& SlotList = (Orientation == EWallOrientation::Horizontal) ? HorizontalWallSlots : VerticalWallSlots;

	for (AWallSlot* Slot : SlotList)
	{
		if (Slot && Slot->GridX == X && Slot->GridY == Y)
		{
			UE_LOG(LogTemp, Warning, TEXT("FindWallSlotAt => Found Slot at (%d, %d) [%s] => Ptr: %p"),
				X, Y, *UEnum::GetValueAsString(Orientation), Slot);
			return Slot;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("FindWallSlotAt => No slot found at (%d, %d) [%s]"),
		X, Y, *UEnum::GetValueAsString(Orientation));
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

bool AQuoridorBoard::IsPathAvailableForPawn(AQuoridorPawn* Pawn)
{
	if (!Pawn || !Pawn->CurrentTile)
		return false;

	struct FNode
	{
		ATile* Tile;
		int32 GCost;
		int32 HCost;
		FNode* Parent;

		FNode(ATile* InTile, int32 InGCost, int32 InHCost, FNode* InParent = nullptr)
			: Tile(InTile), GCost(InGCost), HCost(InHCost), Parent(InParent) {}

		int32 FCost() const { return GCost + HCost; }
	};

	ATile* StartTile = Pawn->CurrentTile;
	const int32 TargetRow = (Pawn->PlayerNumber == 1) ? (GridSize - 1) : 0;

	TArray<FNode*> OpenSet;
	TSet<ATile*> ClosedSet;

	auto Heuristic = [TargetRow](int32 X, int32 Y)
	{
		return FMath::Abs(TargetRow - Y);
	};

	OpenSet.Add(new FNode(StartTile, 0, Heuristic(StartTile->GridX, StartTile->GridY)));

	while (OpenSet.Num() > 0)
	{
		// Ambil node terbaik (lowest F cost)
		FNode* Current = OpenSet[0];
		for (FNode* Node : OpenSet)
		{
			if (Node->FCost() < Current->FCost() ||
				(Node->FCost() == Current->FCost() && Node->HCost < Current->HCost))
			{
				Current = Node;
			}
		}
		OpenSet.Remove(Current);
		ClosedSet.Add(Current->Tile);

		// Check goal
		if ((Pawn->PlayerNumber == 1 && Current->Tile->GridY == TargetRow) ||
			(Pawn->PlayerNumber == 2 && Current->Tile->GridY == TargetRow))
		{
			// Cleanup
			for (FNode* Node : OpenSet) delete Node;
			delete Current;
			return true;
		}

		for (ATile* Neighbor : Current->Tile->ConnectedTiles)
		{
			if (!Neighbor || ClosedSet.Contains(Neighbor) || Neighbor->IsOccupied())
				continue;

			// Tambahkan jika belum di OpenSet
			bool bAlreadyInOpenSet = false;
			for (FNode* Node : OpenSet)
			{
				if (Node->Tile == Neighbor)
				{
					bAlreadyInOpenSet = true;
					break;
				}
			}
			if (!bAlreadyInOpenSet)
			{
				int32 G = Current->GCost + 1;
				int32 H = Heuristic(Neighbor->GridX, Neighbor->GridY);
				OpenSet.Add(new FNode(Neighbor, G, H, Current));
			}
		}

		delete Current;
	}

	for (FNode* Node : OpenSet) delete Node;
	return false; // Tidak ada jalur
}

void AQuoridorBoard::UpdateAllTileConnections()
{
	for (int32 Y = 0; Y < GridSize; ++Y)
	{
		for (int32 X = 0; X < GridSize; ++X)
		{
			ATile* Tile = Tiles[Y][X];
			if (!Tile) continue;

			Tile->ClearConnections();

			// Arah: Atas, Bawah, Kanan, Kiri
			TArray<FIntPoint> Directions = {
				{0, 1},   // Atas
				{0, -1},  // Bawah
				{1, 0},   // Kanan
				{-1, 0}   // Kiri
			};

			for (const FIntPoint& Dir : Directions)
			{
				int32 NX = X + Dir.X;
				int32 NY = Y + Dir.Y;

				if (!Tiles.IsValidIndex(NY) || !Tiles[NY].IsValidIndex(NX))
					continue;

				ATile* Neighbor = Tiles[NY][NX];
				if (!Neighbor) continue;

				// Cek apakah ada wall yang memisahkan
				AWallSlot* WallBetween = nullptr;
				if (Dir.X == 1) // Kanan
					WallBetween = FindWallSlotAt(X, Y, EWallOrientation::Vertical);
				else if (Dir.X == -1) // Kiri
					WallBetween = FindWallSlotAt(X - 1, Y, EWallOrientation::Vertical);
				else if (Dir.Y == 1) // Atas
					WallBetween = FindWallSlotAt(X, Y, EWallOrientation::Horizontal);
				else if (Dir.Y == -1) // Bawah
					WallBetween = FindWallSlotAt(X, Y - 1, EWallOrientation::Horizontal);

				if (WallBetween && WallBetween->bIsOccupied)
					continue;

				// Tambahkan koneksi dua arah
				Tile->AddConnection(Neighbor);
			}
		}
	}
}

void AQuoridorBoard::SimulateWallBlock(const TArray<AWallSlot*>& WallSlotsToSimulate, TMap<TPair<ATile*, ATile*>, bool>& OutRemovedConnections)
{
	for (AWallSlot* Slot : WallSlotsToSimulate)
	{
		if (!Slot) continue;

		int32 X = Slot->GridX;
		int32 Y = Slot->GridY;

		// Tentukan tile yang terhubung oleh wall ini
		ATile* TileA = nullptr;
		ATile* TileB = nullptr;

		if (Slot->Orientation == EWallOrientation::Horizontal)
		{
			if (Tiles.IsValidIndex(Y) && Tiles[Y].IsValidIndex(X) &&
				Tiles.IsValidIndex(Y + 1) && Tiles[Y + 1].IsValidIndex(X))
			{
				TileA = Tiles[Y][X];
				TileB = Tiles[Y + 1][X];
			}
		}
		else // Vertical
		{
			if (Tiles.IsValidIndex(Y) && Tiles[Y].IsValidIndex(X) &&
				Tiles[Y].IsValidIndex(X + 1))
			{
				TileA = Tiles[Y][X];
				TileB = Tiles[Y][X + 1];
			}
		}

		if (TileA && TileB)
		{
			// Simpan koneksi lama untuk bisa di-revert
			if (TileA->ConnectedTiles.Contains(TileB))
			{
				OutRemovedConnections.Add(TPair<ATile*, ATile*>(TileA, TileB), true);
				TileA->RemoveConnection(TileB);
			}

			if (TileB->ConnectedTiles.Contains(TileA))
			{
				OutRemovedConnections.Add(TPair<ATile*, ATile*>(TileB, TileA), true);
				TileB->RemoveConnection(TileA);
			}
		}
	}
}

void AQuoridorBoard::RevertWallBlock(const TMap<TPair<ATile*, ATile*>, bool>& RemovedConnections)
{
	for (const TPair<TPair<ATile*, ATile*>, bool>& Pair : RemovedConnections)
	{
		ATile* A = Pair.Key.Key;
		ATile* B = Pair.Key.Value;

		if (A && B)
		{
			A->AddConnection(B);
		}
	}
}

void AQuoridorBoard::HandleWin(int32 WinningPlayer)
{
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green,
		FString::Printf(TEXT("PLAYER %d WINS!"), WinningPlayer));

	UE_LOG(LogTemp, Warning, TEXT("PLAYER %d WINS!"), WinningPlayer);

	// TODO: Tambahkan logika end game seperti men-disable input, menampilkan UI dsb.
}

















