
// ReSharper disable CppUE4CodingStandardNamingViolationWarning
#include "InventoryComponent.h"

#include "InventorySystemGlobals.h"
#include "PickupActorBase.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "lib/InventorySave.h"
#include "Logging/StructuredLog.h"
#include "Net/UnrealNetwork.h" // Used for replication
#include "TalesDungeoneer/Characters/CharacterBase.h"

void UInventoryComponent::Helper_SaveInventory(USaveGame*& SaveData) const
{
	UInventorySave* InventorySave = Cast<UInventorySave>( SaveData );
	if (IsValid(SaveData))
	{
		InventorySave->SetSaveName(InventorySaveSlotName_);
		InventorySave->InventorySlots_ = m_inventorySlots;
		InventorySave->EquipmentSlots_ = m_equipmentSlots;
	}
}

void UInventoryComponent::BeginPlay()
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): BeginPlay()"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    Super::BeginPlay();
    
    const AActor* ownerActor = GetOwner();
    if (!IsValid(ownerActor))
    {
        UE_LOG(LogTemp, Error, TEXT("Inventory has no owner. Removed."));
        this->DestroyComponent(); // Kills itself, it failed validation
    }
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): Play Started. %d Slots and %d Equipment Slots"),
        GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), getNumInventorySlots(), getNumEquipmentSlots());
    }

    if (!IsValid(UItemSystem::getItemDataTable()))
    {
        UE_LOG(LogEngine, Error, TEXT("%s(%s): Data Table Not Found"),
            *GetName(), GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    else
    {
        UE_LOG(LogEngine, Display, TEXT("%s(%s): Data Table Was Found"),
            *GetName(), GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
}

UInventoryComponent::UInventoryComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
	if (bVerboseOutput)
		bShowDebug = true;
}

/*
 * Runs the initialization after UE has created the UObject system.
 * Sets up the stuff for the inventory that can't be done before the game is running.
 */
void UInventoryComponent::ReinitializeInventory()
{
	if (!bIsInventoryReady)
	{
		ACharacterBase* OwnerCharacter = Cast<ACharacterBase>(GetOwner());
		if (!IsValid(OwnerCharacter))
			return;
	
		// Only needs to be initialized once
		if (isInventoryReady())
			return;
		
		// Ensure the number of inventory slots is valid. If we added an inventory system,
		// it should at least have *one* slot at minimum. Otherwise, wtf is the point?
		if (NumberOfInvSlots < 1)
			NumberOfInvSlots = 6;
    	
		m_inventorySlots.Empty();
		m_equipmentSlots.Empty();
		m_notifications.Empty();
    	
		// Setup Inventory Slots
		for (int i = 0; i < NumberOfInvSlots; i++)
		{
			FStInventorySlot newSlot;
			newSlot.SlotType = EInventorySlotType::GENERAL;
			m_inventorySlots.Add(newSlot);
			// Server doesn't receive "OnRep", fire manually
			OnInventoryUpdated.Broadcast(i);
		}
	
		// Setup Equipment Slots
		for (int i = 0; i < EligibleEquipmentSlots.Num(); i++)
		{
			if (EligibleEquipmentSlots.IsValidIndex(i))
			{
				if (EligibleEquipmentSlots[i] != EEquipmentSlotType::NONE)
				{
					// Ignore slots already added - There can be only one..
					if (getEquipmentSlotNumber(EligibleEquipmentSlots[i]) < 0)
					{
						FStInventorySlot newSlot;
						newSlot.SlotType  = EInventorySlotType::EQUIP;
						newSlot.EquipType = EligibleEquipmentSlots[i];
						m_equipmentSlots.Add(newSlot);
						// Server doesn't receive "OnRep", fire manually
						OnEquipmentUpdated.Broadcast(EligibleEquipmentSlots[i]);
					}
				}
			}
		}
		
		// If the inventory component has a proper actor-owner, proceed
		if (IsValid(GetOwner()))
		{
    	
			UE_LOG(LogTemp, Display, TEXT("%s(%s): Found %d Starting Items"),
				*OwnerCharacter->GetName(),
				OwnerCharacter->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"),
				StartingItems.Num());
    	
			for (int i = 0; i < StartingItems.Num(); i++)
			{
				if (StartingItems.IsValidIndex(i))
				{
					// Determine the starting item by item name given
					const FName itemName = StartingItems[i].startingItem;
					const FStItemData ItemData = UItemSystem::getItemDataFromItemName(itemName);
					int slotNum = -1;

					if (StartingItems[i].bStartEquipped)
					{
						// Check each eligible equip slot for an open slot
						for (const EEquipmentSlotType EquipSlot : ItemData.equipSlots)
						{
							if (isEquipmentSlotEmpty(EquipSlot))
							{
								slotNum = getEquipmentSlotNumber(EquipSlot);
							}
						}

						// Equip item into the first eligible slot
						if (slotNum >= 0)
						{
							// TODO - A check should be added here to ensure the item
							//        is being added to an appropriate slot.
							m_equipmentSlots[slotNum].ItemName = itemName;
							m_equipmentSlots[slotNum].SlotQuantity = StartingItems[i].quantity;

							// If we are the server, we don't receive the "OnRep" call,
							//    so the broadcast needs to be called manually.
							if (GetNetMode() < NM_Client)
								OnEquipmentUpdated.Broadcast(m_equipmentSlots[slotNum].EquipType);

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
						UE_LOG(LogTemp, Warning, TEXT("%s(%s): Item(s) failed to add (StartingItem = %s)"),
							*GetName(), GetOwner()->HasAuthority()?TEXT("SRV"):TEXT("CLI"), *itemName.ToString());
					}
					else
					{
						UE_LOG(LogTemp, Display, TEXT("%s(%s): Added %d of starting item '%s'"),
							*OwnerCharacter->GetName(),
							OwnerCharacter->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"),
							itemsAdded, *itemName.ToString());
					}
				}
			}
		}//is owner valid (starting items)
		bIsInventoryReady = true;
	}
}

void UInventoryComponent::RestoreInventory(
	const TArray<FStInventorySlot>& RestoredInventory,
	const TArray<FStInventorySlot>& RestoredEquipment)
{
	if (GetNetMode() == NM_Client)
	{
		// If the server handles the save, then any info from the client is invalid
		if (bSavesOnServerOnly)
		{
			return;
		}
	}
	
	bInventoryRestored = true;
	bIsInventoryReady = false;
	
	m_inventorySlots.Empty();
	m_equipmentSlots.Empty();

	// If we iterate the arrays, OnRep will trigger.
	// Not the most optimized way, but it avoids any ugly RPC work
	for (int i = 0; i < RestoredInventory.Num(); i++)
	{
		FStInventorySlot NewSlot = RestoredInventory[i];
		m_inventorySlots.Add(NewSlot);
	}
	for (int i = 0; i < RestoredEquipment.Num(); i++)
	{
		FStInventorySlot NewSlot = RestoredEquipment[i];
		m_equipmentSlots.Add(NewSlot);
	}
	
	bInventoryRestored	= true;
	bIsInventoryReady	= true;

	// Update the server, if we are the client
	if (GetNetMode() == NM_Client)
		Server_RestoreSavedInventory(RestoredInventory, RestoredEquipment);
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
	if (bSavesOnServerOnly)
	{
		if (GetNetMode() == NM_Client)
		{
			responseStr = "Saving Only Allowed on Authority";
			return FString();
		}
	}
	
	if (bIsInventoryReady)
	{
		// The inventory save file does not exist, if the string is empty.
		// If so, the script will try to generate one.
		if (InventorySaveSlotName_.IsEmpty())
		{
			FString TempSaveName;
			do
			{
				for (int i = 0; i < 16; i++)
				{
					TArray<int> RandValues = {
						FMath::RandRange(48,57), // Numbers 0-9
						FMath::RandRange(65,90), // Lowercase a-z
						FMath::RandRange(97,122) // Uppercase A-Z
					};
					const char RandChar = static_cast<char>(RandValues[FMath::RandRange(0,2)]);
					TempSaveName.AppendChar(RandChar);
				}
			}
			// Loop until a unique save string has been created
			while (UGameplayStatics::DoesSaveGameExist(TempSaveName, 0));
			InventorySaveSlotName_ = TempSaveName;
		}

		// If still empty, there was a problem with the save slot name
		if (InventorySaveSlotName_.IsEmpty())
		{
			responseStr = "Failed to Generate Unique SaveSlotName";
			return FString();
		}

		if (isAsync)
		{
			FAsyncLoadGameFromSlotDelegate SaveDelegate;
			SaveDelegate.BindUObject(this, &UInventoryComponent::SaveInventoryDelegate);
			UGameplayStatics::AsyncLoadGameFromSlot(InventorySaveSlotName_, 0, SaveDelegate);
			responseStr = "Sent Request for Async Save";
			return InventorySaveSlotName_;
		}

		USaveGame* SaveData;
		if (UGameplayStatics::DoesSaveGameExist(InventorySaveSlotName_, 0))
		{
			SaveData = UGameplayStatics::LoadGameFromSlot(InventorySaveSlotName_, 0);
		}
		else
		{
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
			if (UGameplayStatics::SaveGameToSlot(InventorySave, InventorySaveSlotName_, 0))
			{
				InventorySave->SetSaveName(InventorySaveSlotName_);
				Helper_SaveInventory(SaveData);
				responseStr = "Successful Synchronous Save";
				return InventorySaveSlotName_;
			}
		}
		responseStr = "SaveNameSlot was VALID but UInventorySave was NULL";
	}
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
	if (bSavesOnServerOnly)
	{
		if (GetNetMode() == NM_Client)
		{
			responseStr = "Saving Only Allowed on Authority";
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
		InventorySaveSlotName_ = SaveSlotName;
		
		if (isAsync)
		{
			FAsyncLoadGameFromSlotDelegate LoadDelegate;
			LoadDelegate.BindUObject(this, &UInventoryComponent::LoadDataDelegate);
			UGameplayStatics::AsyncLoadGameFromSlot(InventorySaveSlotName_, 0, LoadDelegate);
			responseStr = "Sent Async Save Request";
			return true;
		}

		const USaveGame* SaveData = UGameplayStatics::LoadGameFromSlot(
											InventorySaveSlotName_, 0);
		
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

/**
 * Returns the item found in the given slot.
 * @param slotNumber An int representing the inventory slot requested
 * @param isEquipment True if this should check for an EQUIPMENT slot. False by default.
 * @return Returns a copy of the found FStItemData or an invalid item
 */
FStItemData UInventoryComponent::getItemInSlot(int slotNumber, bool isEquipment)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): getItemInSlot(%d, %s)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), slotNumber, isEquipment?TEXT("TRUE"):TEXT("FALSE"));
    }
    //UE_LOG(LogTemp, Display, TEXT("InventoryComponent: getItemInSlot"));
    if (!isEmptySlot(slotNumber, isEquipment))
    {
        const FStInventorySlot& ReferenceSlot = isEquipment ? m_equipmentSlots[slotNumber] : m_inventorySlots[slotNumber];
        return ReferenceSlot.GetItemData();
    }
    return FStItemData();
}

/**
 * Returns the inventory enum slot type of the given slot number. Returns NONE
 * if the slot is invalid or the slot is not an inventory slot.
 * @param slotNumber The slot number for the inventory slot inquiry
 * @return The inventory slot type enum, where NONE indicates failure.
 */
EInventorySlotType UInventoryComponent::getInventorySlotType(int slotNumber)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): getInventorySlotType(%d)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), slotNumber);
    }
    //UE_LOG(LogTemp, Display, TEXT("InventoryComponent: getInventorySlotType"));
    if (IsValidSlot(slotNumber))
        return m_inventorySlots[slotNumber].SlotType;
    return EInventorySlotType::NONE;
}

