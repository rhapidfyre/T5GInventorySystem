
#include "PickupActorBase.h"

#include "InventoryComponent.h"
#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"

// Default Constructor. Runs as soon as the actor becomes available in the editor.
APickupActorBase::APickupActorBase()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05;

	// Ensures this actor replicates, such as movement and variable values
	RootComponent->SetIsReplicated(true);
	bNetLoadOnClient = true;
	bReplicates = true;

	PickUpDetection = CreateDefaultSubobject<USphereComponent>("PickupDetection");
	PickUpDetection->SetupAttachment(GetStaticMeshComponent());
	PickUpDetection->InitSphereRadius(48.0);
	PickUpDetection->bAutoActivate = true;
	PickUpDetection->SetSphereRadius(48.0);
	
	SetMobility(EComponentMobility::Movable);
	
}

// base Unreal Engine AActor* override for replication
void APickupActorBase::GetLifetimeReplicatedProps(TArray< class FLifetimeProperty > & OutLifetimeProps) const
{
	// "Super" means to execute the parent's version of this function.
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	// Ensures that the 'worldTransform' replicates to everyone via multicast
	DOREPLIFETIME(APickupActorBase, m_Slot);
	DOREPLIFETIME(APickupActorBase, m_WorldTransform);
    
}

// Later we will change this so that tick only runs on the server. Clients don't need this.
void APickupActorBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime); // Run parent tick first
	if (HasAuthority())
	{
		m_WorldTransform = GetActorTransform();
	}
	else
	{
		SetActorTransform(m_WorldTransform);
	}
}

// Runs *AFTER* the Constructor, AND anytime the object is modified in the editor.
void APickupActorBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	
	if (HasAuthority())
		m_WorldTransform = GetActorTransform();
	
	// Reads the item name and retrieves the relevant item data.
	m_Slot.ItemName = ItemName;
	SetupItemData();
}

void APickupActorBase::PostLoad()
{
	Super::PostLoad(); // Run parent's PostLoad first
}

void APickupActorBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void APickupActorBase::CheckOverlapCall(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (HasAuthority())
	{
		if (IsValid(OtherActor))
		{
			OnPickedUp(OtherActor);
		}
	}
}

void APickupActorBase::BeginPlay()
{
	Super::BeginPlay();
	
	UStaticMeshComponent* sMesh = GetStaticMeshComponent();
	if (IsValid(sMesh))
	{
		sMesh->SetSimulatePhysics(HasAuthority());
		sMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		sMesh->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Ignore);
		sMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		sMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
		sMesh->SetCollisionResponseToChannel(ECC_EngineTraceChannel3, ECR_Ignore);
		sMesh->SetCollisionResponseToChannel(ECC_Destructible, ECR_Ignore);
		
		if (IsValid(PickUpDetection) && HasAuthority())
		{
			if (!PickUpDetection->OnComponentBeginOverlap.IsAlreadyBound(this, &APickupActorBase::CheckOverlapCall))
				 PickUpDetection->OnComponentBeginOverlap.AddDynamic(this, &APickupActorBase::CheckOverlapCall);
		}
	}
}

void APickupActorBase::SetupItemData()
{
	UStaticMeshComponent* sMesh = GetStaticMeshComponent();
	
	m_Slot.ItemName			= ItemName;
	m_Slot.SlotQuantity		= SpawnQuantity;
	m_Slot.SlotDurability	= ItemDurability;
	
	if (!IsValid(sMesh)) return;
	// If the item is valid, sets the static mesh.
	if (!m_Slot.ItemName.IsNone())
	{
		const FStItemData ItemData = UItemSystem::getItemDataFromItemName(m_Slot.ItemName);
		sMesh->SetStaticMesh(m_Slot.GetItemData().staticMesh);
	}
	
	sMesh->SetCollisionObjectType(ECC_PhysicsBody);
	sMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	sMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	
}

void APickupActorBase::SetupItemFromName(FName NewItemName, int NewItemQuantity)
{
	if (NewItemQuantity < 1)
		NewItemQuantity = 1;
	
	if (!HasActorBegunPlay())
	{
		ItemName		= NewItemName;
		SpawnQuantity	= NewItemQuantity;
		
		SetupItemData();
	}
}

void APickupActorBase::OnPickedUp(AActor* targetActor)
{
	if (HasActorBegunPlay())
	{
		if (bIsOperating)
		{
			UE_LOG(LogTemp, Warning, TEXT("%s(%s) is operating and cannot be used right now. Dupe Exploit protection."),
				*GetName(), (HasAuthority()?TEXT("SERVER"):TEXT("CLIENT")));
			return;
		}
		bIsOperating = true;
		if (IsValid(targetActor))
		{
			// Ensure the player isn't unreasonably far from the item
			if (GetDistanceTo(targetActor) >= 1024)
			{
				UE_LOG(LogTemp, Warning, TEXT("%s was too far away to interact with %s"), *targetActor->GetName(), *GetName());
				bIsOperating = false;
				return;
			}
		
			const ACharacter* playerRef = Cast<ACharacter>(targetActor);
			if (IsValid(playerRef))
			{
				UE_LOG(LogTemp, Display, TEXT("%s(%s): Target Actor is a Character"), *GetName(),
					(HasAuthority()?TEXT("SRV"):TEXT("CLI")));
				UInventoryComponent* invComp = targetActor->FindComponentByClass<UInventoryComponent>();
				if (IsValid(invComp))
				{
					if (!invComp->bPickupItems)
						return;
					
					UE_LOG(LogTemp, Display, TEXT("%s(%s): Adding Item x%d of '%s'"), *GetName(),
						(HasAuthority()?TEXT("SRV"):TEXT("CLI")), m_Slot.SlotQuantity, *(m_Slot.ItemName).ToString() );
					int itemsAdded = invComp->AddItemFromDataTable(m_Slot.ItemName,
								m_Slot.SlotQuantity, -1, true, true);
					if (itemsAdded >= 0)
					{
						if (itemsAdded >= m_Slot.SlotQuantity)
						{
							// Destroy self. All items added.
							UE_LOG(LogTemp, Display, TEXT("%s(%s): Quantity zero. Removing pickup actor."),
								*GetName(), HasAuthority()?TEXT("SRV"):TEXT("CLI"));
							K2_DestroyActor();
						}
						else
						{
							// Unable to add all of them. Update the quantity remaining.
							UE_LOG(LogTemp, Display, TEXT("%s(%s): %d still remaining. Pickup actor adjusted."),
								*GetName(), HasAuthority()?TEXT("SRV"):TEXT("CLI"), m_Slot.SlotQuantity);
							m_Slot.SlotQuantity -= itemsAdded;
						}
					}
					else
					{
						UE_LOG(LogTemp, Warning, TEXT("%s(%s): Failed to add item (InventoryComp returned Negative Value)"),
							*GetName(), HasAuthority()?TEXT("SRV"):TEXT("CLI"));
					}
				}
				else
				{
					UE_LOG(LogTemp, Display, TEXT("%s(%s): Failed to find a valid inventory"), *GetName(),
						(HasAuthority()?TEXT("SV"):TEXT("CL")));
				}
			}
		}
		bIsOperating = false;
	}
}