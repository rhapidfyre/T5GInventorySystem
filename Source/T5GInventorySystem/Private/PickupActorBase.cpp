
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
	DOREPLIFETIME(APickupActorBase, WorldTransform_);
	DOREPLIFETIME(APickupActorBase, ItemData);
	DOREPLIFETIME(APickupActorBase, ItemQuantity);
    
}

// Later we will change this so that tick only runs on the server. Clients don't need this.
void APickupActorBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime); // Run parent tick first
	if (HasAuthority())
	{
		WorldTransform_ = GetActorTransform();
	}
	else
	{
		SetActorTransform(WorldTransform_);
	}
}

// Runs *AFTER* the Constructor, AND anytime the object is modified in the editor.
void APickupActorBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (HasAuthority()) { WorldTransform_ = GetActorTransform(); }
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
			{
				PickUpDetection->OnComponentBeginOverlap.AddDynamic(this, &APickupActorBase::CheckOverlapCall);
			}
		}
	}

	if (IsValid(ItemDataAsset))
	{
		ItemData	 = FStItemData(ItemDataAsset);
		ItemQuantity = StartingQuantity > 0 ? StartingQuantity : 1;
		SetupItemData();
	}
	bReady = true;
}

void APickupActorBase::SetupItemData()
{
	UStaticMeshComponent* sMesh = GetStaticMeshComponent();

	if (!ItemData.GetIsValidItem())
	{
		sMesh->SetStaticMesh(ItemData.Data->GetItemStaticMesh());
	}
	
	sMesh->SetCollisionObjectType(ECC_PhysicsBody);
	sMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	sMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	
}

/**
 * Sets up a pickup actor containing a new item by the given name.
 * @param NewItemData The item to be copied
 * @param OrderQuantity The number of items in this pickup actor
 */
void APickupActorBase::SetupItem(const FStItemData& NewItemData, int OrderQuantity)
{
	if (!bReady) { return; }
	if (!HasActorBegunPlay())
	{
		const int NewQuantity = OrderQuantity > 0 ? OrderQuantity : NewItemData.ItemQuantity;
		ItemData		= NewItemData;
		ItemQuantity	= NewQuantity;
		SetupItemData();
	}
}

void APickupActorBase::OnPickedUp(AActor* targetActor)
{
	if (HasActorBegunPlay())
	{
		if (bIsOperating || !bReady)
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
					if (!invComp->GetCanPickUpItems()) { return; }

					UE_LOG(LogTemp, Display, TEXT("%s(%s): Adding Item x%d of '%s'"), *GetName(),
					       (HasAuthority()?TEXT("SRV"):TEXT("CLI")), ItemQuantity, *ItemData.ToString());

					
					FStInventorySlot PsuedoSlot = FStInventorySlot(ItemData);
					const int itemsAdded = invComp->AddItem(
						PsuedoSlot, ItemQuantity, -1, true, true);
					
					if (itemsAdded > 0)
					{
						ItemQuantity -= itemsAdded;
						if (ItemQuantity < 1)
						{
							// Destroy self. All items added.
							UE_LOG(LogTemp, Display, TEXT("%s(%s): All items collected. Deleting pickup actor."),
								*GetName(), HasAuthority()?TEXT("SRV"):TEXT("CLI"));
							//K2_DestroyActor();
							Destroy(true);
						}
						else
						{
							// Unable to add all of them. Update the quantity remaining.
							UE_LOG(LogTemp, Display, TEXT("%s(%s): %d still remaining. Pickup actor adjusted."),
								*GetName(), HasAuthority()?TEXT("SRV"):TEXT("CLI"), ItemQuantity);
						}
					}
					else
					{
						UE_LOG(LogTemp, Warning, TEXT("%s(%s): Failed to add items"),
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