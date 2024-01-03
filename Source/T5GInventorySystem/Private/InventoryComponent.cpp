
// ReSharper disable CppUE4CodingStandardNamingViolationWarning
#include "InventoryComponent.h"

#include "InventorySystemGlobals.h"
#include "PickupActorBase.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "lib/InventorySave.h"
#include "lib/ItemData.h"
#include "lib/EquipmentData.h"
#include "Logging/StructuredLog.h"
#include "Net/UnrealNetwork.h" // Used for replication


void UInventoryComponent::Helper_SaveInventory(USaveGame*& SaveData) const
{
	UInventorySave* InventorySave = Cast<UInventorySave>( SaveData );
	if (IsValid(SaveData))
	{
		InventorySave->SaveInventorySlots(InventorySlots_);
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
	
#ifdef UE_BUILD_DEBUG
	bShowDebug = true;
#endif
	
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
			const FGameplayTag DefaultEquipmentTag = TAG_Inventory_Slot_Equipment.GetTag();

			int SlotNumber = 0;
			for (; SlotNumber < InventoryDataAsset->NumberOfInventorySlots; SlotNumber++)
			{
				FStInventorySlot NewInventorySlot;
				NewInventorySlot.SlotTag	     = DefaultInventoryTag;
				NewInventorySlot.ParentInventory = this;
				NewInventorySlot.SlotNumber		 = SlotNumber;
				NewInventorySlot.OnSlotUpdated.AddUObject(   this, &UInventoryComponent::SlotUpdated   );
				//NewInventorySlot.OnSlotActivated.AddUObject( this, &UInventoryComponent::SlotActivated );
				InventorySlots_.Add(NewInventorySlot);
			}

			// Adds the equipment slots to the end of the inventory
			for (const FGameplayTag& NewEquipmentTag : InventoryDataAsset->EquipmentSlots)
			{
				FStInventorySlot NewInventorySlot;
				NewInventorySlot.SlotTag		 = DefaultEquipmentTag;
				NewInventorySlot.EquipmentTag	 = NewEquipmentTag;
				NewInventorySlot.ParentInventory = this;
				NewInventorySlot.SlotNumber		 = SlotNumber;
				NewInventorySlot.OnSlotUpdated.AddUObject(   this, &UInventoryComponent::SlotUpdated   );
				//NewInventorySlot.OnSlotActivated.AddUObject( this, &UInventoryComponent::SlotActivated );
				InventorySlots_.Add(NewInventorySlot);
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
			FStItemData NewItem(StartingItem);
			
			// Assume that we're putting this item in the first eligible slot
			int SlotNumber = -1;

			// Create a pseudo slot so the item is added as it was generated
			//   (AddItemFromDataAsset will create a whole new item)
			const FStInventorySlot PseudoSlot = FStInventorySlot(NewItem);
			
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
						PseudoSlot, NewItem.ItemQuantity, SlotNumber, false, false, false);

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
					"{InvName}({Server})}: x{Amount} of '{ItemName}' Added to Slot #{SlotNum}",
					GetName(), HasAuthority()?"SRV":"CLI", itemsAdded,
					NewItem.Data->GetItemDisplayNameAsString(), SlotNumber);
			}
			else
			{
				UE_LOGFMT(LogTemp, Warning,
					"{InvName}({Server})}: Starting Item '{ItemName}' failed to add",
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
	OnInventoryRestored.Broadcast(true);
}

/**
 *  Restores inventory slots from a save game. Only runs once,
 *  as bRestoredFromSave is set once this is executed to protect game integrity.
 * @param RestoredInventory One-for-one copy of the inventory slots to restore
 */
void UInventoryComponent::RestoreInventory(
	const TArray<FStInventorySlot>& RestoredInventory)
{
	const bool doServerSave =	HasAuthority() &&   bSavesOnServer;
	const bool doClientSave = ! HasAuthority() && ! bSavesOnServer;
	if ( !doServerSave || !doClientSave || bInventoryRestored)
	{
		const ENetMode netMode = GetNetMode();
		if (netMode != NM_ListenServer && netMode != NM_Standalone)
		{
			return;
		}
	}

	{
		FRWScopeLock WriteLock(InventoryMutex, SLT_Write);
		bInventoryReady = false;
	
		InventorySlots_.Empty( RestoredInventory.Num() );
		for (int i = 0; i < RestoredInventory.Num(); i++)
		{
			FStInventorySlot NewSlot(RestoredInventory[i]);
			// Attempt to restore the inventory slot in the exact same slot as before
			if (InventorySlots_.IsValidIndex(NewSlot.SlotNumber))
			{
				InventorySlots_[NewSlot.SlotNumber] = NewSlot;
			}
			// If that fails, just add it where ever and update the slot Number
			else
			{
				FStInventorySlot* NewReference = &NewSlot;
				NewReference->SlotNumber = InventorySlots_.Add(NewSlot);
				
			}
		}
		bInventoryReady	= true;
	}

	// If restored by the server, send restored notification to owning client
	if (HasAuthority())
	{
		bInventoryRestored = true;
		OnInventoryRestored.Broadcast(true);
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
	const bool doServerSave =	HasAuthority() &&   bSavesOnServer;
	const bool doClientSave = ! HasAuthority() && ! bSavesOnServer;
	if ( !doServerSave || !doClientSave || !GetIsInventorySystemReady())
	{
		// Always allow save if this client is the listen server or standalone
		const ENetMode netMode = GetNetMode();
		if (netMode != NM_ListenServer && netMode != NM_Standalone)
		{
			responseStr = "Authority Violation when Saving Inventory!";
			return FString();
		}
	}
	
	// The inventory save file does not exist, if the string is empty.
	// If so, the script will try to generate one.
	if (SaveSlotName_.IsEmpty())
	{
		FString TempSaveName;
		do
		{
			for (int i = 0; i < 18; i++)
			{
				TArray<int> RandValues = {
					FMath::RandRange(48,57), // Numbers 0-9
					FMath::RandRange(65,90) // Uppercase A-Z
				};
				const char RandChar = static_cast<char>(RandValues[FMath::RandRange(0,RandValues.Num()-1)]);
				TempSaveName.AppendChar(RandChar);
			}
		}
		// Loop until a unique save string has been created
		while (UGameplayStatics::DoesSaveGameExist(TempSaveName, 0));
		SaveSlotName_ = TempSaveName;
	}

	// If still empty, there was a problem with the save slot name
	if (SaveSlotName_.IsEmpty())
	{
		responseStr = "Failed to Generate Unique SaveSlotName";
		return FString();
	}

	if (isAsync)
	{
		FAsyncLoadGameFromSlotDelegate SaveDelegate;
		SaveDelegate.BindUObject(this, &UInventoryComponent::SaveInventoryDelegate);
		UGameplayStatics::AsyncLoadGameFromSlot(SaveSlotName_, 0, SaveDelegate);
		responseStr = "Sent Request for Async Save";
		return SaveSlotName_;
	}

	USaveGame* SaveData;
	if (UGameplayStatics::DoesSaveGameExist(SaveSlotName_, 0))
	{
		SaveData = UGameplayStatics::LoadGameFromSlot(SaveSlotName_, 0);
	}
	else
	{
		// If this is a brand new inventory save, issue the starting items.
		IssueStartingItems();
		SaveData = UGameplayStatics::CreateSaveGameObject( UInventorySave::StaticClass() );
		if (!IsValid(SaveData))
		{
			responseStr = "Failed to Create New Save Object";
			return FString();
		}
	}
		
	UInventorySave* InventorySave = Cast<UInventorySave>(SaveData);
	if (IsValid(InventorySave))
	{
		
		if (UGameplayStatics::SaveGameToSlot(InventorySave, SaveSlotName_, 0))
		{
			Helper_SaveInventory(SaveData);
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


/** @brief Loads the requested save slot name, populating the inventory with
 * any data found in a save file with the given name.
 * @param responseStr The response from the save attempt
 * @param SaveSlotName The name of the save file where the data is found
 * @param isAsync True runs the save asynchronously (False by Default)
 *		Response delegate is "OnInventoryRestored(bool)"
 * @return Returns true on success or async run, false on failure.
 */
bool UInventoryComponent::LoadInventory(
		FString& responseStr, FString SaveSlotName, bool isAsync)
{
	responseStr = "Failed to Save (Inventory Has Not Initialized)";
	const bool doServerSave =	(HasAuthority() &&   bSavesOnServer);
	const bool doClientSave = ! HasAuthority() && ! bSavesOnServer;
	if ( !doServerSave || !doClientSave || bInventoryRestored)
	{
		// Always allow save if this client is the listen server or standalone
		const ENetMode netMode = GetNetMode();
		if (netMode != NM_ListenServer && netMode != NM_Standalone)
		{
			responseStr = "Authority Violation when Loading Inventory '"+SaveSlotName+"'";
			return false;
		}
	}
	
	if (bInventoryReady)
	{
		if (!UGameplayStatics::DoesSaveGameExist(SaveSlotName, SaveUserIndex_))
		{
			responseStr = "No SaveSlotName Exists";
			return false;
		}
		SaveSlotName_ = SaveSlotName;
		
		if (isAsync)
		{
			FAsyncLoadGameFromSlotDelegate LoadDelegate;
			LoadDelegate.BindUObject(this, &UInventoryComponent::LoadDataDelegate);
			UGameplayStatics::AsyncLoadGameFromSlot(SaveSlotName_, SaveUserIndex_, LoadDelegate);
			responseStr = "Sent Async Save Request";
			return true;
		}

		LoadDataDelegate(SaveSlotName_, SaveUserIndex_, nullptr);
		return UGameplayStatics::DoesSaveGameExist(SaveSlotName_, SaveUserIndex_);
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
	return GetCopyOfAllInventorySlots().Num();
}

int UInventoryComponent::GetNumberOfEquipmentSlots() const
{
	return GetCopyOfAllEquipmentSlots().Num();
}

/**
 * Returns the inventory slot type tag of the given slot number.
 * @param SlotNumber The slot number for the inventory slot inquiry
 * @return The inventory slot type tag.
 */
FGameplayTag UInventoryComponent::GetSlotInventoryTag(int SlotNumber) const
{
	UE_LOGFMT(LogTemp, Display, "{Inventory}({Sv}): GetSlotTypeInventory({SlotNum})",
		GetName(), HasAuthority()?"SRV":"CLI", SlotNumber);
	return GetCopyOfSlotNumber(SlotNumber).SlotTag;
}

/**
 * Returns the equipment enum slot type of the given slot number. Returns NONE
 * if the slot is invalid or the slot is not an equipment slot. 
 * @param SlotNumber The slot number for the equipment slot inquiry
 * @return The equip slot type enum, where NONE indicates failure.
 */
FGameplayTag UInventoryComponent::GetSlotEquipmentTag(int SlotNumber) const
{
	UE_LOGFMT(LogTemp, Display, "{Inventory}({Sv}): GetSlotTypeEquipment({SlotNum})",
		GetName(), HasAuthority()?"SRV":"CLI", SlotNumber);
	return GetCopyOfSlotNumber(SlotNumber).EquipmentTag;
}

/**
 * Returns the total quantity of an item in the entire inventory, NOT including equipment slots.
 * @param ItemReference The itemName of the item we're searching for.
 * @return The total quantity of identical items found
 */
int UInventoryComponent::GetTotalQuantityByItem(const FStItemData& ItemReference) const
{
	UE_LOGFMT(LogTemp, Display, "{Inventory}({Sv}): GetTotalQuantityByItem({SlotNum})",
		GetName(), HasAuthority()?"SRV":"CLI", ItemReference.ToString());

    int total = 0;
    for (const FStInventorySlot& InventorySlot : GetCopyOfAllInventorySlots())
    {
    	if (InventorySlot.ContainsItem(ItemReference))
    	{
			total += InventorySlot.GetQuantity();
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
    for (const FStInventorySlot& InventorySlot : GetCopyOfAllInventorySlots())
    {
        if (InventorySlot.IsSlotEmpty())
        {
        	UE_LOGFMT(LogTemp, Display, "GetFirstEmptySlotNumber({Sv}): Empty {SlotType} Found @ {SlotNum}",
				HasAuthority()?"SRV":"CLI", HasAuthority()?"SRV":"CLI", InventorySlot.SlotNumber);
	        return InventorySlot.SlotNumber;
        }
    }
	UE_LOGFMT(LogTemp, Display, "GetFirstEmptySlotNumber({Sv}): No Empty {SlotType} Found",
		HasAuthority()?"SRV":"CLI", HasAuthority()?"SRV":"CLI");
    return -1;
}

/**
 * Returns the total weight of all items in the inventory, sans equipment.
 * @return Float containing the total weight of the inventory. Unitless.
 */
float UInventoryComponent::GetTotalWeightOfInventorySlots() const
{
	float totalWeight = 0.0f;
	for (const FStInventorySlot& InventorySlot : GetCopyOfAllInventorySlots())
	{
		if (!InventorySlot.IsSlotEmpty())
		{
			totalWeight += InventorySlot.GetWeight();
		}
	}
	UE_LOGFMT(LogTemp, Display, "{Inventory}({Sv}): Total Non-Equipment Weight = {Weight}",
		GetName(), HasAuthority()?"SRV":"CLI", totalWeight);
	return totalWeight;
}

/**
 * Returns the total weight of all items in the inventory, sans equipment.
 * @return Float containing the total weight of the inventory. Unitless.
 */
float UInventoryComponent::GetTotalWeightOfEquipmentSlots() const
{
	float totalWeight = 0.0f;
	for (const FStInventorySlot& InventorySlot : GetCopyOfAllEquipmentSlots())
	{
		if (!InventorySlot.IsSlotEmpty())
		{
			totalWeight += InventorySlot.GetWeight();
		}
	}
	UE_LOGFMT(LogTemp, Display, "{Inventory}({Sv}): Total Equipment Weight = {Weight}",
		GetName(), HasAuthority()?"SRV":"CLI", totalWeight);
	return totalWeight;
}

/**
 * Gets the total weight of all items in the given slot.
 * @param SlotNumber The int representing the inventory slot to check.
 * @return Float with the total weight of the given slot. Negative indicates failure. Zero indicates empty slot.
 */
float UInventoryComponent::GetWeightOfSlotNumber(int SlotNumber) const
{
    const FStInventorySlot InventorySlot = GetCopyOfSlotNumber(SlotNumber);
	const float weight = InventorySlot.GetWeight();
    	UE_LOGFMT(LogTemp, Display, "{Inventory}({Sv}): Weight of Slot Number {InvEquip} = {Weight}",
			GetName(), HasAuthority()?"SRV":"CLI", SlotNumber, weight);
    return weight;
}

/**
 * Returns a TArray of int where the item was found in inventory slots
 * @param ItemData The item we're looking for
 * @return TArray of int. Empty Array indicates no items found.
 */
TArray<int> UInventoryComponent::GetInventorySlotNumbersContainingItem(const FStItemData& ItemData) const
{   
    TArray<int> foundSlots = TArray<int>();
    for (const FStInventorySlot& InventorySlot : GetCopyOfAllInventorySlots())
    {
    	if (InventorySlot.ContainsItem(ItemData, true))
    	{
    		foundSlots.Add(InventorySlot.SlotNumber);
    	}
    }
    return foundSlots;
}


/**
 * Returns a TArray of int where the item was found in equipment slots
 * @param ItemData The item we're looking for
 * @return TArray of int. Empty Array indicates no items found.
 */
TArray<int> UInventoryComponent::GetEquipmentSlotNumbersContainingItem(const FStItemData& ItemData) const
{
	TArray<int> foundSlots = TArray<int>();
	for (const FStInventorySlot& EquipmentSlot : GetCopyOfAllEquipmentSlots())
	{
		if (EquipmentSlot.ContainsItem(ItemData))
		{
			foundSlots.Add(EquipmentSlot.SlotNumber);
		}
	}
	return foundSlots;
}

TArray<FStInventorySlot> UInventoryComponent::GetCopyOfAllSlots() const
{
	UE_LOGFMT(LogTemp, Display, "{Inventory}({Sv}): Returning a copy of all inventory slots",
		GetName(), HasAuthority()?"SRV":"CLI");
	return InventorySlots_;
}

TArray<FStInventorySlot> UInventoryComponent::GetCopyOfAllInventorySlots() const
{
	// TODO
	return {};
}

TArray<FStInventorySlot> UInventoryComponent::GetCopyOfAllEquipmentSlots() const
{
	// TODO
	return {};
}

/**
 * Returns a pointer to the ACTUAL slot data for the slot requested.
 * @param SlotNumber An int representing the inventory slot
 */
FStInventorySlot* UInventoryComponent::GetSlotReference(int SlotNumber)
{
    if (IsValidSlotNumber(SlotNumber))
    {
    	UE_LOGFMT(LogTemp, Display, "{Inventory}({Sv}): GetSlot({SlotNum}) executed Successfully",
			GetName(), HasAuthority()?"SRV":"CLI", SlotNumber);
    	return &InventorySlots_[SlotNumber];
    }
	return nullptr;
}

void UInventoryComponent::SlotUpdated(const int SlotNumber)
{
	OnInventoryUpdated.Broadcast(SlotNumber);
}

/**
 * Gets the index (slot) of the slot where gameplay tag type was found.
 * @param SlotTag The Tag to look for
 * @return Int representing the slot number found. Negative indicates failure.
 */
int UInventoryComponent::GetSlotNumberByTag(const FGameplayTag& SlotTag)
{
	
	// Tag was previously mapped, use the existing value.
	const int* MappedSlot = TagsMappedToSlots_.Find(SlotTag);
	const int SlotNumber  = (MappedSlot != nullptr) ? *MappedSlot : -1;
	if (SlotNumber >= 0) { return SlotNumber; }
	
    if (SlotTag.GetGameplayTagParents().HasTag(TAG_Equipment_Slot.GetTag()))
    {
        // Loop through until we find the slot since we only have 1 of each
        for (const FStInventorySlot& EquipmentSlot : GetCopyOfAllEquipmentSlots())
        {
        	if (EquipmentSlot.EquipmentTag == SlotTag)
        	{
        		TagsMappedToSlots_.Add(SlotTag, EquipmentSlot.SlotNumber);
        		return EquipmentSlot.SlotNumber;
        	}
        }
    }
	
	UE_LOGFMT(LogTemp, Warning,
		"{Inventory}({Sv}): No Equipment Slot Exists for Tag '{SlotTag}'",
		GetName(), HasAuthority()?"SRV":"CLI", TAG_Equipment_Slot.GetTag().ToString());
    return -1;
}

/** 
 * Returns the truth of whether the requested slot is a real slot or not.
 * @param SlotNumber An int int of the slot being requested
 * @return Boolean
 */
bool UInventoryComponent::IsValidSlotNumber(int SlotNumber) const
{
	const bool slotValid = InventorySlots_.IsValidIndex(SlotNumber);
	UE_LOGFMT(LogTemp, Log,
		"{Inventory}({Sv}): Validity Check... '{SlotNumber}' {Validity}",
		GetName(), HasAuthority()?"SRV":"CLI", SlotNumber,
		slotValid ? "is a valid slot" : "is NOT a valid slot");
	return slotValid;
}

/**
 * Checks if the given equipment tag is a corresponding valid inventory slot
 * @param SearchTag The equip slot tag to look for
 * @return Truth of outcome
 */
bool UInventoryComponent::IsValidEquipmentSlot(const FGameplayTag& SearchTag) const
{
    for (const FStInventorySlot& InventorySlot : GetCopyOfAllInventorySlots())
    {
    	if (InventorySlot.EquipmentTag == SearchTag)
    	{
    		UE_LOGFMT(LogTemp, Display,
				"{Inventory}({Sv}): Equipment Enum '{SlotEnum}' is a VALID Equipment Slot",
				GetName(), HasAuthority()?"SRV":"CLI", SearchTag.ToString());
		    return true;
    	}
    }
	UE_LOGFMT(LogTemp, Display,
		"{Inventory}({Sv}): Equipment Enum '{SlotEnum}' is NOT a valid Equipment Slot",
		GetName(), HasAuthority()?"SRV":"CLI", SearchTag.ToString());
    return false;
}

bool UInventoryComponent::IsValidSlotByEquipmentTag(const FGameplayTag& SearchTag) const
{
	// TODO
	return false;
}

/**
 * Returns the truth of whether the requested slot is empty.
 * Performs IsValidSlotNumber() internally. No need to call both functions.
 * @param SlotNumber The slot number to check.
 * @return If true, the slot is vacant.
 */
bool UInventoryComponent::IsSlotNumberEmpty(int SlotNumber) const
{
    if (!IsValidSlotNumber(SlotNumber)) { return false; }
	const bool isEmpty = GetQuantityOfItemsInSlotNumber(SlotNumber) < 1;
	UE_LOGFMT(LogTemp, Display,
		"{Inventory}({Sv}): Slot Number {SlotNum} is {EmptyStatus}",
		GetName(), HasAuthority()?"SRV":"CLI", SlotNumber,
		isEmpty ? "EMPTY" : "NOT Empty");
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
    return IsSlotNumberEmpty(SlotNumber);
}

/**
 * Retrieves the quantity of the item in the given slot.
 * @param SlotNumber The int int representing the slot number being requested
 * @return Int of the slot number. Negative indicates failure.
 */
int UInventoryComponent::GetQuantityOfItemsInSlotNumber(int SlotNumber) const
{
    if (!IsValidSlotNumber(SlotNumber)) { return -1; }
    const int SlotQuantity = GetCopyOfSlotNumber(SlotNumber).GetQuantity();
    UE_LOGFMT(LogTemp, Display,
    	"{Inventory}({Sv}): Slot Number {SlotNum} - Quantity = {Amount}",
    	GetName(), HasAuthority()?"SRV":"CLI", SlotNumber, SlotQuantity);
    return SlotQuantity;
}

/**
 * @brief Adds the item in the referenced slot to this inventory with the given quantity
 * @param InventorySlot READONLY - The inventory slot containing the reference for the item being copied
 * @param OrderQuantity How many to add
 * @param SlotNumber The slot to add to. Negative means stack or fill first slot.
 * @param bAddOverflow If true, add to the next eligible slot if this slot fills up
 * @param bDropOverflow If true, drop items to ground if slot fills up
 * @param bNotify If true, notify the player a new item was added
 * @return The amount of items added. Negative indicates error.
 */
int UInventoryComponent::AddItem(const FStInventorySlot& InventorySlot,
    int OrderQuantity, int SlotNumber, bool bAddOverflow, bool bDropOverflow, bool bNotify)
{
    // If reference slot is empty, do nothing
    if (InventorySlot.IsSlotEmpty())
    {
    	UE_LOGFMT(LogTemp, Display,
			"{Inventory}({Sv}): AddItem Failed - Existing Slot is Empty",
			GetName(), HasAuthority()?"SRV":"CLI");
	    return -1;
    }
    int RemainingQuantity = OrderQuantity > 0 ? OrderQuantity : 1;

    // This is a safeguard against an infinite loop
    int LoopIterations = 0;
    do
    {
        LoopIterations += 1;
        
        // If the slot number is invalid, find first stackable or empty slot
        if (SlotNumber < 0)
        {
            // See if a valid existing slot can be stacked
            if (InventorySlot.GetMaxStackAllowance() > 1)
            {
            	// Returns a list of slots that have the exact same item
                TArray<int> MatchingSlots(GetInventorySlotNumbersContainingItem(InventorySlot.SlotItemData));
                for (const int& ExistingSlotNumber : MatchingSlots)
                {
                	if (IsValidSlotNumber(ExistingSlotNumber))
                	{
                        const FStInventorySlot MatchingSlot  = GetCopyOfSlotNumber(ExistingSlotNumber);
						if (!MatchingSlot.IsSlotFull())
						{
							SlotNumber = MatchingSlot.SlotNumber;
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
        	FStInventorySlot* ReferenceSlot = GetSlotReference(SlotNumber);
        	
        	if (ReferenceSlot != nullptr)
        	{
        		// If the slot is empty, set the slot item
        		if (ReferenceSlot->IsSlotEmpty())
        		{
        			ReferenceSlot->SlotItemData = FStItemData(InventorySlot.SlotItemData);
        			// Manually set quantity to 0 so we don't lose the item
        			ReferenceSlot->SlotItemData.ItemQuantity = 0;
        		}

        		// Either adds all the remaining items, or adds as much as it can
        		// Which will be deducted from RemainingQuantity for the next iteration
        		const int itemsAdded = ReferenceSlot->IncreaseQuantity(RemainingQuantity);

        		if (itemsAdded <= 0) { SlotNumber = -1; }
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
    	pickupItem->SetupItem(InventorySlot.SlotItemData, RemainingQuantity);
        RemainingQuantity = 0;
    }
	
	UE_LOGFMT(LogTemp, Display,
		"{Inventory}({Sv}): AddItem() Finished - {NumStart} Requested."
		"{NumAdded} Added, {NumRemain} Remaining",
		GetName(), HasAuthority()?"SRV":"CLI", OrderQuantity, OrderQuantity - RemainingQuantity, RemainingQuantity);
	
    return RemainingQuantity;

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

    const FStItemData NewItemData(ItemDataAsset, QuantityToAdd);
    
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
			const FStInventorySlot CopyOfSlot = GetCopyOfSlotNumber(SlotNumber);
			if (CopyOfSlot.IsSlotFull())
			{
				UE_LOGFMT(LogTemp, Warning,
					"{Inventory}({Sv}): AddItemFromDataTable() Failed - Inventory Slot Full",
					GetName(), HasAuthority()?"SRV":"CLI", CopyOfSlot.SlotItemData.ToString());
				return -3;
			}
		}
    }

    // pseudo-slot to simply function operations & avoid repeated code
    const FStInventorySlot PseudoSlot = FStInventorySlot(NewItemData);

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

/** Returns a copy of the slot data for the slot requested.
 * Public accessor version of 'GetSlot', but returns a copy instead of a reference.
 * @param SlotNumber An int representing the inventory slot
 * @return FStSlotContents of the found slot. Returns default struct if slot is empty or not found.
 */
FStInventorySlot UInventoryComponent::GetCopyOfSlotNumber(int SlotNumber) const
{
	if (IsValidSlotNumber(SlotNumber))
	{
		UE_LOGFMT(LogTemp, Display,
			"{Inventory}({Sv}): Copy of Slot Number {SlotNumber} Requested",
			GetName(), HasAuthority()?"SRV":"CLI", SlotNumber);
		return InventorySlots_[SlotNumber];
	}
	UE_LOGFMT(LogTemp, Display,
		"{Inventory}({Sv}): Copy of Slot Number {SlotNumber} Failed - Invalid Slot",
		GetName(), HasAuthority()?"SRV":"CLI", SlotNumber);
    return FStInventorySlot();
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
	
	const FStInventorySlot InventorySlot = GetCopyOfSlotNumber(OriginSlotNumber);
	if (InventorySlot.IsSlotEmpty())
	{
		UE_LOGFMT(LogTemp, Error,
			"{Inventory}({Sv}): RemoveItemFromSlot() Failed - "
			" #{SlotNum} is EMPTY",
			GetName(), HasAuthority()?"SRV":"CLI", OriginSlotNumber);
		return -1;
	}
	
	// Ensures 'quantity' is not greater than the actually quantity in slot
	const int slotQuantity = GetQuantityOfItemsInSlotNumber(OriginSlotNumber);
	int RemoveQuantity = AdjustedQuantity > slotQuantity ? slotQuantity : AdjustedQuantity;
	
	// All items are being removed
	int ItemsRemoved, NewQuantity;
	if (RemoveQuantity >= slotQuantity || bRemoveAllItems)
	{
		RemoveQuantity = slotQuantity;
	}

	{
		FRWScopeLock WriteLock(InventoryMutex, SLT_Write);

    	FStInventorySlot* SlotReference = GetSlotReference(OriginSlotNumber);

    	NewQuantity = SlotReference->SlotItemData.DecreaseQuantity(RemoveQuantity);
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
	const FStItemData& ItemReference, int OrderQuantity, bool bRemoveEquipment, bool bDropOnGround, bool bRemoveAll)
{
    if (!GetIsInventorySystemReady())
    {
    	UE_LOGFMT(LogTemp, Warning,
			"{Inventory}({Sv}): RemoveItemByQuantity() Failed - Inventory Not Ready",
			GetName(), HasAuthority()?"SRV":"CLI");
	    return -1;
    }
	
    if (!ItemReference.GetIsValidItem())
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
	for (FStInventorySlot& SlotReference : InventorySlots_)
	{
		if (SlotReference.ContainsItem(ItemReference))
		{
			const int itemsRemoved = SlotReference.DecreaseQuantity(AdjustedQuantity);
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
	FStInventorySlot& OriginSlot, FStInventorySlot& TargetSlot, int& RemainingQuantity)
{
    if (!IsValid(OriginSlot.ParentInventory) && !IsValid(TargetSlot.ParentInventory))
    {
    	UE_LOGFMT(LogTemp, Warning,
			"{Inventory}({Sv}): SwapOrStackWithRemainder() Failed - "
			"Swap Slots must have a valid parent inventory.",
			GetName(), HasAuthority()?"SRV":"CLI");
	    return false;
    }
	
    // If origin slot is empty, what are they swapping/stacking? Use Add & Remove.
    if (OriginSlot.IsSlotEmpty() || TargetSlot.IsSlotEmpty())
    {
    	UE_LOGFMT(LogTemp, Log,
			"{Inventory}({Sv}): SwapOrStackWithRemainder() Failed - Slot #{SlotNum} is an Empty Slot",
			GetName(), HasAuthority()?"SRV":"CLI", TargetSlot.SlotNumber);
        return false;
    }

	const bool  		isOriginEquipSlot 	= OriginSlot.GetIsEquipmentSlot();
	const bool  		isTargetEquipSlot 	= TargetSlot.GetIsEquipmentSlot();
	const FStItemData* 	OriginItem 			= OriginSlot.GetItemData();
	const FStItemData* 	TargetItem 			= TargetSlot.GetItemData();

	/**
	 * Equipment Slot Validation
	 */
	 
	// Validates that the target item can fit into the origin slot's equipment slot type
	if (isOriginEquipSlot && TargetItem != nullptr)
	{
		if (isTargetEquipSlot)
		{
			if (!TargetItem->GetValidEquipmentSlots().HasTag(OriginSlot.EquipmentTag))
			{
				UE_LOGFMT(LogTemp, Log,
					"{Inventory}({Sv}): SwapOrStackWithRemainder() Failed - "
					"Target (Slot {nTargetSlot}) is not a valid equipment slot for the origin item (Slot {nOriginSlot}).",
					GetName(), HasAuthority()?"SRV":"CLI", TargetSlot.SlotNumber, OriginSlot.SlotNumber);
				return false;
			}
		}
	}
	
	// Validates that the origin item can fit into the target slot's equipment slot type
	if (isTargetEquipSlot && OriginItem != nullptr)
	{
		if (isOriginEquipSlot)
		{
			// Check equipment item (target) not allowed in the origin slot
			if (!OriginItem->GetValidEquipmentSlots().HasTag(TargetSlot.EquipmentTag))
			{
				UE_LOGFMT(LogTemp, Log,
					"{Inventory}({Sv}): SwapOrStackWithRemainder() Failed - "
					"Target (Slot {nTargetSlot}) is not a valid equipment slot for the origin item (Slot {nOriginSlot}).",
					GetName(), HasAuthority()?"SRV":"CLI", OriginSlot.SlotNumber, TargetSlot.SlotNumber);
				return false;
			}
		}
	}

	/**
	 * Validates like-items
	 */
	const bool	isSameExactItem = TargetSlot.ContainsItem(*OriginItem, true);
	const int   MaxStackSize	= TargetItem->GetMaxStackSize();

	// Exact same item, and stackable
    if (isSameExactItem && (MaxStackSize > 1))
    {
    	const int SpaceRemaining = TargetSlot.GetMaxStackAllowance() - TargetSlot.GetQuantity();
    	if (SpaceRemaining < 1)
    	{
			UE_LOGFMT(LogTemp, Display,
				"{Inventory}({Sv}): {InvName}({SlotNum}) has no space for origin stack request.",
				GetName(), HasAuthority()?"SRV":"CLI", TargetSlot.ParentInventory->GetName(), TargetSlot.SlotNumber);
			return true;
    	}
    	const int itemsRemoved = OriginSlot.DecreaseQuantity(RemainingQuantity);
    	if (itemsRemoved > 0)
    	{ 
    		const int itemsAdded = TargetSlot.IncreaseQuantity(itemsRemoved);
    		if (itemsAdded > 0)
    		{
    			RemainingQuantity -= itemsAdded;
    			UE_LOGFMT(LogTemp, Display,
					"{Inventory}({Sv}): Successfully Stacked x{nQuantity} of {ItemName}",
					GetName(), HasAuthority()?"SRV":"CLI", itemsAdded, OriginItem->ToString());
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
	OriginSlot.SlotItemData = FStItemData(*TargetItem);
	TargetSlot.SlotItemData = FStItemData(*OriginItem);
	RemainingQuantity = 0;
	
    UE_LOGFMT(LogTemp, Display,
		"{Inventory}({Sv}): Successfully Swapped {OriginInv}({OriginNum}) with {TargetInv}({TargetNum})",
		GetName(), HasAuthority()?"SRV":"CLI",
		OriginSlot.ParentInventory->GetName(), OriginSlot.SlotNumber,
		TargetSlot.ParentInventory->GetName(), TargetSlot.SlotNumber);
	return true;
}

/**
 * @brief Finds which slots have changed, and sends an update event
 */
void UInventoryComponent::OnRep_InventorySlotUpdated_Implementation(const TArray<FStInventorySlot>& OldSlot)
{
	bool wasUpdated = false;
	for (int i = 0; i < GetNumberOfTotalSlots(); i++)
	{
		// Existing slot updated
		if (OldSlot.IsValidIndex(i))
		{
			const FStInventorySlot* CurrentSlot = GetSlotReference(i);
			const FStInventorySlot* OldSlotRef  = &OldSlot[i];
			
			if (CurrentSlot->LastUpdate > OldSlotRef->LastUpdate)
			{
				UE_LOGFMT(LogTemp, Log,
					"{Inventory}({Sv}) REPNOTIFY: Updated Inventory Slot {SlotNum}",
					GetName(), HasAuthority()?"SRV":"CLI", i);
				OnInventoryUpdated.Broadcast(i);
				wasUpdated = true;
			}
		}
		
		// New Slot Added
		else
		{
			UE_LOGFMT(LogTemp, Log,
				"{Inventory}({Sv}) REPNOTIFY: Added Inventory Slot {SlotNum}",
				GetName(), HasAuthority()?"SRV":"CLI", i);
			OnInventoryUpdated.Broadcast(i);
			wasUpdated = true;
		}
	}
	if (!wasUpdated)
	{
		UE_LOGFMT(LogTemp, Warning,
			"{Inventory}({Sv}) REPNOTIFY: Unable to ascertain which Inventory Slot was updated.",
			GetName(), HasAuthority()?"SRV":"CLI");
	}
}

void UInventoryComponent::Server_RestoreSavedInventory_Implementation(
	const TArray<FStInventorySlot>& RestoredInventory)
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
    FStInventorySlot& OriginSlot, FStInventorySlot& TargetSlot, int OrderQuantity, bool bNotify)
{
	if (OriginSlot.IsSlotEmpty())
	{
		UE_LOGFMT(LogTemp, Display,
			"{Inventory}({Sv}) Failed - Origin Slot is Empty ({InvName} Slot {SlotNum})",
			GetName(), HasAuthority()?"SRV":"CLI", OriginSlot.ParentInventory->GetName(), OriginSlot.SlotNumber);
		return false;
	}

	const int AddQuantity = OrderQuantity > 0 ? OrderQuantity : 1;
	const FStItemData* OriginItem = OriginSlot.GetItemData();
	if (TargetSlot.ContainsItem( *OriginItem ))
	{
		FStItemData* TargetItem = TargetSlot.GetItemData();
		if (TargetItem != nullptr)
		{
			if (OriginItem->GetIsExactSameItem( *TargetItem ))
			{
				const int itemsRemoved = OriginSlot.DecreaseQuantity(AddQuantity);
				if (itemsRemoved > 0)
				{
					const int itemsAdded = TargetItem->IncreaseQuantity(itemsRemoved);
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
		OriginSlot.ParentInventory->GetName(), OriginSlot.SlotNumber);
    return true;
}

/**
* Increases the amount of an existing item by the quantity given.
* If the given slot is empty, it will do nothing and fail.
*
* @param InventorySlot	The slot number to be increased
* @param OrderQuantity	The quantity to increase the slot by, up to the maximum quantity
* @param bNotify		Show a notification when quantity increases. False by default.
* @return				The number of items actually added. Negative indicates failure.
*/
int UInventoryComponent::IncreaseSlotQuantity(FStInventorySlot& InventorySlot, int OrderQuantity, bool bNotify)
{
    if (!InventorySlot.IsSlotEmpty()) {InventorySlot.IncreaseQuantity(OrderQuantity);}
	return 0;
}

/**
* Increases the amount of an existing item by the quantity given.
* If the given slot is empty, it will do nothing and fail.
*
* @param InventorySlot	The slot number to be decreased
* @param OrderQuantity	The quantity to increase the slot by
* @param bNotify		Show a notification when quantity increases. False by default.
* @return				The number of items actually removed. Negative indicates failure.
*/
int UInventoryComponent::DecreaseSlotQuantity(FStInventorySlot& InventorySlot, int OrderQuantity, bool bNotify)
{
	if (!InventorySlot.IsSlotEmpty()) {InventorySlot.DecreaseQuantity(OrderQuantity);}
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
* Sends a notification that an item was modified. Since the item may or may not exist
* at the time the notification is created, we create a new copy of the item.
* @param ItemData The affected item
* @param OrderQuantity The quantity affected
*/
void UInventoryComponent::SendNotification(const FStItemData ItemData, int OrderQuantity)
{
    Notifications_.Add( FStInventoryNotify(ItemData, OrderQuantity) );
	UE_LOGFMT(LogTemp, Log,
		"{Inventory}({Sv}): New Notification - There are now {n} notifications pending",
		GetName(), HasAuthority()?"SRV":"CLI", Notifications_.Num());
    OnNotificationAvailable.Broadcast();
}

/**
* Wipes the inventory slot without destroying the settings (equipment, etc)
* @param InventorySlot The Slot Reference to reset
*/
void UInventoryComponent::ResetSlot(FStInventorySlot& InventorySlot)
{
	InventorySlot.EmptyAndResetSlot();
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
	FStInventorySlot& OriginSlot, FStInventorySlot& TargetSlot, int OrderQuantity, bool bDropOverflow)
{
	if (OriginSlot.IsSlotEmpty())
	{
		UE_LOGFMT(LogTemp, Warning,
			"{Inventory}({Sv}): tried to transfer from {InvName}({SlotNum}), but the slot is EMPTY",
			GetName(), HasAuthority()?"SRV":"CLI", OriginSlot.ParentInventory->GetName(), OriginSlot.SlotNumber);
		return false;
	}

	bool bNotify = (OriginSlot.ParentInventory != TargetSlot.ParentInventory); 

	int MoveQuantity = OrderQuantity > 0 ? OrderQuantity : 1;
	
    if (OriginSlot.IsSlotEmpty())
    {
        // If an item exists, attempt to swap or stack them
        if (TargetSlot.IsSlotEmpty())
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
		const FStInventorySlot* ActivatedSlot = GetSlotReference(SlotNumber);
    	
    	FStItemData ItemInSlot(ActivatedSlot->SlotItemData);

    	if (ItemInSlot.GetCanActivate())
    	{
    		if (ItemInSlot.ActivateItem(bForceConsume))
    		{

    		}
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
    	FStInventorySlot* ActivatedSlot = GetSlotReference(SlotNumber);
    	ActivatedSlot->Activate();
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
			( UGameplayStatics::LoadGameFromSlot(SaveSlotName_, SaveUserIndex_) );
		if (!IsValid(InventorySave))
		{
			OnInventoryRestored.Broadcast(false);
			return;
		}
	}

	RestoreInventory( InventorySave->LoadInventorySlots() );
	OnInventoryRestored.Broadcast(true);
}

/**
 *  Saving Delegate that is called when save data was found asynchronously
 * @param SaveSlotName The name of the save slot to be used
 * @param UserIndex Usually zero
 * @param SaveData Pointer to the save data. Invalid if it does not exist.
 */
 void UInventoryComponent::SaveInventoryDelegate(
 		const FString& SaveSlotName, int32 UserIndex, USaveGame* SaveData)
{
	if (!IsValid(SaveData))
	{
		SaveData = UGameplayStatics::CreateSaveGameObject( UInventorySave::StaticClass() );
		if (!IsValid(SaveData))
		{
			UE_LOGFMT(LogTemp, Error,
				"{Inventory}({Sv}): (Async Response) Failed to create a new inventory save object.",
				GetName(), HasAuthority()?"SRV":"CLI", SaveSlotName, UserIndex);
			OnInventoryRestored.Broadcast(false);
			return;
		}
		UE_LOGFMT(LogTemp, Log,
			"{Inventory}({Sv}): (Async Response) Created new Inventory Save with name '{SaveName}' @ index '{Index}'",
			GetName(), HasAuthority()?"SRV":"CLI", SaveSlotName, UserIndex);
	}
	
	Helper_SaveInventory(SaveData);
	UGameplayStatics::SaveGameToSlot(SaveData, SaveSlotName_, SaveUserIndex_);

	UE_LOGFMT(LogTemp, Log,
		"{Inventory}({Sv}): (Async Response) Successfully saved the inventory with name '{SaveName}' @ index {Index}'",
		GetName(), HasAuthority()?"SRV":"CLI", SaveSlotName_, SaveUserIndex_);
	OnInventoryRestored.Broadcast(true);
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
	FStInventorySlot* FromSlot = OriginInventory->GetSlotReference(OriginSlotNumber);
	FStInventorySlot* ToSlot   = TargetInventory->GetSlotReference(TargetSlotNumber);
    if (FromSlot == ToSlot)
    {
    	UE_LOGFMT(LogTemp, Log,
			"{Inventory}({Sv}): Server_TransferItems() Canceled - Transfer is the exact same slot",
			GetName(), HasAuthority()?"SRV":"CLI");
	    return;
    }


	const bool ToSlotLocked		= ToSlot->SlotTag == TAG_Inventory_Slot_Locked.GetTag();
	const bool FromSlotLocked	= ToSlot->SlotTag == TAG_Inventory_Slot_Locked.GetTag();
	if (ToSlotLocked || FromSlotLocked)
    {
    	UE_LOGFMT(LogTemp, Warning,
			"{Inventory}({Sv}): {InvName}, Slot Number {SlotNum}, is a read-only slot (locked).",
			GetName(), HasAuthority()?"SRV":"CLI",
			ToSlotLocked ? ToSlot->ParentInventory->GetName() : FromSlot->ParentInventory->GetName(),
			ToSlotLocked ? ToSlot->SlotNumber : FromSlot->SlotNumber);
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
	
    if (OriginPlayer->GetDistanceTo(TargetPlayer) > MaxInventoryReach_)
    {
    	UE_LOGFMT(LogTemp, Error,
			"{Inventory}({Sv}): Transfer Rejected. Too Far Away ({Distance} (Max: {MaxDistance})",
			GetName(), HasAuthority()?"SRV":"CLI", OriginPlayer->GetName(),
			OriginPlayer->GetDistanceTo(TargetPlayer), MaxInventoryReach_);
    	return;
    }

	OrderQuantity = OrderQuantity > 0 ? OrderQuantity : 1;
        
    // Make a copy of the origin item, and decrease it from the origin inventory.
	// Always TAKE items first to avoid dupe exploits
	const FStItemData FromSlotCopy = FromSlot->GetCopyOfItemData();
	const FStItemData ToSlotCopy   = ToSlot->GetCopyOfItemData();
    if (!FromSlot->IsSlotEmpty())
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
    const int itemsAdded = TargetInventory->AddItem(FromSlotCopy, OrderQuantity, OriginSlotNumber,
    	false, false, bNotify);
    if (itemsAdded > 0)
    {
    	UE_LOGFMT(LogTemp, Log,
			"{Inventory}({Sv}): Successfully Transferred x{Quantity} of {ItemName} "
			"From '{FromInv} {SlotType} #{SlotNum}' to '{ToInv} {ToSlotType} #{ToSlotNum}'",
			GetName(), HasAuthority()?"SRV":"CLI",
			OriginInventory->GetName(), OriginSlotNumber,
			TargetInventory->GetName(), TargetSlotNumber);
    }
    
    // If the add failed, reinstate the item to the origin inventory
	// If the original from slot and current from slots quantities don't match, reimburse the items
    else if (FromSlotCopy.ItemQuantity != FromSlot->GetQuantity())
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

	FStInventorySlot  SlotCopy		= OriginInventory->GetCopyOfSlotNumber(SlotNumber);
	FStInventorySlot* InventorySlot	= OriginInventory->GetSlotReference(SlotNumber);
	if (InventorySlot == nullptr) { return; }
	
	// Not all items being tossed will be in the player's inventory. Sometimes
	//  they will be storage chests, bodies, etc.
	FStInventorySlot* FromSlot = OriginInventory->GetSlotReference(SlotNumber);
	if (FromSlot == nullptr)
	{
		UE_LOGFMT(LogTemp, Warning,
			"{Inventory}({Sv}): Attempted to drop item from {FromInv} Slot #{SlotNum}, "
			"but the slot is Empty (or Invalid)", GetName(), HasAuthority()?"SRV":"CLI",
			OriginInventory->GetName(), SlotNumber);
		return;
	}
	if (FromSlot->IsSlotEmpty()) { return; }

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

	FStItemData ItemCopy	= InventorySlot->GetCopyOfItemData();
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
		UE_LOGFMT(LogTemp, Error,
			"{Inventory}({Sv}): Item '{ItemName} (x{Quantity}) was dropped "
			"successfully, but the pickup actor failed to spawn. Will attempt to reimburse.",
			GetName(), HasAuthority()?"SRV":"CLI", ItemCopy.ToString(), ItemCopy.ItemQuantity);
		OriginInventory->AddItem(SlotCopy, OrderQuantity, SlotNumber);
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