/**
 * Returns the equipment enum slot type of the given slot number. Returns NONE
 * if the slot is invalid or the slot is not an equipment slot.
 * @param slotNumber The slot number for the equipment slot inquiry
 * @return The equip slot type enum, where NONE indicates failure.
 */
EEquipmentSlotType UInventoryComponent::getEquipmentSlotType(int slotNumber)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): getEquipmentSlotType(%d)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), slotNumber);
    }
    if (isValidEquipmentSlot(slotNumber))
        return m_equipmentSlots[slotNumber].EquipType;
    return EEquipmentSlotType::NONE;
}

/**
 * Returns the total quantity of an item in the entire inventory, NOT including equipment slots.
 * @param itemName The itemName of the item we're searching for.
 * @return The total quantity of items found. Negative value indicates failure.
 */
int UInventoryComponent::getTotalQuantityInAllSlots(FName itemName)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): getTotalQuantityInAllSlots(%s)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), *itemName.ToString());
    }
    //UE_LOG(LogTemp, Display, TEXT("InventoryComponent: GetTotalQuantityInAllSlots"));
    int total = 0;
    for (int i = 0; i < getNumInventorySlots(); i++)
    {
        if (IsValidSlot(i))
        {
            if (getItemNameInSlot(i) == itemName)
                total += m_inventorySlots[i].SlotQuantity;
        }
    }
    return total;
}

/**
 * Returns the slotNumber of the first empty slot found in the inventory.
 * @return The slotNumber of the first empty slot. Negative indicates full inventory.
 */
int UInventoryComponent::GetFirstEmptySlot()
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): GetFirstEmptySlot()"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    //UE_LOG(LogTemp, Display, TEXT("InventoryComponent: GetFirstEmptySlot"));
    for (int i = 0; i < getNumInventorySlots(); i++)
    {
        if (isEmptySlot(i))
            return i;
    }
    return -1;
}

/**
 * Returns the total weight of all items in the inventory.
 * @param incEquip If true, the total weight will also include all equipment slots. False by default.
 * @return Float containing the total weight of the inventory. Unitless.
 */
float UInventoryComponent::getTotalWeight(bool incEquip)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): getTotalWeight()"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    float totalWeight = 0.0f;
    for (int i = 0; i < getNumInventorySlots(); i++)
    {
        if (!isEmptySlot(i))
        {
            totalWeight += getWeightInSlot(i);
        }
    }
    return totalWeight;
}

/**
 * Gets the total weight of all items in the given slot.
 * @param slotNumber The int representing the inventory slot to check.
 * @return Float with the total weight of the given slot. Negative indicates failure. Zero indicates empty slot.
 */
float UInventoryComponent::getWeightInSlot(int slotNumber)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): getWeightInSlot(%d)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), slotNumber);
    }
    const FStInventorySlot InventorySlot = GetInventorySlot(slotNumber);
    if (UItemSystem::getItemNameIsValid(InventorySlot.ItemName))
    {
        const float weight = UItemSystem::getItemWeight(InventorySlot.GetItemData());
        return (weight * getQuantityInSlot(slotNumber));
    }
    return 0.0f;
}

/**
 * Returns a TArray of int ints where the item was found
 * @param itemName The item we're looking for
 * @return TArray of ints. Empty Array indicates no items found.
 */
TArray<int> UInventoryComponent::getSlotsContainingItem(FName itemName)
{   
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): getSlotsContainingItem(%s)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), *itemName.ToString());
    }
    TArray<int> foundSlots = TArray<int>();
    for (int i = 0; i < getNumInventorySlots(); i++)
    {
        const FStInventorySlot InventorySlot = GetInventorySlot(i);
        if (UItemSystem::getItemNameIsValid(InventorySlot.ItemName))
        {
            if (InventorySlot.ItemName == itemName)
            {
                foundSlots.Add(i);
            }
        }
    }
    return foundSlots;
}

/**
 * Returns a reference to the slot data for the slot requested.
 * @param slotNumber An int representing the inventory slot
 * @param IsEquipment True if the slot is an equipment slot
 * @return FStSlotContents of the found slot. Returns default struct if slot is empty or not found.
 */
FStInventorySlot& UInventoryComponent::GetInventorySlot(int slotNumber, bool IsEquipmentSlot)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): GetInventorySlot(%d)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), slotNumber);
    }
    if (!IsValidSlot(slotNumber, IsEquipmentSlot))
        return m_EmptySlot;
    return IsEquipmentSlot ? m_equipmentSlots[slotNumber] : m_inventorySlots[slotNumber];
}

/**
 * Gets the index (slot) of the equipment slot where equipment type was found.
 * @param equipEnum Enum of the slotType to find.
 * @return Int representing the slot number found. Negative indicates failure.
 */
int UInventoryComponent::getEquipmentSlotNumber(EEquipmentSlotType equipEnum)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): getEquipmentSlotNumber(%s)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), *UEnum::GetValueAsString(equipEnum));
    }
    // Makes sure incoming param is a valid equip slot
    if (equipEnum != EEquipmentSlotType::NONE)
    {
        // Loop through until we find the equip slot since we only have 1 of each
    	const int numEquipSlots = getNumEquipmentSlots();
        for (int i = 0; i < numEquipSlots; i++)
        {
            const FStInventorySlot equipSlot = getEquipmentSlotByNumber(i);
            if (equipSlot.EquipType == equipEnum)
            {
                return i;
            }
        }
    }
    return -1;
}

/**
 * DEPRECATED - Use GetInventorySlot
 * @deprecated Use GetInventorySlot(SlotNumber, true)
 * @param equipEnum Enum of the slotType to find.
 * @return A copy of an FStEquipmentSlot struct that was found.
 */
FStInventorySlot UInventoryComponent::getEquipmentSlot(EEquipmentSlotType equipEnum)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): getEquipmentSlot(%s)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), *UEnum::GetValueAsString(equipEnum));
    }
    //UE_LOG(LogTemp, Display, TEXT("InventoryComponent: getEquipmentSlot(enum)"));
    if (equipEnum != EEquipmentSlotType::NONE)
    {
        for (int i = 0; i < m_equipmentSlots.Num(); i++)
        {
            const FStInventorySlot equipSlot = getEquipmentSlotByNumber(i);
            if (equipSlot.EquipType == equipEnum)
            {
                return equipSlot; // Returns a copy of the equipment slot
            }
        }
    }
    return FStInventorySlot();
}

/**
 * Finds the given equipment slot by specific slot number
 * @param slotNumber Represents the slot of 'm_equipmentSlots' to be examined.
 * @return FStEquipmentSlot representing the slot found. Check 'FStEquipmentSlot.slotType = NONE' for failure
 */
