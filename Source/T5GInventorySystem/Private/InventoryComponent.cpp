
// ReSharper disable CppUE4CodingStandardNamingViolationWarning
#include "InventoryComponent.h"

#include "InventorySystemGlobals.h"
#include "PickupActorBase.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "lib/InventoryData.h"
#include "lib/InventorySave.h"
#include "lib/ItemData.h"
#include "Logging/StructuredLog.h"
#include "Net/UnrealNetwork.h" // Used for replication


void UInventoryComponent::Helper_SaveInventory(USaveGame*& SaveData) const
{
	UInventorySave* InventorySave = Cast<UInventorySave>( SaveData );
	if (IsValid(SaveData))
	{
		TArray<FInventorySlotSaveData> InventorySaveSlots;
		TArray<UInventorySlot*> InventorySlots = GetAllInventorySlots();
		for (int i = 0; i < InventorySlots.Num(); i++)
		{
			UInventorySlot* inventorySlot = InventorySlots[i];
			if (IsValid(inventorySlot))
			{
				InventorySaveSlots[i].Quantity			= inventorySlot->GetQuantity();
				InventorySaveSlots[i].SavedItemStatics	= inventorySlot->GetItemStatics();
				InventorySaveSlots[i].SavedAssetId		= inventorySlot->GetPrimaryAssetId();
				InventorySaveSlots[i].SavedSlotTags		= inventorySlot->GetSlotTags();
			}
		}
		InventorySave->SavedInventorySlots = InventorySaveSlots;
	}
}

void UInventoryComponent::BeginPlay()
{
    Super::BeginPlay();
	ReinitializeInventory();
	IssueStartingItems();
	bInventoryReady = true;
}

UInventoryComponent::UInventoryComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}


bool UInventoryComponent::HasAuthority() const
{
	// In any case where the client isn't playing by themselves,
	//   the inventory component's owner only has authority on the server.
	if (IsValid(GetOwner()))
		{return GetOwner()->HasAuthority();}
	return false;
}

/**
 * Called after BeginPlay() executes, for objects that cannot exist beforehand.
 * Attempts to load the saved inventory. If the saved inventory does not exist
 * then it will use the given UInventoryDataAsset. If the Data Asset is not
 * specified, the class defaults are used and this method will cease execution.
 */
void UInventoryComponent::ReinitializeInventory()
{
	// The inventory component is only valid on Characters
	ACharacter* OwnerCharacter = Cast<ACharacter>( GetOwner() );
	if (!IsValid(OwnerCharacter))
	{
		UE_LOGFMT(LogTemp, Error, "{Name}({Authority}): "
			"UInventoryComponent is only valid when owned by ACharacter",
			GetName(), HasAuthority()?"SRV":"CLI");
		return;
	}

	{
		FRWScopeLock WriteLock(InventoryMutex, SLT_Write);
	
		InventorySlots_.Empty(); // Reset the Inventory Slots
		Notifications_.Empty();  // Clear any pending notifications
	
		// Set up Inventory Slots
		if (IsValid(InventoryDataAsset))
		{
			const FGameplayTag DefaultInventoryTag = TAG_Inventory_Slot_Generic.GetTag();

			int SlotNumber = 0;
			for (; SlotNumber < InventoryDataAsset->NumberOfInventorySlots; SlotNumber++)
			{
				UInventorySlot* NewInventorySlot = NewObject<UInventorySlot>(this);
				NewInventorySlot->AddInventorySlotTag(DefaultInventoryTag);
				NewInventorySlot->InitializeSlot(this, SlotNumber);
				NewInventorySlot->OnSlotUpdated.AddDynamic(this, &UInventoryComponent::NotifySlotUpdated);
				InventorySlots_.Add(NewInventorySlot);
			}

			// Adds the equipment slots to the end of the inventory
			for (const FGameplayTag& NewEquipmentTag : InventoryDataAsset->EquipmentSlots)
			{
				UInventorySlot* NewEquipmentSlot = NewObject<UInventorySlot>(this);
				NewEquipmentSlot->AddInventorySlotTag(TAG_Inventory_Slot_Equipment);
				NewEquipmentSlot->AddInventorySlotTag(NewEquipmentTag);
				NewEquipmentSlot->InitializeSlot(this, SlotNumber);
				NewEquipmentSlot->OnSlotUpdated.AddDynamic(this, &UInventoryComponent::NotifySlotUpdated);
				InventorySlots_.Add(NewEquipmentSlot);
				
				// Set the O(1) variables for combat slots
				MapEquipmentSlot(NewEquipmentTag, SlotNumber);
				if (NewEquipmentTag == TAG_Equipment_Slot_Primary)
					{ PrimarySlot_  	= SlotNumber; }
				else if (NewEquipmentTag == TAG_Equipment_Slot_Secondary)
					{ SecondarySlot_	= SlotNumber; }
				else if (NewEquipmentTag == TAG_Equipment_Slot_Ranged)
					{ RangedSlot_		= SlotNumber; }
				else if (NewEquipmentTag == TAG_Equipment_Slot_Ammunition)
					{ AmmunitionSlot_	= SlotNumber; }
				
				SlotNumber++;
			}
		}
	}
	UE_LOGFMT(LogTemp, Display, "{Name}({Authority}): (Re)Initialized. "
		"Inventory has {NumSlots} Slots, of which {NumEquip} are equipment slots.",
		OwnerCharacter->GetName(), HasAuthority()?"SRV":"CLI",
		GetNumberOfTotalSlots(), GetNumberOfEquipmentSlots());
}

void UInventoryComponent::IssueStartingItems()
{
	// Never issue items if the inventory has been restored from save
	if (bInventoryRestored) {return;}

	// Add starting items to the inventory
	if (IsValid(InventoryDataAsset))
	{
		TArray<FStItemData> StartingItems = InventoryDataAsset->GetStartingItems();
		for (const FStItemData& StartingItem : StartingItems)
		{
			// Create the item by building a struct
			FItemStatics NewItem(StartingItem);
			
			// Assume that we're putting this item in the first eligible slot
			int SlotNumber = -1;

			// Create a pseudo slot so the item is added as it was generated
			//   (AddItemFromDataAsset will create a whole new item)
			UInventorySlot* PseudoSlot = NewObject<UInventorySlot>(this);
			PseudoSlot->AddItem(NewItem.ItemName);
			PseudoSlot->InitializeSlot(this, 0);
			
			if (StartingItem.bIsEquipped)
			{
				if (StartingItem.GetIsValidEquipmentItem())
				{
					for (const FGameplayTag EquipSlot : StartingItem.GetValidEquipmentSlots())
					{
						if (IsSlotEmptyByTag(EquipSlot))
						{
							SlotNumber = GetSlotNumberByTag(EquipSlot);
						}
					}
				}

				if (SlotNumber >= 0)
				{
					const int itemsAdded = AddItem(
						PseudoSlot, NewItem.ItemQuantity, SlotNumber,
						false, false, false);

					if (itemsAdded > 0)
					{
						// If the item gets equipped, continue the for loop at the next iteration
						continue;
					}
            	
				}
				// No eligible equipment slot found, send to inventory slot
				NewItem.bIsEquipped = false;
			}
        	
			// Item isn't equipped, or all eligible slots were full
			const int itemsAdded = AddItem(PseudoSlot, NewItem.ItemQuantity,
				SlotNumber, false, false, false);
			
			if (itemsAdded > 0)
			{
				UE_LOGFMT(LogTemp, Display,
					"{InvName}({Server}): x{Amount} of '{ItemName}' Added to Slot #{SlotNum}",
					GetName(), HasAuthority()?"SRV":"CLI", itemsAdded,
					NewItem.Data->GetItemDisplayNameAsString(), SlotNumber);
			}
			else
			{
				UE_LOGFMT(LogTemp, Warning,
					"{InvName}({Server}): Starting Item '{ItemName}' failed to add",
					GetName(), HasAuthority()?"SRV":"CLI", NewItem.Data->GetItemDisplayNameAsString());
			}
			
		}//For each starting item
	}//DataAsset Is Valid
}

/**
 * Returns the truth value of whether or not this inventory can pick up new items.
 * Also returns false if the inventory is full and cannot receive new items.
 */
bool UInventoryComponent::GetCanPickUpItems() const
{
	return bCanPickUpItems && !bInventoryFull;
}

void UInventoryComponent::Client_InventoryRestored_Implementation()
{
	if (OnInventoryRestored.IsBound()) { OnInventoryRestored.Broadcast(true); }
}

/**
 *  Restores inventory slots from a save game. Only runs once,
 *  as bRestoredFromSave is set once this is executed to protect game integrity.
 * @param RestoredInventory One-for-one copy of the inventory slots to restore
 */
void UInventoryComponent::RestoreInventory(
	const TArray<FInventorySlotSaveData>& RestoredInventory)
{
	// Explicit Scope for write-lock
	{
		FRWScopeLock WriteLock(InventoryMutex, SLT_Write);
		bInventoryReady = false;

		InventorySlots_.Empty(RestoredInventory.Num());

		for (int i = 0; i < RestoredInventory.Num(); i++)
		{
			UInventorySlot* NewSlot = NewObject<UInventorySlot>(this);
			NewSlot->RestoreInventorySlot(RestoredInventory[i]);

			// Attempt to restore the inventory slot in the exact same slot as before
			const int slotNumber = NewSlot->GetSlotNumber();
			if (InventorySlots_.IsValidIndex(slotNumber))
			{
				InventorySlots_[slotNumber] = NewSlot;
			}

			// If that fails, just add it where ever and update the slot Number
			else
			{
				InventorySlots_.Add(NewSlot);
			}
		}
		bInventoryReady	= true;
	}

	// If restored by the server, send restored notification to owning client
	if (HasAuthority())
	{
		bInventoryRestored = true;
		if (OnInventoryRestored.IsBound()) { OnInventoryRestored.Broadcast(true); }
		Client_InventoryRestored();
	}
	
	// If restored by a client, send to the server for update
	// Client will receive the restored broadcast when server updates
	else
	{
		bInventoryRestored = true;
		Server_RestoreSavedInventory(RestoredInventory);
	}
	
}

