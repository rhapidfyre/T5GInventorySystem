
// ReSharper disable CppUE4CodingStandardNamingViolationWarning
#include "InventoryComponent.h"

#include "InventorySystemGlobals.h"
#include "PickupActorBase.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "lib/InventorySave.h"
#include "lib/ItemData.h"
#include "Logging/StructuredLog.h"
#include "Net/UnrealNetwork.h" // Used for replication


void UInventoryComponent::Helper_SaveInventory(USaveGame*& SaveData) const
{
	UInventorySave* InventorySave = Cast<UInventorySave>( SaveData );
	if (IsValid(SaveData))
	{
		InventorySave->SetSaveName(SaveSlotName_);
		InventorySave->InventorySlots_ = InventorySlots_;
		InventorySave->EquipmentSlots_ = EquipmentSlots_;
	}
}

void UInventoryComponent::BeginPlay()
{
    if (bShowDebug)
    {
        UE_LOGFMT(LogTemp, Display, "{InventoryName}({Sv}): BeginPlay()",
            GetName(), HasAuthority()?"SRV":"CLI");
    }
    Super::BeginPlay();
    
    const AActor* ownerActor = GetOwner();
    if (!IsValid(ownerActor))
    {
        UE_LOGFMT(LogTemp, Error, "{InventoryName} has no owner. Removed.", GetName());
        this->DestroyComponent(); // Kills itself, it failed validation
    }
	
	UE_LOGFMT(LogTemp, Display, "InventoryComponent({Sv}): Began Play with {iNum} Inventory Slots and {eNum} Equipment Slots",
		HasAuthority()?"SRV":"CLI", GetNumberOfSlots(), GetNumberOfSlots(true));

    if (!IsValid(UItemSystem::getItemDataTable()))
    {
        UE_LOGFMT(LogEngine, Error, "{InventoryName}({Sv}): Item Data Table Not Found!",
            GetName(), HasAuthority()?"SRV":"CLI");
    }
    else
    {
        UE_LOGFMT(LogEngine, Display, "{InventoryName}({Sv}): Data Table Was Found",
            GetName(), HasAuthority()?"SRV":"CLI");
    }
}

UInventoryComponent::UInventoryComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);

	// TODO - Switch over to gameplay tags
	EligibleEquipmentSlots = {
		EEquipmentSlotType::PRIMARY,		EEquipmentSlotType::SECONDARY,
		EEquipmentSlotType::HELMET,			EEquipmentSlotType::NECK,
		EEquipmentSlotType::EARRINGLEFT,	EEquipmentSlotType::EARRINGRIGHT,
		EEquipmentSlotType::FACE, 			EEquipmentSlotType::SHOULDERS,
		EEquipmentSlotType::BACK, 			EEquipmentSlotType::SLEEVES,
		EEquipmentSlotType::WRISTLEFT,		EEquipmentSlotType::WRISTRIGHT,
		EEquipmentSlotType::HANDS,			EEquipmentSlotType::RINGLEFT,
		EEquipmentSlotType::RINGRIGHT,		EEquipmentSlotType::TORSO,
		EEquipmentSlotType::WAIST,			EEquipmentSlotType::LEGS,
		EEquipmentSlotType::FEET,			EEquipmentSlotType::COSMETIC
	};
}


bool UInventoryComponent::HasAuthority() const
{
	if (IsValid(GetOwner()))
	{
		return GetOwner()->HasAuthority();
	}
	return GetNetMode() < NM_Client;
}

/*
 * Runs the initialization after UE has created the UObject system.
 * Sets up the stuff for the inventory that can't be done before the game is running.
 */
void UInventoryComponent::ReinitializeInventory()
{
	if (!GetIsInventoryReady())
	{
		// Cancel if the character is invalid
		ACharacter* OwnerCharacter = Cast<ACharacter>( GetOwner() );
		if (!IsValid(OwnerCharacter))
		{
			return;
		}

		// Ensure the number of inventory slots is valid.
		NumberOfInvSlots = (NumberOfInvSlots > 0) ? NumberOfInvSlots : 6;

		// Reset the Inventory & Equipment Arrays
		InventorySlots_.Empty();
		EquipmentSlots_.Empty();
		Notifications_.Empty(); // and any pending notifications
    	
		// Set up Inventory Slots
		for (int i = 0; i < NumberOfInvSlots; i++)
		{
			// Initialize with inventory slot type makes it a non-equipment slot
			FStInventorySlot newSlot(EInventorySlotType::GENERAL);
			newSlot.SlotNumber = i;
			InventorySlots_.Add(newSlot);
			OnInventoryUpdated.Broadcast(i, false);
		}
	
		// Set up Equipment Slots
		for (int i = 0; i < EligibleEquipmentSlots.Num(); i++)
		{
			if (EligibleEquipmentSlots.IsValidIndex(i))
			{
				if (EligibleEquipmentSlots[i] != EEquipmentSlotType::NONE)
				{
					// Ignore slots already added - There can be only one..
					const int slotNumber = GetSlotNumberFromEquipmentType(EligibleEquipmentSlots[i]);
					if (slotNumber < 0)
					{
						// Initialize with Equip Slot type makes it an equipment slot
						FStInventorySlot newSlot(EligibleEquipmentSlots[i]);
						newSlot.SlotNumber = i;
						EquipmentSlots_.Add(newSlot);
						OnInventoryUpdated.Broadcast(slotNumber, true);
					}
				}
			}
		}
    	
		UE_LOGFMT(LogTemp, Display, "{OwnerName}({Authority}): Reinitialized. Found {nItems} Starting Items",
			OwnerCharacter->GetName(), HasAuthority()?"SRV":"CLI", StartingItems.Num());

		bIsInventoryReady = true;
	}
}

void UInventoryComponent::IssueStartingItems()
{
	// Never issue items if the inventory has been restored from save
	if (bInventoryRestored)
	{
		return;
	}

	// Add starting items to the inventory
	for (int i = 0; i < StartingItems.Num(); i++)
	{
		if (StartingItems.IsValidIndex(i))
		{
			// Determine the starting item by item name given
			const FName itemName = StartingItems[i].startingItem;
			const FStItemData ItemData = UItemSystem::getItemDataFromItemName(itemName);
			int slotNum = -1;

			if (StartingItems[i].bStartEquipped && IsValidSlot(i, true))
			{
				// Check each eligible equip slot for an open slot
				for (const EEquipmentSlotType EquipSlot : ItemData.equipSlots)
				{
					if (IsSlotEmptyByEnum(EquipSlot))
					{
						slotNum = GetSlotNumberFromEquipmentType(EquipSlot);
					}
				}

				// Equip item into the first eligible slot
				if (slotNum >= 0)
				{
					// TODO - A check should be added here to ensure the item
					//        is being added to an appropriate slot.
					EquipmentSlots_[slotNum].ItemName = itemName;
					EquipmentSlots_[slotNum].SlotQuantity = StartingItems[i].quantity;

					// If we are the server, we don't receive the "OnRep" call,
					//    so the broadcast needs to be called manually.
					if (GetNetMode() < NM_Client)
					{
						OnInventoryUpdated.Broadcast(slotNum, true);
					}

					// If the item gets equipped, continue the for loop at the next iteration
					continue;
                	
				}
			}
        	
			// Item isn't equipped, or all eligible slots were full
			const int itemsAdded = AddItemFromDataTable(itemName,
				StartingItems[i].quantity, slotNum,
				false, false, false);
			if (itemsAdded < 1)
			{
				UE_LOGFMT(LogTemp, Warning,
					"{InvName}({Server})}: Starting Item '{ItemName}' failed to get added",
					GetName(), HasAuthority()?"SRV":"CLI", itemName);
			}
			else
			{
				UE_LOGFMT(LogTemp, Warning,
					"{InvName}({Server})}: x{Amount} of '{ItemName}' Added to Slot #{SlotNum}",
					GetName(), HasAuthority()?"SRV":"CLI", itemsAdded, itemName, slotNum);
			}
		}
	}
}

/**
 * Returns true if the inventory is in use by any player
 * @param InUseActors An array of actors using the inventory
 * @return True if in use, false otherwise.
 */
bool UInventoryComponent::GetIsInventoryInUse(TArray<AActor*>& InUseActors) const
{
	InUseActors = InUseByActors_;
	return InUseByActors_.Num() > 0;
}

/**
 * Sets the inventory to be in use by the given player, or releases use.
 * @param UseActor The actor using the inventory. Released if nullptr.
 * @param isInUse True if the actor should be using the inventory, false if releasing.
 */
void UInventoryComponent::SetInventoryInUse(AActor* UseActor, bool isInUse)
{
	if (IsValid(UseActor))
	{
		if (InUseByActors_.Contains(UseActor))
		{
			// Actor is releasing the inventory usage
			if (!isInUse)
			{
				UE_LOGFMT(LogTemp, Log,
					"{InvName}({Server})}: Is no longer in use.",
					GetName(), GetName(), HasAuthority()?"SRV":"CLI");
				InUseByActors_.Remove(UseActor);
			}
		}
		else
		{
			// Actor is starting usage of the inventory
			if (isInUse)
			{
				UE_LOGFMT(LogTemp, Log,
					"{InvName}({Server})}: Is now being used by {UseActor}",
					GetName(), HasAuthority()?"SRV":"CLI", UseActor->GetName());
				InUseByActors_.Add(UseActor);
			}
		}
		UE_LOGFMT(LogTemp, Log,
			"{InvName}({Server})}: SetInventoryInUse() Unchanged (Same Actor, Same Status)",
			GetName(), GetName(), HasAuthority()?"SRV":"CLI");
	}
	else
	{
		UE_LOGFMT(LogTemp, Log,
			"{InvName}({Server})}: SetInventoryInUse() received invalid UseActor.",
			GetName(),  HasAuthority()?"SRV":"CLI");
	}
}

/**
 *  Restores inventory & equipment slots from a save game. Only runs once,
 *  as bRestoredFromSave is set once this is executed to protect game integrity.
 * @param RestoredInventory One-for-one copy of the inventory slots to restore
 * @param RestoredEquipment One for one copy of the equipment slots to restore
 */
void UInventoryComponent::RestoreInventory(
	const TArray<FStInventorySlot>& RestoredInventory,
	const TArray<FStInventorySlot>& RestoredEquipment)
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
	
	bIsInventoryReady = false;
	
	InventorySlots_.Empty();
	EquipmentSlots_.Empty();

	// If we iterate the arrays, OnRep will trigger.
	// Not the most optimized way, but it avoids any ugly RPC work
	for (int i = 0; i < RestoredInventory.Num(); i++)
	{
		FStInventorySlot NewSlot = RestoredInventory[i];
		InventorySlots_.Add(NewSlot);
	}
	for (int i = 0; i < RestoredEquipment.Num(); i++)
	{
		FStInventorySlot NewSlot = RestoredEquipment[i];
		EquipmentSlots_.Add(NewSlot);
	}
	
	bIsInventoryReady	= true;
	if (HasAuthority())
	{
		bInventoryRestored = true;
	}
	else
	{
		Server_RestoreSavedInventory(RestoredInventory, RestoredEquipment);
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
	if ( !doServerSave || !doClientSave || !GetIsInventoryReady())
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
		SaveSlotName_ = SavePath + TempSaveName;
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
			InventorySave->SetSaveName(SaveSlotName_);
			Helper_SaveInventory(SaveData);
			responseStr = "Successful Synchronous Save";
			UE_LOGFMT(LogInventory, Log, "{InventoryName}({Sv}): "
				"Successfully Saved Inventory '{InventorySave} ({OwnerName})",
				GetName(), HasAuthority()?"SRV":"CLI", SaveSlotName_, GetOwner()->GetName());
			return SaveSlotName_;
		}
	}
	responseStr = "SaveNameSlot was VALID but UInventorySave was NULL";
	return FString();
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
	
	if (bIsInventoryReady)
	{
		if (!UGameplayStatics::DoesSaveGameExist(SaveSlotName, 0))
		{
			responseStr = "No SaveSlotName Exists";
			return false;
		}
		SaveSlotName_ = SaveSlotName;
		
		if (isAsync)
		{
			FAsyncLoadGameFromSlotDelegate LoadDelegate;
			LoadDelegate.BindUObject(this, &UInventoryComponent::LoadDataDelegate);
			UGameplayStatics::AsyncLoadGameFromSlot(SaveSlotName_, 0, LoadDelegate);
			responseStr = "Sent Async Save Request";
			return true;
		}

		const USaveGame* SaveData = UGameplayStatics::LoadGameFromSlot(
											SaveSlotName_, 0);
		
		if (IsValid(SaveData))
		{
			const UInventorySave* InventorySave = Cast<UInventorySave>( SaveData );
			if (IsValid(InventorySave))
			{
				RestoreInventory(InventorySave->InventorySlots_,
								 InventorySave->EquipmentSlots_);
				responseStr = "Successful Synchronous Load";
				return true;	
			}
		}
	}
	return false;
}

void UInventoryComponent::OnComponentCreated()
{
	SetAutoActivate(true);
    Super::OnComponentCreated();
    RegisterComponent();
}

int UInventoryComponent::GetNumberOfSlots(bool isEquipment) const
{
	return isEquipment ? EquipmentSlots_.Num() : InventorySlots_.Num();
}

/**
 * Returns the inventory enum slot type of the given slot number. Returns NONE
 * if the slot is invalid or the slot is not an inventory slot.
 * @param slotNumber The slot number for the inventory slot inquiry
 * @return The inventory slot type enum, where NONE indicates failure.
 */