FStInventorySlot UInventoryComponent::getEquipmentSlotByNumber(int slotNumber)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): getEquipmentSlot(%d)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), slotNumber);
    }
    // If the slot is valid, returns a copy of the requested slot
    if (isValidEquipmentSlot(slotNumber))
        return m_equipmentSlots[slotNumber];
    return FStInventorySlot();
}

/**
 * Returns the truth of whether the requested slot is a real inventory slot or not.
 * @param slotNumber An int int of the slot being requested
 * @poaram IsEquipmentSlot True if checking an equipment slot
 * @return Boolean
 */
bool UInventoryComponent::IsValidSlot(int slotNumber, bool IsEquipmentSlot)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): IsValidSlot(%d)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), slotNumber);
    }
    if (IsEquipmentSlot)
        return (m_equipmentSlots.IsValidIndex(slotNumber));
    return (m_inventorySlots.IsValidIndex(slotNumber));
}

/**
 * Returns the truth of whether the requested slot is a real equipment slot or not.
 * @param slotNumber The slot being requested
 * @return Boolean
 */
bool UInventoryComponent::isValidEquipmentSlot(int slotNumber)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): isValidEquipmentSlot(%d)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), slotNumber);
    }
    return (m_equipmentSlots.IsValidIndex(slotNumber));
}

/**
 * (overload) Returns the truth of whether the requested slot is a real equipment slot or not.
 * Not preferred, as the int version of this function is O(1)
 * @param equipSlot The equip slot enum to look for ( O(n) lookup )
 * @return Truth of outcome
 */
bool UInventoryComponent::isValidEquipmentSlotEnum(EEquipmentSlotType equipSlot)
{
    //UE_LOG(LogTemp, Display, TEXT("InventoryComponent: isValidEquipmentSlotEnum"));
    for (int i = 0; m_equipmentSlots.Num(); i++)
    {
        if (isValidEquipmentSlot(i))
        {
            if (m_equipmentSlots[i].EquipType == equipSlot)
                return true;
        }
    }
    return false;
}

/**
 * Returns the truth of whether the requested slot is empty.
 * Performs IsValidSlot() internally. No need to call both functions.
 * @param slotNumber The slot number to check.
 * @param isEquipment If true, checks equipment slots. If false, checks inventory slots.
 * @return If true, the slot is vacant.
 */
bool UInventoryComponent::isEmptySlot(int slotNumber, bool isEquipment)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): isEmptySlot(%d, %s)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), slotNumber, isEquipment?TEXT("TRUE"):TEXT("FALSE"));
    }
    if (!IsValidSlot(slotNumber, isEquipment))
        return false;
    return (getQuantityInSlot(slotNumber, isEquipment) < 1);
}

/**
 * Returns the truth of whether the requested slot is empty.
 * Performs IsValidSlot() internally. No need to call both functions.
 * @param equipSlot The Equip enum for the slot we're checking.
 * @return True if the equipment slot is unoccupied.
 */
bool UInventoryComponent::isEquipmentSlotEmpty(EEquipmentSlotType equipSlot)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): GetFirstEmptySlot(%s)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), *UEnum::GetValueAsString(equipSlot));
    }
    const int slotNum = getEquipmentSlotNumber(equipSlot);
    if (slotNum >= 0)
    {
        if (isValidEquipmentSlot(slotNum))
            return (m_equipmentSlots[slotNum].SlotQuantity < 1);
    }
    return false;
}

/**
 * Gets the name of the item that resides in the requested inventory slot.
 * @param slotNumber an int int representing the slot number requested
 * @param isEquipment True if looking for an equipment slot
 * @return FName the itemName of the item in slot. Returns 'invalidName' if empty or nonexistent.
 */
FName UInventoryComponent::getItemNameInSlot(int slotNumber, bool isEquipment)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): getItemNameInSlot(%d, %s)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), slotNumber, isEquipment?TEXT("TRUE"):TEXT("FALSE"));
    }
    const FStInventorySlot InventorySlot = GetInventorySlot(slotNumber, isEquipment);
    return InventorySlot.ItemName;
}

/**
 * Retrieves the quantity of the item in the given slot.
 * @param slotNumber The int int representing the slot number being requested
 * @param isEquipment True if the slot is an equipment slot.
 * @return Int of the slot number. Negative indicates failure.
 */
int UInventoryComponent::getQuantityInSlot(int slotNumber, bool isEquipment)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): getQuantityInSlot(%d, %s)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), slotNumber, isEquipment?TEXT("TRUE"):TEXT("FALSE"));
    }
    if (!IsValidSlot(slotNumber, isEquipment))
        return -1;
    
    if (isEquipment)
    {
        if (m_equipmentSlots.IsValidIndex(slotNumber))
            return m_equipmentSlots[slotNumber].SlotQuantity;
        return 0;
    }
    if (m_inventorySlots.IsValidIndex(slotNumber))
    {
        return m_inventorySlots[slotNumber].SlotQuantity;
    }
    return 0;
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
        return -1;
    
    if (quantity < 1)
        quantity = 1;

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
                const TArray<int> MatchingSlots = getSlotsContainingItem(InventorySlot.ItemName);
                for (const int ExistingSlot : MatchingSlots)
                {
                    if (MatchingSlots.IsValidIndex(ExistingSlot))
                    {
                        const int TempSlotNumber        = MatchingSlots[ExistingSlot];
                        const FStInventorySlot& MatchingSlot  = GetInventorySlot(TempSlotNumber);

                        const float MaxDurability = UItemSystem::getDurability(InventorySlot.ItemName);
                        const bool DoesDurabilityMatch = MatchingSlot.SlotDurability == InventorySlot.SlotDurability;
                        const bool DoesNewItemHaveMaxDurability = InventorySlot.SlotDurability >= MaxDurability;

                        if ( DoesDurabilityMatch || DoesNewItemHaveMaxDurability)
                        {
                            const int QuantityInSlot    = getQuantityInSlot(TempSlotNumber);
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
                const int FirstFreeSlot = GetFirstEmptySlot();
                if (IsValidSlot(FirstFreeSlot))
                {
                    SlotNumber = FirstFreeSlot;
                }
            }
        }

        // If the slot is now valid (or was valid to start with), add the item
        if (SlotNumber >= 0)
        {
            const int QuantityInSlot = getQuantityInSlot(SlotNumber);
            const int MaxStackSize = UItemSystem::getMaximumStackSize(InventorySlot.ItemName);
            const int NewQuantity = QuantityInSlot + RemainingQuantity;

            if (QuantityInSlot < MaxStackSize)
            {
                m_inventorySlots[SlotNumber].ItemName     = InventorySlot.ItemName;
                m_inventorySlots[SlotNumber].SlotQuantity = NewQuantity > MaxStackSize ? MaxStackSize : NewQuantity;

            	// If we are the server, we don't receive the "OnRep" call,
            	//    so the broadcast needs to be called manually.
            	if (GetNetMode() < NM_Client)
            		OnInventoryUpdated.Broadcast(SlotNumber);
            	
                // The current quantity minus the original quantity is how many items were added
                RemainingQuantity -= (m_inventorySlots[SlotNumber].SlotQuantity - QuantityInSlot);
            }

            // If we're hunting for a slot, keep looping if more items remain.
            if (RemainingQuantity > 0 && IsHuntingForSlot)
                return RemainingQuantity;
        }
        
        // If the slot is still not valid, the inventory is full
        else
        {
            UE_LOG(LogTemp, Display, TEXT("%s(%s): AddItemFromDataTable() Failed - Inventory Full"),
                *GetName(), GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
            break;
        }
        
    }
    /* Keep looping so long as there is:
     *      - 
     *      - 
     *      - 
     *      - 
     */
    while (
        RemainingQuantity > 0       // There is more to add,                            AND
        && overflowAdd              // Add all overflowing items,                       AND
        && IsHuntingForSlot         // No specific spot was specified to begin with,    AND
        && LoopIterations < 1000    // The loop is not suspected to be infinite/hung
        );

    if (LoopIterations >= 1000)
    {
        UE_LOG(LogTemp, Error, TEXT("%s(%s): AddItemFromDataTable() was safeguarded against an infinite loop"),
            *GetName(), GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
        return RemainingQuantity;
    }

    // If dropping overflow on the ground
    if (overflowDrop && RemainingQuantity > 0)
    {
        const ACharacter* OwnerCharacter = Cast<ACharacter>( GetOwner() );
        const USkeletalMeshComponent* thisMesh = OwnerCharacter->GetMesh();
        if (!IsValid(thisMesh))
            return RemainingQuantity;
        
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
        return -1;
    
    const FStItemData ItemData = UItemSystem::getItemDataFromItemName(ItemName);
    
    // Slot number was given, but is an invalid inventory slot
    if (SlotNumber >= 0)
    {
        // Invalid Slot
        if (!IsValidSlot(SlotNumber))
            return -2;
        
        // Slot is Full
        if (GetInventorySlot(SlotNumber).SlotQuantity >= ItemData.maxStackSize)
            return -3;
    }
    
    // If there's nothing to add, then do nothing
    if (quantity < 1)
        return -4;

    // Use a temp variable so we're not operating on the quantity parameter directly
    const int RemainingQuantity = quantity;

    // pseudo-slot to simply function operations & avoid repeated code
    const FStInventorySlot NewSlotReference = FStInventorySlot(ItemName, quantity);

    const int ItemsAdded = AddItemFromExistingSlot(NewSlotReference, quantity, SlotNumber, overflowAdd, overflowDrop, showNotify);
    if (ItemsAdded >= 0)
    {
        return RemainingQuantity - ItemsAdded;
    }
    return RemainingQuantity;
}

/** Returns a copy of the slot data for the slot requested.
 * Public accessor version of 'GetInventorySlot', but returns a copy instead of a reference.
 * @param slotNumber An int representing the inventory slot
 * @param IsEquipment True if the slot is an equipment slot
 * @return FStSlotContents of the found slot. Returns default struct if slot is empty or not found.
 */
FStInventorySlot UInventoryComponent::GetCopyOfSlot(int slotNumber, bool IsEquipment)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): GetInventorySlot(%d)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), slotNumber);
    }
    if (IsEquipment)
    {
        if (IsValidSlot(slotNumber))
        {
            return m_inventorySlots[slotNumber];
        }
    }
    else
    {
        if (isValidEquipmentSlot(slotNumber))
        {
            return m_equipmentSlots[slotNumber];
        }
    }
    return FStInventorySlot();
}