/** @brief Saves the inventory by overwriting the old save file.
 *			If the save doesn't exist, it will generate one.
 * @param responseStr The response from the save attempt
 * @param isAsync True runs the save asynchronously (False by Default)
 *			The response delegate is "OnInventorySaved(bool)"
 * @return Returns the save name on success, empty string on failure or async
 */
FString UInventoryComponent::SaveInventory(FString& responseStr, bool isAsync)
{
	responseStr = "Failed to Save (Inventory Not Ready)";

	// The inventory save file does not exist, if the string is empty.
	// If so, the script will try to generate one.
	if (SaveSlotName_.IsEmpty())
	{
		FString TempSaveName;
		do
		{
			for (int i = 0; i < 18; i++)
			{
				TArray RandValues = {
					FMath::RandRange(48,57), // Numbers 0-9
					FMath::RandRange(65,90) // Uppercase A-Z
				};
				const char RandChar = static_cast<char>
					(RandValues[FMath::RandRange(0,RandValues.Num()-1)]);
				TempSaveName.AppendChar(RandChar);
			}
		}
		// Loop until a unique save string has been created
		while (UGameplayStatics::DoesSaveGameExist(SaveFolder + TempSaveName, 0));
		SaveSlotName_ = TempSaveName;
	}

	// If still empty, there was a problem with the save slot name
	if (SaveSlotName_.IsEmpty())
	{
		responseStr = "Failed to Generate Unique SaveSlotName";
		return FString();
	}

	USaveGame* SaveData = UGameplayStatics::CreateSaveGameObject( UInventorySave::StaticClass() );
	if (!IsValid(SaveData))
	{
		responseStr = "Failed to Create New Save Object";
		return FString();
	}
	UInventorySave* InventorySave = Cast<UInventorySave>(SaveData);

	if (IsValid(InventorySave))
	{
		Helper_SaveInventory(SaveData);

		if (isAsync)
		{
			FAsyncSaveGameToSlotDelegate SaveDelegate;
			SaveDelegate.BindUObject(this, &UInventoryComponent::SaveInventoryDelegate);
			UGameplayStatics::AsyncSaveGameToSlot(InventorySave,
					GetFullSavePath(), GetSaveUserIndex(), SaveDelegate);
			responseStr = "Sent Request for Async Save";
			return SaveSlotName_;
		}

		if (UGameplayStatics::SaveGameToSlot(InventorySave, GetFullSavePath(), GetSaveUserIndex()))
		{
			responseStr = "Successful Synchronous Save";
			UE_LOGFMT(LogTemp, Log, "{InventoryName}({Sv}): "
				"Successfully Saved Inventory '{InventorySave} ({OwnerName})",
				GetName(), HasAuthority()?"SRV":"CLI", SaveSlotName_, GetOwner()->GetName());
			return SaveSlotName_;
		}
	}
	responseStr = "SaveNameSlot was VALID but UInventorySave was NULL";
	return FString();
}

FString UInventoryComponent::GetInventorySaveName() const
{
	return DoesInventorySaveExist() ? SaveSlotName_ : "";
}

/**
 * Checks if the incoming slot is the exact same slot as the reference slot
 * @param ComparisonSlot The slot to compare to this inventory slot
 * @param SlotNumber The slot number of the slot for this inventory
 * @return True if the slots are the same slot (identical)
 */
bool UInventoryComponent::CheckIfSameSlot(const UInventorySlot* ComparisonSlot, const int SlotNumber) const
{
	if (IsValid(ComparisonSlot) && IsValid(ComparisonSlot->GetParentInventory()))
	{
		if (ComparisonSlot->GetSlotNumber() >= 0)
		{
			if (ComparisonSlot->GetParentInventory() == this)
			{
				const UInventorySlot* thisSlot = GetInventorySlot(SlotNumber);
				return ComparisonSlot == thisSlot;
			}
		}
	}
	return false;
}


/** @brief Loads the requested save slot name, populating the inventory with
 * any data found in a save file with the given name.
 * @param responseStr The response from the save attempt
 * @param SaveSlotName The name of the save file where the data is found
 * @param isAsync True runs the save asynchronously (False by Default)
 *		Response delegate is "OnInventoryRestored(bool)"
 * @return Returns true on success or async run, false on failure.
 */
bool UInventoryComponent::LoadInventory(
		FString& responseStr, FString SaveSlotName, int32 SaveUserIndex, bool isAsync)
{
	responseStr = "Failed to Load (Inventory Has Not Initialized)";
	const bool bHasAuthority = HasAuthority();
	if ( bHasAuthority && !bSavesOnServer )
	{
		// Always allow save if this client is the listen server or standalone
		const ENetMode netMode = GetNetMode();
		if (netMode != NM_ListenServer && netMode != NM_Standalone)
		{
			responseStr = "Authority Violation when Loading Inventory '"+SaveSlotName+"'";
			return false;
		}
	}
	if ( !bHasAuthority && bSavesOnServer )
	{
		responseStr = "Authority Violation when Loading Inventory '"+SaveSlotName+"'";
		return false;
	}
	
	if (bInventoryReady)
	{
		if (!UGameplayStatics::DoesSaveGameExist(SaveFolder + SaveSlotName, SaveUserIndex_))
		{
			responseStr = "No SaveSlotName Exists";
			return false;
		}
		SaveSlotName_ = SaveSlotName;
		
		if (isAsync)
		{
			FAsyncLoadGameFromSlotDelegate LoadDelegate;
			LoadDelegate.BindUObject(this, &UInventoryComponent::LoadDataDelegate);
			UGameplayStatics::AsyncLoadGameFromSlot(GetFullSavePath(), SaveUserIndex_, LoadDelegate);
			responseStr = "Sent Async Load Request";
			return true;
		}

		LoadDataDelegate(SaveSlotName_, SaveUserIndex_, nullptr);
		return UGameplayStatics::DoesSaveGameExist(GetFullSavePath(), SaveUserIndex_);
	}
	return false;
}

void UInventoryComponent::OnComponentCreated()
{
	SetAutoActivate(true);
    Super::OnComponentCreated();
    RegisterComponent();
}

int UInventoryComponent::GetNumberOfTotalSlots() const
{
	return InventorySlots_.Num();
}

int UInventoryComponent::GetNumberOfInventorySlots() const
{
	return GetAllInventorySlots().Num();
}

int UInventoryComponent::GetNumberOfEquipmentSlots() const
{
	return GetAllEquipmentSlots().Num();
}

/**
 * Returns the inventory slot type tag of the given slot number.
 * @param SlotNumber The slot number for the inventory slot inquiry
 * @return The inventory slot type tag.
 */
FGameplayTagContainer UInventoryComponent::GetSlotInventoryTags(int SlotNumber) const
{
	return GetInventorySlot(SlotNumber)->GetSlotTags();
}

/**
 * Returns the first equipment slot tag (primary, secondary, etc) encountered,
 * since slots are supposed to only ever be one equipment slot.
 * @param SlotNumber The slot number for the equipment slot inquiry
 * @return The first equipment slot tag found, or EmptyTag.
 */
FGameplayTag UInventoryComponent::GetSlotEquipmentTag(int SlotNumber) const
{
	for (auto SlotTag : GetSlotInventoryTags(SlotNumber))
	{
		if (SlotTag.MatchesTag(TAG_Equipment_Slot))
		{
			return SlotTag;
		}
	}
	return FGameplayTag::EmptyTag;
}

/**
 * Returns the total quantity of an item in the entire inventory, NOT including equipment slots.
 * @param ItemName The itemName of the item (DA_Asset) we're searching for.
 * @return The total quantity of identical items found
 */
int UInventoryComponent::GetTotalQuantityByItem(const FName& ItemName) const
{
	UE_LOGFMT(LogTemp, Display, "{Inventory}({Sv}): GetTotalQuantityByItem({ItemName})",
		GetName(), HasAuthority()?"SRV":"CLI", ItemName);

    int total = 0;
    for (const UInventorySlot* InventorySlot : GetAllSlots())
    {
    	if (InventorySlot->ContainsItem(ItemName))
    	{
			total += InventorySlot->GetQuantity();
        }
    }
    return total;
}

/**
 * Returns the SlotNumber of the first empty slot found in the inventory.
 * @return The SlotNumber of the first empty slot. Negative indicates full inventory.
 */
int UInventoryComponent::GetFirstEmptySlotNumber() const
{
    for (const UInventorySlot* InventorySlot : GetAllSlots())
    {
        if (InventorySlot->IsEmpty())
        {
	        return InventorySlot->GetSlotNumber();
        }
    }
    return -1;
}

/**
 * Returns the total weight of all items in the inventory, sans equipment.
 * @return Float containing the total weight of the inventory. Unitless.
 */
float UInventoryComponent::GetTotalWeightOfInventorySlots() const
{
	float totalWeight = 0.0f;
	const TArray<UInventorySlot*> allSlots = GetAllSlots();
	for (int i = 0; i < allSlots.Num(); ++i)
	{
		totalWeight += GetWeightOfSlotNumber(i);
	}
	return totalWeight;
}

/**
 * Returns the total weight of all items in the inventory, sans equipment.
 * @return Float containing the total weight of the inventory. Unitless.
 */
float UInventoryComponent::GetTotalWeightOfEquipmentSlots() const
{
	float totalWeight = 0.0f;
	const TArray<UInventorySlot*> equipmentSlots = GetAllEquipmentSlots();
	for (int i = 0; i < equipmentSlots.Num(); ++i)
	{
		totalWeight += GetWeightOfSlotNumber(i);
	}
	return totalWeight;
}