EInventorySlotType UInventoryComponent::GetSlotTypeInventory(int slotNumber) const
{
	UE_LOGFMT(LogTemp, Display, "{Inventory}({Sv}): GetSlotTypeInventory({SlotNum})",
		GetName(), HasAuthority()?"SRV":"CLI", slotNumber);
	return GetCopyOfSlot(slotNumber, false).SlotType;
}

/**
 * Returns the equipment enum slot type of the given slot number. Returns NONE
 * if the slot is invalid or the slot is not an equipment slot. 
 * @param slotNumber The slot number for the equipment slot inquiry
 * @return The equip slot type enum, where NONE indicates failure.
 */
EEquipmentSlotType UInventoryComponent::GetSlotTypeEquipment(int slotNumber) const
{
	UE_LOGFMT(LogTemp, Display, "{Inventory}({Sv}): GetSlotTypeEquipment({SlotNum})",
		GetName(), HasAuthority()?"SRV":"CLI", slotNumber);
	return GetCopyOfSlot(slotNumber, true).EquipType;
}

/**
 * Returns the total quantity of an item in the entire inventory, NOT including equipment slots.
 * @param itemName The itemName of the item we're searching for.
 * @param checkEquipment Also checks the equipment slots if true.
 * @return The total quantity of items found. Negative value indicates failure.
 */
int UInventoryComponent::GetQuantityOfItem(FName itemName, bool checkEquipment) const
{
	UE_LOGFMT(LogTemp, Display, "{Inventory}({Sv}): GetQuantityOfItem({SlotNum})",
		GetName(), HasAuthority()?"SRV":"CLI", itemName);

    int total = 0;
    for (int i = 0; i < GetNumberOfSlots(checkEquipment); i++)
    {
        if (IsValidSlot(i))
        {
            if (GetNameOfItemInSlot(i) == itemName)
            {
	            total += InventorySlots_[i].SlotQuantity;
            }
        }
    }
    return total;
}

/**
 * Returns the slotNumber of the first empty slot found in the inventory.
 * @param checkEquipment If true, gets the first empty equipment slot.
 * @return The slotNumber of the first empty slot. Negative indicates full inventory.
 */
int UInventoryComponent::GetFirstEmptySlotNumber(bool checkEquipment) const
{
    for (int i = 0; i < GetNumberOfSlots(checkEquipment); i++)
    {
        if (IsSlotEmpty(i, checkEquipment))
        {
        	UE_LOGFMT(LogTemp, Display, "GetFirstEmptySlotNumber({Sv}): Empty {SlotType} Found @ {SlotNum}",
				HasAuthority()?"SRV":"CLI", HasAuthority()?"SRV":"CLI", i);
	        return i;
        }
    }
	UE_LOGFMT(LogTemp, Display, "GetFirstEmptySlotNumber({Sv}): No Empty {SlotType} Found",
		HasAuthority()?"SRV":"CLI", HasAuthority()?"SRV":"CLI");
    return -1;
}

/**
 * Returns the total weight of all items in the inventory.
 * @param incEquip If true, the total weight will also include all equipment slots. False by default.
 * @return Float containing the total weight of the inventory. Unitless.
 */
float UInventoryComponent::GetWeightTotal(bool incEquip) const
{
    float totalWeight = 0.0f;
    for (int i = 0; i < GetNumberOfSlots(); i++)
    {
        if (!IsSlotEmpty(i))
        {
            totalWeight += getWeightInSlot(i, false);
        }
    }
	if (incEquip)
	{
		for (int i = 0; i < GetNumberOfSlots(true); i++)
		{
			if (!IsSlotEmpty(i, true))
			{
				totalWeight += getWeightInSlot(i, true);
			}
		}
	}
	UE_LOGFMT(LogTemp, Display, "{Inventory}({Sv}): Total Inventory Weight = {Weight}",
		GetName(), HasAuthority()?"SRV":"CLI", totalWeight);
    return totalWeight;
}

/**
 * Gets the total weight of all items in the given slot.
 * @param slotNumber The int representing the inventory slot to check.
 * @param isEquipmentSlot True if getting the weight of an equipment slot.
 * @return Float with the total weight of the given slot. Negative indicates failure. Zero indicates empty slot.
 */
float UInventoryComponent::getWeightInSlot(int slotNumber, bool isEquipmentSlot) const
{
    const FStInventorySlot InventorySlot = GetCopyOfSlot(slotNumber, isEquipmentSlot);
	
    if (UItemSystem::getItemNameIsValid(InventorySlot.ItemName))
    {
    	const float weight =
    		UItemSystem::getItemWeight(InventorySlot.GetItemData())
    		* GetSlotQuantity(slotNumber, isEquipmentSlot);
    	
    	UE_LOGFMT(LogTemp, Display, "{Inventory}({Sv}): Weight of Slot {SlotNum} ({InvEquip}) = {Weight}",
			GetName(), HasAuthority()?"SRV":"CLI", slotNumber,
			isEquipmentSlot?"Inventory Slot":"Equipment Slot", weight);
    	
        return weight;
    }
    	
	UE_LOGFMT(LogTemp, Display, "{Inventory}({Sv}): Weight of Slot {SlotNum} ({InvEquip}) = 0 (Empty or Invalid Slot)",
		GetName(), HasAuthority()?"SRV":"CLI", slotNumber,
		isEquipmentSlot?"Inventory Slot":"Equipment Slot");
	
    return 0.0f;
}

/**
 * Returns a TArray of int where the item was found
 * @param itemName The item we're looking for
 * @return TArray of int. Empty Array indicates no items found.
 */
TArray<int> UInventoryComponent::GetSlotsWithItem(FName itemName) const
{   
    TArray<int> foundSlots = TArray<int>();
    for (int i = 0; i < GetNumberOfSlots(); i++)
    {
        if (GetCopyOfSlot(i).ContainsItem(itemName))
        {
        	foundSlots.Add(i);
        }
    }
	if (bShowDebug)
	{
		if (foundSlots.Num() > 0)
		{
			FString SlotString(FString::FromInt(foundSlots[0]));
			for (int i = 1; i < foundSlots.Num(); i++)
			{
				SlotString.Append(", " + FString::FromInt(foundSlots[i]));
			}
			UE_LOGFMT(LogTemp, Display, "{Inventory}({Sv}): Slots Containing '{ItemName}': {SlotList}",
				GetName(), HasAuthority()?"SRV":"CLI", itemName, SlotString);
		}
		else
		{
			UE_LOGFMT(LogTemp, Display, "{Inventory}({Sv}): No Slots contain the item '{ItemName}'",
				GetName(), HasAuthority()?"SRV":"CLI", itemName);
		}
	}
    return foundSlots;
}

/**
 * Returns a reference to the actual slot data for the slot requested.
 * @param slotNumber An int representing the inventory slot
 * @param IsEquipmentSlot True if the slot is an equipment slot
 * @return FStSlotContents of the found slot. Returns default struct if slot is empty or not found.
 */
FStInventorySlot& UInventoryComponent::GetSlot(int slotNumber, bool IsEquipmentSlot)
{
	UE_LOGFMT(LogTemp, Display, "{Inventory}({Sv}): GetSlot({SlotNum})",
		GetName(), HasAuthority()?"SRV":"CLI", slotNumber);
	
    if (!IsValidSlot(slotNumber, IsEquipmentSlot))
    {
    	FStInventorySlot* EmptySlot = new FStInventorySlot();
    	UE_LOGFMT(LogTemp, Display, "{Inventory}({Sv}): GetSlot({SlotNum}) returned Empty Slot (Invalid Slot)",
			GetName(), HasAuthority()?"SRV":"CLI", slotNumber);
	    return *EmptySlot;
    }
	
	UE_LOGFMT(LogTemp, Display, "{Inventory}({Sv}): GetSlot({SlotNum}) executed Successfully",
		GetName(), HasAuthority()?"SRV":"CLI", slotNumber);
    return IsEquipmentSlot ? EquipmentSlots_[slotNumber] : InventorySlots_[slotNumber];
}

TArray<FStInventorySlot> UInventoryComponent::GetCopyOfAllSlots(bool getEquipmentSlots) const
{
	UE_LOGFMT(LogTemp, Display, "{Inventory}({Sv}): Getting an array of all {SlotType}s",
		GetName(), HasAuthority()?"SRV":"CLI", getEquipmentSlots ? "Equipment Slot" : "Inventory Slot");
	return getEquipmentSlots ? EquipmentSlots_ : InventorySlots_;
}

/**
 * Gets the index (slot) of the equipment slot where equipment type was found.
 * @param equipEnum Enum of the slotType to find.
 * @return Int representing the slot number found. Negative indicates failure.
 */
int UInventoryComponent::GetSlotNumberFromEquipmentType(EEquipmentSlotType equipEnum) const
{
	
    // Makes sure incoming param is a valid equip slot
    if (equipEnum != EEquipmentSlotType::NONE)
    {
        // Loop through until we find the equip slot since we only have 1 of each
        for (int i = 0; i < GetNumberOfSlots(true); i++)
        {
            if (GetCopyOfSlot(i, true).EquipType == equipEnum)
            {
            	UE_LOGFMT(LogTemp, Display,
            		"{Inventory}({Sv}): Equipment Slot '{SlotEnum}' is Slot Number {SlotNum}",
					GetName(), HasAuthority()?"SRV":"CLI", UEnum::GetValueAsString(equipEnum), i);
                return i;
            }
        }
    }
	UE_LOGFMT(LogTemp, Warning,
		"{Inventory}({Sv}): No Equipment Slot Exists for Enum '{SlotEnum}'",
		GetName(), HasAuthority()?"SRV":"CLI", UEnum::GetValueAsString(equipEnum));
    return -1;
}

/** 
 * Returns the truth of whether the requested slot is a real inventory slot or not.
 * Raw - Does not run any internal methods.
 * @param slotNumber An int int of the slot being requested
 * @param IsEquipmentSlot True if checking an equipment slot
 * @return Boolean
 */
bool UInventoryComponent::IsValidSlot(int slotNumber, bool IsEquipmentSlot) const
{
	const bool slotValid = IsEquipmentSlot
		? EquipmentSlots_.IsValidIndex(slotNumber)
		: InventorySlots_.IsValidIndex(slotNumber);
	UE_LOGFMT(LogTemp, Log,
		"{Inventory}({Sv}): Validity Check... '{SlotNumber}' ({SlotType}) - {Validity}",
		GetName(), HasAuthority()?"SRV":"CLI", slotNumber,
		IsEquipmentSlot ? "Equipment Slot" : "Inventory Slot",
		slotValid ? "Valid Slot" : "INVALID SLOT");
	return slotValid;
}

/**
 * (overload) Returns the truth of whether the requested slot is a real equipment slot or not.
 * Not preferred, as the int version of this function is O(1)
 * @param equipSlot The equip slot enum to look for ( O(n) lookup )
 * @return Truth of outcome
 */
bool UInventoryComponent::IsValidEquipmentSlotEnum(EEquipmentSlotType equipSlot) const
{
    for (int i = 0; EquipmentSlots_.Num(); i++)
    {
        if (IsValidSlot(i, true))
        {
            if (EquipmentSlots_[i].EquipType == equipSlot)
            {
            	UE_LOGFMT(LogTemp, Display,
					"{Inventory}({Sv}): Equipment Enum '{SlotEnum}' is a VALID Equipment Slot",
					GetName(), HasAuthority()?"SRV":"CLI", UEnum::GetValueAsString(equipSlot));
	            return true;
            }
        }
    }
	UE_LOGFMT(LogTemp, Display,
		"{Inventory}({Sv}): Equipment Enum '{SlotEnum}' is NOT a valid Equipment Slot",
		GetName(), HasAuthority()?"SRV":"CLI", UEnum::GetValueAsString(equipSlot));
    return false;
}

/**
 * Returns the truth of whether the requested slot is empty.
 * Performs IsValidSlot() internally. No need to call both functions.
 * @param slotNumber The slot number to check.
 * @param isEquipment If true, checks equipment slots. If false, checks inventory slots.
 * @return If true, the slot is vacant.
 */
bool UInventoryComponent::IsSlotEmpty(int slotNumber, bool isEquipment) const
{
    if (!IsValidSlot(slotNumber, isEquipment))
    {
    	UE_LOGFMT(LogTemp, Display,
			"{Inventory}({Sv}): {SlotType} #{SlotNum} is NOT an Empty Slot, or is an INVALID slot!",
			GetName(), HasAuthority()?"SRV":"CLI",
			isEquipment ? "Equipment Slot" : "Inventory Slot", slotNumber);
	    return false;
    }
	const bool isEmpty = GetSlotQuantity(slotNumber, isEquipment) < 1;
	UE_LOGFMT(LogTemp, Display,
		"{Inventory}({Sv}): {SlotType} #{SlotNum} is {EmptyStatus}",
		GetName(), HasAuthority()?"SRV":"CLI",
		isEquipment ? "Equipment Slot" : "Inventory Slot", slotNumber,
		isEmpty ? "EMPTY" : "NOT Empty");
    return isEmpty;
}

/**
 * Returns the truth of whether the requested slot is empty.
 * @param equipSlot The Equip enum for the slot we're checking.
 * @return False if occupied, or invalid. True otherwise.
 */
bool UInventoryComponent::IsSlotEmptyByEnum(EEquipmentSlotType equipSlot) const
{
    const int slotNum = GetSlotNumberFromEquipmentType(equipSlot);
	if (!IsValidSlot(slotNum, true))
	{
		return false;
	}
    return IsSlotEmpty(slotNum, true);
}

/**
 * Gets the name of the item that resides in the requested inventory slot.
 * @param slotNumber an int int representing the slot number requested
 * @param isEquipment True if looking for an equipment slot
 * @return FName the itemName of the item in slot. Returns 'invalidName' if empty or nonexistent.
 */