/**
 * Attempts to read the item from the provided slot, and if it is a valid
 * target for equipping, it will don the equipment to the appropriate slot.
 *
 * @param fromInventory The inventory we are equipping the item from.
 * @param fromSlot The slot where the item reference can be found
 * @param toSlot (optional) If not NONE it will equip the item directly to this slot.
 * @return True if the item was added successfully
 */
bool UInventoryComponent::donEquipment(UInventoryComponent* fromInventory, int fromSlot, EEquipmentSlotType eSlot)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): donEquipment()"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    if (!IsValid(fromInventory)) fromInventory = this;
    
    // Per UE coding convention, "IsValid" should be used when possible over "nullptr" check.
    const FStInventorySlot InventorySlot = fromInventory->GetInventorySlot(fromSlot);
    if (!UItemSystem::getItemNameIsValid(InventorySlot.ItemName))
        return false;

    // If the parameter was not a specific equipment slot, find one that works.
    EEquipmentSlotType equipSlot = eSlot;
    if (eSlot == EEquipmentSlotType::NONE)
    {
        // Get all slots this item can be equipped into as an array
        // Attempt to add the item to each slot until it fits or fails
        TArray<EEquipmentSlotType> eligibleSlots = UItemSystem::getItemEquipSlots(InventorySlot.GetItemData());
        for (int i = 0; i < eligibleSlots.Num(); i++)
        {
            const FStInventorySlot foundSlot = getEquipmentSlot(eligibleSlots[i]);
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
        const int slotNumber = getEquipmentSlotNumber(eSlot);
        if (!isValidEquipmentSlot(slotNumber))                  return false;
        if (getEquipmentSlotByNumber(slotNumber).EquipType != eSlot)    return false;

        m_equipmentSlots[slotNumber].ItemName = fromInventory->getItemNameInSlot(fromSlot);
        const int itemsRemoved = fromInventory->removeItemFromSlot(fromSlot, INT_MAX, false, false, false, true);
    	m_equipmentSlots[slotNumber].SlotQuantity = itemsRemoved;

    	// If we are the server, we don't receive the "OnRep" call,
    	//    so the broadcast needs to be called manually.
    	if (GetNetMode() < NM_Client)
    		OnEquipmentUpdated.Broadcast(equipSlot);
        return true;
    }
    
    return false;
}