/**
 * Gets the total weight of all items in the given slot.
 * @param SlotNumber The int representing the inventory slot to check.
 * @return Float with the total weight of the given slot. Negative indicates failure. Zero indicates empty slot.
 */
float UInventoryComponent::GetWeightOfSlotNumber(int SlotNumber) const
{
    const UInventorySlot* InventorySlot = GetInventorySlot(SlotNumber);
	return IsValid(InventorySlot) ? InventorySlot->GetCarryWeight() : ITEM_WEIGHT_EMPTY;
}

/**
 * Returns a TArray of int where the item was found in inventory slots
 * @param ItemName The item we're looking for
 * @param ItemStatics (opt) If provided, checks for an exact match (durability, rarity, etc)
 * @return TArray of int. Empty Array indicates no items found.
 */
TArray<int> UInventoryComponent::GetInventorySlotNumbersContainingItem(
	const FName& ItemName, const FItemStatics& ItemStatics) const
{   
    TArray<int> foundSlots = TArray<int>();
    for (const UInventorySlot* InventorySlot : GetAllSlots())
    {
    	if (InventorySlot->ContainsItem(ItemName, ItemStatics))
    	{
    		foundSlots.Add(InventorySlot->GetSlotNumber());
    	}
    }
    return foundSlots;
}


/**
 * Returns a TArray of int where the item was found in equipment slots
 * @param ItemName The item we're looking for
 * @param ItemStatics (opt) If provided, checks for an exact match (durability, rarity, etc)
 * @return TArray of int. Empty Array indicates no items found.
 */
TArray<int> UInventoryComponent::GetEquipmentSlotNumbersContainingItem(
	const FName& ItemName, const FItemStatics& ItemStatics) const
{
	TArray<int> foundSlots = TArray<int>();
	for (const UInventorySlot* InventorySlot : GetAllEquipmentSlots())
	{
		if (InventorySlot->ContainsItem(ItemName, ItemStatics))
		{
			foundSlots.Add(InventorySlot->GetSlotNumber());
		}
	}
	return foundSlots;
}

/**
 * Returns a pointer that can manipulate the slot 
 * @param SlotNumber An int representing the inventory slot
 */
UInventorySlot* UInventoryComponent::GetInventorySlot(int SlotNumber) const
{
    if (IsValidSlotNumber(SlotNumber))
    {
    	return InventorySlots_[SlotNumber];
    }
	return nullptr;
}


/**
 * Utility function to get the slot number for the requested slot reference.
 * @param SlotReference The slot to get the number for
 * @return The index of the slot (slot number), or negative on failure. 
 */
int UInventoryComponent::GetSlotNumber(const UInventorySlot* SlotReference) const
{
	const TArray InventorySlots = GetAllSlots();
	for (int i = 0; i < InventorySlots.Num(); i++)
	{
		if (InventorySlots[i] == SlotReference)
		{
			return i;
		}
	}
	return -1;
}


TArray<UInventorySlot*> UInventoryComponent::GetAllSlots() const
{
	return InventorySlots_;
}


TArray<UInventorySlot*> UInventoryComponent::GetAllInventorySlots() const
{
	TArray<UInventorySlot*> returnArray;
	for (UInventorySlot* Slot : GetAllSlots())
	{
		if (!Slot->GetIsEquipmentSlot())
		{
			returnArray.Add(Slot);
		}
	}
	return returnArray;
}


TArray<UInventorySlot*> UInventoryComponent::GetAllEquipmentSlots() const
{
	TArray<UInventorySlot*> returnArray;
	for (UInventorySlot* Slot : GetAllSlots())
	{
		if (Slot->GetIsEquipmentSlot())
		{
			returnArray.Add(Slot);
		}
	}
	return returnArray;
}


void UInventoryComponent::NotifySlotUpdated(const int SlotNumber)
{
	if (OnInventoryUpdated.IsBound())
	{
		OnInventoryUpdated.Broadcast(SlotNumber);
	}
}

/**
 * Gets the index (slot) of the slot where gameplay tag type was found.
 * @param SlotTag The Tag to look for
 * @return Int representing the slot number found. Negative indicates failure.
 */
int UInventoryComponent::GetSlotNumberByTag(const FGameplayTag& SlotTag) const
{
	
	// If the tag was previously mapped, use the existing value.
	const int* MappedSlot = TagsMappedToSlots_.Find(SlotTag);
	const int SlotNumber  = (MappedSlot != nullptr) ? *MappedSlot : -1;
	if (SlotNumber >= 0) { return SlotNumber; }
	
    if (SlotTag.MatchesTag(TAG_Equipment_Slot))
    {
        // Loop through until we find the slot since we only have 1 of each
        for (const UInventorySlot* EquipmentSlot : GetAllEquipmentSlots())
        {
        	if (EquipmentSlot->ContainsTag(SlotTag))
        	{
        		return EquipmentSlot->GetSlotNumber();
        	}
        }
    }
	
	UE_LOGFMT(LogTemp, Warning,
		"{Inventory}({Sv}): No Equipment Slot Exists for Tag '{SlotTag}'",
		GetName(), HasAuthority()?"SRV":"CLI", TAG_Equipment_Slot.GetTag().ToString());
    return -1;
}


/**
 * @param SlotNumber The slot number to get the item statics from
 * @return The item statics
 */
FItemStatics UInventoryComponent::GetSlotNumberItem(int SlotNumber) const
{
	if (IsValidItemInSlot(SlotNumber))
	{
		return GetInventorySlot(SlotNumber)->GetItemStatics();
	}
	return FItemStatics();
}


const UItemDataAsset* UInventoryComponent::GetSlotNumberItemData(int SlotNumber) const
{
	if (IsValidItemInSlot(SlotNumber))
	{
		return GetInventorySlot(SlotNumber)->GetItemData();
	}
	return nullptr;
}


bool UInventoryComponent::IsValidItemInSlot(int SlotNumber) const
{
	if (IsValidSlotNumber(SlotNumber))
	{
		return GetInventorySlot(SlotNumber)->ContainsValidItem();
	}
	return false;
}

/** 
 * Returns the truth of whether the requested slot is a real slot or not.
 * @param SlotNumber An int int of the slot being requested
 * @return Boolean
 */
bool UInventoryComponent::IsValidSlotNumber(int SlotNumber) const
{
	return InventorySlots_.IsValidIndex(SlotNumber);
}

/**
 * Checks if the given slot is a corresponding valid equipment slot
 * @return Truth of outcome
 */
bool UInventoryComponent::IsValidEquipmentSlot(int SlotNumber) const
{
	if (IsValidSlotNumber(SlotNumber))
	{
		if (GetInventorySlot(SlotNumber)->ContainsTag(TAG_Equipment_Slot))
		{
			return true;
		}
	}
	return false;
}

bool UInventoryComponent::IsValidSlotByEquipmentTag(const FGameplayTag& SearchTag) const
{
	for ( const auto& InventorySlot : GetAllSlots() )
	{
		if (InventorySlot->ContainsTag(SearchTag))
		{
			return true;
		}
	}
	return false;
}

/**
 * Returns the truth of whether the requested slot is empty.
 * Performs IsValidSlotNumber() internally. No need to call both functions.
 * @param SlotNumber The slot number to check.
 * @return If true, the slot is vacant.
 */
bool UInventoryComponent::IsSlotEmpty(int SlotNumber) const
{
    if (!IsValidSlotNumber(SlotNumber)) { return false; }
	const bool isEmpty = GetQuantityInSlotNumber(SlotNumber) < 1;
    return isEmpty;
}

/**
 * Returns the truth of whether the requested slot is empty.
 * @param SearchTag The Equip tag for the slot we're checking.
 * @return False if occupied, or invalid. True otherwise.
 */
bool UInventoryComponent::IsSlotEmptyByTag(const FGameplayTag& SearchTag)
{
    const int SlotNumber = GetSlotNumberByTag(SearchTag);
    return IsSlotEmpty(SlotNumber);
}

/**
 * Retrieves the quantity of the item in the given slot.
 * @param SlotNumber The int int representing the slot number being requested
 * @return Int of the slot number. Negative indicates failure.
 */
int UInventoryComponent::GetQuantityInSlotNumber(int SlotNumber) const
{
    if (!IsValidSlotNumber(SlotNumber)) { return -1; }
    const int SlotQuantity = GetInventorySlot(SlotNumber)->GetQuantity();
    return SlotQuantity;
}

/**
 * @brief Adds the item in the referenced slot to this inventory with the given quantity
 * @param SlotReference The inventory slot containing the reference for the item being copied
 * @param OrderQuantity How many to add
 * @param SlotNumber The slot to add to. Negative means stack or fill first slot.
 * @param bAddOverflow If true, add to the next eligible slot if this slot fills up
 * @param bDropOverflow If true, drop items to ground if slot fills up
 * @param bNotify If true, notify the player a new item was added
 * @return The amount of items added. Negative indicates error.
 */
