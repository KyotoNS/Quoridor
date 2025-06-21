
#include "QuoridorBoard.h"

#include "AI_VS_AI.h"
#include "EngineUtils.h"
#include "Quoridor/Pawn/QuoridorPawn.h"
#include "Quoridor/Wall/WallSlot.h"
#include "Quoridor/Wall/WallDefinition.h"
#include "Quoridor/Board/MinimaxBoardAI.h"
#include "Kismet/GameplayStatics.h"
#include "Quoridor/Tile/Tile.h"
#include "Engine/World.h"
#include "Quoridor/Board/MinimaxEngine.h"
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
	bIsGameFinished = false;
	WinningTurn = 0;

	// // North Wall
	// SpawnWall(BoardCenter + FVector(0.0f, BoardHalfLength + HalfTileSize, 0.0f), FRotator::ZeroRotator, FVector(GridSize, 1, 1));
	//
	// // South Wall
	// SpawnWall(BoardCenter + FVector(0.0f, -BoardHalfLength - HalfTileSize, 0.0f), FRotator::ZeroRotator, FVector(GridSize, 1, 1));
	//
	// // East Wall
	// SpawnWall(BoardCenter + FVector(BoardHalfLength + HalfTileSize, 0.0f, 0.0f), FRotator(0.0f, 90.0f, 0.0f), FVector(GridSize + 1.32, 1, 1));
	//
	// // West Wall
	// SpawnWall(BoardCenter + FVector(-BoardHalfLength - HalfTileSize, 0.0f, 0.0f), FRotator(0.0f, 90.0f, 0.0f), FVector(GridSize + 1.32, 1, 1));
    
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
            	if (AAI_VS_AI* AI = Cast<AAI_VS_AI>(this))
            	{
            		if (PlayerNumber == AI->AI1Player || PlayerNumber == AI->AI2Player)
            		{
            			TArray<int32> LengthCounts = {0, 0, 0}; // L1, L2, L3
            			for (const FWallDefinition& Wall : RandomWalls)
            			{
            				if (Wall.Length >= 1 && Wall.Length <= 3)
            					LengthCounts[Wall.Length - 1]++;
            			}

            			AI->InitialWallInventory = LengthCounts;
            		}
            	}
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
	// Hanya proses jika ada pion yang dipilih dan tile yang valid
	if (!SelectedPawn || !ClickedTile)
	{
		return;
	}

	// Cek apakah gerakan valid menggunakan fungsi CanMoveToTile
	if (SelectedPawn->CanMoveToTile(ClickedTile))
	{
		// --- GERAKAN VALID ---
		UE_LOG(LogTemp, Log, TEXT("HandleTileClick: Move is valid. Executing..."));

		// Mainkan suara
		if (PawnMoveSound)
		{
			UGameplayStatics::PlaySound2D(this, PawnMoveSound);
		}

		// Pindahkan pion
		SelectedPawn->MoveToTile(ClickedTile, false);

		// Ganti giliran
		CurrentPlayerTurn = (CurrentPlayerTurn == 1) ? 2 : 1;
		TurnCount++;

		// Bersihkan seleksi HANYA SETELAH aksi berhasil
		ClearSelection();
	}
	else
	{
		// --- GERAKAN TIDAK VALID ---
		// Biarkan pion tetap terpilih agar pemain bisa mencoba lagi.
		// Jangan panggil ClearSelection() di sini.
		// Anda bisa memberikan feedback ke pemain.
		UE_LOG(LogTemp, Warning, TEXT("HandleTileClick: Move is invalid."));
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("Gerakan tidak valid!"));
        
		// Anda juga bisa memainkan suara "error" di sini jika punya
		// if (ErrorSound) { UGameplayStatics::PlaySound2D(this, ErrorSound); }
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
    const bool bIsAIPlacing = !bIsHumanPlacing && IsA(AAI_VS_AI::StaticClass());

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

    // 1. Simulate the wall placement by temporarily marking slots as occupied.
    for (AWallSlot* Slot : AffectedSlots)
    {
        Slot->SetOccupied(true);
    }

    // 2. Create a Minimax state from this simulated board.
    FMinimaxState SimulatedState = FMinimaxState::FromBoard(this);

    // 3. Check paths for both players using the Minimax engine's pathfinder.
    int32 PathLen1 = 100; // Default to "no path"
    int32 PathLen2 = 100;
    MinimaxEngine::ComputePathToGoal(SimulatedState, 1, &PathLen1);
    MinimaxEngine::ComputePathToGoal(SimulatedState, 2, &PathLen2);

    UE_LOG(LogTemp, Log, TEXT("Path check after simulation: Player 1 Length = %d, Player 2 Length = %d"), PathLen1, PathLen2);

    // 4. If either path is blocked (length >= 100 signifies no path), revert the simulation and fail.
    if (PathLen1 >= 100 || PathLen2 >= 100)
    {
        // Revert the temporary change by un-occupying the slots.
        for (AWallSlot* Slot : AffectedSlots)
        {
            Slot->SetOccupied(false);
        }
        UE_LOG(LogTemp, Warning, TEXT("TryPlaceWall Failed: Path would be blocked (checked with MinimaxEngine)"));
        return false;
    }

	if (WallClickSound)
	{
		UGameplayStatics::PlaySound2D(this, WallClickSound);
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
    TurnCount++;

    UE_LOG(LogTemp, Warning, TEXT("TryPlaceWall Success: Wall placed. Turn now: Player %d"), CurrentPlayerTurn);
    return true;
}