/**
* Unequip a piece of equipment. Provides an argument for destroying the item,
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
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): doffEquipment()"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    // If destination inventory not provided, use this inventory
    if (!IsValid(toInventory)) toInventory = this;
    if (toInventory->isInventoryReady())
    {
        // Make sure the destination slot is available
        if (slotNumber >= 0)
        {
            if (!toInventory->isEmptySlot(slotNumber))
                return false;
        }

        // Remove the equipment
        const int fromSlot = getEquipmentSlotNumber(equipSlot);
        {
            if ( !isEmptySlot(fromSlot, true) )
            {
                FStItemData itemData = getItemInSlot(fromSlot, true);
                
                // Always remove the item first, to avoid duplication exploits
                const FStInventorySlot InventorySlotCopy = GetCopyOfSlot(slotNumber, true);
                const int itemsRemoved = removeItemFromSlot(fromSlot, INT_MAX,
                        true, false, false, true);
                if (itemsRemoved > 0)
                {
                    toInventory->AddItemFromExistingSlot(InventorySlotCopy, itemsRemoved, false, false, false);
                }
            }
        }
    }
    return false;
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
int UInventoryComponent::removeItemFromSlot(int slotNumber, int quantity, bool isEquipment, bool showNotify, bool dropToGround, bool removeAll)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): removeItemFromSlot(%d)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), slotNumber);
    }
    if (quantity < 1) quantity = 1;
    if (isInventoryReady())
    {
        if (isEquipment && isValidEquipmentSlot(slotNumber) || !isEquipment && IsValidSlot(slotNumber))
        {
            const FStInventorySlot InventorySlot = GetInventorySlot(slotNumber, isEquipment);
            const FStItemData existingItem = getItemInSlot(slotNumber, isEquipment);
            if (UItemSystem::getItemNameIsValid(InventorySlot.ItemName))
            {
            
                // Can't remove an item that doesn't exist.
                const FName itemName = InventorySlot.ItemName;
                if (itemName == UItemSystem::getInvalidName())
                {
                    UE_LOG(LogTemp, Warning, TEXT("removeItemFromSlot(%d) called, but slot was invalid/empty."), slotNumber);
                    return -1;
                }
            
                // Ensures 'quantity' is not greater than the actually quantity in slot
                const int slotQuantity = getQuantityInSlot(slotNumber, isEquipment);
                int qty = (slotQuantity < quantity) ? slotQuantity : quantity;

                // All items are being removed
                if (removeAll) { qty = slotQuantity; }
                if (qty == slotQuantity || removeAll)
                {
                    if (isEquipment)
                    {
                        m_equipmentSlots[slotNumber].SlotQuantity = 0;
                        m_equipmentSlots[slotNumber].ItemName = FName();
                    }
                    else
                    {
                        m_inventorySlots[slotNumber].SlotQuantity = 0;
                        m_inventorySlots[slotNumber].ItemName = FName();
                    }
                }
                // Only a portion of the items are being removed.
                else
                {
                    if (isEquipment) m_equipmentSlots[slotNumber].SlotQuantity -= qty;
                    else             m_inventorySlots[slotNumber].SlotQuantity -= qty;
                }
            
                if (showNotify) sendNotification(itemName, qty, false);
            
                // Removes the item from memory
                if (getQuantityInSlot(slotNumber) < 1)
                {
                    if (isEquipment) m_equipmentSlots[slotNumber].ItemName = FName();
                    else             m_inventorySlots[slotNumber].ItemName = FName();
                }

            	// If we are the server, we don't receive the "OnRep" call,
            	//    so the broadcast needs to be called manually.
            	if (GetNetMode() < NM_Client)
            	{
            		if (isEquipment)
            			OnEquipmentUpdated.Broadcast(getEquipmentSlotType(slotNumber));
            		else
            			OnInventoryUpdated.Broadcast(slotNumber);
				}
                return qty;
            }
        }
    }
    return -1;
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
int UInventoryComponent::removeItemByQuantity(FName itemName, int quantity, bool isEquipment, bool dropToGround, bool removeAll)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): removeItemByQuantity(%d)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), quantity);
    }
    if (quantity < 1) quantity = 1;
    if (isInventoryReady() && itemName != UItemSystem::getInvalidName())
    {
        int qty = quantity; // Track how many we've removed
        const TArray<int> itemSlots = getSlotsContainingItem(itemName);

        for (int i = 0; i < itemSlots.Num(); i++)
        {
            // Continues removing items until quantity is met
            const int numRemoved = removeItemFromSlot(itemSlots[i], qty,
                isEquipment, true, dropToGround, removeAll);
            qty -= numRemoved;

            if (qty < 1) break;
        }
        
        // If current quantity is not equal to the requested quantity, we succeeded
        if (qty != quantity)
            return (quantity - qty); // The amount actually removed
        
    }
    return -1;
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
bool UInventoryComponent::swapOrStackSlots(UInventoryComponent* fromInventory, int fromSlotNum, int toSlotNum, int quantity, bool showNotify, bool isEquipmentSlot, bool fromEquipmentSlot)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): swapOrStackSlots()"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    int remainder;
    return swapOrStackWithRemainder(fromInventory, fromSlotNum, toSlotNum, quantity, remainder, showNotify, isEquipmentSlot, fromEquipmentSlot);
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
bool UInventoryComponent::swapOrStackWithRemainder(UInventoryComponent* fromInventory, int fromSlotNum,
    int toSlotNum, int quantity, int& remainder, bool showNotify, bool isEquipmentSlot, bool fromEquipmentSlot)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): swapOrStackWithRemainder()"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    
    // If the 'fromInventory' is invalid, we're transferring an item within this same inventory;
    if (!IsValid(fromInventory)) fromInventory = this;
    
    if (!fromInventory->IsValidSlot(fromSlotNum)) return false;
    if (!IsValidSlot(toSlotNum)) toSlotNum = -1;
    if (quantity < 1) quantity = 1;

    // Remember which items were in which slots
    const FStInventorySlot FromInventorySlot = fromInventory->GetInventorySlot(fromSlotNum);
    const FStInventorySlot ToInventorySlot   = GetInventorySlot(toSlotNum);
    
    const int fromQuantity = FromInventorySlot.SlotQuantity;

    // If the FROM item isn't valid, this is not a valid swap. Otherwise.... wtf are they swapping?
    if (!UItemSystem::getItemNameIsValid(FromInventorySlot.ItemName))
    {
        return false;
    }

    // Ensures the slot matches what the client says it is and that the FROM slot is a valid slot type
    // ReSharper disable once CppInitializedValueIsAlwaysRewritten
    bool isCorrectSlotType = false;
    if (fromEquipmentSlot)
    {
        isCorrectSlotType = (fromInventory->getEquipmentSlotType(fromSlotNum)) != EEquipmentSlotType::NONE;
    }
    else
        isCorrectSlotType = (fromInventory->getInventorySlotType(fromSlotNum)) != EInventorySlotType::NONE;
    if (!isCorrectSlotType)
    {
        return false;
    }

    // Ensures the destination matches what the client says, and that the TO slot is a valid slot for this type.
    // ReSharper disable once CppInitializedValueIsAlwaysRewritten
    bool destinationValid = false;
    if (isEquipmentSlot)
        destinationValid = getEquipmentSlotType(toSlotNum) != EEquipmentSlotType::NONE;
    else
    {
        if (toSlotNum < 0) toSlotNum = GetFirstEmptySlot();
        destinationValid = getInventorySlotType(toSlotNum) != EInventorySlotType::NONE;
    }
    if (!destinationValid)
    {
        return false;
    }

    // Check if there's an existing item in the destination slot
    if (UItemSystem::getItemNameIsValid(ToInventorySlot.ItemName))
    {

        // Check if items can be stacked. This will be the easiest, fastest way to manage this call.
        if (UInventorySystem::IsSameItem(fromInventory->GetInventorySlot(fromSlotNum), GetInventorySlot(toSlotNum)))
        {
            if (fromInventory->decreaseQuantityInSlot(fromSlotNum, quantity, showNotify))
            {
                const int itemsAdded = increaseQuantityInSlot(toSlotNum, quantity, showNotify);
                remainder = (quantity - itemsAdded);
                return true;   
            }
        }
        
    }

    // We have one last problem to look for; If one of the slots (or both) is an equipment slot, we obviously can't
    // swap something into it that is not a matching equipment type. So we need to check that the types match.
    // i.e
    //   You can't take a sword out of primary, and swap a carrot from the inventory into the primary slot.
    if ((fromEquipmentSlot || isEquipmentSlot) && UItemSystem::getItemNameIsValid(ToInventorySlot.ItemName))
    {
        // Gets the eligible slot types from the existing item (the item in the origination slot)
        const EEquipmentSlotType ourSlotType = getEquipmentSlotType(toSlotNum);
        const TArray<EEquipmentSlotType> slotTypes = UItemSystem::getItemEquipSlots(ToInventorySlot.GetItemData());
        const int numSlots = slotTypes.Num();
        bool typeMatches = false;
        if (numSlots > 0)
        {
            for (int i = 0; i < numSlots; i++)
            {
                // If this equipment slot type matches this destination slot's equip type, we're good.
                if (slotTypes[i] == ourSlotType)
                {
                    typeMatches = true;
                    break;
                }
            }
        }
        if (!typeMatches)
        {
            return false;
        }
    }

    // Step 1: Validate the amount of item to move over.
    const int moveQuantity = quantity > fromQuantity ? fromQuantity : quantity;

    // Step 2: Remember the item we used to have in this slot.
    FStItemData oldItem = getItemInSlot(toSlotNum, isEquipmentSlot); // Returns -1 if this slot was empty

    // If the items match or the destination slot is empty, it will stack.
    // Otherwise it will swap the two slots.
    const bool isSameItem = UInventorySystem::IsSameItem(fromInventory->GetInventorySlot(fromSlotNum), GetInventorySlot(toSlotNum)); 
    if (isSameItem || !UItemSystem::getItemNameIsValid(ToInventorySlot.ItemName))
    {
        if (isEquipmentSlot)
        {
            const EEquipmentSlotType eSlot = getEquipmentSlotType(toSlotNum);
            return donEquipment(this, fromSlotNum, eSlot);
        }
        if (fromInventory->decreaseQuantityInSlot(fromSlotNum, moveQuantity, showNotify) > 0)
        {
            const int ItemsAdded = AddItemFromExistingSlot(FromInventorySlot, moveQuantity, toSlotNum, false, false, showNotify);
            remainder = moveQuantity - getQuantityInSlot(toSlotNum, isEquipmentSlot);
            if (ItemsAdded <= 0)
                return false;
            return true;
        }
    }
    else
    {
        if (fromInventory->decreaseQuantityInSlot(fromSlotNum, fromQuantity, fromEquipmentSlot, showNotify) > 0)
        {
            const int ItemsAdded = AddItemFromExistingSlot(FromInventorySlot, fromQuantity, toSlotNum, false, false, showNotify);
            remainder = moveQuantity - getQuantityInSlot(toSlotNum, isEquipmentSlot);
            if (ItemsAdded <= 0)
            {
                return false;
            }
            return true;
        }
        
        remainder = moveQuantity - getQuantityInSlot(toSlotNum, isEquipmentSlot);
        return true;
    }
    return false;
}

/**
 * @brief Finds which slots have changed, and sends an update event
 */
void UInventoryComponent::OnRep_InventorySlotUpdated(TArray<FStInventorySlot> OldInventory)
{
	for (int i = 0; i < getNumInventorySlots(); i++)
	{
		if (OldInventory.IsValidIndex(i))
		{
			// If it's the exact same item, nothing was changed.
			FStInventorySlot& SlotReference = GetInventorySlot(i);
			if (!UInventorySystem::IsExactSameItem(GetInventorySlot(i), OldInventory[i]))
			{
				OnInventoryUpdated.Broadcast(i);
			}
		}
		// If the old slot didn't exist, then broadcast the addition of this new slot
		else
			OnInventoryUpdated.Broadcast(i);
	}
}

/**
 * @brief Finds which slots have changed, and sends an update event
 */
void UInventoryComponent::OnRep_EquipmentSlotUpdated(TArray<FStInventorySlot> OldEquipment)
{
	for (int i = 0; i < getNumInventorySlots(); i++)
	{
		if (OldEquipment.IsValidIndex(i))
		{
			// If it's the exact same item, nothing was changed.
			FStInventorySlot& SlotReference = GetInventorySlot(i, true);
			if (!UInventorySystem::IsExactSameItem(GetInventorySlot(i), OldEquipment[i]))
			{
				OnEquipmentUpdated.Broadcast( getEquipmentSlotType(i) );
			}
		}
		// If the old slot didn't exist, then broadcast the addition of this new slot
		else
			OnEquipmentUpdated.Broadcast( getEquipmentSlotType(i) );
	}
}

void UInventoryComponent::Server_InitSavedInventory_Implementation(
	const TArray<FStStartingItem>& StartingItemList)
{
	if (!bIsInventoryReady)
	{
		StartingItems.Empty();
		for (const FStStartingItem& StartingItem : StartingItemList)
		{
			StartingItems.Add(StartingItem);
		}
		ReinitializeInventory();
	}

	if (!bInventoryRestored)
		return;

	for (const FStStartingItem& StartingItem : StartingItemList)
	{
		if (StartingItem.bStartEquipped)
		{
			const FName itemName = StartingItem.startingItem;
			const FStItemData ItemData = UItemSystem::getItemDataFromItemName(itemName);
			
			int slotNumber = -1;

			// Check each eligible equip slot for an open slot
			for (const EEquipmentSlotType EquipSlot : ItemData.equipSlots)
			{
				if (isEquipmentSlotEmpty(EquipSlot))
				{
					slotNumber = getEquipmentSlotNumber(EquipSlot);
				}
			}

			// If an eligible equipment slot was available, equip the item.
			if (slotNumber >= 0)
			{
				// TODO - A check should be added here to ensure the item
				//        is being added to an appropriate slot.
				m_equipmentSlots[slotNumber].ItemName = StartingItem.startingItem;
				m_equipmentSlots[slotNumber].SlotQuantity = StartingItem.quantity;

				// Server doesn't receive the "OnRep" call, fire manually
				OnEquipmentUpdated.Broadcast(m_equipmentSlots[slotNumber].EquipType);
			}
		}
		// If it isn't an equipment item going into an equipment slot,
		// it can just be added from front to back
		else
			AddItemFromDataTable(StartingItem.startingItem, StartingItem.quantity);
	}
}