int UInventoryComponent::AddItem(const UInventorySlot* SlotReference,
    int OrderQuantity, int SlotNumber, bool bAddOverflow, bool bDropOverflow, bool bNotify)
{
    // If reference slot is empty, do nothing
    if (SlotReference->IsEmpty())
    {
    	UE_LOGFMT(LogTemp, Display,
			"{Inventory}({Sv}): AddItem Failed - Existing Slot is Empty",
			GetName(), HasAuthority()?"SRV":"CLI");
	    return -1;
    }
	
	int itemsAdded = 0;
	const int StartingQuantity = OrderQuantity > 0 ? OrderQuantity : 1;
    int RemainingQuantity = StartingQuantity;

    // This is a safeguard against an infinite loop
    int LoopIterations = 0;
    do
    {
        LoopIterations += 1;
        
        // If the slot number is invalid, find first stackable or empty slot
        if (SlotNumber < 0)
        {
            // See if a valid existing slot can be stacked
            if (SlotReference->GetMaxStackAllowance() > 1)
            {
            	// Returns a list of slots that have the exact same item
                TArray MatchingSlots(GetInventorySlotNumbersContainingItem(SlotReference->GetItemName()));
                for (const auto& ExistingSlotNumber : MatchingSlots)
                {
                	if (IsValidSlotNumber(ExistingSlotNumber))
                	{
                        const UInventorySlot* MatchingSlot  = GetInventorySlot(ExistingSlotNumber);
						if (!MatchingSlot->IsFull())
						{
							SlotNumber = MatchingSlot->GetSlotNumber();
						}
                        
                    }//valid slot
                }//existing slots
            }// is stackable
            
            // If no stackable slot is available, add to the first empty slot
            if (SlotNumber < 0)
            {
                const int FirstFreeSlot = GetFirstEmptySlotNumber();
                if (IsValidSlotNumber(FirstFreeSlot))
                {
                    SlotNumber = FirstFreeSlot;
                }
            }

        	// If the slot number is still invalid, the inventory is full
        	if (SlotNumber < 0)
        	{
        		UE_LOGFMT(LogTemp, Display, "{Name}({Authority}): AddItem() "
        			"Failed. The inventory is full.", GetName(), HasAuthority()?"SRV":"CLI");
				if (!bDropOverflow) { return 0; }
        		bInventoryFull = true;
        		break;
        	}
        	
        }// invalid slot number

        // If the slot is now valid (or was valid to start with), add the item
        if (SlotNumber >= 0)
        {
        	FRWScopeLock WriteLock(InventoryMutex, SLT_Write);
        	
        	UInventorySlot* ReferenceSlot = GetInventorySlot(SlotNumber);
        	if (IsValid(ReferenceSlot))
        	{
        		// If the slot is empty, set the slot item
        		if (ReferenceSlot->IsEmpty())
        		{
        			itemsAdded += ReferenceSlot->AddItem(ReferenceSlot->GetItemStatics(), OrderQuantity);
        		}
				else
				{
					// Either adds all the remaining items, or adds as much as it can
					// Which will be deducted from RemainingQuantity for the next iteration
					itemsAdded += ReferenceSlot->IncreaseQuantity(RemainingQuantity);
				}
        		
        		// No changes; No items were added
        		if (itemsAdded <= 0)
        		{
        			SlotNumber = -1;
        		}
        		// Items were successfully added, or there were none to add
        		else
        		{
        			// If items are still remaining, this stack is full
        			RemainingQuantity -= itemsAdded;
        			if (bAddOverflow)
        			{
        				SlotNumber = -1;
        			}
        		}
        	}
        }// valid slot number
        
    }
    // Keep looping so long as there is:
    while (
        RemainingQuantity > 0       // There are more items to be added,   AND
        && bAddOverflow             // Add all overflowing items,          AND
        && SlotNumber < 0           // No eligible spot has been found,    AND
        && LoopIterations < 1000    // The loop is not hanging
        );

    if (LoopIterations >= 1000)
    {
        UE_LOGFMT(LogTemp, Error,
        	"{Inventory}({Sv}): AddItem() - Terminated an Infinite Loop",
            GetName(), HasAuthority()?"SRV":"CLI");
        return RemainingQuantity;
    }

    if (RemainingQuantity > 0 && bDropOverflow)
    {
    	const int ItemsAdded = OrderQuantity - RemainingQuantity;
        const ACharacter* OwnerCharacter = Cast<ACharacter>( GetOwner() );
        const USkeletalMeshComponent* thisMesh = OwnerCharacter->GetMesh();
        if (!IsValid(thisMesh))
        {
        	UE_LOGFMT(LogTemp, Error,
				"{Inventory}({Sv}): AddItem() - Invalid Mesh. "
				"Cannot spawn Pick Up actor! {NumStart} Requested. {NumAdded} Added, {NumLeft} Remaining", 
				GetName(), HasAuthority()?"SRV":"CLI", OrderQuantity, ItemsAdded, RemainingQuantity);
	        return RemainingQuantity;
        }

        const FTransform spawnTransform(
            FRotator(
                FMath::RandRange(0.f,359.f),
                 FMath::RandRange(0.f,359.f),
                 FMath::RandRange(0.f,359.f)),
            thisMesh->GetBoneLocation("Root"));
        FActorSpawnParameters spawnParams;
        spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        APickupActorBase* pickupItem = GetWorld()->SpawnActor<APickupActorBase>(
                                                APickupActorBase::StaticClass(), spawnTransform);
    	pickupItem->SetupItem(SlotReference->GetItemStatics(), RemainingQuantity);
        RemainingQuantity = 0;
    }
	
	UE_LOGFMT(LogTemp, Display,
		"{Inventory}({Sv}): AddItem() Finished - {NumStart} Requested."
		"{NumAdded} Added, {NumRemain} Remaining",
		GetName(), HasAuthority()?"SRV":"CLI", OrderQuantity, OrderQuantity - RemainingQuantity, RemainingQuantity);
	
    return FMath::Clamp((StartingQuantity - RemainingQuantity), 0, INT_MAX);

}

/**
    * @brief Adds the requested item to this inventory with the given quantity.
    *       Internally calls 'AddItemFromExistingSlot()' by making a pseudo-slot.
    * @param ItemDataAsset Item to be added from the data table
    * @param OrderQuantity The total quantity to add
    * @param SlotNumber The slot to add to. Negative means stack or fill first slot.
    * @param bAddOverflow If true, add to the next slot if this slot fills up
    * @param bDropOverflow If true, drop items to ground if slot fills up
    * @param bNotify If true, notify the player a new item was added
    * @return The number of items added. Zero indicates failure.
    */
int UInventoryComponent::AddItemFromDataAsset(
		const UItemDataAsset* ItemDataAsset, int OrderQuantity, int SlotNumber,
		bool bAddOverflow, bool bDropOverflow, bool bNotify)
{
    if (!IsValid(ItemDataAsset))
    {
    	UE_LOGFMT(LogTemp, Display,
			"{Inventory}({Sv}): AddItemFromDataTable() - Item '{ItemName}' is not a valid item",
			GetName(), HasAuthority()?"SRV":"CLI", ItemDataAsset->GetItemDisplayNameAsString());
	    return -1;
    }

	const int QuantityToAdd = OrderQuantity > 0 ? OrderQuantity : 1;

    
    if (SlotNumber >= 0)
    {
        // Invalid Slot
        if (!IsValidSlotNumber(SlotNumber))
        {
        	SlotNumber = -1;
        }
		else
		{
			// Slot is Full
			if (GetInventorySlot(SlotNumber)->IsFull())
			{
				UE_LOGFMT(LogTemp, Warning,
					"{Inventory}({Sv}): AddItem() Failed - Inventory Slot Full",
					GetName(), HasAuthority()?"SRV":"CLI");
				return -3;
			}
		}
    }

	// pseudo-slot to simply function operations & avoid repeated code
	UInventorySlot* PseudoSlot = NewObject<UInventorySlot>(this);
	PseudoSlot->AddItem( FItemStatics(ItemDataAsset) );
	PseudoSlot->InitializeSlot(this, 0);
	
    const int ItemsAdded = AddItem(
    	PseudoSlot, QuantityToAdd, SlotNumber, bAddOverflow, bDropOverflow, bNotify);

	const int RemainingQuantity = QuantityToAdd - ItemsAdded;
    if (ItemsAdded >= 0)
    {
    	UE_LOGFMT(LogTemp, Display,
			"{Inventory}({Sv}): AddItemFromDataTable() - Finished. "
			"{NumStart} Requested. {NumAdded} Added, {NumRemain} Remaining",
			GetName(), HasAuthority()?"SRV":"CLI", OrderQuantity, ItemsAdded, RemainingQuantity);
    }
	else
	{
		UE_LOGFMT(LogTemp, Display,
			"{Inventory}({Sv}): AddItemFromDataTable() Failed - "
			"Misfire of internal request to AddItem(). "
			"{NumStart} Requested. {NumAdded} Added, {NumRemain} Remaining",
			GetName(), HasAuthority()?"SRV":"CLI", OrderQuantity, ItemsAdded, RemainingQuantity);
	}
    return ItemsAdded;
}

const UInventorySlot* UInventoryComponent::GetPrimaryEquipmentSlot()
{
	if (PrimarySlot_ < 0)
	{
		PrimarySlot_ = GetSlotNumberByTag(TAG_Equipment_Slot_Primary.GetTag());
		MapEquipmentSlot(TAG_Equipment_Slot_Primary, PrimarySlot_);
	}
	return GetInventorySlot(PrimarySlot_);
}

const UInventorySlot* UInventoryComponent::GetSecondaryEquipmentSlot()
{
	if (SecondarySlot_ < 0)
	{
		SecondarySlot_ = GetSlotNumberByTag(TAG_Equipment_Slot_Secondary.GetTag());
		MapEquipmentSlot(TAG_Equipment_Slot_Secondary, SecondarySlot_);
	}
	return GetInventorySlot(SecondarySlot_);
}

const UInventorySlot* UInventoryComponent::GetRangedEquipmentSlot()
{
	if (RangedSlot_ < 0)
	{
		RangedSlot_ = GetSlotNumberByTag(TAG_Equipment_Slot_Ranged.GetTag());
		MapEquipmentSlot(TAG_Equipment_Slot_Ranged, RangedSlot_);
	}
	return GetInventorySlot(RangedSlot_);
}

const UInventorySlot*  UInventoryComponent::GetAmmunitionEquipmentSlot()
{
	if (AmmunitionSlot_ < 0)
	{
		AmmunitionSlot_ = GetSlotNumberByTag(TAG_Equipment_Slot_Ammunition.GetTag());
		MapEquipmentSlot(TAG_Equipment_Slot_Ammunition, AmmunitionSlot_);
	}
	return GetInventorySlot(AmmunitionSlot_);
}

