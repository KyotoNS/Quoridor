#include "WallSlot.h"

#include "Components/BoxComponent.h"

AWallSlot::AWallSlot()
{
	PrimaryActorTick.bCanEverTick = false;

	BoxCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxCollision"));
	SetRootComponent(BoxCollision);

	BoxCollision->SetBoxExtent(FVector(50.f, 10.f, 100.f)); // Atur ukuran sesuai kebutuhan
	BoxCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BoxCollision->SetCollisionObjectType(ECollisionChannel::ECC_WorldStatic);
	BoxCollision->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	BoxCollision->SetCollisionResponseToChannel(ECC_Visibility, ECollisionResponse::ECR_Block);

	BoxCollision->SetGenerateOverlapEvents(true);
	BoxCollision->SetNotifyRigidBodyCollision(false);
}