void UInventoryComponent::Server_RestoreSavedInventory_Implementation(
	const TArray<FStInventorySlot>& RestoredInventory,
	const TArray<FStInventorySlot>& RestoredEquipment)
{
	if (GetNetMode() < NM_Client)
		RestoreInventory(RestoredInventory, RestoredEquipment);
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
bool UInventoryComponent::splitStack(
    UInventoryComponent* fromInventory, int fromSlotNum, int splitQuantity, int toSlotNum, bool showNotify)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): splitStack()"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    //UE_LOG(LogTemp, Display, TEXT("InventoryComponent: splitStack"));
    if (splitQuantity < 1) splitQuantity = 1;
    if (fromInventory == nullptr) fromInventory = this;

    // Always show notification if the inventory isn't the same (item gained/lost)
    if (!showNotify) showNotify = (fromInventory == this);
    const FStInventorySlot InventorySlot = fromInventory->GetCopyOfSlot(fromSlotNum);
    
    const FStItemData existingItem = fromInventory->getItemInSlot(fromSlotNum);
    
    if (UItemSystem::getItemNameIsValid(InventorySlot.ItemName))
    {
        const int existingQty = fromInventory->getQuantityInSlot(fromSlotNum) - 1;
        // Never move the entire stack, always a maximum of 1 less than the stack quantity
        const int qty = (splitQuantity >= existingQty) ? existingQty : splitQuantity;
        if (qty < 1)
        {
            return false;
        }
        
        // Always take the item first, to avoid duplication exploits.
        if (fromInventory->decreaseQuantityInSlot(fromSlotNum, qty) >= 0, showNotify)
        {
            const int itemsAdded = AddItemFromExistingSlot(InventorySlot, qty,
                                                           toSlotNum, false, false, showNotify);
            
            if (itemsAdded > 0)
            {
                // If items added were less than we intended, we weren't able to add all of them.
                if (itemsAdded < qty)
                {
                    // Re-add the ones that weren't able to stack
                    fromInventory->increaseQuantityInSlot(fromSlotNum, qty - itemsAdded, showNotify);
                }
                return true;
            }
            // We failed to add the items. Refund the original item.
            fromInventory->AddItemFromExistingSlot(InventorySlot, qty, fromSlotNum, false, false);
            
        }
    }
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
int UInventoryComponent::increaseQuantityInSlot(int slotNumber, int quantity, bool showNotify)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): increaseQuantityInSlot()"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    int newQty;
    return increaseQuantityInSlot(slotNumber, quantity, newQty, showNotify);
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
int UInventoryComponent::increaseQuantityInSlot(int slotNumber, int quantity, int& newQuantity, bool showNotify)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): increaseQuantityInSlot(overload)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    if (quantity < 1) quantity = 1;
    const FStInventorySlot InventorySlot = GetInventorySlot(slotNumber);
    const FStItemData slotItem = getItemInSlot(slotNumber);
    if (UItemSystem::getItemNameIsValid(InventorySlot.ItemName) && !InventorySlot.IsSlotEmpty())
    {
        const int maxQty = UItemSystem::getMaximumStackSize(InventorySlot.ItemName);
        const int startQty = getQuantityInSlot(slotNumber);
        const int newTotal = startQty + abs(quantity);
        newQuantity = (newTotal >= maxQty) ? maxQty : newTotal;
        const int amountAdded = (newQuantity - startQty);
    	m_inventorySlots[slotNumber].SlotQuantity = newQuantity;

    	// If we are the server, we don't receive the "OnRep" call,
    	//    so the broadcast needs to be called manually.
    	if (GetNetMode() < NM_Client)
    		OnInventoryUpdated.Broadcast(slotNumber);
    	
        if (showNotify)
        {
            sendNotification(getItemNameInSlot(slotNumber), amountAdded);
        }
        //InventoryUpdate(slotNumber);
        return amountAdded;
    }
    return -1;
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
int UInventoryComponent::decreaseQuantityInSlot(int slotNumber, int quantity, int& remainder, bool isEquipment, bool showNotify)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): decreaseQuantityInSlot(overload)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    if (quantity < 1) quantity = 1;
    const FStInventorySlot InventorySlot = GetInventorySlot(slotNumber);
    const FStItemData slotItem = getItemInSlot(slotNumber);
    if (UItemSystem::getItemNameIsValid(InventorySlot.ItemName))
    {
        const int startQty = getQuantityInSlot(slotNumber);
        const int tempQty = startQty - abs(quantity);
        remainder = (tempQty >= 0) ? tempQty : 0;
        
        // Since we may have only removed 'n' amount instead of 'quantity', we need to recalculate
        const int amountRemoved = startQty - remainder;

        FStInventorySlot& ReferenceSlot = isEquipment ? m_equipmentSlots[slotNumber] : m_inventorySlots[slotNumber];
        ReferenceSlot.SlotQuantity = remainder;
        
        if (showNotify)
        {
            sendNotification(getItemNameInSlot(slotNumber), amountRemoved, false);
        }

        // If nothing is left, remove it altogether.
        if (ReferenceSlot.SlotQuantity < 1)
        {
            ReferenceSlot.EmptyAndResetSlot();
        }
        
        //InventoryUpdate(slotNumber);
        return amountRemoved;
    }
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
int UInventoryComponent::decreaseQuantityInSlot(int slotNumber, int quantity, bool isEquipment, bool showNotify)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): decreaseQuantityInSlot()"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    int remainder;
    return decreaseQuantityInSlot(slotNumber, quantity, remainder, isEquipment, showNotify);
}

/**
* CLIENT event that retrieves and subsequently removes all inventory notifications.
* @return A TArray of FStInventoryNotify
*/
TArray<FStInventoryNotify> UInventoryComponent::getNotifications()
{
    TArray<FStInventoryNotify> invNotify;
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): getNotifications()"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    if (!m_notifications.IsEmpty())
    {
        invNotify = m_notifications;
        m_notifications.Empty();
        return invNotify;
    }
    return invNotify;
}

/**
* Sends a notification that an item was modified. Since the item may or may not exist
* at the time the notification is created, we just use a simple FName and the class defaults.
* 
* @param itemName The FName of the affected item
* @param quantity The quantity affected
* @param wasAdded True if the item was added, False if the item was removed
*/
void UInventoryComponent::sendNotification_Implementation(FName itemName, int quantity, bool wasAdded)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): sendNotification_Implementation()"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    if (quantity <= 0) return;
    FStInventoryNotify invNotify;
    invNotify.itemName = itemName;
    invNotify.itemQuantity = (wasAdded ? abs(quantity) : abs(quantity) * (-1));
    UE_LOG(LogTemp, Display, TEXT("A new inventory notification is available"));
    m_notifications.Add(invNotify);
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("%s(%s): OnNotificationAvailable.Broadcast()"),
            *GetName(), GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    OnNotificationAvailable.Broadcast();
}

/**
* Wipes the inventory slot, making it empty and completely destroying anything previously in the slot.
* Mostly used for cases where we know we want the slot to absolutely be empty in all cases.
* @param slotNumber The slot number to reset
*/
void UInventoryComponent::resetInventorySlot(int slotNumber)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): resetInventorySlot(%d)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), slotNumber);
    }
    removeItemFromSlot(slotNumber, 1, false,true);
    //InventoryUpdate(slotNumber);
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
bool UInventoryComponent::transferItemBetweenSlots(
    UInventoryComponent* fromInventory, UInventoryComponent* toInventory,
    int fromSlotNum, int toSlotNum, int moveQuantity, bool overflowDrop, bool isFromEquipSlot, bool isToEquipSlot)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): transferItemBetweenSlots()"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    if (moveQuantity < 1) moveQuantity = 1;
    if (!IsValid(fromInventory)) fromInventory = this;
    if (!IsValid(toInventory)) toInventory = this;

    bool showNotify = false;
    if (fromInventory != toInventory) showNotify = true;

    // The from item MUST be valid since the authoritative inventory is the one GAINING the item.
    const FStInventorySlot FromInventorySlot = fromInventory->GetCopyOfSlot(fromSlotNum, isFromEquipSlot);
    FStItemData fromItem = FromInventorySlot.GetItemData();
    if (UItemSystem::getItemNameIsValid(FromInventorySlot.ItemName))
    {
        // Does an item exist in the destination slot?
        const FStInventorySlot ToInventorySlot = GetInventorySlot(toSlotNum, isToEquipSlot);
        FStItemData toItem = ToInventorySlot.GetItemData();
        if (UItemSystem::getItemNameIsValid(ToInventorySlot.ItemName) || toSlotNum < 0)
        {
            // If to slot is valid, we're swapping or stacking the slots.
            if (toSlotNum >= 0)
            {
                if (toInventory->swapOrStackSlots(fromInventory, fromSlotNum, toSlotNum, moveQuantity, showNotify))
                {
                    return true;
                }
            }
            // If TO slot is not valid, move item to first available slot.
            else
            {
                int itemsRemoved = fromInventory->removeItemFromSlot(fromSlotNum, moveQuantity, showNotify, overflowDrop);
                if (itemsRemoved > 0)
                {
                    int slotNum = toInventory->AddItemFromExistingSlot(FromInventorySlot, fromSlotNum, moveQuantity, true, overflowDrop, showNotify);
                    if (slotNum >= 0)
                    {
                        return true;
                    }
                }
            }
        }
        // If an existing item doesn't exist, we're splitting.
        else
        {
            int fromQuantity = fromInventory->getQuantityInSlot(fromSlotNum);
            if (fromQuantity > moveQuantity)
            {
                toInventory->splitStack(fromInventory, fromSlotNum, moveQuantity, toSlotNum, showNotify);
            }
        }
    }
    
    return false;
}