/**
* Removes a quantity of the item in the requested inventory slot. Verifications should
* be performed before running this function. This function will remove whatever is in the slot
* with the given parameters. It does not perform name/type verification.
*
* @param OriginSlotNumber Which slot we are taking an item away from
* @param OrderQuantity How many to remove. Defaults to 1. If amount exceeds quantity, will set removeAll to true.
* @param bDropOnGround If the remove is successful, the item will fall to the ground (spawns a pickup item)
* @param bRemoveAllItems If true, removes all of the item in the slot, making the slot empty.
* @param bNotify True if the player should get a notification.
* @return The number of items actually removed. Negative indicates failure.
*/
int UInventoryComponent::RemoveItemFromSlot(
	int OriginSlotNumber, int OrderQuantity, bool bDropOnGround, bool bRemoveAllItems, bool bNotify)
{
    if (!GetIsInventorySystemReady())
    {
	    UE_LOGFMT(LogTemp, Error,
			"{Inventory}({Sv}): RemoveItemFromSlot() Failed - Inventory Not Ready",
			GetName(), HasAuthority()?"SRV":"CLI");
    	return -1;
    }
	
    if (!IsValidSlotNumber(OriginSlotNumber))
    {
	    UE_LOGFMT(LogTemp, Error,
			"{Inventory}({Sv}): RemoveItemFromSlot() Failed - "
			"#{SlotNum} is not a valid Slot",
			GetName(), HasAuthority()?"SRV":"CLI", OriginSlotNumber);
    	return -1;
    }
	
	const int AdjustedQuantity = OrderQuantity > 0 ? OrderQuantity : 1;
	
	const UInventorySlot* InventorySlot = GetInventorySlot(OriginSlotNumber);
	if (InventorySlot->IsEmpty())
	{
		UE_LOGFMT(LogTemp, Error,
			"{Inventory}({Sv}): RemoveItemFromSlot() Failed - "
			" #{SlotNum} is EMPTY",
			GetName(), HasAuthority()?"SRV":"CLI", OriginSlotNumber);
		return -1;
	}
	
	// Ensures 'quantity' is not greater than the actually quantity in slot
	const int slotQuantity = GetQuantityInSlotNumber(OriginSlotNumber);
	int RemoveQuantity = AdjustedQuantity > slotQuantity ? slotQuantity : AdjustedQuantity;
	
	// All items are being removed
	int ItemsRemoved, NewQuantity;
	if (RemoveQuantity >= slotQuantity || bRemoveAllItems)
	{
		RemoveQuantity = slotQuantity;
	}

	{
		FRWScopeLock WriteLock(InventoryMutex, SLT_Write);

    	UInventorySlot* SlotReference = GetInventorySlot(OriginSlotNumber);

    	NewQuantity = SlotReference->DecreaseQuantity(RemoveQuantity);
    	ItemsRemoved = AdjustedQuantity - NewQuantity;
	}
	
	UE_LOGFMT(LogTemp, Display,
		"{Inventory}({Sv}): RemoveItemFromSlot() Finished - "
		"{StartQty} Requested. {NumRemoved} Actually Removed. New Quantity = {NewQuantity}",
		GetName(), HasAuthority()?"SRV":"CLI", AdjustedQuantity, ItemsRemoved, NewQuantity);
	
	if (bInventoryFull)
	{
		// If any items were removed, the inventory is no longer full
		UE_LOGFMT(LogTemp, Display,
			"{Inventory}({Sv}): RemoveItemFromSlot() - The inventory is no longer full.",
			GetName(), HasAuthority()?"SRV":"CLI");
		bInventoryFull = (ItemsRemoved <= 0);
	}
	return ItemsRemoved;
}

/**
* Starting from the first slot where the item is found, removes the given quantity of
* the given ItemName until all of the item has been removed or the removal quantity
* has been reached (inclusive).
*
* @param ItemReference The item we are wanting to remove from the inventory
* @param OrderQuantity How many to remove. Defaults to 1. If amount exceeds quantity, will set removeAll to true.
* @param bRemoveEquipment True if equipment slots should be considered in the removal.
* @param bDropOnGround If true, spawns a pickup for the item removed at the total quantity removed.
* @param bRemoveAll If true, removes ALL occurrence of the item in the entire inventory.
* @return The total number of items successfully removed. Negative indicates failure.
*/
int UInventoryComponent::RemoveItemByQuantity(
	const FItemStatics& ItemReference, int OrderQuantity, bool bRemoveEquipment, bool bDropOnGround, bool bRemoveAll)
{
    if (!GetIsInventorySystemReady())
    {
    	UE_LOGFMT(LogTemp, Warning,
			"{Inventory}({Sv}): RemoveItemByQuantity() Failed - Inventory Not Ready",
			GetName(), HasAuthority()?"SRV":"CLI");
	    return -1;
    }
	
    if (ItemReference.ItemName.IsNone())
    {
    	UE_LOGFMT(LogTemp, Error,
			"{Inventory}({Sv}): RemoveItemFromSlot() Failed - "
			"Reference Item is not a valid item.",
			GetName(), HasAuthority()?"SRV":"CLI");
	    return -1;
    }
	
	const int AdjustedQuantity = OrderQuantity > 0 ? OrderQuantity : 1;
    int RemainingQuantity = AdjustedQuantity; // Track how many we've removed

	FRWScopeLock WriteLock(InventoryMutex, SLT_Write);
	for (UInventorySlot* SlotReference : InventorySlots_)
	{
		if (SlotReference->ContainsItem(ItemReference.ItemName))
		{
			const int itemsRemoved = SlotReference->DecreaseQuantity(AdjustedQuantity);
			RemainingQuantity -= itemsRemoved;
			if (RemainingQuantity < 1 || itemsRemoved < 1) { break; }
		}
    }
    
	const int ItemsRemoved = AdjustedQuantity - RemainingQuantity;
	UE_LOGFMT(LogTemp, Display,
		"{Inventory}({Sv}): RemoveItemFromSlot() Finished - "
		"{NumRequest} Requested. {NumRemoved} Removed, {NumRemain} Remaining",
		GetName(), HasAuthority()?"SRV":"CLI", OrderQuantity, ItemsRemoved, RemainingQuantity);
	
	if (bInventoryFull)
	{
		// If any items were removed, the inventory is no longer full
		UE_LOGFMT(LogTemp, Display,
			"{Inventory}({Sv}): RemoveItemFromSlot() - The inventory is no longer full.",
			GetName(), HasAuthority()?"SRV":"CLI");
		bInventoryFull = (ItemsRemoved <= 0);
	}
	return ItemsRemoved;
}