FName UInventoryComponent::GetNameOfItemInSlot(int slotNumber, bool isEquipment) const
{
	if (!IsSlotEmpty(slotNumber, isEquipment))
	{
		const FStInventorySlot InventorySlot = GetCopyOfSlot(slotNumber, isEquipment);
		UE_LOGFMT(LogTemp, Display,
			"{Inventory}({Sv}): Item in {SlotType} #{SlotNum} is '{ItemName}'",
			GetName(), HasAuthority()?"SRV":"CLI",
			isEquipment ? "Equipment Slot" : "Inventory Slot", InventorySlot.ItemName);
		return InventorySlot.ItemName;
	}
	UE_LOGFMT(LogTemp, Display,
		"{Inventory}({Sv}): Item in {SlotType} #{SlotNum} is '-EMPTY SLOT-'",
		GetName(), HasAuthority()?"SRV":"CLI",
		isEquipment ? "Equipment Slot" : "Inventory Slot");
    return FName();
}

/**
 * Retrieves the quantity of the item in the given slot.
 * @param slotNumber The int int representing the slot number being requested
 * @param isEquipment True if the slot is an equipment slot.
 * @return Int of the slot number. Negative indicates failure.
 */
int UInventoryComponent::GetSlotQuantity(int slotNumber, bool isEquipment) const
{
    if (!IsValidSlot(slotNumber, isEquipment))
    {
	    return -1;
    }
    const int slotQuantity = GetCopyOfSlot(slotNumber,isEquipment).SlotQuantity;
    UE_LOGFMT(LogTemp, Display,
    	"{Inventory}({Sv}): Equipment Slot #{SlotNum} - Quantity = {Amount}",
    	GetName(), HasAuthority()?"SRV":"CLI",
    	isEquipment ? "Equipment Slot" : "Inventory Slot",
    	slotNumber, slotQuantity);
    return slotQuantity;
}

/**
 * @brief Adds the item in the referenced slot to this inventory with the given quantity
 * @param InventorySlot READONLY - The inventory slot containing the reference for the item being copied
 * @param quantity How many to add
 * @param SlotNumber The slot to add to. Negative means stack or fill first slot.
 * @param overflowAdd If true, add to the next slot if this slot fills up
 * @param overflowDrop If true, drop items to ground if slot fills up
 * @param showNotify If true, notify the player a new item was added
 * @return The amount of items added. Negative indicates error.
 */