/**
* Called when an item is activated from the inventory. This function is
* executed from the inventory of the player who activated the item.
*
* @param slotNumber The number of the inventory slot containing the item to be activated
* @param isEquipment True if this is an equipment slot. False by default.
* @param consumeItem True consumes the item, whether it's consumable or not. False by default.
* @return True if the item was successfully activated
*/
bool UInventoryComponent::activateItemInSlot(int slotNumber, bool isEquipment, bool consumeItem)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): activateItemInSlot(%d)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), slotNumber);
    }
    if (IsValidSlot(slotNumber))
    {
        const FStInventorySlot InventorySlot = GetInventorySlot(slotNumber, isEquipment);
        FStItemData itemInSlot = InventorySlot.GetItemData();
        if (UItemSystem::getItemNameIsValid(InventorySlot.ItemName))
        {
            if (itemInSlot.itemActivation == EItemActivation::EQUIP)
            {
                int destinationSlot = -1;
                for ( const EEquipmentSlotType slotType : itemInSlot.equipSlots )
                {
                    const int eSlotNum = getEquipmentSlotNumber(slotType);
                    if (isValidEquipmentSlot(eSlotNum))
                    {
                        destinationSlot = eSlotNum;
                        break;
                    }
                }
                return swapOrStackSlots(this,slotNumber, destinationSlot, 1,
                false, !isEquipment, isEquipment);
            }
            // The item is consumed, or consume is forced
            if (consumeItem || itemInSlot.consumeOnUse)
            {
                // If the item fails to be removed/deducted
                if (decreaseQuantityInSlot(slotNumber,1,isEquipment) < 1)
                {
                    return false;
                }
            }
            OnItemActivated.Broadcast(InventorySlot.ItemName);
            return true;
        }
    }
    return false;
}

void UInventoryComponent::Server_RequestItemActivation_Implementation(
    UInventoryComponent* inventoryUsed, int slotNumber, bool isEquipment)
{
    if (GetOwner()->HasAuthority())
    {
        const FStInventorySlot InventorySlot = GetInventorySlot(slotNumber, isEquipment);
        const FStItemData itemData = inventoryUsed->getItemInSlot(slotNumber, isEquipment);
        if (UItemSystem::getItemNameIsValid(InventorySlot.ItemName))
        {
            inventoryUsed->activateItemInSlot(slotNumber, isEquipment);
        }
    }
}

/**
 * Adds the item to the notification TMap, notifying the player about a modification to their inventory.
 * @param itemName The item name that is being notified
 * @param quantityAffected The quantity that was affected (how many the player gained/lost)
 * @return True if the notification was successfully added to the queue.
 */
bool UInventoryComponent::setNotifyItem(FName itemName, int quantityAffected)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): setNotifyItem(%s, %d)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), *itemName.ToString(), quantityAffected);
    }
    if (UItemSystem::getItemNameIsValid(itemName))
    {
        sendNotification(itemName, quantityAffected, quantityAffected > 0);
        return true;
    }
    return false;
}

void UInventoryComponent::LoadDataDelegate(const FString& SaveSlotName, int32 UserIndex, USaveGame* SaveData)
{	
	if (!IsValid(SaveData))
	{
		if (UGameplayStatics::CreateSaveGameObject( UInventorySave::StaticClass() ))
		{
			InventorySaveSlotName_ = SaveSlotName;
			OnInventoryRestored.Broadcast(true);
			return;
		}
		OnInventoryRestored.Broadcast(false);
		return;
	}
	const UInventorySave* InventorySave = Cast<UInventorySave>( SaveData );
	
	if (!IsValid(InventorySave))
		return;
	
	RestoreInventory(InventorySave->InventorySlots_, InventorySave->EquipmentSlots_);
	OnInventoryRestored.Broadcast(true);
}

/**
 * @brief Gets the existing save data file or creates it, then performs a save.
 * @param SaveSlotName The name of the save slot to be used
 * @param UserIndex Usually zero
 * @param SaveData Pointer to the save data. Invalid if it does not exist.
 */
 void UInventoryComponent::SaveInventoryDelegate(const FString& SaveSlotName, int32 UserIndex, USaveGame* SaveData)
{
	bool wasSuccess = false;
	if (!IsValid(SaveData))
	{
		SaveData = UGameplayStatics::CreateSaveGameObject( UInventorySave::StaticClass() );
	}

	// If the save is valid, run the save
	Helper_SaveInventory(SaveData);
	UGameplayStatics::SaveGameToSlot(SaveData, InventorySaveSlotName_, 0);
	wasSuccess = true;
	OnInventoryRestored.Broadcast(wasSuccess);
}