/**
* Performs a swap FROM the origin slot TO the target slot.
* If the items match and can be stacked, the items FROM will be stacked
* onto the TO items until the stack max is reached.
* @param OriginSlot The slot referenced that initiated the swap
* @param TargetSlot The slot referenced that is receiving the swap
* @param RemainingQuantity The amount that did NOT transfer or stack
* @return True if swap or stack was successful
*/
int UInventoryComponent::SwapOrStackSlots(
	UInventorySlot* OriginSlot, UInventorySlot* TargetSlot, int& RemainingQuantity)
{
    if (!IsValid(OriginSlot->GetParentInventory()) && !IsValid(TargetSlot->GetParentInventory()))
    {
    	UE_LOGFMT(LogTemp, Warning,
			"{Inventory}({Sv}): SwapOrStackWithRemainder() Failed - "
			"Swap Slots must have a valid parent inventory.",
			GetName(), HasAuthority()?"SRV":"CLI");
	    return false;
    }
	
    // If origin slot is empty, what are they swapping/stacking? Use Add & Remove.
    if (OriginSlot->IsEmpty() || TargetSlot->IsEmpty())
    {
    	UE_LOGFMT(LogTemp, Log,
			"{Inventory}({Sv}): SwapOrStackWithRemainder() Failed - Slot #{SlotNum} is an Empty Slot",
			GetName(), HasAuthority()?"SRV":"CLI", TargetSlot->GetSlotNumber());
        return false;
    }

	bool isOriginEquipSlot 	= OriginSlot->GetIsEquipmentSlot();
	bool isTargetEquipSlot 	= TargetSlot->GetIsEquipmentSlot();

	int OriginQuantity = OriginSlot->GetQuantity();
	int TargetQuantity = TargetSlot->GetQuantity();
	
	const UItemDataAsset* OriginItem 	= OriginSlot->GetItemData();
	const UItemDataAsset* TargetItem 	= TargetSlot->GetItemData();

	const FItemStatics OriginStaticsCopy = OriginSlot->GetItemStatics();
	const FItemStatics TargetStaticsCopy = TargetSlot->GetItemStatics();
	
	const UEquipmentDataAsset* OriginEquipment 	= OriginSlot->GetItemDataAsEquipment();
	const UEquipmentDataAsset* TargetEquipment 	= TargetSlot->GetItemDataAsEquipment();

	/**
	 * Equipment Slot Validation
	 */
	 
	// If origin slot is an equipment slot, target item going into it must be eligible equipment
	if (OriginSlot->GetIsEquipmentSlot() && IsValid(TargetEquipment))
	{
		// Checks if the origin slot contains any of the equipment's tags
		if (!OriginSlot->ContainsTag(TargetEquipment->EquippableSlots))
		{
			UE_LOGFMT(LogTemp, Log,
				"{Inventory}({Sv}): SwapOrStackWithRemainder() Failed - "
				"Target (Slot {nTargetSlot}) is not a valid equipment slot for the origin item (Slot {nOriginSlot}).",
				GetName(), HasAuthority()?"SRV":"CLI", TargetSlot->GetSlotNumber(), OriginSlot->GetSlotNumber());
			return false;
		}
	}
	
	// If target slot is an equipment slot, origin item going into it must be eligible equipment
	if (TargetSlot->GetIsEquipmentSlot() && IsValid(OriginEquipment))
	{
		// Checks if the target slot contains any of the equipment's tags
		if (!TargetSlot->ContainsTag(OriginEquipment->EquippableSlots))
		{
			UE_LOGFMT(LogTemp, Log,
				"{Inventory}({Sv}): SwapOrStackWithRemainder() Failed - "
				"Target (Slot {nTargetSlot}) is not a valid equipment slot for the origin item (Slot {nOriginSlot}).",
				GetName(), HasAuthority()?"SRV":"CLI", OriginSlot->GetSlotNumber(), TargetSlot->GetSlotNumber());
			return false;
		}
	}

	/**
	 * Validates like-items
	 */
	const bool	isSameExactItem = TargetSlot->ContainsItem(OriginItem, OriginSlot->GetItemStatics());
	const int   MaxStackSize	= TargetItem->GetItemMaxStackSize();

	// Exact same item, and stackable
    if (isSameExactItem && (MaxStackSize > 1))
    {
    	const int SpaceRemaining = TargetSlot->GetMaxStackAllowance() - TargetSlot->GetQuantity();
    	if (SpaceRemaining < 1)
    	{
			UE_LOGFMT(LogTemp, Display,
				"{Inventory}({Sv}): {InvName}({SlotNum}) has no space for origin stack request.",
				GetName(), HasAuthority()?"SRV":"CLI",
				TargetSlot->GetParentInventory()->GetName(), TargetSlot->GetSlotNumber());
			return true;
    	}
    	
    	const int itemsRemoved = OriginSlot->DecreaseQuantity(RemainingQuantity);
    	if (itemsRemoved > 0)
    	{ 
    		const int itemsAdded = TargetSlot->IncreaseQuantity(itemsRemoved);
    		if (itemsAdded > 0)
    		{
    			RemainingQuantity -= itemsAdded;
    			UE_LOGFMT(LogTemp, Display,
					"{Inventory}({Sv}): Successfully Stacked x{nQuantity} of {ItemName}",
					GetName(), HasAuthority()?"SRV":"CLI", itemsAdded, OriginItem->GetName());
    			return true;
    		}
    		UE_LOGFMT(LogTemp, Error,
				"{Inventory}({Sv}): SwapOrStackWithRemainder() - "
				"Internal Request to TargetSlot->IncreaseQuantityInSlot Failed",
				GetName(), HasAuthority()?"SRV":"CLI");
    		return false;
    	}
    	UE_LOGFMT(LogTemp, Error,
			"{Inventory}({Sv}): SwapOrStackWithRemainder() - "
			"Internal Request to OriginSlot->DecreaseQuantityInSlot Failed",
			GetName(), HasAuthority()?"SRV":"CLI");
    	return false;
    }

	// If they are not the same item or not stackable, swap 1-for-1
	OriginSlot->SetItem(TargetStaticsCopy, TargetQuantity);
	TargetSlot->SetItem(OriginStaticsCopy, OriginQuantity);
	RemainingQuantity = 0;
	
    UE_LOGFMT(LogTemp, Display,
		"{Inventory}({Sv}): Successfully Swapped {OriginInv}({OriginNum}) with {TargetInv}({TargetNum})",
		GetName(), HasAuthority()?"SRV":"CLI",
		OriginSlot->GetParentInventory()->GetName(), OriginSlot->GetSlotNumber(),
		TargetSlot->GetParentInventory()->GetName(), TargetSlot->GetSlotNumber());
	return true;
}

/**
 * @brief Finds which slots have changed, and sends an update event
 */
void UInventoryComponent::OnRep_InventorySlotUpdated_Implementation(const TArray<UInventorySlot*>& OldSlots)
{
	for (int i = 0; i < GetNumberOfTotalSlots(); i++)
	{
		// Existing slot updated
		if (OldSlots.IsValidIndex(i))
		{
			const UInventorySlot* CurrentSlot = GetInventorySlot(i);
			const UInventorySlot* OldSlotRef  = OldSlots[i];
			
			if (   CurrentSlot->GetItemStatics() != OldSlotRef->GetItemStatics()
				|| CurrentSlot->GetItemName()	 != OldSlotRef->GetItemName()  )
			{
				UE_LOGFMT(LogTemp, Log,
					"{Inventory}({Sv}) REPNOTIFY: Updated Inventory Slot {SlotNum}",
					GetName(), HasAuthority()?"SRV":"CLI", i);
				if (OnInventoryUpdated.IsBound())
				{
					OnInventoryUpdated.Broadcast(i);
				}
			}
			
			// The item or its quantity has changed
			else if ( CurrentSlot->GetQuantity() != OldSlotRef->GetQuantity() )
			{
				// One of the slots must have a valid item in order to be valid
				// i.e. there was no update if the slot was empty before and after
				if (CurrentSlot->ContainsValidItem() || OldSlotRef->ContainsValidItem())
				{
					UE_LOGFMT(LogTemp, Log,
						"{Inventory}({Sv}) REPNOTIFY: Updated Inventory Slot {SlotNum}",
						GetName(), HasAuthority()?"SRV":"CLI", i);
					if (OnInventoryUpdated.IsBound())
					{
						OnInventoryUpdated.Broadcast(i);
					}
				}
			}
		}
		
		// New Slot Added
		else
		{
			UE_LOGFMT(LogTemp, Log,
				"{Inventory}({Sv}) REPNOTIFY: Added Inventory Slot {SlotNum}",
				GetName(), HasAuthority()?"SRV":"CLI", i);
			if (OnInventoryUpdated.IsBound())
			{
				OnInventoryUpdated.Broadcast(i);
			}
		}
	}
}

void UInventoryComponent::Server_RestoreSavedInventory_Implementation(
	const TArray<FInventorySlotSaveData>& RestoredInventory)
{
	if (HasAuthority())
	{
		RestoreInventory(RestoredInventory);
	}
	else
	{
		UE_LOGFMT(LogTemp, Log,
			"{Inventory}({Sv}): Request to restore inventory must run on the authority.",
			GetName(), HasAuthority()?"SRV":"CLI");
	}
}

/**
* Splits off n-items (up to Quantity - 1) from the reference slot into the specified slot number.
* If the specified slot number is invalid, it will split the item off into the first empty inventory slot.
* If no slot is valid, the split will be rejected altogether.
*
* @param OriginSlot The slot of origin that is being split
* @param TargetSlot The slot being targeted.
* @param OrderQuantity The amount of split off from the original stack.
* @param bNotify If true, shows a notification. False by default.
* @return The number of items actually split off
*/
int UInventoryComponent::SplitStack(
    UInventorySlot* OriginSlot, UInventorySlot* TargetSlot, int OrderQuantity, bool bNotify)
{
	if (OriginSlot->IsEmpty())
	{
		UE_LOGFMT(LogTemp, Display,
			"{Inventory}({Sv}) Failed - Origin Slot is Empty ({InvName} Slot {SlotNum})",
			GetName(), HasAuthority()?"SRV":"CLI",
			OriginSlot->GetParentInventory()->GetName(), OriginSlot->GetSlotNumber());
		return false;
	}

	const int AddQuantity = OrderQuantity > 0 ? OrderQuantity : 1;
	const UItemDataAsset* OriginItem = OriginSlot->GetItemData();
	if (TargetSlot->ContainsItem( OriginItem ))
	{
		const UItemDataAsset* TargetItem = TargetSlot->GetItemData();
		if (IsValid(TargetItem))
		{
			if (OriginItem == TargetItem )
			{
				const int itemsRemoved = OriginSlot->DecreaseQuantity(AddQuantity);
				if (itemsRemoved > 0)
				{
					const int itemsAdded = TargetSlot->IncreaseQuantity(itemsRemoved);
					if (itemsAdded > 0)
					{
						UE_LOGFMT(LogTemp, Display,
							"{Inventory}({Sv}): Successfully stacked {nQuantity}",
							GetName(), HasAuthority()?"SRV":"CLI", itemsAdded);
					}
				}
			}
		}
		UE_LOGFMT(LogTemp, Warning,
			"{Inventory}({Sv}) Failed - .",
			GetName(), HasAuthority()?"SRV":"CLI");
	}
	
	UE_LOGFMT(LogTemp, Display,
		"{Inventory}({Sv}): Successfully split off {NumSplit} from {OriginInv} Slot #{SlotNum}",
		GetName(), HasAuthority()?"SRV":"CLI", OrderQuantity,
		OriginSlot->GetParentInventory()->GetName(), OriginSlot->GetSlotNumber());
    return true;
}

/**
* Increases the amount of an existing item by the quantity given.
* If the given slot is empty, it will do nothing and fail.
*
* @param SlotNumber		The slot number to be increased
* @param OrderQuantity	The quantity to increase the slot by, up to the maximum quantity
* @param bNotify		Show a notification when quantity increases. False by default.
* @return				The number of items actually added. Negative indicates failure.
*/
int UInventoryComponent::IncreaseSlotQuantity(int SlotNumber, int OrderQuantity, bool bNotify)
{
    if (IsValidSlotNumber(SlotNumber))
    {
	    GetInventorySlot(SlotNumber)->IncreaseQuantity(OrderQuantity);
    }
	return 0;
}

/**
* Increases the amount of an existing item by the quantity given.
* If the given slot is empty, it will do nothing and fail.
*
* @param SlotNumber		The slot number to be decreased
* @param OrderQuantity	The quantity to increase the slot by
* @param bNotify		Show a notification when quantity increases. False by default.
* @return				The number of items actually removed. Negative indicates failure.
*/
int UInventoryComponent::DecreaseSlotQuantity(int SlotNumber, int OrderQuantity, bool bNotify)
{
	if (IsValidSlotNumber(SlotNumber))
	{
		GetInventorySlot(SlotNumber)->DecreaseQuantity(OrderQuantity);
	}
	return 0;
}