AWallSlot* AQuoridorBoard::FindWallSlotAt(int32 X, int32 Y, EWallOrientation Orientation) const
{
	// Buat sebuah pointer yang akan menunjuk ke TArray yang benar.
	// Pointer ini menunjuk ke sebuah TArray konstan.
	const TArray<AWallSlot*>* pSlotList = nullptr;

	if (Orientation == EWallOrientation::Horizontal)
	{
		// Arahkan pointer ke HorizontalWallSlots
		pSlotList = &HorizontalWallSlots;
	}
	else
	{
		// Arahkan pointer ke VerticalWallSlots
		pSlotList = &VerticalWallSlots;
	}

	// Selalu baik untuk memeriksa apakah pointer valid sebelum digunakan
	if (pSlotList)
	{
		// Gunakan operator dereference (*) untuk mengakses isi TArray yang ditunjuk oleh pointer
		for (AWallSlot* Slot : *pSlotList)
		{
			if (Slot && Slot->GridX == X && Slot->GridY == Y)
			{
				UE_LOG(LogTemp, Warning, TEXT("FindWallSlotAt => Found Slot at (%d, %d) [%s] => Ptr: %p"),
					X, Y, *UEnum::GetValueAsString(Orientation), Slot);
				return Slot;
			}
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
int32 AQuoridorBoard::GetPlayerTotalWallCount(int32 PlayerNumber) const
{
	// Memeriksa jika nomor pemain yang diberikan valid (opsional, tapi praktik yang baik)
	if (PlayerNumber <= 0)
	{
		return 0; // Nomor pemain tidak valid
	}

	// Melakukan iterasi pada semua AQuoridorPawn di dalam world
	for (TActorIterator<AQuoridorPawn> It(GetWorld()); It; ++It)
	{
		AQuoridorPawn* Pawn = *It;

		// Memeriksa apakah Pawn valid dan PlayerNumber-nya cocok dengan yang dicari
		if (Pawn && Pawn->PlayerNumber == PlayerNumber)
		{
			// Jika cocok, kembalikan total jumlah dinding dari Pawn tersebut.
			// Fungsi ini bergantung pada GetTotalWallCount() di kelas AQuoridorPawn.
			return Pawn->GetTotalWallCount();
		}
	}

	// Mengembalikan 0 jika Pawn dengan PlayerNumber tersebut tidak ditemukan
	return 0;
}
int32 AQuoridorBoard::GetWallCountForPlayer(int32 PlayerNum, int32 WallLength) const
{
	// Loop melalui semua aktor AQuoridorPawn di dalam world
	for (TActorIterator<AQuoridorPawn> It(GetWorld()); It; ++It)
	{
		AQuoridorPawn* Pawn = *It;

		// Cek apakah pion ini adalah milik pemain yang kita cari (sesuai parameter)
		if (Pawn && Pawn->PlayerNumber == PlayerNum) // <-- PERUBAHAN UTAMA DI SINI
		{
			// Jika ya, panggil fungsi di pion tersebut dan kembalikan hasilnya
			return Pawn->GetWallCountOfLength(WallLength);
		}
	}

	// Jika setelah loop selesai pion untuk pemain tersebut tidak ditemukan, kembalikan 0
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
        int32  GCost;
        int32  HCost;
        FNode* Parent;

        FNode(ATile* InTile, int32 InGCost, int32 InHCost, FNode* InParent = nullptr)
            : Tile(InTile), GCost(InGCost), HCost(InHCost), Parent(InParent) {}

        int32 FCost() const { return GCost + HCost; }
    };

    ATile* StartTile = Pawn->CurrentTile;
    const int32 TargetRow = (Pawn->PlayerNumber == 1) ? (GridSize - 1) : 0;

    TArray<FNode*> OpenSet;
    TSet<ATile*> ClosedSet;
    TArray<FNode*> AllNodes;

    auto Heuristic = [TargetRow](int32 X, int32 Y)
    {
        return FMath::Abs(TargetRow - Y);
    };

    FNode* StartNode = new FNode(StartTile, 0, Heuristic(StartTile->GridX, StartTile->GridY), nullptr);
    OpenSet.Add(StartNode);
    AllNodes.Add(StartNode);

    while (OpenSet.Num() > 0)
    {
        FNode* Current = OpenSet[0];
        for (FNode* Node : OpenSet)
        {
            if (Node->FCost() < Current->FCost() || (Node->FCost() == Current->FCost() && Node->HCost < Current->HCost))
            {
                Current = Node;
            }
        }

        OpenSet.Remove(Current);
        ClosedSet.Add(Current->Tile);

        if ((Pawn->PlayerNumber == 1 && Current->Tile->GridY == TargetRow) ||
            (Pawn->PlayerNumber == 2 && Current->Tile->GridY == TargetRow))
        {
            TArray<FIntPoint> PathTiles;
            FNode* PathNode = Current;
            while (PathNode)
            {
                if (PathNode->Tile)
                {
                    PathTiles.Insert(FIntPoint(PathNode->Tile->GridX, PathNode->Tile->GridY), 0);
                }
                PathNode = PathNode->Parent;
            }

            for (FNode* NodePtr : AllNodes) delete NodePtr;
            AllNodes.Empty();
            OpenSet.Empty();

            CachedPaths.Add(Pawn->PlayerNumber, PathTiles);
            return true;
        }

        TArray<ATile*> CandidateMoves;

        for (ATile* Neighbor : Current->Tile->ConnectedTiles)
        {
            if (!Neighbor)
                continue;

            AQuoridorPawn* BlockingPawn = Neighbor->GetOccupyingPawn();
            if (BlockingPawn)
            {
                // Attempt to jump over the pawn
                FVector Dir = Neighbor->GetActorLocation() - Current->Tile->GetActorLocation();
                FVector JumpTargetLoc = Neighbor->GetActorLocation() + Dir;

                ATile* JumpTile = GetTileAtWorldPosition(JumpTargetLoc);
                if (JumpTile && Neighbor->ConnectedTiles.Contains(JumpTile) && !JumpTile->IsOccupied())
                {
                    CandidateMoves.Add(JumpTile); // Jump over
                }
                else
                {
                    // Side steps
                    FVector SideStep1 = FVector::CrossProduct(Dir.GetSafeNormal(), FVector::UpVector) * TileSize;
                    FVector SideStep2 = -SideStep1;

                    ATile* SideTile1 = GetTileAtWorldPosition(Neighbor->GetActorLocation() + SideStep1);
                    ATile* SideTile2 = GetTileAtWorldPosition(Neighbor->GetActorLocation() + SideStep2);

                    if (SideTile1 && Neighbor->ConnectedTiles.Contains(SideTile1) && !SideTile1->IsOccupied())
                        CandidateMoves.Add(SideTile1);
                    if (SideTile2 && Neighbor->ConnectedTiles.Contains(SideTile2) && !SideTile2->IsOccupied())
                        CandidateMoves.Add(SideTile2);
                }
            }
            else if (!ClosedSet.Contains(Neighbor) && !Neighbor->IsOccupied())
            {
                CandidateMoves.Add(Neighbor);
            }
        }

        for (ATile* MoveTile : CandidateMoves)
        {
            if (ClosedSet.Contains(MoveTile))
                continue;

            bool bAlreadyInOpenSet = OpenSet.ContainsByPredicate([&](FNode* Node) { return Node->Tile == MoveTile; });
            if (!bAlreadyInOpenSet)
            {
                int32 G = Current->GCost + 1;
                int32 H = Heuristic(MoveTile->GridX, MoveTile->GridY);
                FNode* NewNode = new FNode(MoveTile, G, H, Current);
                OpenSet.Add(NewNode);
                AllNodes.Add(NewNode);
            }
        }
    }

    for (FNode* NodePtr : AllNodes) delete NodePtr;
    AllNodes.Empty();
    CachedPaths.Remove(Pawn->PlayerNumber);
    return false;
}



ATile* AQuoridorBoard::GetTileAtWorldPosition(const FVector& WorldPosition)
{
	for (int32 y = 0; y < GridSize; ++y)
	{
		for (int32 x = 0; x < GridSize; ++x)
		{
			if (Tiles[y][x] && FVector::DistSquared(Tiles[y][x]->GetActorLocation(), WorldPosition) < TileSize * TileSize * 0.5f)
			{
				return Tiles[y][x];
			}
		}
	}
	return nullptr;
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

// Di dalam AQuoridorBoard.cpp

void AQuoridorBoard::HandleWin(int32 WinningPlayer)
{
	
	if (bIsGameFinished)
	{
		return;
	}
	bIsGameFinished = true;
	
	WinningTurn = WinningPlayer;
	
    // Pemeriksaan awal untuk memastikan world valid
    if (!IsValid(this) || GetWorld() == nullptr)
       return;
		
    // Tampilkan pesan kemenangan di layar dan di log
    FString Message = FString::Printf(TEXT("PLAYER %d WINS!"), WinningPlayer);
    GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, Message);
    UE_LOG(LogTemp, Warning, TEXT("%s"), *Message);

    // Coba cast board ke untuk mengakses properti AI
    AAI_VS_AI* AI = Cast<AAI_VS_AI>(this);
	
    // Jika cast berhasil (berarti ini adalah permainan yang melibatkan AI)
    if (AI)
    {
       // Hentikan semua proses berpikir AI yang sedang berjalan
       AI->bMinimaxInProgress = false;
       AI->bIsAITurnRunning = false;
       AI->SetActorTickEnabled(false);

       // =================================================================
       // === Logika untuk menyimpan statistik Pemenang DAN Pecundang ===
       // =================================================================

       // 1. Tentukan siapa pemain yang kalah berdasarkan pemenang
       const int32 LosingPlayer = (WinningPlayer == 1) ? 2 : 1;

       // 2. Simpan statistik PEMENANG jika pemenangnya adalah AI
       if (WinningPlayer == AI->AI1Player || WinningPlayer == AI->AI2Player)
       {
          float WinningThinkingTime = (WinningPlayer == 1)
             ? AI->TotalThinkingTimeP1
             : AI->TotalThinkingTimeP2;

          SaveAIStatsToTextFile(
             WinningPlayer,
             AI->AITypeName1,
             TurnCount,
             WinningThinkingTime,
             AI->InitialWallInventory
          );
       }

       // 3. Simpan statistik PECUNDANG jika pecundangnya adalah AI
       //    (Pemeriksaan ini dilakukan secara terpisah dari pemeriksaan pemenang)
       if (LosingPlayer == AI->AI1Player || LosingPlayer == AI->AI2Player)
       {
          float LosingThinkingTime = (LosingPlayer == 1)
             ? AI->TotalThinkingTimeP1
             : AI->TotalThinkingTimeP2;
           
          // Panggil fungsi untuk menyimpan data yang kalah
          SaveLosingAIStatsToTextFile(
             LosingPlayer,
             AI->AITypeName2,
             TurnCount,
             LosingThinkingTime,
             AI->InitialWallInventory
          );
       }
       // ==============================================================
    }

    // Matikan input untuk kedua pemain setelah permainan berakhir
    for (int32 Player = 1; Player <= 2; ++Player)
    {
       if (AQuoridorPawn* P = GetPawnForPlayer(Player))
       {
          if (APlayerController* PC = Cast<APlayerController>(P->GetController()))
          {
             P->DisableInput(PC);
          }
       }
    }

    // Atur timer untuk tindakan akhir permainan (misalnya kembali ke menu utama)
    FTimerHandle EndTimerHandle;
    GetWorld()->GetTimerManager().SetTimer(EndTimerHandle, [this]()
    {
       UE_LOG(LogTemp, Warning, TEXT("Game Over. Tampilkan UI atau kembali ke menu di sini."));
       // Contoh: UGameplayStatics::OpenLevel(this, FName("MainMenu"));
    }, 2.0f, false);
}

void AQuoridorBoard::PrintLastComputedPath(int32 PlayerNumber)
{
	if (!CachedPaths.Contains(PlayerNumber))
	{
		UE_LOG(LogTemp, Warning, TEXT("No cached path for player %d."), PlayerNumber);
		return;
	}

	const TArray<FIntPoint>& Path = CachedPaths[PlayerNumber];
	UE_LOG(LogTemp, Warning, TEXT("Cached path for Player %d: Length = %d"), PlayerNumber, Path.Num());

	const int32 MaxPrint = 5;
	for (int32 i = 0; i < Path.Num(); ++i)
	{
		// Print first 5 and last 5 tiles; collapse the middle if it's long
		if (i < MaxPrint || i >= Path.Num() - MaxPrint)
		{
			UE_LOG(LogTemp, Warning, TEXT("  (%d, %d)"), Path[i].X, Path[i].Y);
		}
		else if (i == MaxPrint)
		{
			UE_LOG(LogTemp, Warning, TEXT("  ..."));
		}
	}
}

void AQuoridorBoard::Debug_PrintOccupiedWalls() const
{
	// 1) Horizontal walls
	UE_LOG(LogTemp, Warning, TEXT("--- Occupied Horizontal Walls ---"));
	for (AWallSlot* Slot : HorizontalWallSlots)
	{
		if (!Slot)
			continue;

		if (Slot->bIsOccupied)
		{
			int slotX = Slot->GridX;  // 0..7
			int slotY = Slot->GridY;  // 0..8
			UE_LOG(LogTemp, Warning,
				TEXT("H‐Wall @ (X=%d, Y=%d)"), 
				slotX, slotY);
		}
	}

	// 2) Vertical walls
	UE_LOG(LogTemp, Warning, TEXT("--- Occupied Vertical Walls ---"));
	for (AWallSlot* Slot : VerticalWallSlots)
	{
		if (!Slot)
			continue;

		if (Slot->bIsOccupied)
		{
			int slotX = Slot->GridX;  // 0..8
			int slotY = Slot->GridY;  // 0..7
			UE_LOG(LogTemp, Warning,
				TEXT("V‐Wall @ (X=%d, Y=%d)"), 
				slotX, slotY);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("----- End Occupied Walls -----"));
}

void AQuoridorBoard::SaveAIStatsToTextFile(int32 WinningPlayer, const FString& AIType, int32 TotalTurns, float TotalThinkingTime, const TArray<int32>& WallInventory)
{
	FString FilePath = FPaths::ProjectDir() + "Saved/AIStatsLog.txt";

	FString Timestamp = FDateTime::Now().ToString();
	FString WallString = FString::Printf(TEXT("[L1=%d, L2=%d, L3=%d]"),
		WallInventory.IsValidIndex(0) ? WallInventory[0] : -1,
		WallInventory.IsValidIndex(1) ? WallInventory[1] : -1,
		WallInventory.IsValidIndex(2) ? WallInventory[2] : -1);

	FString LogText = FString::Printf(
		TEXT("[%s] AI Player %d (%s) won in %d turns. Time: %.2f sec. Walls at start: %s\n"),
		*Timestamp,
		WinningPlayer,
		*AIType,
		TotalTurns,
		TotalThinkingTime,
		*WallString
	);

	// Always append, never overwrite
	FFileHelper::SaveStringToFile(
		LogText,
		*FilePath,
		FFileHelper::EEncodingOptions::AutoDetect,
		&IFileManager::Get(),
		FILEWRITE_Append
	);

	UE_LOG(LogTemp, Warning, TEXT("Appended to AIStatsLog.txt: %s"), *LogText);
}

void AQuoridorBoard::SaveLosingAIStatsToTextFile(int32 WinningPlayer, const FString& AIType, int32 TotalTurns, float TotalThinkingTime, const TArray<int32>& WallInventory)
{
	FString FilePath = FPaths::ProjectDir() + "Saved/AILosingStatsLog.txt";

	FString Timestamp = FDateTime::Now().ToString();
	FString WallString = FString::Printf(TEXT("[L1=%d, L2=%d, L3=%d]"),
		WallInventory.IsValidIndex(0) ? WallInventory[0] : -1,
		WallInventory.IsValidIndex(1) ? WallInventory[1] : -1,
		WallInventory.IsValidIndex(2) ? WallInventory[2] : -1);

	FString LogText = FString::Printf(
		TEXT("[%s] AI Player %d (%s) won in %d turns. Time: %.2f sec. Walls at start: %s\n"),
		*Timestamp,
		WinningPlayer,
		*AIType,
		TotalTurns,
		TotalThinkingTime,
		*WallString
	);

	// Always append, never overwrite
	FFileHelper::SaveStringToFile(
		LogText,
		*FilePath,
		FFileHelper::EEncodingOptions::AutoDetect,
		&IFileManager::Get(),
		FILEWRITE_Append
	);

	UE_LOG(LogTemp, Warning, TEXT("Appended to AILosingStatsLog.txt: %s"), *LogText);
}

bool AQuoridorBoard::IsGameFinished() const
{
	return bIsGameFinished;
}

int32 AQuoridorBoard::GetWinningTurn() const
{
	return WinningTurn;
}
