//---------------------------------------------------------------------------------------------------------------
//---------------------------------- REPLICATION ----------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------



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
    UE_LOG(LogTemp, Display, TEXT("InventoryComponent: Server_TransferItems_Implementation"));
    // Validation
    if (!IsValid(fromInventory)) fromInventory = this;
    if (!IsValid(toInventory)) toInventory = this;

    // Move is from and to the exact same slot. Do nothing.
    if (fromInventory == toInventory && fromSlot == toSlot && isFromEquipSlot == isToEquipSlot)
        return;
    
    AActor* fromOwner = fromInventory->GetOwner();
    AActor* toOwner = toInventory->GetOwner();
    const ACharacter* fromPlayer = Cast<ACharacter>(fromOwner);
    const ACharacter* toPlayer   = Cast<ACharacter>(toOwner);

    // Make sure destination inventory is not withdraw only. Ignore this if it's the player's own inventory.
    if (toInventory != this && toInventory->m_WithdrawOnly)
    {
        UE_LOG(LogTemp, Display, TEXT("Inventory %s is WITHDRAW ONLY. Unable to place items into it."), *toInventory->GetName());
        return;
    }
    
    // This player is trying to modify a player-owned inventory.
    if (IsValid(fromPlayer))
    {
        // The *from* inventory is not this player. The transfer is invalid.
        if (fromPlayer != GetOwner())
        {
            UE_LOG(LogTemp, Error, TEXT("TransferItems(%s): Player attempting to move items from another player's inventory!"),
                (GetOwner()->HasAuthority()?TEXT("SRV"):TEXT("CLI")));
            return;
        }
    }
    // This player is trying to modify a player-owned inventory.
    if (IsValid(toPlayer))
    {
        // The *TO* inventory is not this player. The transfer is invalid.
        if (toPlayer != GetOwner())
        {
            UE_LOG(LogTemp, Error, TEXT("TransferItems(%s): Player attempting to move items from another player's inventory!"),
                (GetOwner()->HasAuthority()?TEXT("SRV"):TEXT("CLI")));
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
            UE_LOG(LogTemp, Display, TEXT("TransferItems(%s): Same inventory, but the player isn't the owner."),
                (GetOwner()->HasAuthority()?TEXT("SRV"):TEXT("CLI")));
            
            // Validate distance
            if (fromOwner->GetDistanceTo(toOwner) > 1024.0f)
            {
                UE_LOG(LogTemp, Display, TEXT("TransferItems(%s): Operation was Rejected (Distance too Far)"),
                    (GetOwner()->HasAuthority()?TEXT("SRV"):TEXT("CLI")));
                return;
            }
        }
        else
        {
            UE_LOG(LogTemp, Display, TEXT("TransferItems(%s): Moving items within the player's own inventory component."),
                (GetOwner()->HasAuthority()?TEXT("SRV"):TEXT("CLI")));
        }
        
        // If origin is inventory and destination is equipment, perform a DonEquipment
        if (!isFromEquipSlot && isToEquipSlot)
        {
            if (!fromInventory->IsValidSlot(fromSlot))
            {
                UE_LOG(LogTemp, Warning, TEXT("TransferItems(%s): From slot specified as inventory but was not a valid inventory slot."),
                    (GetOwner()->HasAuthority()?TEXT("SRV"):TEXT("CLI")));
                return;
            }
            if (!toInventory->isValidEquipmentSlot(toSlot))
            {
                UE_LOG(LogTemp, Warning, TEXT("TransferItems(%s): To slot specified as equipment but was not a valid equipment slot."),
                    (GetOwner()->HasAuthority()?TEXT("SRV"):TEXT("CLI")));
                return;
            }
            UE_LOG(LogTemp, Display, TEXT("TransferItems(%s): Moving Inventory Item into Equipment Slot."),
                (GetOwner()->HasAuthority()?TEXT("SRV"):TEXT("CLI")));
            if (!toInventory->donEquipment(fromInventory, fromSlot, toInventory->getEquipmentSlotType(toSlot)))
            {
                UE_LOG(LogTemp, Error, TEXT("TransferItems(%s): Failed to execute donEquipment()"),
                    (GetOwner()->HasAuthority()?TEXT("SRV"):TEXT("CLI")));
            }
        }
        
        // If origin is equipment and destination is inventory, perform a DoffEquipment
        else if (isFromEquipSlot && !isToEquipSlot)
        {
            const EEquipmentSlotType fromEquipSlot = fromInventory->getEquipmentSlotType(fromSlot);
            if (!fromInventory->isValidEquipmentSlot(fromSlot))
            {
                UE_LOG(LogTemp, Error, TEXT("TransferItems(%s): From slot specified as equipment but was not a valid equipment slot."),
                    (GetOwner()->HasAuthority()?TEXT("SRV"):TEXT("CLI")));
                return;
            }
            if (!toInventory->IsValidSlot(toSlot))
            {
                UE_LOG(LogTemp, Error, TEXT("TransferItems(%s): To slot specified as inventory but was not a valid inventory slot."),
                    (GetOwner()->HasAuthority()?TEXT("SRV"):TEXT("CLI")));
                return;
            }
            
            UE_LOG(LogTemp, Display, TEXT("TransferItems(%s): Moving Equipment Item into Inventory Slot."),
                (GetOwner()->HasAuthority()?TEXT("SRV"):TEXT("CLI")));
            if (!fromInventory->doffEquipment(fromEquipSlot, toSlot, toInventory, false))
            {
                UE_LOG(LogTemp, Error, TEXT("TransferItems(%s): Failed to execute doffEquipment()."),
                    (GetOwner()->HasAuthority()?TEXT("SRV"):TEXT("CLI")));
            }
        }
        
        // Both items are equipment or both items are non-equipment. Perform swap-or-stack.
        else
        {
            if (!toInventory->swapOrStackSlots(fromInventory,fromSlot,toSlot,moveQty,false,isToEquipSlot,isFromEquipSlot))
            {
                UE_LOG(LogTemp, Error, TEXT("TransferItems(%s): swapOrStack() Failed."),
                    (GetOwner()->HasAuthority()?TEXT("SRV"):TEXT("CLI")));
            }
        }
    }
    
    // If they are different inventories, first verify that the player is allowed to do it.
    else
    {
        UE_LOG(LogTemp, Display, TEXT("TransferItems(%s): Moving items between different inventory components."),
            (GetOwner()->HasAuthority()?TEXT("SRV"):TEXT("CLI")));
        
        if (fromOwner->GetDistanceTo(toOwner) > 1024.0f)
        {
            UE_LOG(LogTemp, Display, TEXT("TransferItems(%s): Operation was Rejected (Distance too Far)"),
                (GetOwner()->HasAuthority()?TEXT("SRV"):TEXT("CLI")));
            return;
        }
        
        // Make a copy of the origin item, and decrease it from the origin inventory.
        const FStItemData originItem = FromInventorySlot.GetItemData();
        if (UItemSystem::getItemNameIsValid(FromInventorySlot.ItemName))
        {
            // Remove the items from the origin inventory
            moveQty = fromInventory->decreaseQuantityInSlot(fromSlot,moveQty,isFromEquipSlot);
            if (moveQty < 1)
            {
                UE_LOG(LogTemp, Error, TEXT("TransferItems(%s): Failed to remove any items from origin inventory."),
                    (GetOwner()->HasAuthority()?TEXT("SRV"):TEXT("CLI")));
                return;
            }
        }
        
        // If decrease was successful, add it to the destination inventory.
        const int itemsAdded = toInventory->AddItemFromDataTable(FromInventorySlot.ItemName, moveQty, toSlot,
                    false,false,true);
        if (itemsAdded > 0)
        {
            UE_LOG(LogTemp, Display, TEXT("TransferItems(%s): The item inventory transfer was successful."),
                (GetOwner()->HasAuthority()?TEXT("SRV"):TEXT("CLI")));
        }
        
        // If the add failed, reinstate the item to the origin inventory
        else
        {
            UE_LOG(LogTemp, Error, TEXT("TransferItems(%s): Failed to add item. Reinstating the original item."),
                (GetOwner()->HasAuthority()?TEXT("SRV"):TEXT("CLI")));
            fromInventory->AddItemFromExistingSlot(FromInventorySlot, fromSlot,moveQty,true,false,false);
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
    UE_LOG(LogTemp, Display, TEXT("InventoryComponent: Server_DropItemOnGround_Implementation"));
    if (!IsValid(fromInventory)) fromInventory = this;

    FStInventorySlot FromInventorySlot = fromInventory->GetInventorySlot(fromSlot,isFromEquipSlot);
    if (FromInventorySlot.IsSlotEmpty())
        return;
    
    AActor* FromActor = fromInventory->GetOwner();
    if (!IsValid(FromActor)) return; // Just in case something went terribly wrong.

    AActor* ToActor = Cast<AActor>(GetOwner());
    if (!IsValid(ToActor)) return; // Safety catch, should never be possible
    
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
                return;
            }
        }
    }

    ACharacter* thisCharacter = Cast<ACharacter>(ToActor);
    if (IsValid(thisCharacter))
    {
        USkeletalMeshComponent* thisMesh = thisCharacter->GetMesh();
        if (!IsValid(thisMesh)) return;
        
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
            if (fromInventory->decreaseQuantityInSlot(fromSlot, tossQuantity) > 0)
            {
                pickupItem->SetupItemFromName(InventorySlot.ItemName, tossQuantity);
                pickupItem->FinishSpawning(spawnTransform);
            }
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
    UE_LOG(LogTemp, Display, TEXT("RequestOtherInventory() Received (SERVER EVENT) - Player wants to load an inventory"));

    AActor* thisActor = GetOwner();
    if (!IsValid(thisActor)) return;

    // This should never be invalid because the player is the one calling this function.
    const ACharacter* thisPlayer = Cast<ACharacter>(thisActor);
    if (!IsValid(thisPlayer)) return;
    
    if (!IsValid(targetInventory)) targetInventory = this;
    if (targetInventory == this)
    {
        const APlayerState* thisPlayerState = thisPlayer->GetPlayerState();
        UE_LOG(LogTemp, Display, TEXT("Player '%d' tried to request their own inventory, or target inventory was invalid"),
            IsValid(thisPlayerState) ? thisPlayerState->GetPlayerId() : 0);
        return;
    }
    
    // Validate the components, and ensure it isn't another player.
    AActor* ownerActor = targetInventory->GetOwner();
    if (!IsValid(ownerActor)) return; // Should never happen but doesn't let the server crash

    // If this cast comes back valid, the owner the player is trying to access is owned
    // by another player. This should NEVER be allowed.
    const ACharacter* playerRef = Cast<ACharacter>(ownerActor);
    if (IsValid(playerRef))
    {
        const APlayerState* thisPlayerState   = thisPlayer->GetPlayerState();
        const APlayerState* ownerState        = playerRef->GetPlayerState();
        UE_LOG(LogTemp, Warning, TEXT("Player '%d' tried to request the inventory of Player '%d'!!!"),
            IsValid(thisPlayerState) ? thisPlayerState->GetPlayerId() : 0,   // The player running the func
            IsValid(ownerState)      ? ownerState->GetPlayerId()      : 0 ); // The player being accessed
    }

    // at this point we know both references and their inventories are valid
    // and the player isn't accessing another player's inventory.

    // If the inventory's actor is owned, the player can't use it. THis means that it's already in use.
    if (IsValid(ownerActor->GetOwner()))
    {
        const APlayerState* thisPlayerState = thisPlayer->GetPlayerState();
        UE_LOG(LogTemp, Display, TEXT("Player '%d' tried to access the inventory of '%s', but it is already in use."),
            IsValid(thisPlayerState) ? thisPlayerState->GetPlayerId() : 0,   // The player running the func
            *GetOwner()->GetName());

        //TODO - Send a message to the player notifying them that the inventory is in use by another player.
        
    }

    // Set the accessing player as the owner of the inventory's actor.
    // This should enable replication to the player who is using the inventory.
    ownerActor->SetOwner(thisActor);
    FString ownerActorName = "NULL";
    if (IsValid(ownerActor->GetOwner()))
        ownerActorName = ownerActor->GetOwner()->GetName();
    UE_LOG(LogTemp, Display, TEXT("%s(%s): My owner is now %s"),
        *GetName(), GetOwner()->HasAuthority()?TEXT("SRV"):TEXT("CLI"), *ownerActorName);
    
}

/**
 * Called when the player has opened a new inventory (such as loot or
 * a treasure chest) and needs the information for the first time.
 */
void UInventoryComponent::Server_StopUseOtherInventory_Implementation(UInventoryComponent* otherInventory)
{
    if (IsValid(otherInventory))
    {
        AActor* ownerActor = otherInventory->GetOwner();
        if(IsValid(ownerActor))
        {
            ownerActor->SetOwner(nullptr);
            FString ownerActorName = "NULL";
            if (IsValid(ownerActor->GetOwner()))
                ownerActorName = ownerActor->GetOwner()->GetName();
            UE_LOG(LogTemp, Display, TEXT("%s(%s): My owner is now %s"),
                *GetName(), GetOwner()->HasAuthority()?TEXT("SRV"):TEXT("CLI"), *ownerActorName);
        }
    }
}


/****************************************
 * REPLICATION
***************************************/

void UInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> & OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // Update to owner only. When someone opens something like a storage chest, they'll be set to the owner.
    DOREPLIFETIME(UInventoryComponent, m_inventorySlots);
    DOREPLIFETIME(UInventoryComponent, m_equipmentSlots);
}