/**
* Event that retrieves and subsequently removes all inventory notifications.
* @return A TArray of FStInventoryNotify
*/
TArray<FStInventoryNotify> UInventoryComponent::GetNotifications()
{
    TArray<FStInventoryNotify> invNotify;
    if (!Notifications_.IsEmpty())
    {
        invNotify = Notifications_;
    	Notifications_.Empty();
    	UE_LOGFMT(LogTemp, Log,
			"{Inventory}({Sv}): GetNotifications() found {n} pending notifications",
			GetName(), HasAuthority()?"SRV":"CLI", invNotify.Num());
        return invNotify;
    }
    return invNotify;
}

/**
* Wipes the inventory slot without destroying the settings (equipment, etc)
* @param InventorySlot The Slot Reference to reset
*/
void UInventoryComponent::ResetSlot(UInventorySlot* InventorySlot)
{
	InventorySlot->ResetAndEmptySlot();
}

/**
* Called when the server is reporting that a PLAYER wants to transfer items between
* two inventories. This is NOT a delegate/event. It should be invoked by the delegate
* that intercepted the net event request. This function transfers two items between inventories.
* Validation is required if you want to keep game integrity.
*
* @param OriginSlot		The slot losing the item
* @param TargetSlot		The slot gaining the item
* @param OrderQuantity	The amount to be moved. Will default to 1.
* @param bDropOverflow	If true, anything that overflows will spawn a pickup actor.
* @return True if at least 1 of the item was successfully moved. False if the transfer failed entirely.
*/
bool UInventoryComponent::TransferItemBetweenSlots(
	UInventorySlot* OriginSlot, UInventorySlot* TargetSlot, int OrderQuantity, bool bDropOverflow)
{
	if (OriginSlot->IsEmpty())
	{
		UE_LOGFMT(LogTemp, Warning,
			"{Inventory}({Sv}): tried to transfer from {InvName}({SlotNum}), but the slot is EMPTY",
			GetName(), HasAuthority()?"SRV":"CLI",
			OriginSlot->GetParentInventory()->GetName(), OriginSlot->GetSlotNumber());
		return false;
	}

	bool bNotify = (OriginSlot->GetParentInventory() != TargetSlot->GetParentInventory()); 

	int MoveQuantity = OrderQuantity > 0 ? OrderQuantity : 1;
	
    if (OriginSlot->IsEmpty())
    {
        // If an item exists, attempt to swap or stack them
        if (TargetSlot->IsEmpty())
        {
        	if (SwapOrStackSlots(OriginSlot, TargetSlot, MoveQuantity) > 0)
        	{
        		return true;
        	}
        }
    	
        // If destination slot is empty we can shortcut the logic
        //	by just using SplitStack, regardless of the quantity.
        else
        {
        	if (SplitStack(OriginSlot, TargetSlot, MoveQuantity, bNotify) > 0)
        	{
        		return true;
        	}
        }
    }
	
	UE_LOGFMT(LogTemp, Log,
		"{Inventory}({Sv}): Transfer Failed",
		GetName(), HasAuthority()?"SRV":"CLI");
    return false;
}

/**
* Called when an item is activated from the inventory. This function is
* executed from the inventory of the player who activated the item.
*
* @param SlotNumber		The number of the inventory slot containing the item to be activated
* @param bForceConsume	True consumes the item, whether it's consumable or not. False by default.
*						If false, item will still be consumed if it is flagged to be consumed.
* @return True if the item was successfully activated
*/
bool UInventoryComponent::ActivateSlot(int SlotNumber, bool bForceConsume)
{
    if (IsValidSlotNumber(SlotNumber))
    {
		UInventorySlot* ActivatedSlot = GetInventorySlot(SlotNumber);
    	
    	const UItemDataAsset* ItemInSlot(ActivatedSlot->GetItemData());

    	if (ItemInSlot->GetItemCanActivate())
    	{
    		ActivatedSlot->ActivateSlot();
    	}
    	return true;
    	
    }
    return false;
}

void UInventoryComponent::Server_RequestItemActivation_Implementation(
    UInventoryComponent* OriginInventory, int SlotNumber)
{
    if (HasAuthority() && IsValid(OriginInventory))
    {
    	UInventorySlot* ActivatedSlot = GetInventorySlot(SlotNumber);
    	ActivatedSlot->ActivateSlot();
    }
	else
	{
    	UE_LOGFMT(LogTemp, Log,
			"{Inventory}({Sv}): Server_RequestItemActivation Authority Violation or NULLPTR",
			GetName(), HasAuthority()?"SRV":"CLI");
	}
}

/**
 * Loading Delegate that is called when save data has been loaded asynchronously
 * @param SaveSlotName The name of the inventory save slot
 * @param UserIndex Usually zero
 * @param SaveData The save object
 */
void UInventoryComponent::LoadDataDelegate(const FString& SaveSlotName, int32 UserIndex, USaveGame* SaveData)
{	
	UInventorySave* InventorySave = Cast<UInventorySave>( SaveData );
	if (!IsValid(InventorySave))
	{
		InventorySave = Cast<UInventorySave>
			( UGameplayStatics::LoadGameFromSlot(GetFullSavePath(), SaveUserIndex_) );
		if (!IsValid(InventorySave))
		{
			if (OnInventoryRestored.IsBound()) { OnInventoryRestored.Broadcast(false); }
			return;
		}
	}

	RestoreInventory( InventorySave->SavedInventorySlots );
	if (OnInventoryRestored.IsBound()) { OnInventoryRestored.Broadcast(true); }
}

/**
 *  Saving Delegate that is called when save data was found asynchronously
 * @param SaveSlotName The name of the save slot to be used
 * @param UserIndex User Index
 * @param bSuccess True if the save was successful
 */
 void UInventoryComponent::SaveInventoryDelegate(
 		const FString& SaveSlotName, int32 UserIndex, const bool bSuccess)
{
	UE_LOGFMT(LogTemp, Log,
		"{Inventory}({Sv}): (Async Response) {SuccessTruth} the inventory with name '{SaveName}' @ index {Index}'",
		GetName(), HasAuthority()?"SRV":"CLI", bSuccess?"Successfully Saved":"Failed to Save", SaveSlotName_, SaveUserIndex_);
	if (OnInventoryRestored.IsBound()) { OnInventoryRestored.Broadcast(bSuccess); }
}


void UInventoryComponent::MapEquipmentSlot(const FGameplayTag& EquipmentTag, int SlotNumber)
{
	TagsMappedToSlots_.Add(EquipmentTag, SlotNumber);
}


//---------------------------------------------------------------------------------------------------------------
//---------------------------------- REPLICATION ----------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------

void UInventoryComponent::OnRep_NewNotification_Implementation()
{
	UE_LOGFMT(LogTemp, Log,
		"{Inventory}({Sv}): New Notification Received. There are now {NumNotifies} notifications pending.",
		GetName(), HasAuthority()?"SRV":"CLI", Notifications_.Num());
	OnNotificationAvailable.Broadcast();
}

/**
 * Sends a request to the server to transfer items between inventories.
 * The origin item should always be where the item originated from, or started the drag operation.
 * Requires Validation. Called by the client. Executes from the player's inventory.
 * @param OriginInventory	The inventory of the FROM item (the 'origination')
 * @param TargetInventory	The inventory of the TO item (the 'receiving')
 * @param OriginSlotNumber	The slot number of the origination inventory being affected
 * @param TargetSlotNumber	The slot number of the receiving inventory (-1 means first available slot)
 * @param OrderQuantity		The amount to move. Negative means move everything.
 */
