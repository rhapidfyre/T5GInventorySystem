#include "PickupActorBase.h"

#include "NavigationSystemTypes.h"
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
	
	SetMobility(EComponentMobility::Movable);

	// Creates the interaction component. This is PRE-SETTINGS.
	// If you implement different variable values in blueprints, this runs before those values are read.
	InteractionComponent = CreateDefaultSubobject<UInteractionComponent>(TEXT("Interaction"));
	
}

// base Unreal Engine AActor* override for replication
void APickupActorBase::GetLifetimeReplicatedProps(TArray< class FLifetimeProperty > & OutLifetimeProps) const
{
	// "Super" means to execute the parent's version of this function.
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	// Ensures that the 'worldTransform' replicates to everyone via multicast
	DOREPLIFETIME_CONDITION(APickupActorBase, m_itemName, COND_None);
	DOREPLIFETIME_CONDITION(APickupActorBase, m_itemData, COND_None);
	DOREPLIFETIME_CONDITION(APickupActorBase, m_itemQuantity, COND_None);
    
}

// Later we will change this so that tick only runs on the server. Clients don't need this.
void APickupActorBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime); // Run parent tick first
}

// Runs *AFTER* the Constructor, AND anytime the object is modified in the editor.
void APickupActorBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Reads the item name and retrieves the relevant item data.
	m_itemData = UItemSystem::getItemDataFromItemName(m_itemName);

	UStaticMeshComponent* sMesh = GetStaticMeshComponent();
	if(IsValid(sMesh))
	{
		sMesh->SetStaticMesh(m_itemData.staticMesh);
		sMesh->SetIsReplicated(true);
		sMesh->SetSimulatePhysics(false);
		SetReplicates(true);
		//SetupItemData();
	}
}

void APickupActorBase::PostLoad()
{
	Super::PostLoad(); // Run parent's PostLoad first
	bSetupComplete = true;
}

void APickupActorBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		// Bind pickup to interaction event
		if (IsValid(InteractionComponent))
		{
			InteractionComponent->OnInteraction.RemoveAll(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void APickupActorBase::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Display, TEXT("%s(%s): BeginPlay() - Using Item '%s'"),
		*GetName(), HasAuthority()?TEXT("SRV"):TEXT("CLI"), *m_itemName.ToString());

	// If the item data is invalid, the actor will destroy itself before seeing the light of day
	if (m_itemData.properName == UItemSystem::getInvalidName())
	{
		// If the actor is spawned without any item information, check again before deleting it.
		if (HasAuthority())
		{
			GetWorld()->GetTimerManager().SetTimer(
				m_waitTimer, this, &APickupActorBase::CheckForValidity,1.0f, true, -1);
		}
	}
	else
		SetupItemData();
	
	if (HasAuthority())
	{
		// Bind pickup to interaction event
		if (IsValid(InteractionComponent))
		{
			if (!InteractionComponent->OnInteraction.IsAlreadyBound(this, &APickupActorBase::OnPickedUp))
				 InteractionComponent->OnInteraction.AddDynamic(this, &APickupActorBase::OnPickedUp);
		}
	}
	
}

void APickupActorBase::CheckForValidity()
{
	UE_LOG(LogTemp, Display, TEXT("%s(%s): CheckForValidity()"), *GetName(), HasAuthority()?TEXT("SRV"):TEXT("CLI"));
	if (m_itemData.properName == UItemSystem::getInvalidName())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s(%s) failed to validate (invalid item), and was destroyed."),
				*GetName(), HasAuthority()?TEXT("SRV"):TEXT("CLI"));
		m_retryCount++;
		if (m_retryCount > 5) K2_DestroyActor();
	}
	else
	{
		GetWorld()->GetTimerManager().ClearTimer(m_waitTimer);
	}
}

void APickupActorBase::SetupItemData()
{
	UStaticMeshComponent* sMesh = GetStaticMeshComponent();
	if (!IsValid(sMesh)) return;
	if (HasAuthority())
	{
	
		// If the item is valid, sets the static mesh.
		if (m_itemData.properName != UItemSystem::getInvalidName())
		{
			sMesh->SetStaticMesh(m_itemData.staticMesh);
		}

	}
	sMesh->SetCollisionObjectType(ECC_PhysicsBody);
	sMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	sMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	UStaticMesh* stMesh = sMesh->GetStaticMesh();
	FString meshName = "NULL";
	if (IsValid(stMesh))
		meshName = stMesh->GetName();
	
	UE_LOG(LogTemp, Display, TEXT("%s(%s): SetupItemData() - itemName'%s' qty'%d' mesh'%s'"),
		*GetName(), HasAuthority()?TEXT("SRV"):TEXT("CLI"), *m_itemName.ToString(), m_itemQuantity, *meshName);
}

void APickupActorBase::SetupItemData(FName itemName, int quantity)
{
	UE_LOG(LogTemp, Display, TEXT("%s(%s): SetupItemData(%s, %d)"), *GetName(), HasAuthority()?TEXT("SRV"):TEXT("CLI"), *itemName.ToString(), quantity);
	m_itemName		= itemName;
	m_itemQuantity	= quantity;
	m_itemData		= UItemSystem::getItemDataFromItemName(m_itemName);
	SetupItemData();
}

void APickupActorBase::SetupItemFromData(FStItemData itemData, int quantity)
{
	if (UItemSystem::getItemDataIsValid(itemData))
	{
		m_itemData = itemData;
		m_itemQuantity = (quantity > 1 ? quantity : 1);
		m_itemName = UItemSystem::getItemName(itemData);
		SetupItemData();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("%s(%s): SetupItemFromData received invalid FStItemData."), *GetName(), HasAuthority()?TEXT("SRV"):TEXT("CLI"));
	}
}

void APickupActorBase::OnPickedUp(AActor* targetActor)
{
	if (m_isOperating)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s(%s) is operating and cannot be used right now. Dupe Exploit protection."),
			*GetName(), (HasAuthority()?TEXT("SERVER"):TEXT("CLIENT")));
		return;
	}
	m_isOperating = true;
	if (IsValid(targetActor))
	{
		// Ensure the player isn't unreasonably far from the item
		if (GetDistanceTo(targetActor) >= 1024)
		{
			UE_LOG(LogTemp, Warning, TEXT("%s was too far away to interact with %s"), *targetActor->GetName(), *GetName());
			m_isOperating = false;
			return;
		}
		
		APrimaryPlayerCharacter* playerRef = Cast<APrimaryPlayerCharacter>(targetActor);
		if (IsValid(playerRef))
		{
			UE_LOG(LogTemp, Display, TEXT("%s(%s): Target Actor is a Player"), *GetName(),
				(HasAuthority()?TEXT("SRV"):TEXT("CLI")));
			UInventoryComponent* invComp = targetActor->FindComponentByClass<UInventoryComponent>();
			if (IsValid(invComp))
			{
				UE_LOG(LogTemp, Display, TEXT("%s(%s): Adding Item x%d of '%s'"), *GetName(),
					(HasAuthority()?TEXT("SRV"):TEXT("CLI")), m_itemQuantity, *(m_itemData.properName).ToString() );
				int itemsAdded = invComp->addItemByName(
					m_itemData.properName, m_itemQuantity, true, true, true);
				if (itemsAdded >= 0)
				{
					if (itemsAdded >= m_itemQuantity)
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
							*GetName(), HasAuthority()?TEXT("SRV"):TEXT("CLI"), m_itemQuantity);
						m_itemQuantity -= itemsAdded;
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
	m_isOperating = false;
}