int UInventoryComponent::AddItemFromExistingSlot(const FStInventorySlot& InventorySlot,
    int quantity, int SlotNumber, bool overflowAdd, bool overflowDrop, bool showNotify)
{
    // If reference slot is empty, do nothing
    if (InventorySlot.IsSlotEmpty())
    {
    	UE_LOGFMT(LogTemp, Display,
			"{Inventory}({Sv}): AddItemFromExistingSlot Failed - Existing Slot is Empty",
			GetName(), HasAuthority()?"SRV":"CLI");
	    return -1;
    }
    
    if (quantity < 1)
    {
	    quantity = 1;
    }

    int RemainingQuantity = quantity;
    const bool IsHuntingForSlot = (SlotNumber < 0);

    // This is a safeguard against an infinite loop
    int LoopIterations = 0;
    do
    {
        LoopIterations += 1;
        
        // If the slot number is invalid, find first stackable or empty slot
        if (SlotNumber < 0)
        {
            
            // Item must be stackable
            if (UItemSystem::GetIsStackable(InventorySlot.ItemName))
            {
                // Find the first slot with the same item & durability
                const TArray<int> MatchingSlots = GetSlotsWithItem(InventorySlot.ItemName);
                for (const int ExistingSlot : MatchingSlots)
                {
                    if (MatchingSlots.IsValidIndex(ExistingSlot))
                    {
                        const int TempSlotNumber        = MatchingSlots[ExistingSlot];
                        const FStInventorySlot& MatchingSlot  = GetSlot(TempSlotNumber);

                        const float MaxDurability = UItemSystem::getDurability(InventorySlot.ItemName);
                        const bool DoesDurabilityMatch = MatchingSlot.SlotDurability == InventorySlot.SlotDurability;
                        const bool DoesNewItemHaveMaxDurability = InventorySlot.SlotDurability >= MaxDurability;

                        if ( DoesDurabilityMatch || DoesNewItemHaveMaxDurability)
                        {
                            const int QuantityInSlot    = GetSlotQuantity(TempSlotNumber);
                            const int MaxStackSize      = UItemSystem::getMaximumStackSize(InventorySlot.ItemName);

                            // The slot has space for this item
                            if (QuantityInSlot < MaxStackSize)
                            {
                                SlotNumber = TempSlotNumber;
                                break; // We found our slot. Break the loop.
                            }
                        }
                        
                    }
                }
            }// is stackable
            
            // If the item wasn't stacked/can't be stacked, find first empty slot
            if ((SlotNumber < 0) && (quantity > 0))
            {
                const int FirstFreeSlot = GetFirstEmptySlotNumber();
                if (IsValidSlot(FirstFreeSlot))
                {
                    SlotNumber = FirstFreeSlot;
                }
            }
        }

        // If the slot is now valid (or was valid to start with), add the item
        if (SlotNumber >= 0)
        {
            const int QuantityInSlot = GetSlotQuantity(SlotNumber);
            const int MaxStackSize = UItemSystem::getMaximumStackSize(InventorySlot.ItemName);
            const int NewQuantity = QuantityInSlot + RemainingQuantity;

            if (QuantityInSlot < MaxStackSize)
            {
                InventorySlots_[SlotNumber].ItemName     = InventorySlot.ItemName;
                InventorySlots_[SlotNumber].SlotQuantity = NewQuantity > MaxStackSize ? MaxStackSize : NewQuantity;

            	// If we are the server, we don't receive the "OnRep" call,
            	//    so the broadcast needs to be called manually.
            	if (GetNetMode() < NM_Client)
	            {
		            OnInventoryUpdated.Broadcast(SlotNumber, false);
	            }

                // The current quantity minus the original quantity is how many items were added
                RemainingQuantity -= (InventorySlots_[SlotNumber].SlotQuantity - QuantityInSlot);
            }
        }
        
        // If the slot is still not valid, the inventory is full
        else
        {
            UE_LOGFMT(LogTemp, Display,
            	"{Inventory}({Sv}): AddItemFromExistingSlot() Failed - Inventory Full",
                GetName(), HasAuthority()?"SRV":"CLI");
            break;
        }
        
    }
    // Keep looping so long as there is:
    while (
        RemainingQuantity > 0       // There is more to add,                            AND
        && overflowAdd              // Add all overflowing items,                       AND
        && IsHuntingForSlot         // No specific spot was specified to begin with,    AND
        && LoopIterations < 1000    // The loop is not hanging
        );

    if (LoopIterations >= 1000)
    {
        UE_LOGFMT(LogTemp, Error,
        	"{Inventory}({Sv}): AddItemFromExistingSlot() - Terminated an Infinite Loop",
            GetName(), HasAuthority()?"SRV":"CLI");
        return RemainingQuantity;
    }

    // If dropping overflow on the ground
    if (overflowDrop && RemainingQuantity > 0)
    {
        const ACharacter* OwnerCharacter = Cast<ACharacter>( GetOwner() );
        const USkeletalMeshComponent* thisMesh = OwnerCharacter->GetMesh();
        if (!IsValid(thisMesh))
        {
        	UE_LOGFMT(LogTemp, Error,
				"{Inventory}({Sv}): AddItemFromExistingSlot() - Invalid Mesh. "
				"Cannot spawn Pick Up actor! {NumStart} Requested. {NumAdded} Added, {NumLeft} Remaining", 
				GetName(), HasAuthority()?"SRV":"CLI", quantity, quantity - RemainingQuantity, RemainingQuantity);
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

        APickupActorBase* pickupItem = GetWorld()->SpawnActorDeferred<APickupActorBase>(
                                                APickupActorBase::StaticClass(), spawnTransform);
        if (IsValid(pickupItem))
        {
            pickupItem->SetupItemFromName(InventorySlot.ItemName, RemainingQuantity);
            pickupItem->FinishSpawning(spawnTransform);
        }
        RemainingQuantity = 0;
    }

    // There are either still items remaining (inventory is full), or
	// the specified slot given was not found or is invalid (doesn't exist)
	UE_LOGFMT(LogTemp, Display,
		"{Inventory}({Sv}): AddItemFromExistingSlot() Finished - {NumStart} Requested."
		"{NumAdded} Added, {NumRemain} Remaining",
		GetName(), HasAuthority()?"SRV":"CLI", quantity, quantity - RemainingQuantity, RemainingQuantity);
    return RemainingQuantity;

}

/**
    * @brief Adds the requested item to this inventory with the given quantity.
    *       Internally calls 'AddItemFromExistingSlot()' by making a pseudo-slot.
    * @param ItemName Name of the item to be added from the data table
    * @param quantity The total quantity to add
    * @param SlotNumber The slot to add to. Negative means stack or fill first slot.
    * @param overflowAdd If true, add to the next slot if this slot fills up
    * @param overflowDrop If true, drop items to ground if slot fills up
    * @param showNotify If true, notify the player a new item was added
    * @return The number of items added. Zero indicates failure.
    */
int UInventoryComponent::AddItemFromDataTable(FName ItemName, int quantity, int SlotNumber, bool overflowAdd,
    bool overflowDrop, bool showNotify)
{
    if (!UItemSystem::getItemNameIsValid(ItemName))
    {
    	UE_LOGFMT(LogTemp, Display,
			"{Inventory}({Sv}): AddItemFromDataTable() - Item '{ItemName}' is not a valid item",
			GetName(), HasAuthority()?"SRV":"CLI", ItemName);
	    return -1;
    }

    const FStItemData ItemData = UItemSystem::getItemDataFromItemName(ItemName);
    
    // Slot number was given, but is an invalid inventory slot
    if (SlotNumber >= 0)
    {
        // Invalid Slot
        if (!IsValidSlot(SlotNumber))
        {
        	UE_LOGFMT(LogTemp, Error,
				"{Inventory}({Sv}): AddItemFromDataTable() Failed - Slot {SlotNum} is not a valid Inventory Slot",
				GetName(), HasAuthority()?"SRV":"CLI", SlotNumber);
	        return -2;
        }

        // Slot is Full
        if (GetSlot(SlotNumber).SlotQuantity >= ItemData.maxStackSize)
        {
        	UE_LOGFMT(LogTemp, Warning,
				"{Inventory}({Sv}): AddItemFromDataTable() Failed - Inventory Full",
				GetName(), HasAuthority()?"SRV":"CLI", ItemName);
	        return -3;
        }
    }
    
    // If there's nothing to add, then do nothing
    if (quantity < 1)
    {
    	UE_LOGFMT(LogTemp, Display,
			"{Inventory}({Sv}): AddItemFromDataTable() Failed - Invalid Quantity ({Quantity}) Specified",
			GetName(), HasAuthority()?"SRV":"CLI", quantity);
	    return -4;
    }

    // Use a temp variable so we're not operating on the quantity parameter directly
    const int RemainingQuantity = quantity;

    // pseudo-slot to simply function operations & avoid repeated code
    const FStInventorySlot NewSlotReference = FStInventorySlot(ItemName, quantity);

    const int ItemsAdded = AddItemFromExistingSlot(
    	NewSlotReference, quantity, SlotNumber, overflowAdd, overflowDrop, showNotify);
	
    if (ItemsAdded >= 0)
    {
    	UE_LOGFMT(LogTemp, Display,
			"{Inventory}({Sv}): AddItemFromDataTable() - Finished. "
			"{NumStart} Requested. {NumAdded} Added, {NumRemain} Remaining",
			GetName(), HasAuthority()?"SRV":"CLI", quantity, ItemsAdded, RemainingQuantity);
    }
	else
	{
		UE_LOGFMT(LogTemp, Display,
			"{Inventory}({Sv}): AddItemFromDataTable() Failed - "
			"Misfire of internal request to AddItemFromExistingSlot(). "
			"{NumStart} Requested. {NumAdded} Added, {NumRemain} Remaining",
			GetName(), HasAuthority()?"SRV":"CLI", quantity, ItemsAdded, RemainingQuantity);
	}
    return ItemsAdded;
}

/** Returns a copy of the slot data for the slot requested.
 * Public accessor version of 'GetSlot', but returns a copy instead of a reference.
 * @param slotNumber An int representing the inventory slot
 * @param IsEquipment True if the slot is an equipment slot
 * @return FStSlotContents of the found slot. Returns default struct if slot is empty or not found.
 */
FStInventorySlot UInventoryComponent::GetCopyOfSlot(int slotNumber, bool IsEquipment) const
{
	if (IsValidSlot(slotNumber, IsEquipment))
	{
		UE_LOGFMT(LogTemp, Display,
			"{Inventory}({Sv}): Copy of {SlotType} #{SlotNumber} Requested",
			GetName(), HasAuthority()?"SRV":"CLI",
			IsEquipment ? "Equipment Slot" : "Inventory Slot", slotNumber);
		return InventorySlots_[slotNumber];
	}
	UE_LOGFMT(LogTemp, Display,
		"{Inventory}({Sv}): Copy of {SlotType} #{SlotNumber} Failed - Invalid Slot",
		GetName(), HasAuthority()?"SRV":"CLI",
		IsEquipment ? "Equipment Slot" : "Inventory Slot", slotNumber);
    return FStInventorySlot();
}

/**
 * Attempts to read the item from the provided slot, and if it is a valid
 * target for equipping, it will don the equipment to the appropriate slot.
 *
 * @param fromInventory The inventory we are equipping the item from.
 * @param fromSlot The slot where the item reference can be found
 * @param eSlot (optional) If not NONE it will equip the item directly to this slot.
 * @return True if the item was added successfully
 */
bool UInventoryComponent::donEquipment(UInventoryComponent* fromInventory, int fromSlot, EEquipmentSlotType eSlot)
{
    if (!IsValid(fromInventory))
    {
	    fromInventory = this;
    }

    // Per UE coding convention, "IsValid" should be used when possible over "nullptr" check.
    const FStInventorySlot InventorySlot = fromInventory->GetSlot(fromSlot);
    if (!UItemSystem::getItemNameIsValid(InventorySlot.ItemName))
    {
    	UE_LOGFMT(LogTemp, Display,
			"{Inventory}({Sv}): DonEquipment Failed - '{ItemName}' is not a valid item",
			GetName(), HasAuthority()?"SRV":"CLI", InventorySlot.ItemName);
	    return false;
    }

    // If the parameter was not a specific equipment slot, find one that works.
    EEquipmentSlotType equipSlot = eSlot;
    if (eSlot == EEquipmentSlotType::NONE)
    {
        // Get all slots this item can be equipped into as an array
        // Attempt to add the item to each slot until it fits or fails
        TArray<EEquipmentSlotType> eligibleSlots =
        	UItemSystem::getItemEquipSlots(InventorySlot.GetItemData());
    	
        for (int i = 0; i < eligibleSlots.Num(); i++)
        {
        	const int EquipSlotNumber = GetSlotNumberFromEquipmentType(eligibleSlots[i]);
            const FStInventorySlot foundSlot =
            		GetSlot(EquipSlotNumber, true);
            		
            // If the found slot is valid and matches, then we equip it.
            if (foundSlot.EquipType != EEquipmentSlotType::NONE)
            {
                equipSlot = eligibleSlots[i];
                break;
            }
        }
    }

    // An equip slot was either provided, or found
    if (equipSlot != EEquipmentSlotType::NONE)
    {
        // If the slot is invalid, or the slot types don't match, fail.
        const int slotNumber = GetSlotNumberFromEquipmentType(eSlot);
        if (!IsValidSlot(slotNumber, true))
        {
        	UE_LOGFMT(LogTemp, Display,
				"{Inventory}({Sv}): DonEquipment Failed - "
				"Equipment Slot #{NumSlot} is not a valid Equipment Slot",
				GetName(), HasAuthority()?"SRV":"CLI", slotNumber);
	        return false;
        }

    	const FStInventorySlot EquipSlotData = GetSlot(slotNumber, true);
        if (EquipSlotData.EquipType != eSlot)
        {
        	UE_LOGFMT(LogTemp, Display,
				"{Inventory}({Sv}): DonEquipment Failed - "
				"Equipment Slot #{NumSlot} is a mismatch (of Type '{SlotType}', Need '{EquipType}')",
				GetName(), HasAuthority()?"SRV":"CLI", slotNumber,
				UEnum::GetValueAsString(EquipSlotData.EquipType),
				UEnum::GetValueAsString(eSlot));
	        return false;
        }

        EquipmentSlots_[slotNumber].ItemName = fromInventory->GetNameOfItemInSlot(fromSlot);
        const int itemsRemoved = fromInventory->RemoveItemFromSlot(fromSlot, INT_MAX, false, false, false, true);
    	EquipmentSlots_[slotNumber].SlotQuantity = itemsRemoved;

    	// If we are the server, we don't receive the "OnRep" call,
    	//    so the broadcast needs to be called manually.
    	if (GetNetMode() < NM_Client)
	    {
		    OnInventoryUpdated.Broadcast(slotNumber, true);
	    }
    	
    	UE_LOGFMT(LogTemp, Display,
			"{Inventory}({Sv}): DonEquipment() - "
			"Successfully Equipped '{ItemName}' into Slot #{SlotNum}",
			GetName(), HasAuthority()?"SRV":"CLI",
			slotNumber, InventorySlot.ItemName,	UEnum::GetValueAsString(EquipSlotData.EquipType),
			UEnum::GetValueAsString(eSlot));
        return true;
    }
    
    return false;
}

/**
* Un-equip a piece of equipment. Provides an argument for destroying the item,
* instead of returning it to the inventory (such as a sword being destroy). If
* the item isn't destroyed, it gets moved to the requested slot.
*
* @param equipSlot The slot to remove the item from. 
* @param slotNumber Which slot to put the doffed equipment into.
* @param destroyResult If true, the item will be destroyed. If false, moved into the inventory.
* @param toInventory (optional) The inventory to doff the equipment into.
* @return True if the operation succeeded with the given parameters
*/
bool UInventoryComponent::doffEquipment(EEquipmentSlotType equipSlot, int slotNumber, UInventoryComponent* toInventory, bool destroyResult)
{
    // If destination inventory not provided, use this inventory
    if (!IsValid(toInventory))
    {
	    toInventory = this;
    }
	
    if (!toInventory->GetIsInventoryReady())
    {
	    UE_LOGFMT(LogTemp, Warning,
			"{Inventory}({Sv}): DoffEquipment() Failed - Inventory Not Ready.",
			GetName(), HasAuthority()?"SRV":"CLI", slotNumber);
    	return false;
    }
	
    // Make sure the destination slot is available
    if (slotNumber >= 0)
    {
        if (!toInventory->IsSlotEmpty(slotNumber))
        {
        	UE_LOGFMT(LogTemp, Display,
				"{Inventory}({Sv}): DoffEquipment() Failed - "
				"Destination Inventory Slot #{InvSlot} is not empty.",
				GetName(), HasAuthority()?"SRV":"CLI", slotNumber);
	        return false;
        }
    }

    // Remove the equipment
	const int fromSlot = GetSlotNumberFromEquipmentType(equipSlot);
	
	const FStInventorySlot FromInventorySlotCopy = GetCopyOfSlot(fromSlot, true);
	
	if ( !IsSlotEmpty(fromSlot, true) )
	{
		FStItemData itemData = FromInventorySlotCopy.GetItemData();
            
		// Always remove the item first, to avoid duplication exploits
		const int itemsRemoved = RemoveItemFromSlot(fromSlot, INT_MAX,
				true, false, false, true);
		
		if (itemsRemoved < 1)
		{
			UE_LOGFMT(LogTemp, Error,
				"{Inventory}({Sv}): DoffEquipment() - "
				"Internal Request to RemoveItemFromSlot() Failed",
				GetName(), HasAuthority()?"SRV":"CLI");
			return false;
		}
		
		const int ItemsAdded = toInventory->AddItemFromExistingSlot(
			FromInventorySlotCopy, itemsRemoved, false, false, false);
		
		if (ItemsAdded < 0)
		{
			UE_LOGFMT(LogTemp, Error,
				"{Inventory}({Sv}): DoffEquipment() - "
				"Internal Request to AddItemFromExistingSlot() Failed",
				GetName(), HasAuthority()?"SRV":"CLI", slotNumber);
			return false;
		}
		
	}
	UE_LOGFMT(LogTemp, Display,
		"{Inventory}({Sv}): DoffEquipment() Successfully doffed "
		"'{ItemName}' into Inventory Slot {SlotNum}",
		GetName(), HasAuthority()?"SRV":"CLI", FromInventorySlotCopy.ItemName, slotNumber);
	return true;
}

/**
* Removes a quantity of the item in the requested inventory slot. Verifications should
* be performed before running this function. This function will remove whatever is in the slot
* with the given parameters. It does not perform name/type verification.
*
* @param slotNumber Which slot we are taking an item away from
* @param quantity How many to remove. Defaults to 1. If amount exceeds quantity, will set removeAll to true.
* @param isEquipment True if the slot is an equipment slot.
* @param showNotify True if the player should get a notification.
* @param dropToGround If the remove is successful, the item will fall to the ground (spawns a pickup item)
* @param removeAll If true, removes all of the item in the slot, making the slot empty.
* @return The number of items actually removed. Negative indicates failure.
*/
int UInventoryComponent::RemoveItemFromSlot(
	int slotNumber, int quantity, bool isEquipment, bool showNotify, bool dropToGround, bool removeAll)
{
    if (quantity < 1)
    {
	    quantity = 1;
    }
	
    if (!GetIsInventoryReady())
    {
	    UE_LOGFMT(LogTemp, Error,
			"{Inventory}({Sv}): RemoveItemFromSlot() Failed - Inventory Not Ready",
			GetName(), HasAuthority()?"SRV":"CLI");
    	return -1;
    }
	
    if (!IsValidSlot(slotNumber, isEquipment))
    {
	    UE_LOGFMT(LogTemp, Error,
			"{Inventory}({Sv}): RemoveItemFromSlot() Failed - "
			"{SlotType} #{SlotNum} is not a valid Slot",
			GetName(), HasAuthority()?"SRV":"CLI",
			isEquipment ? "Equipment Slot" : "Inventory Slot", slotNumber);
    	return -1;
    }
	
	const FStInventorySlot InventorySlot = GetCopyOfSlot(slotNumber, isEquipment);
	if (!InventorySlot.ContainsItem())
	{
		UE_LOGFMT(LogTemp, Error,
			"{Inventory}({Sv}): RemoveItemFromSlot() Failed - "
			"{SlotType} #{SlotNum} is an Empty Slot",
			GetName(), HasAuthority()?"SRV":"CLI",
			isEquipment ? "Equipment Slot" : "Inventory Slot", slotNumber);
		return -1;
	}
	
	const FStItemData ExistingItem = InventorySlot.GetItemData();
	
	// Ensures 'quantity' is not greater than the actually quantity in slot
	const int slotQuantity = GetSlotQuantity(slotNumber, isEquipment);
	int itemsRemoved = quantity > slotQuantity ? slotQuantity : quantity;
	
	// All items are being removed
	if (quantity >= slotQuantity || removeAll)
	{
		itemsRemoved = slotQuantity;
	}
	
	// Only a portion of the items are being removed.
	else
	{
		if (isEquipment)
		{
			EquipmentSlots_[slotNumber].SlotQuantity -= itemsRemoved;
		}
		else
		{
			InventorySlots_[slotNumber].SlotQuantity -= itemsRemoved;
		}
	}
	
	if (showNotify)
	{
		SendNotification(InventorySlot.ItemName, itemsRemoved, false);
	}

	// Reset Empty Slots
	if (GetSlotQuantity(slotNumber) < 1)
	{
		if (isEquipment)
		{
			EquipmentSlots_[slotNumber].EmptyAndResetSlot();
		}
		else
		{
			InventorySlots_[slotNumber].EmptyAndResetSlot();
		}
	}

	OnInventoryUpdated.Broadcast(slotNumber, isEquipment);
	UE_LOGFMT(LogTemp, Log,
		"{Inventory}({Sv}): RemoveItemFromSlot() Finished - "
		"{StartQty} Requested. {NumRemoved} Removed, {NumRemain} Remaining",
		GetName(), HasAuthority()?"SRV":"CLI", quantity, itemsRemoved, quantity - itemsRemoved);
	return itemsRemoved;
}

/**
* Starting from the first slot where the item is found, removes the given quantity of
* the given ItemName until all of the item has been removed or the removal quantity
* has been reached (inclusive).
*
* @param itemName The name of the item we are wanting to remove from the inventory
* @param quantity How many to remove. Defaults to 1. If amount exceeds quantity, will set removeAll to true.
* @param isEquipment True if this is an equipment slot. False by default.
* @param dropToGround If true, spawns a pickup for the item removed at the total quantity removed.
* @param removeAll If true, removes ALL occurrence of the item in the entire inventory.
* @return The total number of items successfully removed. Negative indicates failure.
*/
int UInventoryComponent::RemoveItemByQuantity(
	FName itemName, int quantity, bool isEquipment, bool dropToGround, bool removeAll)
{
    if (quantity < 1)
    {
	    quantity = 1;
    }
	
    if (!GetIsInventoryReady())
    {
    	UE_LOGFMT(LogTemp, Warning,
			"{Inventory}({Sv}): RemoveItemByQuantity() Failed - Inventory Not Ready",
			GetName(), HasAuthority()?"SRV":"CLI");
	    return -1;
    }
	
    if (!UItemSystem::getItemNameIsValid(itemName))
    {
    	UE_LOGFMT(LogTemp, Error,
			"{Inventory}({Sv}): RemoveItemFromSlot() Failed - "
			"'{ItemName}' is not a valid item",
			GetName(), HasAuthority()?"SRV":"CLI", itemName);
	    return -1;
    }

    int qty = quantity; // Track how many we've removed
    const TArray<int> itemSlots = GetSlotsWithItem(itemName);

    //for (int i = 0; i < itemSlots.Num(); i++)
	for (const int iSlotNum : itemSlots)
    {
        // Continues removing items until quantity is met or we run out of slots
        const int numRemoved = RemoveItemFromSlot(iSlotNum, qty,
            isEquipment, true, dropToGround, removeAll);
		
        if (qty -= numRemoved < 1)
        {
	        break;
        }
    }
    
	const int ItemsRemoved = quantity - qty;
	UE_LOGFMT(LogTemp, Log,
		"{Inventory}({Sv}): RemoveItemFromSlot() Finished - "
		"{NumRequest} Requested. {NumRemoved} Removed, {NumRemain} Remaining",
		GetName(), HasAuthority()?"SRV":"CLI", quantity, ItemsRemoved, quantity - ItemsRemoved);
	
	return ItemsRemoved;
}

/**
* Performs a swap where the destination slot is another slot. If the items are the same, it will
* stack items FROM the original spot on top of the items in the TO slot. For example, both slots
* contain carrots. Two different FStItemData, but they are effectively the same item. These will stack instead of swap.
* 
* @param fromInventory A pointer to the UInventoryComponent* the item is being moved FROM. Nullptr uses this inventory.
* @param fromSlotNum The slot number of the FROM inventory
* @param toSlotNum The slot number of this inventory to swap with or stack into.
* @param quantity The quantity to move. Values less tan 1 signify to move the entire slot's stack.
* @param showNotify True if a notification should be shown to both inventory owners.
* @param isEquipmentSlot True if the TO slot is an EQUIPMENT slot. False means INVENTORY slot.
* @param fromEquipmentSlot True if the FROM slot is an EQUIPMENT slot. False means INVENTORY slot.
*/
bool UInventoryComponent::SwapOrStackSlots(UInventoryComponent* fromInventory, int fromSlotNum, int toSlotNum, int quantity, bool showNotify, bool isEquipmentSlot, bool fromEquipmentSlot)
{
    int remainder;
    const bool didSwap = SwapOrStackSlotsWithRemainder(
    	fromInventory, fromSlotNum, toSlotNum, quantity,
    	remainder, showNotify, isEquipmentSlot, fromEquipmentSlot);
	
	UE_LOGFMT(LogTemp, Log,
		"{Inventory}({Sv}): SwapOrStackSlots() {PassOrFail} - "
		"{FromInventory} {FromType} #{FromSlot} -> {ToInventory} {ToType} #{ToSlot}",
		GetName(), HasAuthority()?"SRV":"CLI", didSwap ? "Finished" : "Failed",
		fromInventory->GetName(), fromEquipmentSlot ? "Equipment Slot" : "Inventory Slot", fromSlotNum,
		GetName(), isEquipmentSlot ? "Equipment Slot" : "Inventory Slot", toSlotNum);
	
	return didSwap;
}

/**
* (overload) Performs a swap-or-stack where the destination slot is another slot. If the items are the same, it will
* stack items FROM the original spot on top of the items in the TO slot. For example, both slots
* contain carrots. Two different FStItemData, but they are effectively the same item. These will stack instead of swap.
* 
* @param fromInventory A pointer to the UInventoryComponent* the item is being moved FROM. Nullptr uses this inventory.
* @param fromSlotNum The slot number of the FROM inventory. Fails if invalid slot.
* @param toSlotNum The slot number of this inventory to swap with or stack into. Defaults to -1.
* @param quantity The quantity to move. Values less tan 1 signify to move the entire slot's stack.
* @param remainder The amount remaining after swapping/stacking.
* @param showNotify True if a notification should be shown to both inventory owners.
* @param isEquipmentSlot True if the TO slot is an EQUIPMENT slot. False means INVENTORY slot.
* @param fromEquipmentSlot True if the FROM slot is an EQUIPMENT slot. False means INVENTORY slot.
*/
bool UInventoryComponent::SwapOrStackSlotsWithRemainder(
	UInventoryComponent* fromInventory, int fromSlotNum, int toSlotNum, int quantity,
	int& remainder, bool showNotify, bool isEquipmentSlot, bool fromEquipmentSlot)
{    
    // If the 'fromInventory' is invalid, we're transferring an item within this same inventory;
    if (!IsValid(fromInventory))
    {
	    fromInventory = this;
    }

    if (!fromInventory->IsValidSlot(fromSlotNum, fromEquipmentSlot))
    {
    	UE_LOGFMT(LogTemp, Warning,
			"{Inventory}({Sv}): SwapOrStackWithRemainder() Failed - "
			"{FromInventory} {SlotType} #{SlotNum} is not a Valid Slot",
			GetName(), HasAuthority()?"SRV":"CLI", fromInventory->GetName(),
			fromEquipmentSlot ? "Equipment Slot" : "Inventory Slot", fromSlotNum);
	    return false;
    }
	
    if (!IsValidSlot(toSlotNum))
    {
	    toSlotNum = -1;
    }
	
    if (quantity < 1)
    {
	    quantity = 1;
    }

	// Remember which items were in which slots
	if (toSlotNum < 0)
	{
		toSlotNum = GetFirstEmptySlotNumber();
	}
	const FStInventorySlot ToInventorySlot   = GetSlot(toSlotNum);
    const FStInventorySlot FromInventorySlot = fromInventory->GetSlot(fromSlotNum);
	
    // If the FROM item isn't valid, this is not a valid swap. Otherwise.... wtf are they swapping?
    if (!FromInventorySlot.ContainsItem())
    {
    	UE_LOGFMT(LogTemp, Log,
			"{Inventory}({Sv}): SwapOrStackWithRemainder() Failed - {SlotType} #{SlotNum} is an Empty Slot",
			GetName(), HasAuthority()?"SRV":"CLI", isEquipmentSlot ? "Equipment Slot" : "Inventory Slot", toSlotNum);
        return false;
    }

	{ // Explicit Scope to clear bool
		const bool isOriginTypeCorrect = fromEquipmentSlot
						? FromInventorySlot.EquipType != EEquipmentSlotType::NONE
						: FromInventorySlot.SlotType  != EInventorySlotType::NONE;
    	if (!isOriginTypeCorrect)
    	{
    		UE_LOGFMT(LogTemp, Warning,
				"{Inventory}({Sv}): SwapOrStackWithRemainder() Failed - "
				"Origination {ToSlotType} #{ToSlotNum} (Type {ToType}) failed to validate.",
				GetName(), HasAuthority()?"SRV":"CLI", isEquipmentSlot ? "Equipment Slot" : "Inventory Slot",
				toSlotNum, isEquipmentSlot ? UEnum::GetValueAsString(FromInventorySlot.EquipType) : UEnum::GetValueAsString(ToInventorySlot.SlotType));
    		return false;
    	}
	}
	
	{
		const bool isDestinationValid = isEquipmentSlot
						? ToInventorySlot.EquipType != EEquipmentSlotType::NONE
						: ToInventorySlot.SlotType  != EInventorySlotType::NONE;
	
    	if (!isDestinationValid)
    	{
    		UE_LOGFMT(LogTemp, Warning,
				"{Inventory}({Sv}): SwapOrStackWithRemainder() Failed - "
				"Destination {ToSlotType} #{ToSlotNum} (Type {ToType}) failed to validate.",
				GetName(), HasAuthority()?"SRV":"CLI", isEquipmentSlot ? "Equipment Slot" : "Inventory Slot",
				toSlotNum, isEquipmentSlot ? UEnum::GetValueAsString(FromInventorySlot.EquipType) : UEnum::GetValueAsString(ToInventorySlot.SlotType));
    		return false;
    	}
	}
	
	const bool isSameItem = UInventorySystem::IsSameItem(
		fromInventory->GetSlot(fromSlotNum),
		GetSlot(toSlotNum));
	
	// Stop if the destination slot isn't the same item & can't stack
    if (ToInventorySlot.ContainsItem())
    {
    	// If they are the same item, this is the easiest case to handle
    	if (isSameItem)
    	{
    		if (fromInventory->DecreaseSlotQuantity(fromSlotNum, quantity, showNotify))
    		{ 
    			const int itemsAdded = IncreaseSlotQuantity(toSlotNum, quantity, showNotify);
    			remainder = (quantity - itemsAdded);
    			return true;
    		}
    		UE_LOGFMT(LogTemp, Error,
				"{Inventory}({Sv}): SwapOrStackWithRemainder() - "
				"Internal Request to DecreaseQuantityInSlot Failed",
				GetName(), HasAuthority()?"SRV":"CLI",
				ToInventorySlot.ItemName, FromInventorySlot.ItemName);
    		return false;
    	}
    	
    	UE_LOGFMT(LogTemp, Error,
			"{Inventory}({Sv}): SwapOrStackWithRemainder() Failed - "
			"To-Item ({ToItem}) != ({FromItem}), or items have different durability.",
			GetName(), HasAuthority()?"SRV":"CLI",
			ToInventorySlot.ItemName, FromInventorySlot.ItemName);
    	return false;
	}

    // If equipment, the slots must be eligible.
    // i.e. You can't swap a sword and a carrot in/out of the Primary Weapon slot
    if (fromEquipmentSlot || isEquipmentSlot)
    {
    	const TArray<EEquipmentSlotType> FromEquipmentSlotTypes =
    		UItemSystem::getItemEquipSlots( FromInventorySlot.GetItemData() );
    	
    	const TArray<EEquipmentSlotType> ToEquipmentSlotTypes =
			UItemSystem::getItemEquipSlots( ToInventorySlot.GetItemData() );
    	
    	// Terminate if the equipment going OUT doesn't fit in the DESTINATION slot
    	if (fromEquipmentSlot && !FromEquipmentSlotTypes.Contains( FromInventorySlot.EquipType ))
    	{
    		UE_LOGFMT(LogTemp, Warning,
				"{Inventory}({Sv}): SwapOrStackWithRemainder() Failed - "
				"Equipment Going ({ToType}) does not match From Slot Type ({FromType})",
				GetName(), HasAuthority()?"SRV":"CLI",
				UEnum::GetValueAsString(ToInventorySlot.EquipType),
				UEnum::GetValueAsString(FromInventorySlot.EquipType));
    		return false;
    	}

    	// Terminate if the equipment coming IN doesn't fit in the ORIGIN slot
    	if (isEquipmentSlot && !ToEquipmentSlotTypes.Contains( FromInventorySlot.EquipType ))
    	{
    		UE_LOGFMT(LogTemp, Warning,
				"{Inventory}({Sv}): SwapOrStackWithRemainder() Failed - "
				"Equipment Going ({ToType}) does not match From Slot Type ({FromType})",
				GetName(), HasAuthority()?"SRV":"CLI",
				UEnum::GetValueAsString(ToInventorySlot.EquipType),
				UEnum::GetValueAsString(FromInventorySlot.EquipType));
    		return false;
    	}
    }

    const int moveQuantity =  quantity <= FromInventorySlot.SlotQuantity
							? quantity :  FromInventorySlot.SlotQuantity;

	// If the destination is an equipment slot, we're donning new equipment
    if (isEquipmentSlot)
    {
        const EEquipmentSlotType eSlot = GetSlotTypeEquipment(toSlotNum);
    	const bool wasDonned = donEquipment(this, fromSlotNum, eSlot);
    	UE_LOGFMT(LogTemp, Warning,
			"{Inventory}({Sv}): SwapOrStackWithRemainder(): Internal Request to donEquipment() {PassOrFail} ",
			GetName(), HasAuthority()?"SRV":"CLI", wasDonned ? "was Successful" : "FAILED");
    	return wasDonned;
    }

	// Otherwise, remove the item from the origination slot, and add it to the new slot
	const int ItemsRemoved = fromInventory->DecreaseSlotQuantity(
			fromSlotNum, FromInventorySlot.SlotQuantity, fromEquipmentSlot, showNotify);
	
    if (ItemsRemoved > 0)
    {
        const int ItemsAdded = AddItemFromExistingSlot(
        	FromInventorySlot, ItemsRemoved, toSlotNum, false, false, showNotify);
    	
        if (ItemsAdded <= 0)
        {
        	UE_LOGFMT(LogTemp, Warning,
				"{Inventory}({Sv}): SwapOrStackWithRemainder(): "
				"Internal Request to AddItemFromExistingSlot() FAILED",
				GetName(), HasAuthority()?"SRV":"CLI");
            return false;
        }
    	
    	remainder = moveQuantity - GetSlotQuantity(toSlotNum, isEquipmentSlot);
    	UE_LOGFMT(LogTemp, Log,
			"{Inventory}({Sv}): SwapOrStackWithRemainder() Ran Successfully. "
			"{NumRequest} Requested. {NumMoved} Moved, {NumRemain} Remaining",
			GetName(), HasAuthority()?"SRV":"CLI", quantity,
			moveQuantity - ItemsAdded, ItemsRemoved - ItemsAdded);
        return true;
    }
    UE_LOGFMT(LogTemp, Warning,
		"{Inventory}({Sv}): SwapOrStackWithRemainder():  "
		"External Request to FromInventory->DecreaseQuantityInSlot() FAILED",
		GetName(), HasAuthority()?"SRV":"CLI");
	return false;
}

/**
 * @brief Finds which slots have changed, and sends an update event
 */
void UInventoryComponent::OnRep_InventorySlotUpdated_Implementation(const TArray<FStInventorySlot>& OldSlot)
{
	bool wasUpdated = false;
	for (int i = 0; i < GetNumberOfSlots(); i++)
	{
		if (OldSlot.IsValidIndex(i))
		{
			// If it's the exact same item, nothing was changed.
			if (!UInventorySystem::IsExactSameItem(GetSlot(i), OldSlot[i]))
			{
				UE_LOGFMT(LogTemp, Log,
					"{Inventory}({Sv}) REPNOTIFY: Updated Inventory Slot {SlotNum}",
					GetName(), HasAuthority()?"SRV":"CLI", i);
				OnInventoryUpdated.Broadcast(i, false);
				wasUpdated = true;
			}
		}
		// If the old slot didn't exist, then broadcast the addition of this new slot
		else
		{
			UE_LOGFMT(LogTemp, Log,
				"{Inventory}({Sv}) REPNOTIFY: Added Inventory Slot {SlotNum}",
				GetName(), HasAuthority()?"SRV":"CLI", i);
			OnInventoryUpdated.Broadcast(i, false);
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

/**
 * @brief Finds which slots have changed, and sends an update event
 */
void UInventoryComponent::OnRep_EquipmentSlotUpdated_Implementation(const TArray<FStInventorySlot>& OldSlot)
{
	bool wasUpdated = false;
	for (int i = 0; i < GetNumberOfSlots(); i++)
	{
		FStInventorySlot thisSlot = GetCopyOfSlot(i, true);
		if (OldSlot.IsValidIndex(i))
		{
			// If it's the exact same item, nothing was changed.
			if (!UInventorySystem::IsExactSameItem(thisSlot, OldSlot[i]))
			{
				UE_LOGFMT(LogTemp, Log,
					"{Inventory}({Sv}) REPNOTIFY: Updated Equipment Slot {SlotNum} (Type '{EnumType}')",
					GetName(), HasAuthority()?"SRV":"CLI", i, UEnum::GetValueAsString(thisSlot.EquipType));
				OnInventoryUpdated.Broadcast(i, true);
				wasUpdated = true;
			}
		}
		// If the old slot didn't exist, then broadcast the addition of this new slot
		else
		{
			UE_LOGFMT(LogTemp, Log,
				"{Inventory}({Sv}) REPNOTIFY: Added Equipment Slot {SlotNum} (Type '{EnumType}')",
				GetName(), HasAuthority()?"SRV":"CLI", i, UEnum::GetValueAsString(thisSlot.EquipType));
			OnInventoryUpdated.Broadcast(i, true);
			wasUpdated = true;
		}
	}
	
	if (!wasUpdated)
	{
		UE_LOGFMT(LogTemp, Warning,
			"{Inventory}({Sv}) REPNOTIFY: Unable to ascertain which Equipment Slot was updated.",
			GetName(), HasAuthority()?"SRV":"CLI");
	}
	
}

void UInventoryComponent::OnRep_InUseByActors_Implementation(const TArray<AActor*>& PreviousUseActors)
{
	TArray<AActor*> InUseActors = {};
	const bool InventoryInUse = GetIsInventoryInUse(InUseActors);
	
	// Broadcast no-longer-in-use by actors that are no longer using the inventory
	for (const AActor* OldUseActor : PreviousUseActors)
	{
		if (!InUseActors.Contains(OldUseActor))
		{
			UE_LOGFMT(LogTemp, Warning,
				"{Inventory}({Sv}) REPNOTIFY: Inventory is no longer being used by {CharacterName}",
				GetName(), HasAuthority()?"SRV":"CLI", OldUseActor->GetName());
			OnInventoryInUse.Broadcast(OldUseActor, false);
		}
	}

	// Broadcast now-in-use by actors that are now using the inventory
	if (InventoryInUse)
	{
		for (const AActor* InUseActor : InUseActors)
		{
			if (!PreviousUseActors.Contains(InUseActor))
			{
				OnInventoryInUse.Broadcast(InUseActor, true);
			}
		}
	}
}


void UInventoryComponent::Server_RestoreSavedInventory_Implementation(
	const TArray<FStInventorySlot>& RestoredInventory,
	const TArray<FStInventorySlot>& RestoredEquipment)
{
	if (HasAuthority())
	{
		RestoreInventory(RestoredInventory, RestoredEquipment);
	}
	else
	{
		UE_LOGFMT(LogTemp, Log,
			"{Inventory}({Sv}): Request to restore inventory must run on the authority.",
			GetName(), HasAuthority()?"SRV":"CLI");
	}
}

/**
* Starting from the first slot where the item is found, removes the given quantity of
* the given FStItemData until all of the item has been removed or the removal quantity
* has been reached (inclusive).
*
* @param fromInventory The inventory of the stack being split.
* @param fromSlotNum The slot number of where the split is originating FROM
* @param splitQuantity The amount of split off from the original stack.
* @param toSlotNum The slot number of the destination slot. Negative means use first available slot found.
* @param showNotify If true, shows a notification. False by default.
* @return True if the split was successful. Returns false on failure
						or if firstEmptySlot is true with no slots open
*/
bool UInventoryComponent::SplitStack(
    UInventoryComponent* fromInventory, int fromSlotNum, int splitQuantity, int toSlotNum, bool showNotify)
{
    if (splitQuantity < 1)
    {
	    splitQuantity = 1;
    }
	
    if (!IsValid(fromInventory))
    {
	    fromInventory = this;
    }

    // Always show notification if the inventory isn't the same (item gained/lost)
	showNotify = (fromInventory == this) ? showNotify : true;
	
    const FStInventorySlot	FromSlotCopy = fromInventory->GetCopyOfSlot(fromSlotNum);
    const FStItemData		ExistingItem = FromSlotCopy.GetItemData();
    
    if (FromSlotCopy.ContainsItem())
    {
        const int existingQty = FromSlotCopy.SlotQuantity;
        const int qty = (splitQuantity >= existingQty) ? existingQty : splitQuantity;

        if (qty < 1)
        {
        	UE_LOGFMT(LogTemp, Log,
				"{Inventory}({Sv}) Failed - The validated move quantity was an invalid quantity.",
				GetName(), HasAuthority()?"SRV":"CLI");
            return false;
        }

    	const FStInventorySlot ToSlotCopy = GetCopyOfSlot(toSlotNum);
    	if (ToSlotCopy.ContainsItem())
    	{
    		if (!UInventorySystem::IsExactSameItem(
				fromInventory->GetSlot(fromSlotNum),
				GetSlot(toSlotNum)))
    		{
    			UE_LOGFMT(LogTemp, Warning,
					"{Inventory}({Sv}): SplitStack() Failed - Slots are occupied by different items",
					GetName(), HasAuthority()?"SRV":"CLI");
    			return false;
    		}
    	}
        
        // Always take the item first, to avoid duplication exploits.
    	const bool isEquipmentSlot = FromSlotCopy.EquipType != EEquipmentSlotType::NONE;
    	const int ItemsRemoved = fromInventory->DecreaseSlotQuantity(
    		fromSlotNum, qty, isEquipmentSlot);

		if (ItemsRemoved > 0)
        {
            const int ItemsAdded = AddItemFromExistingSlot(FromSlotCopy, qty,
				toSlotNum, false, false, showNotify);
            
            if (ItemsAdded > 0)
            {
                // If items added were less than we intended, we weren't able to add all of them.
                if (ItemsAdded < qty)
                {
                    // Re-add the ones that weren't able to stack
                    fromInventory->IncreaseSlotQuantity(fromSlotNum, qty - ItemsAdded, showNotify);
                }
            	UE_LOGFMT(LogTemp, Log,
					"{Inventory}({Sv}): Successfully Split a Stack. "
					"{NumRequest} Requested. {NumMoved} Moved, {NumRemain} Remaining",
					GetName(), HasAuthority()?"SRV":"CLI",
					splitQuantity, ItemsAdded, qty - ItemsAdded);
                return true;
            }
            // We failed to add the items. Refund the original item.
            fromInventory->AddItemFromExistingSlot(FromSlotCopy, qty, fromSlotNum, false, false);
        }
    	UE_LOGFMT(LogTemp, Error,
			"{Inventory}({Sv}): External Request to DecreaseQuantityInSlot() failed",
			GetName(), HasAuthority()?"SRV":"CLI");
    }
	UE_LOGFMT(LogTemp, Warning,
		"{Inventory}({Sv}) Failed - The origination slot did not contain any items.",
		GetName(), HasAuthority()?"SRV":"CLI");
    return false;
}

/**
* Increases the amount of an existing item by the quantity given.
* If the given slot is empty, it will do nothing and fail.
*
* @param slotNumber The slot number to be increased
* @param quantity The quantity to increase the slot by, up to the maximum quantity
* @param showNotify Show a notification when quantity increases. False by default.
* @return The number of items actually added. Negative indicates failure.
*/
int UInventoryComponent::IncreaseSlotQuantity(int slotNumber, int quantity, bool showNotify)
{
    int newQty;
	const int ItemsAdded = IncreaseSlotQuantity(slotNumber, quantity, newQty, showNotify);
	UE_LOGFMT(LogTemp, Log,
		"{Inventory}({Sv}): IncreaseQuantityInSlot() Override added {n} items in Inventory Slot #{SlotNum}",
		GetName(), HasAuthority()?"SRV":"CLI", quantity, ItemsAdded, slotNumber);
	return ItemsAdded;
}

/**
* Increases the amount of an existing item by the quantity given.
* If the given slot is empty, it will do nothing and fail.
*
* @param slotNumber The slot number to be increased
* @param quantity The quantity to increase the slot by, up to the maximum quantity
* @param newQuantity The new amount after increase (override)
* @param showNotify Show a notification when quantity increases. False by default.
* @return The number of items actually added. Negative indicates failure.
*/
int UInventoryComponent::IncreaseSlotQuantity(int slotNumber, int quantity, int& newQuantity, bool showNotify)
{
    if (quantity < 1)
    {
	    quantity = 1;
    }
    const FStInventorySlot CopyOfSlot = GetCopyOfSlot(slotNumber);
	
    if (!CopyOfSlot.ContainsItem())
    {
	    UE_LOGFMT(LogTemp, Warning,
			"{Inventory}({Sv}): IncreaseQuantityInSlot Failed - Inventory Slot #{SlotNum} is Empty",
			GetName(), HasAuthority()?"SRV":"CLI", slotNumber);
    	return -1;
    }
	
	const FStItemData ItemInSlot = CopyOfSlot.GetItemData();
    const int maximumQuantity  	 = CopyOfSlot.GetMaxStackAllowance();
    const int startingQuantity 	 = CopyOfSlot.SlotQuantity;
    const int newTotal		   	 = startingQuantity + abs(quantity);
	
    newQuantity = (newTotal >= maximumQuantity) ? maximumQuantity : newTotal;
	
    const int ItemsAdded = (newQuantity - startingQuantity);
    InventorySlots_[slotNumber].SlotQuantity = newQuantity;

    // If we are the server, we don't receive the "OnRep" call,
    //    so the broadcast needs to be called manually.
    if (HasAuthority())
    {
	    OnInventoryUpdated.Broadcast(slotNumber, false);
    }

    if (showNotify)
    {
        SendNotification(GetNameOfItemInSlot(slotNumber), ItemsAdded);
    }
	
	UE_LOGFMT(LogTemp, Log,
		"{Inventory}({Sv}): IncreaseQuantityInSlot - "
		"{NumRequest} Requested. {NumMoved} Added, {NumRemain} Ignored. New Quantity: {NumNow}",
		GetName(), HasAuthority()?"SRV":"CLI", quantity, ItemsAdded, quantity - ItemsAdded, newQuantity);
    return ItemsAdded;
}

/**
* Increases the amount of an existing item by the quantity given.
* If the given slot is empty, it will do nothing and fail.
*
* @param slotNumber The slot number to be decreased
* @param quantity The quantity to decrease the slot by
* @param remainder The remaining amount after decrease (override)
* @param isEquipment True if the slot is an equipment slot
* @param showNotify Show a notification when quantity increases. False by default.
* @return The number of items actually removed. Negative indicates failure.
*/
int UInventoryComponent::DecreaseSlotQuantity(int slotNumber, int quantity, int& remainder, bool isEquipment, bool showNotify)
{
    if (quantity < 1)
    {
	    quantity = 1;
    }
	
    const FStInventorySlot CopyOfSlot = GetCopyOfSlot(slotNumber);
	
    if (CopyOfSlot.ContainsItem())
    {
		const FStItemData slotItem = CopyOfSlot.GetItemData();
        const int startingQuantity = CopyOfSlot.SlotQuantity;
        const int tempQuantity     = startingQuantity - abs(quantity);
        const int newQuantity	   = (tempQuantity >= 0) ? tempQuantity : 0;
        const int ItemsRemoved     = startingQuantity - newQuantity;
    	remainder = quantity - ItemsRemoved;

        FStInventorySlot* ReferenceSlot = isEquipment ? &EquipmentSlots_[slotNumber] : &InventorySlots_[slotNumber];
    	if (newQuantity > 0)
    	{
    		ReferenceSlot->SlotQuantity = newQuantity;
    	}
    	else
    	{
    		ReferenceSlot->EmptyAndResetSlot();
    	}

    	OnInventoryUpdated.Broadcast(slotNumber, isEquipment);
    	if (showNotify)
    	{
    		SendNotification(CopyOfSlot.ItemName, ItemsRemoved, false);
    	}
    	UE_LOGFMT(LogTemp, Log,
			"{Inventory}({Sv}): DecreaseQuantityInSlot() was Successful"
			"{NumRequest} Requested. {NumRemoved} Removed, {NumRemain} Remaining. New Quantity = {NewNum}",
			GetName(), HasAuthority()?"SRV":"CLI", quantity, ItemsRemoved, remainder, newQuantity);
        return ItemsRemoved;
    }
	UE_LOGFMT(LogTemp, Log,
		"{Inventory}({Sv}): DecreaseQuantityInSlot() Failed - {SlotType} #{SlotNum} is Empty",
		GetName(), HasAuthority()?"SRV":"CLI",
		isEquipment ? "Equipment Slot" : "Inventory Slot", slotNumber);
    return -1;
}

/**
* Increases the amount of an existing item by the quantity given.
* If the given slot is empty, it will do nothing and fail.
*
* @param slotNumber The slot number to be decreased
* @param quantity The quantity to decrease the slot by
* @param isEquipment True if the slot is an equipment slot
* @param showNotify Show a notification when quantity increases. False by default.
* @return The number of items actually removed. Negative indicates failure.
*/
int UInventoryComponent::DecreaseSlotQuantity(int slotNumber, int quantity, bool isEquipment, bool showNotify)
{
    int remainder;
	const int ItemsRemoved = DecreaseSlotQuantity(slotNumber, quantity, remainder, isEquipment, showNotify);
	UE_LOGFMT(LogTemp, Log,
		"{Inventory}({Sv}): DecreaseQuantityInSlot() Override removed {n} items in {SlotType} #{SlotNum}",
		GetName(), HasAuthority()?"SRV":"CLI", ItemsRemoved, 
		isEquipment ? "Equipment Slot" : "Inventory Slot", slotNumber);
	return ItemsRemoved;
}

/**
* CLIENT event that retrieves and subsequently removes all inventory notifications.
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
* at the time the notification is created, we just use a simple FName and the class defaults.
* @param itemName The FName of the affected item
* @param quantity The quantity affected
* @param wasAdded True if the item was added, False if the item was removed
*/
void UInventoryComponent::SendNotification(FName itemName, int quantity, bool wasAdded)
{
    if (quantity == 0)
    {
    	UE_LOGFMT(LogTemp, Warning,
			"{Inventory}({Sv}): SendNotification() received an invalid quantity.",
			GetName(), HasAuthority()?"SRV":"CLI");
	    return;
    }
    FStInventoryNotify InventoryNotification(itemName, abs(quantity));
	InventoryNotification.wasAdded = quantity > 0;
    Notifications_.Add(InventoryNotification);
	UE_LOGFMT(LogTemp, Log,
		"{Inventory}({Sv}): New Notification - There are now {n} notifications pending",
		GetName(), HasAuthority()?"SRV":"CLI", Notifications_.Num());
    OnNotificationAvailable.Broadcast();
}

/**
* Wipes the inventory slot, making it empty and completely destroying anything previously in the slot.
* Mostly used for cases where we know we want the slot to absolutely be empty in all cases.
* @param slotNumber The slot number to reset
* @param isEquipment True if the slot is an equipment slot
*/
void UInventoryComponent::ResetSlot(int slotNumber, bool isEquipment)
{
    if (IsValidSlot(slotNumber, isEquipment))
    {
    	GetSlot(slotNumber, isEquipment).EmptyAndResetSlot();
    	UE_LOGFMT(LogTemp, Log,
			"{Inventory}({Sv}): {SlotType} #{SlotNum} has been Reset",
			GetName(), HasAuthority()?"SRV":"CLI",
			isEquipment ? "Equipment Slot" : "Inventory Slot", slotNumber);
    }
}

/**
* Called when the server is reporting that a PLAYER wants to transfer items between
* two inventories. This is NOT a delegate/event. It should be invoked by the delegate
* that intercepted the net event request. This function transfers two items between inventories.
* Validation is required if you want to keep game integrity.
*
* @param fromInventory The inventory losing the item
* @param toInventory The inventory gaining the item. Can be the same inventory.
* @param fromSlotNum The slot of origination. Required to be zero or greater. Must be a real item.
* @param toSlotNum The slot of the destination. Negative indicates to use the first available slot.
* @param moveQuantity The amount to be moved. Will default to 1.
* @param overflowDrop If true, anything that overflows will spawn a pickup actor.
* @param isFromEquipSlot If true, signifies the FROM slot is an equipment slot.
* @param isToEquipSlot If true, signifies the TO slot is an equipment slot.
* @return True if at least 1 of the item was successfully moved. False if the transfer failed entirely.
*/
bool UInventoryComponent::TransferItemBetweenSlots(
    UInventoryComponent* fromInventory, UInventoryComponent* toInventory,
    int fromSlotNum, int toSlotNum, int moveQuantity, bool overflowDrop, bool isFromEquipSlot, bool isToEquipSlot)
{
    if (!IsValid(fromInventory))
    {
	    fromInventory = this;
    }
    if (!IsValid(toInventory))
    {
	    toInventory = this;
    }

	// Always notify if the inventory isn't the same inventory
    const bool showNotify = fromInventory != toInventory;

    // The from item MUST be valid since the authoritative inventory is the one GAINING the item.
    const FStInventorySlot FromInventorySlot = fromInventory->GetCopyOfSlot(fromSlotNum, isFromEquipSlot);
	const FStInventorySlot ToInventorySlot   = toInventory->GetCopyOfSlot(toSlotNum, isToEquipSlot);
	
	moveQuantity = FromInventorySlot.SlotQuantity < moveQuantity ? FromInventorySlot.SlotQuantity : moveQuantity;
	if (moveQuantity < 1)
	{
		moveQuantity = 1;
	}
	
    if (FromInventorySlot.ContainsItem())
    {
        // Does an item exist in the destination slot?
        if (ToInventorySlot.ContainsItem())
        {
            // If to slot is valid, we're swapping (or stacking) the two slots.
            if (toSlotNum >= 0)
            {
                if (toInventory->SwapOrStackSlots(fromInventory, fromSlotNum, toSlotNum, moveQuantity, showNotify))
                {
                	UE_LOGFMT(LogTemp, Log,
						"{Inventory}({Sv}): Successful Transfer of Item (Swapped)",
						GetName(), HasAuthority()?"SRV":"CLI");
                    return true;
                }
            }
        	
            // If to slot is NOT valid, move item to first available slot.
            else
            {
                const int itemsRemoved = fromInventory->RemoveItemFromSlot(fromSlotNum, moveQuantity, showNotify, overflowDrop);
                if (itemsRemoved > 0)
                {
                    const int itemsAdded = toInventory->AddItemFromExistingSlot(FromInventorySlot, fromSlotNum, moveQuantity, true, overflowDrop, showNotify);
                    if (itemsAdded >= 0)
                    {
                    	UE_LOGFMT(LogTemp, Log,
							"{Inventory}({Sv}): Successful Transfer of Item (First Empty Slot)",
							GetName(), HasAuthority()?"SRV":"CLI");
                        return true;
                    }
                }
            }
        }
    	
        // If destination slot is empty we can shortcut the logic
        //	by just using SplitStack, regardless of the quantity.
        else
        {
        	if (toInventory->SplitStack(fromInventory, fromSlotNum, moveQuantity, toSlotNum, showNotify))
        	{
        		UE_LOGFMT(LogTemp, Log,
					"{Inventory}({Sv}): Successfully Received x{Quantity} {ItemName} from '{FromInv}'",
					GetName(), HasAuthority()?"SRV":"CLI", moveQuantity, FromInventorySlot.ItemName,
					fromInventory->GetName());
        		return true;
        	}
        }
    }
	
	UE_LOGFMT(LogTemp, Log,
		"{Inventory}({Sv}): Transfer Failed - Origination Slot Empty ({SlotType} #{Slot})",
		GetName(), HasAuthority()?"SRV":"CLI",
		isFromEquipSlot ? "Equipment Slot" : "Inventory Slot", fromSlotNum);
    return false;
}

/**
* Called when an item is activated from the inventory. This function is
* executed from the inventory of the player who activated the item.
*
* @param SlotOfActivation The number of the inventory slot containing the item to be activated
* @param isEquippedActivation True if item was activated from an equipment slot
* @param forceConsume	True consumes the item, whether it's consumable or not. False by default.
*						If false, item will still be consumed if it is flagged to be consumed.
* @return True if the item was successfully activated
*/
bool UInventoryComponent::ActivateSlot(int SlotOfActivation, bool isEquippedActivation, bool forceConsume)
{
    if (IsValidSlot(SlotOfActivation))
    {
		const FStInventorySlot ActivatedSlot = GetCopyOfSlot(SlotOfActivation, isEquippedActivation);
    	const FStItemData ItemInSlot		 = ActivatedSlot.GetItemData();
    	const EItemActivation ActivationType = ItemInSlot.itemActivation;

    	if (ActivationType == EItemActivation::EQUIP)
    	{
    		// Equipment activated FROM an equipment slot (activates it's ability effects)
    		if (isEquippedActivation)
    		{
    			// TODO - For a later time
    		}
    	
    		// Otherwise, it should be equipped when activated
    		else
    		{
    			if (ActivatedSlot.ContainsItem())
    			{
    				// Find the first eligible equipment slot
    				int destinationSlot = -1;
    				for ( const EEquipmentSlotType slotType : ItemInSlot.equipSlots )
    				{
    					const int eSlotNum = GetSlotNumberFromEquipmentType(slotType);
    					if (IsValidSlot(eSlotNum))
    					{
    						FStInventorySlot ToSlot = GetSlot(eSlotNum, true);
    						// If the slot contains the same item and it is stackable
    						const bool isSameItem =
								UInventorySystem::IsExactSameItem(ToSlot,
									GetSlot(SlotOfActivation));

    						const bool isStackable = (isSameItem && ToSlot.GetMaxStackAllowance() > 1);
    						const bool isEmptySlot = ToSlot.IsSlotEmpty();
    						if (isEmptySlot || isStackable)
    						{
    							destinationSlot = eSlotNum;
    							break;
    						}
    					}
    				}
    				
    				if (destinationSlot < 0)
    				{
    					UE_LOGFMT(LogTemp, Warning,
							"{Inventory}({Sv}): Activate Item - Failed to Activate (No Eligible Slot Found)",
							GetName(), HasAuthority()?"SRV":"CLI");
    					return false;
    				}
    				
    			}//Activation Slot is not empty
    			
    			UE_LOGFMT(LogTemp, Warning,
					"{Inventory}({Sv}): Activate Item - {SlotType} #{SlotNum} was Empty",
					GetName(), HasAuthority()?"SRV":"CLI",
					isEquippedActivation ? "Equipment Slot" : "Inventory Slot", SlotOfActivation);
    			return false;
    			
    		}//Activation Type Check: Equipment
    	}
    	
    	UE_LOGFMT(LogTemp, Log,
			"{Inventory}({Sv}): Activate Item - Successful Activation of type '{ActivateType}' on item '{ItemName}'",
			GetName(), HasAuthority()?"SRV":"CLI", UEnum::GetValueAsString(ActivationType), ActivatedSlot.ItemName);
    	
    	if (forceConsume || ItemInSlot.consumeOnUse)
    	{
    		DecreaseSlotQuantity(SlotOfActivation,1,isEquippedActivation);
    		UE_LOGFMT(LogTemp, Log,
				"{Inventory}({Sv}): Activate Item - The activation of item '{ItemName}' caused one to be consumed.",
				GetName(), HasAuthority()?"SRV":"CLI", UEnum::GetValueAsString(ActivationType), ActivatedSlot.ItemName);
    	}
    	
    	OnItemActivated.Broadcast(ActivatedSlot.ItemName, isEquippedActivation);
    	return true;
    	
    }
    return false;
}

void UInventoryComponent::Server_RequestItemActivation_Implementation(
    UInventoryComponent* inventoryUsed, int slotNumber, bool isEquipment)
{
    if (HasAuthority())
    {
        GetSlot(slotNumber, isEquipment).Activate();
    }
	else
	{
    	UE_LOGFMT(LogTemp, Log,
			"{Inventory}({Sv}): Server_RequestItemActivation Authority Violation",
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
	if (!IsValid(SaveData))
	{
		if (UGameplayStatics::CreateSaveGameObject( UInventorySave::StaticClass() ))
		{
			SaveSlotName_ = SaveSlotName;
			SaveUserIndex_ = UserIndex;
			OnInventoryRestored.Broadcast(true);
			UE_LOGFMT(LogTemp, Log,
				"{Inventory}({Sv}): (Async Response) Created new Inventory Save with name '{SaveName}' @ index '{Index}'",
				GetName(), HasAuthority()?"SRV":"CLI", SaveSlotName, UserIndex);
			return;
		}
		OnInventoryRestored.Broadcast(false);
		UE_LOGFMT(LogTemp, Error,
			"{Inventory}({Sv}): (Async Response) Failed to create new inventory save object '{SaveName}' @ index '{Index}'",
			GetName(), HasAuthority()?"SRV":"CLI", SaveSlotName, UserIndex);
		return;
	}
	
	const UInventorySave* InventorySave = Cast<UInventorySave>( SaveData );
	if (!IsValid(InventorySave))
	{
		UE_LOGFMT(LogTemp, Log,
			"{Inventory}({Sv}): (Async Response) Save '{SaveName}' @ index '{Index}'"
			"exists, but it is not an inventory save object",
			GetName(), HasAuthority()?"SRV":"CLI", SaveSlotName, UserIndex);
		return;
	}

	UE_LOGFMT(LogTemp, Log,
		"{Inventory}({Sv}): (Async Response) Restoring Inventory Save '{SaveName}' (Index {Index})'",
		GetName(), HasAuthority()?"SRV":"CLI", SaveSlotName, UserIndex);
	RestoreInventory(InventorySave->InventorySlots_, InventorySave->EquipmentSlots_);
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
 * @param fromInventory The inventory of the FROM item (the 'origination')
 * @param toInventory The inventory of the TO item (the 'receiving')
 * @param fromSlot The slot number of the origination inventory being affected
 * @param toSlot The slot number of the receiving inventory (-1 means first available slot)
 * @param moveQty The amount to move. Negative means move everything.
 * @param isFromEquipSlot If TRUE, the origination slot will be treated as an equipment slot. False means inventory.
 * @param isToEquipSlot If TRUE, the receiving slot will be treated as an equipment slot. If false, inventory.
 */
void UInventoryComponent::Server_TransferItems_Implementation(UInventoryComponent* fromInventory,
    UInventoryComponent* toInventory, int fromSlot, int toSlot, int moveQty, bool isFromEquipSlot, bool isToEquipSlot)
{
    UE_LOGFMT(LogTemp, Display, "InventoryComponent: Server_TransferItems_Implementation");
	
    // Validation
    if (!IsValid(fromInventory))
    {
	    fromInventory = this;
    }
    if (!IsValid(toInventory))
    {
	    toInventory = this;
    }

    // Move is from-and-to the exact same slot.. Do nothing.
	const FStInventorySlot FromSlotCopy = fromInventory->GetCopyOfSlot(fromSlot, isFromEquipSlot);
	const FStInventorySlot ToSlotCopy   = GetCopyOfSlot(toSlot, isToEquipSlot);
    if (fromInventory == toInventory && fromSlot == toSlot && isFromEquipSlot == isToEquipSlot)
    {
    	UE_LOGFMT(LogTemp, Log,
			"{Inventory}({Sv}): Server_TransferItems() Canceled - Transfer is the exact same slot",
			GetName(), HasAuthority()?"SRV":"CLI");
	    return;
    }

    const ACharacter* fromPlayer = Cast<ACharacter>( fromInventory->GetOwner() );
    const ACharacter* toPlayer	 = Cast<ACharacter>( toInventory->GetOwner() );

    // Make sure destination inventory is not withdraw only. Ignore this if it's the player's own inventory.
    if (toInventory != this && toInventory->WithdrawOnly)
    {
    	UE_LOGFMT(LogTemp, Warning,
			"{Inventory}({Sv}): Destination Inventory ({DestInv}) is Withdraw-Only.",
			GetName(), HasAuthority()?"SRV":"CLI", toInventory->GetName());
    	return;
    }
    
    // This player is trying to modify a player-owned inventory.
    if (IsValid(fromPlayer))
    {
        // The *from* inventory is not this player. The transfer is invalid.
        if (fromPlayer != GetOwner())
        {
        	UE_LOGFMT(LogTemp, Warning,
				"{Inventory}({Sv}): An attempt to take item from another player's inventory was prevented ({FromInv} -> {ToInv})",
				GetName(), HasAuthority()?"SRV":"CLI", fromInventory->GetName(), toInventory->GetName());
            return;
        }
    }
    // This player is trying to modify a player-owned inventory.
    if (IsValid(toPlayer))
    {
        // The *TO* inventory is not this player. The transfer is invalid.
        if (toPlayer != GetOwner())
        {
        	UE_LOGFMT(LogTemp, Warning,
				"{Inventory}({Sv}): An attempt to put items into another player's inventory was prevented ({FromInv} -> {ToInv})",
				GetName(), HasAuthority()?"SRV":"CLI", fromInventory->GetName(), toInventory->GetName());
            return;
        }
    }

    const FStInventorySlot FromInventorySlot = fromInventory->GetCopyOfSlot(fromSlot, isFromEquipSlot);
    
    // If it's the same inventory
    if (fromInventory == toInventory)
    {
        // The operation is NOT taking place within the client's own inventory.
        if (fromInventory != this)
        {
            // Validate distance
            if (fromPlayer->GetDistanceTo(toPlayer) > 1024.0f)
            {
            	UE_LOGFMT(LogTemp, Warning,
					"{Inventory}({Sv}): Transfer Rejected - Too Far Away ({Distance})",
					GetName(), HasAuthority()?"SRV":"CLI", fromInventory->GetName(),
					fromPlayer->GetDistanceTo(toPlayer));
                return;
            }
        }
        
        // If origin is inventory and destination is equipment, perform a DonEquipment
        if (!isFromEquipSlot && isToEquipSlot)
        {
            if (!fromInventory->IsValidSlot(fromSlot, false))
            {
            	UE_LOGFMT(LogTemp, Log,
					"{Inventory}({Sv}): Transfer from {FromInv} Inventory Slot #{SlotNum} failed. Invalid Slot.",
					GetName(), HasAuthority()?"SRV":"CLI", fromInventory->GetName(), fromSlot);
                return;
            }
            if (!toInventory->IsValidSlot(toSlot, true))
            {
            	UE_LOGFMT(LogTemp, Log,
					"{Inventory}({Sv}): Transfer to {ToInv} Equipment Slot #{SlotNum} failed. Invalid Equipment Slot.",
					GetName(), HasAuthority()?"SRV":"CLI", toInventory->GetName(), toSlot);
                return;
            }
        	
        	const EEquipmentSlotType equipSlotType = fromInventory->GetSlotTypeEquipment(toSlot);
            if (!toInventory->donEquipment(fromInventory, fromSlot, equipSlotType))
            {
            	UE_LOGFMT(LogTemp, Error,
					"{Inventory}({Sv}): Internal Request to DonEquipment() Failed. Transfer Canceled.",
					GetName(), HasAuthority()?"SRV":"CLI", fromInventory->GetName(), fromSlot);
            }
        }
        
        // If origin is equipment and destination is inventory, perform a DoffEquipment
        else if (isFromEquipSlot && !isToEquipSlot)
        {
        	if (!fromInventory->IsValidSlot(fromSlot, true))
        	{
        		UE_LOGFMT(LogTemp, Log,
					"{Inventory}({Sv}): Transfer from {FromInv} Equipment Slot #{SlotNum} failed. Invalid Equipment Slot.",
					GetName(), HasAuthority()?"SRV":"CLI", fromInventory->GetName(), fromSlot);
        		return;
        	}
        	if (!toInventory->IsValidSlot(toSlot, false))
        	{
        		UE_LOGFMT(LogTemp, Log,
					"{Inventory}({Sv}): Transfer to {ToInv} Inventory Slot #{SlotNum} failed. Invalid Slot.",
					GetName(), HasAuthority()?"SRV":"CLI", toInventory->GetName(), toSlot);
        		return;
        	}
            
        	const EEquipmentSlotType equipSlotType = fromInventory->GetSlotTypeEquipment(fromSlot);
            if (!fromInventory->doffEquipment(equipSlotType, toSlot, toInventory, false))
            {
            	UE_LOGFMT(LogTemp, Error,
					"{Inventory}({Sv}): Internal Request to DonEquipment() Failed. Transfer Canceled.",
					GetName(), HasAuthority()?"SRV":"CLI", fromInventory->GetName(), fromSlot);
            }
        }
        
        // Both items are equipment, or both items are non-equipment. Perform swap-or-stack.
        else
        {
            if (!toInventory->SwapOrStackSlots(fromInventory,fromSlot,toSlot,moveQty,false,isToEquipSlot,isFromEquipSlot))
            {
            	UE_LOGFMT(LogTemp, Error,
					"{Inventory}({Sv}): External Request to SwapOrStackSlots() Failed. Transfer Canceled.",
					GetName(), HasAuthority()?"SRV":"CLI", toInventory->GetName(), toSlot);
            }
        }
    }
    
    // If they are different inventories, first verify that the player is allowed to do it.
    else
    {
        if (fromPlayer->GetDistanceTo(toPlayer) > 1024.0f)
        {
        	UE_LOGFMT(LogTemp, Error,
				"{Inventory}({Sv}): Transfer Rejected. Too Far Away ({Distance})",
				GetName(), HasAuthority()?"SRV":"CLI", toInventory->GetName(),
				fromPlayer->GetDistanceTo(toPlayer));
        	return;
        }
        
        // Make a copy of the origin item, and decrease it from the origin inventory.
        const FStItemData originItem = FromInventorySlot.GetItemData();
        if (FromInventorySlot.ContainsItem())
        {
            // Remove the items from the origin inventory
            moveQty = fromInventory->DecreaseSlotQuantity(fromSlot,moveQty,isFromEquipSlot);
            if (moveQty < 1)
            {
            	UE_LOGFMT(LogTemp, Error,
					"{Inventory}({Sv}): Transfer Failed. "
					"No items were able to be removed from {FromInv} {SlotType} #{SlotNum}",
					GetName(), HasAuthority()?"SRV":"CLI", fromInventory->GetName(),
					isFromEquipSlot ? "Equipment Slot" : "Inventory Slot", fromSlot);
                return;
            }
        }
        
        // If decrease was successful, add it to the destination inventory.
        const int itemsAdded = toInventory->AddItemFromDataTable(FromInventorySlot.ItemName, moveQty, toSlot,
                    false,false,true);
        if (itemsAdded > 0)
        {
        	UE_LOGFMT(LogTemp, Log,
				"{Inventory}({Sv}): Successfully Transferred x{Quantity} of {ItemName} "
				"From '{FromInv} {SlotType} #{SlotNum}' to '{ToInv} {ToSlotType} #{ToSlotNum}'",
				GetName(), HasAuthority()?"SRV":"CLI",
				fromInventory->GetName(), isFromEquipSlot ? "Equipment Slot" : "Inventory Slot", fromSlot,
				toInventory->GetName(),   isToEquipSlot   ? "Equipment Slot" : "Inventory Slot", toSlot    );
        }
        
        // If the add failed, reinstate the item to the origin inventory
        else
        {
        	UE_LOGFMT(LogTemp, Error,
				"{Inventory}({Sv}): Transfer Failed. "
				"No items were able to be added to '{ToInv} {SlotType} #{SlotNum}'. Will attempt to reimburse loss.",
				GetName(), HasAuthority()?"SRV":"CLI", toInventory->GetName(),
				isToEquipSlot ? "Equipment Slot" : "Inventory Slot", toSlot);
            fromInventory->AddItemFromExistingSlot(
            	FromInventorySlot, fromSlot,moveQty,true,false,false);
        }
    }
}


/**
 * Sends a request to the server to take the item from the FROM inventory, and throw it on the ground as a pickup.
 * @param fromInventory The inventory of the FROM item (the 'origination')
 * @param fromSlot The slot number of the origination inventory being affected
 * @param quantity The amount to toss out. Negative means everything.
 * @param isFromEquipSlot If TRUE, the origination slot will be treated as an equipment slot. False means inventory.
 */
void UInventoryComponent::Server_DropItemOnGround_Implementation(UInventoryComponent* fromInventory, int fromSlot,
    int quantity, bool isFromEquipSlot)
{
    if (!IsValid(fromInventory))
    {
	    fromInventory = this;
    }

	// We know the owners are valid, because the component de-registers itself if they aren't
    AActor* FromActor = fromInventory->GetOwner();
    AActor* ToActor = Cast<AActor>(GetOwner());

	FStInventorySlot FromInventorySlot = fromInventory->GetSlot(fromSlot,isFromEquipSlot);
	if (FromInventorySlot.IsSlotEmpty())
	{
		UE_LOGFMT(LogTemp, Warning,
			"{Inventory}({Sv}): Attempted to drop {SlotType} #{SlotNum}, but the slot is Empty (or Invalid)",
			GetName(), HasAuthority()?"SRV":"CLI", isFromEquipSlot ? "Equipment Slot" : "Inventory Slot", fromSlot);
		return;
	}

    // If the inventories belong to different actors, make sure it's not player v. player
    if (ToActor != FromActor)
    {
        AController* thisController = FromActor->GetInstigatorController();
        AController* thatController = FromActor->GetInstigatorController();

        // If both actors are controlled, we may have a conflict
        if (IsValid(thisController) && IsValid(thatController))
        {
            // Player trying to access another player's inventory. Deny.
            if (thisController->IsPlayerController() && thatController->IsPlayerController())
            {
            	UE_LOGFMT(LogTemp, Warning,
					"{Inventory}({Sv}): An attempt to drop item out of another player's inventory was prevented.",
					GetName(), HasAuthority()?"SRV":"CLI");
                return;
            }
        }
    }

    ACharacter* thisCharacter = Cast<ACharacter>(ToActor);
    if (IsValid(thisCharacter))
    {
        USkeletalMeshComponent* thisMesh = thisCharacter->GetMesh();
        if (!IsValid(thisMesh))
        {
        	UE_LOGFMT(LogTemp, Error,
				"{Inventory}({Sv}): Attempt to Drop Failed. Dropping player has no GetMesh().",
				GetName(), HasAuthority()?"SRV":"CLI");
	        return;
        }

        FTransform spawnTransform(
            FRotator(FMath::RandRange(0.f,359.f),FMath::RandRange(0.f,359.f),FMath::RandRange(0.f,359.f)),
            thisMesh->GetBoneLocation("Root"));
        FActorSpawnParameters spawnParams;
        spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        FStInventorySlot InventorySlot = fromInventory->GetCopyOfSlot(fromSlot, isFromEquipSlot);
        FStItemData itemCopy    = InventorySlot.GetItemData();
        int itemQuantity        = InventorySlot.SlotQuantity;
        int tossQuantity        = (quantity > itemQuantity) ? itemQuantity : abs(quantity);

        APickupActorBase* pickupItem = GetWorld()->SpawnActorDeferred<APickupActorBase>(
                                                APickupActorBase::StaticClass(), spawnTransform);
        if (IsValid(pickupItem))
        {
            if (fromInventory->DecreaseSlotQuantity(fromSlot, tossQuantity) > 0)
            {
                pickupItem->SetupItemFromName(InventorySlot.ItemName, tossQuantity);
                pickupItem->FinishSpawning(spawnTransform);
            }
        }
    	else
    	{
    		UE_LOGFMT(LogTemp, Error,
				"{Inventory}({Sv}): Item '{ItemName} (x{Quantity}) was dropped "
				"successfully, but the pickup actor failed to spawn.",
				GetName(), HasAuthority()?"SRV":"CLI", InventorySlot.ItemName);
    	}
        
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
    targetInventory->SetInventoryInUse(OwnerCharacter, true);
    
}

/**
 * Called when the player has opened a new inventory (such as loot or
 * a treasure chest) and needs the information for the first time.
 */
void UInventoryComponent::Server_StopUseOtherInventory_Implementation(UInventoryComponent* otherInventory)
{
    if (IsValid(otherInventory))
    {
    	otherInventory->SetInventoryInUse(GetOwner(), false);
    }
}


/****************************************
 * REPLICATION
***************************************/

void UInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> & OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(UInventoryComponent, InventorySlots_);
    DOREPLIFETIME(UInventoryComponent, EquipmentSlots_);
    DOREPLIFETIME(UInventoryComponent, InUseByActors_);
	
    DOREPLIFETIME_CONDITION(UInventoryComponent, Notifications_, COND_OwnerOnly);
}