void UInventoryComponent::Server_TransferItems_Implementation(
	UInventoryComponent* OriginInventory, UInventoryComponent* TargetInventory,
	int OriginSlotNumber, int TargetSlotNumber, int OrderQuantity)
{
    UE_LOGFMT(LogTemp, Display, "InventoryComponent: Server_TransferItems_Implementation");
	
    // Validation
    if (!IsValid(OriginInventory)) { OriginInventory = this; }
    if (!IsValid(TargetInventory)) { TargetInventory = this; }

	const bool bNotify = OriginInventory != TargetInventory;

    // Move is from-and-to the exact same slot.. Do nothing.
	UInventorySlot* FromSlot = OriginInventory->GetInventorySlot(OriginSlotNumber);
	UInventorySlot* ToSlot   = TargetInventory->GetInventorySlot(TargetSlotNumber);

	const int OriginQuantity = FromSlot->GetQuantity();
	const int TargetQuantity = ToSlot->GetQuantity();
	
    if (FromSlot == ToSlot)
    {
    	UE_LOGFMT(LogTemp, Log,
			"{Inventory}({Sv}): Server_TransferItems() Canceled - Transfer is the exact same slot",
			GetName(), HasAuthority()?"SRV":"CLI");
	    return;
    }

	const bool ToSlotLocked		= ToSlot->ContainsTag(TAG_Inventory_Slot_Locked);
	const bool FromSlotLocked	= ToSlot->ContainsTag(TAG_Inventory_Slot_Locked);
	
	if (ToSlotLocked || FromSlotLocked)
    {
    	UE_LOGFMT(LogTemp, Warning,
			"{Inventory}({Sv}): {InvName}, Slot Number {SlotNum}, is a read-only slot (locked).",
			GetName(), HasAuthority()?"SRV":"CLI",
			ToSlotLocked ? ToSlot->GetParentInventory()->GetName() : FromSlot->GetParentInventory()->GetName(),
			ToSlotLocked ? ToSlot->GetSlotNumber() : FromSlot->GetSlotNumber());
    	return;
    }
    
	// Player is trying to modify a player-owned inventory that is not their own
	const ACharacter* OriginPlayer = Cast<ACharacter>( OriginInventory->GetOwner() );
	const ACharacter* TargetPlayer = Cast<ACharacter>( TargetInventory->GetOwner() );
    if (IsValid(OriginPlayer))
    {
        // The *from* inventory is not this player. The transfer is invalid.
        if (OriginPlayer->IsPlayerControlled() && OriginPlayer != GetOwner())
        {
        	UE_LOGFMT(LogTemp, Warning,
				"{Inventory}({Sv}): An attempt to take item from another player's inventory was prevented ({FromInv} -> {ToInv})",
				GetName(), HasAuthority()?"SRV":"CLI", OriginInventory->GetName(), TargetInventory->GetName());
            return;
        }
    }
    // This player is trying to modify a player-owned inventory.
    if (IsValid(TargetPlayer))
    {
        // The *TO* inventory is not this player. The transfer is invalid.
        if (TargetPlayer->IsPlayerControlled() && TargetPlayer != GetOwner())
        {
        	UE_LOGFMT(LogTemp, Warning,
				"{Inventory}({Sv}): An attempt to put items into another player's inventory was prevented ({FromInv} -> {ToInv})",
				GetName(), HasAuthority()?"SRV":"CLI", OriginInventory->GetName(), TargetInventory->GetName());
            return;
        }
    }

	// Ensure the transfer is within the reach distance of the inventory
    if (OriginPlayer->GetDistanceTo(TargetPlayer) > MaxInventoryReach_)
    {
    	UE_LOGFMT(LogTemp, Error,
			"{Inventory}({Sv}): Transfer Rejected. Too Far Away ({Distance} (Max: {MaxDistance})",
			GetName(), HasAuthority()?"SRV":"CLI", OriginPlayer->GetName(),
			OriginPlayer->GetDistanceTo(TargetPlayer), MaxInventoryReach_);
    	return;
    }
        
	// Make a copy of the origin item, and decrease it from the origin inventory.
	// Always TAKE items first to avoid dupe exploits
	const FItemStatics FromSlotCopy = FromSlot->GetItemStatics();
	const FItemStatics ToSlotCopy   = ToSlot->GetItemStatics();

	const UEquipmentDataAsset* FromEquipmentItem = FromSlot->GetItemDataAsEquipment();
	const UEquipmentDataAsset* ToEquipmentItem = ToSlot->GetItemDataAsEquipment();

	// Check if destination slot is an equipment slot and can tolerate the item
    if (ToSlot->GetIsEquipmentSlot())
    {
		if ( !ToSlot->ContainsTag(FromEquipmentItem->EquippableSlots) )
		{
			return;
		}
	}

	// Validations
	OrderQuantity = OrderQuantity > 0 ? OrderQuantity : 1;
    if (!FromSlot->IsEmpty())
    {
        // Remove the items from the origin inventory
        const int itemsRemoved = FromSlot->DecreaseQuantity(OrderQuantity);
        if (itemsRemoved < 1)
        {
        	UE_LOGFMT(LogTemp, Error,
				"{Inventory}({Sv}): Transfer Failed. "
				"No items were able to be removed from {FromInv} Slot #{SlotNum}",
				GetName(), HasAuthority()?"SRV":"CLI", OriginInventory->GetName(), OriginSlotNumber);
            return;
        }
    	OrderQuantity = itemsRemoved;
    }
    
    // If decrease was successful (or slot is empty), add it to the destination inventory.
    const int itemsAdded = TargetInventory->AddItem(FromSlot, OrderQuantity, TargetSlotNumber,
    	false, false, bNotify);
    if (itemsAdded > 0)
    {
    	// obsolete..?
    }
    
    // If the add failed, reinstate the item to the origin inventory
	// If the original from slot and current from slots quantities don't match, reimburse the items
    else if (OriginQuantity != FromSlot->GetQuantity())
    {
    	UE_LOGFMT(LogTemp, Error,
			"{Inventory}({Sv}): Transfer Failed. "
			"No items were able to be added to '{ToInv} Slot Number #{SlotNum}'"
			" from '{FromInv}' Slot Number #{FromSlot}... Will attempt to reimburse loss.",
			GetName(), HasAuthority()?"SRV":"CLI", TargetInventory->GetName(), TargetSlotNumber,
			OriginInventory->GetName(), OriginSlotNumber);
    }
}


/**
 * Sends a request to the server to take the item from the FROM inventory, and throw it on the ground as a pickup.
 * @param OriginInventory	The inventory of the FROM item (the 'origination')
 * @param SlotNumber		The slot number of the origination inventory being affected
 * @param OrderQuantity		The amount to toss out. Negative means everything.
 */
void UInventoryComponent::Server_DropItemOnGround_Implementation(
	UInventoryComponent* OriginInventory, int SlotNumber, int OrderQuantity)
{
	if (!IsValid(OriginInventory)) { OriginInventory = this; }

	ACharacter* OwningCharacter = Cast<ACharacter>( OriginInventory->GetOwner() );
	if (!IsValid(OwningCharacter)) { return; }

	UInventorySlot* InventorySlot	= OriginInventory->GetInventorySlot(SlotNumber);
	
	if (InventorySlot == nullptr) { return; }
	
	// Not all items being tossed will be in the player's inventory. Sometimes
	//  they will be storage chests, bodies, etc.
	UInventorySlot* FromSlot = OriginInventory->GetInventorySlot(SlotNumber);
	
	if (FromSlot == nullptr)
	{
		UE_LOGFMT(LogTemp, Warning,
			"{Inventory}({Sv}): Attempted to drop item from {FromInv} Slot #{SlotNum}, "
			"but the slot is Empty (or Invalid)", GetName(), HasAuthority()?"SRV":"CLI",
			OriginInventory->GetName(), SlotNumber);
		return;
	}
	if (FromSlot->IsEmpty()) { return; }

	USkeletalMeshComponent* thisMesh = OwningCharacter->GetMesh();
	if (!IsValid(thisMesh))
	{
		UE_LOGFMT(LogTemp, Error,
			"{Inventory}({Sv}): Attempt to Drop Failed. Dropping player has no GetMesh().",
			GetName(), HasAuthority()?"SRV":"CLI");
		return;
	}

	FTransform spawnTransform(FRotator(
			FMath::RandRange(0.f,359.f),
			FMath::RandRange(0.f,359.f),
			FMath::RandRange(0.f,359.f)) );
	spawnTransform.SetLocation(thisMesh->GetBoneLocation("Root"));
	
	FActorSpawnParameters spawnParams;
	spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const FItemStatics ItemCopy	= InventorySlot->GetItemStatics();
	const int itemsRemoved	= InventorySlot->DecreaseQuantity(OrderQuantity);

	if (itemsRemoved > 0)
	{
		APickupActorBase* pickupItem = GetWorld()->SpawnActorDeferred<APickupActorBase>(
											APickupActorBase::StaticClass(), spawnTransform);
		pickupItem->SetupItem(ItemCopy);
		pickupItem->FinishSpawning(spawnTransform);
	}
	else
	{
		OriginInventory->AddItem(FromSlot, OrderQuantity, SlotNumber);
	}
}

/**
 * Sends a request to the server when the player opens something like a storage container, and needs the
 * information of the inventory for their UI. Called by the client.
 * @requestInventory The inventory component the player wishes to load in
 */
void UInventoryComponent::Server_RequestOtherInventory_Implementation(UInventoryComponent* targetInventory)
{
	if (!IsValid(targetInventory))
	{
		targetInventory = this;
	}
	
    ACharacter* OwnerCharacter = Cast<ACharacter>( GetOwner() );
	if (IsValid(OwnerCharacter))
	{
		if (targetInventory == this)
		{
			const APlayerState* thisPlayerState = OwnerCharacter->GetPlayerState();
			UE_LOGFMT(LogTemp, Display, "{Inventory}({Sv}): Attempt to request players own inventory denied.",
				IsValid(thisPlayerState) ? thisPlayerState->GetPlayerId() : 0);
			return;
		}
	}

	// Target inventory is another characters inventory (using TargetCharacter)
	AActor* TargetActor = targetInventory->GetOwner();
	const ACharacter* TargetCharacter = Cast<ACharacter>( TargetActor );
    if (IsValid(TargetCharacter))
    {
    	// Stop Execution if inventory belongs to another player
    	const AController* TargetController = TargetCharacter->GetController();
    	if (IsValid(TargetController))
    	{
    		// Is the character controlled by the player?
    		if (IsValid( Cast<APlayerController>(TargetController) ))
    		{
    			UE_LOGFMT(LogTemp, Display, "{Inventory}({Sv}): RequestOtherInventory() - "
					"{ThisPlayer} was denied access to the inventory of {TargetPlayer}",
					GetName(), HasAuthority()?"SRV":"CLI", OwnerCharacter->GetName(), TargetCharacter->GetName());
    			return;
    		}

    		// Character is controlled by an NPC
    		UE_LOGFMT(LogTemp, Display, "{Inventory}({Sv}): RequestOtherInventory() - "
				"{ThisPlayer} was denied access to the inventory of {TargetPlayer} - NPC is still controlled (alive)",
				GetName(), HasAuthority()?"SRV":"CLI", OwnerCharacter->GetName(), TargetCharacter->GetName());
    		return;
    	}
    }
	
	UE_LOGFMT(LogTemp, Display, "{Inventory}({Sv}): Request to access '{TargetInventory}' of {TargetName} was successful",
			  GetName(), HasAuthority()?"SRV":"CLI", targetInventory->GetName(), TargetCharacter->GetName());
    
}


/****************************************
 * REPLICATION
***************************************/

void UInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> & OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(UInventoryComponent, InventorySlots_);
	
    DOREPLIFETIME_CONDITION(UInventoryComponent, Notifications_, COND_OwnerOnly);
}