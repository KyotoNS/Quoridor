﻿
#include "Tile.h"
#include "Components/StaticMeshComponent.h"
#include "Quoridor/Pawn/QuoridorPawn.h"

ATile::ATile()
{
	PrimaryActorTick.bCanEverTick = false;

	TileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TileMesh"));
	RootComponent = TileMesh;
	if (TileMesh)
	{
		TileMesh->SetHiddenInGame(false);
	}
}
bool ATile::IsAdjacent(const ATile* OtherTile) const
{
	if (!OtherTile) return false;
	return (FMath::Abs(GridX - OtherTile->GridX) + 
		   FMath::Abs(GridY - OtherTile->GridY)) == 1;
}

void ATile::AddConnection(ATile* OtherTile)
{
	if (OtherTile && OtherTile != this)
	{
		ConnectedTiles.Add(OtherTile);
	}
}

void ATile::RemoveConnection(ATile* OtherTile)
{
	if (OtherTile)
	{
		ConnectedTiles.Remove(OtherTile);
	}
}

void ATile::ClearConnections()
{
	ConnectedTiles.Empty();
}
