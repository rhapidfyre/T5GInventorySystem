
// ReSharper disable CppUE4CodingStandardNamingViolationWarning
#include "InventoryComponent.h"

#include "InterchangeResult.h"
#include "PickupActorBase.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h" // Used for replication


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
        UE_LOG(LogEngine, Warning, TEXT("%s(%s): Data Table Not Found"),
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
    //UE_LOG(LogTemp, Display, TEXT("InventoryComponent: Constructor"));
    // Inventory Component doesn't need to tick
    // It will be updated as needed
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
	SetAutoActivate(true);
}

void UInventoryComponent::InitializeInventory()
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): InitializeInventory()"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    //UE_LOG(LogTemp, Display, TEXT("InventoryComponent: InitializeInventory"));
    // Ensure the number of inventory slots is valid. If we added an inventory system,
    // it should at least have *one* slot at minimum. Otherwise, wtf is the point?
    if (NumberOfInvSlots < 1) NumberOfInvSlots = 6;
    
    m_inventorySlots.Empty();
    m_equipmentSlots.Empty();
    m_notifications.Empty();
    
    // Setup Inventory Slots
    for (int i = 0; i < NumberOfInvSlots; i++)
    {
        FStInventorySlot newSlot;
        newSlot.slotType = EInventorySlotType::GENERAL;

        // Allow the assignment of hotkeys
        if (HotkeyInputs.IsValidIndex(i))
        {
            if (IsValid(HotkeyInputs[i]))
            {
                newSlot.inputAction = HotkeyInputs[i];
            }
        }

        m_inventorySlots.Add(newSlot);
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
                    newSlot.slotType  = EInventorySlotType::EQUIP;
                    newSlot.equipType = EligibleEquipmentSlots[i];
                    //UE_LOG(LogTemp, Display, TEXT("Adding new equipment slot @ %d"), i);
                    m_equipmentSlots.Add(newSlot);
                }
            }
        }
    }
    
    if (IsValid(GetOwner()))
    {
        if (GetOwner()->HasAuthority())
        {
            while (!StartingItems.IsEmpty())
            {
                if (StartingItems.IsValidIndex(0))
                {
                    // Determine the starting item by item name given
                    int slotNum         = StartingItems[0].startingSlot;
                    bool isEquipSlot    = StartingItems[0].equipType != EEquipmentSlotType::NONE;
                    FName newItemName   = UItemSystem::getItemName(StartingItems[0].itemData);
                    FStItemData newItem = UItemSystem::getItemDataFromItemName(newItemName);
                    
                    if(newItem.currentDurability != StartingItems[0].itemData.currentDurability)
                        newItem.currentDurability = StartingItems[0].itemData.currentDurability;
                    
                    if(newItem.itemRareness != StartingItems[0].itemData.itemRareness)
                        newItem.itemRareness = StartingItems[0].itemData.itemRareness;

                    if (isEquipSlot)
                    {
                        if (slotNum < 0) slotNum = getEquipmentSlotNumber(StartingItems[0].equipType);
                        if (slotNum >= 0)
                        {
                            m_equipmentSlots[slotNum].itemData = newItem;
                            m_equipmentSlots[slotNum].slotQuantity = StartingItems[0].quantity;
                        }
                    }
                    else
                    {
                        //int numItemsFailed = addItemFromExistingToSlot(newItem, slotNum, StartingItems[0].slotQuantity, false, false, false);
                        int itemsAdded = addItemFromExisting(newItem, StartingItems[0].quantity, false, false, false);
                        if (itemsAdded < 1)
                        {
                            UE_LOG(LogTemp, Warning, TEXT("%s(%s): Item(s) failed to add (StartingItem = %s)"),
                                *GetName(), GetOwner()->HasAuthority()?TEXT("SRV"):TEXT("CLI"), *newItemName.ToString());
                        }
                    }

                    StartingItems.RemoveAt(0);
                }
            }
        }
    }

}

void UInventoryComponent::OnComponentCreated()
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): OnComponentCreated()"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    //UE_LOG(LogTemp, Display, TEXT("InventoryComponent: OnComponentCreated"));
    Super::OnComponentCreated();
    RegisterComponent();
    InitializeInventory();
    if (bVerboseOutput) bShowDebug = true;
    m_bIsInventoryReady = true;
}

FStInventorySlot UInventoryComponent::getInventorySlotData(int slotNumber)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): getInventorySlotData(%d)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), slotNumber);
    }
    //UE_LOG(LogTemp, Display, TEXT("InventoryComponent: getInventorySlotData"));
    if (isValidInventorySlot(slotNumber))
    {
        if (!isEmptySlot(slotNumber))
        {
            return m_inventorySlots[slotNumber];
        }
    }
    return FStInventorySlot(); // Return an empty inventory slot
}

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
        if (isEquipment)
        {
            FStInventorySlot equipSlot = getEquipmentSlot(slotNumber);
            if ( m_equipmentSlots.IsValidIndex(slotNumber) )
            {
                return m_equipmentSlots[slotNumber].itemData;
            }
        }
        else
        {
            FStInventorySlot invSlot = getInventorySlot(slotNumber);
            if ( m_inventorySlots.IsValidIndex(slotNumber) )
            {
                return m_inventorySlots[slotNumber].itemData;
            }
        }
    }
    return FStItemData();
}

EInventorySlotType UInventoryComponent::getInventorySlotType(int slotNumber)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): getInventorySlotType(%d)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), slotNumber);
    }
    //UE_LOG(LogTemp, Display, TEXT("InventoryComponent: getInventorySlotType"));
    if (isValidInventorySlot(slotNumber))
        return m_inventorySlots[slotNumber].slotType;
    return EInventorySlotType::NONE;
}

EEquipmentSlotType UInventoryComponent::getEquipmentSlotType(int slotNumber)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): getEquipmentSlotType(%d)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), slotNumber);
    }
    if (isValidEquipmentSlot(slotNumber))
        return m_equipmentSlots[slotNumber].equipType;
    return EEquipmentSlotType::NONE;
}

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
        if (isValidInventorySlot(i))
        {
            if (getItemNameInSlot(i) == itemName)
                total += m_inventorySlots[i].slotQuantity;
        }
    }
    return 0;
}

int UInventoryComponent::getFirstEmptySlot()
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): getFirstEmptySlot()"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    //UE_LOG(LogTemp, Display, TEXT("InventoryComponent: getFirstEmptySlot"));
    for (int i = 0; i < getNumInventorySlots(); i++)
    {
        if (isEmptySlot(i))
            return i;
    }
    return -1;
}

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

float UInventoryComponent::getWeightInSlot(int slotNumber)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): getWeightInSlot(%d)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), slotNumber);
    }
    const FStItemData itemData = getItemInSlot(slotNumber);
    if (UItemSystem::getItemDataIsValid(itemData))
    {
        const float weight = UItemSystem::getItemWeight(itemData);//slotItem->getItemWeight();
        return (weight * getQuantityInSlot(slotNumber));
    }
    return 0.0f;
}

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
        FStItemData itemData = getItemInSlot(i);
        if (UItemSystem::getItemDataIsValid(itemData))
        {
            if (itemData.properName == itemName)
            {
                foundSlots.Add(i);
            }
        }
    }
    return foundSlots;
}

FStInventorySlot UInventoryComponent::getInventorySlot(int slotNumber)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): getInventorySlot(%d)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), slotNumber);
    }
    if (isValidInventorySlot(slotNumber))
        return m_inventorySlots[slotNumber];
    return FStInventorySlot();
}

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
        for (int i = 0; i < getNumEquipmentSlots(); i++)
        {
            const FStInventorySlot equipSlot = getEquipmentSlot(i);
            if (equipSlot.equipType == equipEnum)
            {
                return i;
            }
        }
    }
    return -1;
}

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
            const FStInventorySlot equipSlot = getEquipmentSlot(i);
            if (equipSlot.equipType == equipEnum)
            {
                return equipSlot; // Returns a copy of the equipment slot
            }
        }
    }
    return FStInventorySlot();
}

FStInventorySlot UInventoryComponent::getEquipmentSlot(int slotNumber)
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

bool UInventoryComponent::isValidInventorySlot(int slotNumber)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): isValidInventorySlot(%d)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), slotNumber);
    }
    return (m_inventorySlots.IsValidIndex(slotNumber));
}
bool UInventoryComponent::isValidEquipmentSlot(int slotNumber)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): isValidEquipmentSlot(%d)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), slotNumber);
    }
    return (m_equipmentSlots.IsValidIndex(slotNumber));
}
bool UInventoryComponent::isValidEquipmentSlotEnum(EEquipmentSlotType equipSlot)
{
    //UE_LOG(LogTemp, Display, TEXT("InventoryComponent: isValidEquipmentSlotEnum"));
    for (int i = 0; m_equipmentSlots.Num(); i++)
    {
        if (isValidEquipmentSlot(i))
        {
            if (m_equipmentSlots[i].equipType == equipSlot)
                return true;
        }
    }
    return false;
}

bool UInventoryComponent::isEmptySlot(int slotNumber, bool isEquipment)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): isEmptySlot(%d, %s)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), slotNumber, isEquipment?TEXT("TRUE"):TEXT("FALSE"));
    }
    return (getQuantityInSlot(slotNumber, isEquipment) < 1);
}

bool UInventoryComponent::isEquipmentSlotEmpty(EEquipmentSlotType equipSlot)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): getFirstEmptySlot(%s)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), *UEnum::GetValueAsString(equipSlot));
    }
    const int slotNum = getEquipmentSlotNumber(equipSlot);
    if (slotNum >= 0)
    {
        if (isValidEquipmentSlot(slotNum))
            return (m_equipmentSlots[slotNum].slotQuantity < 1);
    }
    return false;
}

FName UInventoryComponent::getItemNameInSlot(int slotNumber, bool isEquipment)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): getItemNameInSlot(%d, %s)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), slotNumber, isEquipment?TEXT("TRUE"):TEXT("FALSE"));
    }
    const FStItemData itemData = getItemInSlot(slotNumber, isEquipment);
    return UItemSystem::getItemName(itemData);
}

int UInventoryComponent::getQuantityInSlot(int slotNumber, bool isEquipment)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): getQuantityInSlot(%d, %s)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), slotNumber, isEquipment?TEXT("TRUE"):TEXT("FALSE"));
    }
    if (isEquipment)
    {
        if (m_equipmentSlots.IsValidIndex(slotNumber))
            return m_equipmentSlots[slotNumber].slotQuantity;
        return 0;
    }
    if (m_inventorySlots.IsValidIndex(slotNumber))
    {
        return m_inventorySlots[slotNumber].slotQuantity;
    }
    return 0;
}

int UInventoryComponent::addItemByName(
    FName itemName, int quantity, bool overflowAdd, bool overflowDrop, bool showNotify)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): addItemByName(%s)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), *itemName.ToString());
    }
    if (isInventoryReady() && UItemSystem::getItemNameIsValid(itemName))
    {
        // Adds the item to the first applicable slot. If the add fills the slot, it will only add what it can fit.
        return addItemByNameToSlot(itemName, -1, quantity, overflowAdd, overflowDrop, showNotify);
    }
    return -1;//failure
}

int UInventoryComponent::addItemByNameToSlot(
    FName itemName, int slotNumber, int quantity, bool overflowAdd, bool overflowDrop, bool showNotify)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): addItemByNameToSlot(%s, %d)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), *itemName.ToString(), slotNumber);
    }
    if (!isValidInventorySlot(slotNumber)) slotNumber = -1;
    if (quantity < 1) quantity = 1;
    if ( itemName != UItemSystem::getInvalidName() )
    {
        int remainingItems = quantity;
        bool encounteredError;

        // Continue adding the item until we hit an error or there are no items left to be added
        // This does not loop if 'overflowAdd' is set to FALSE
        do
        {
            const int itemsAdded = addItem(itemName, remainingItems, showNotify, slotNumber);
            remainingItems -= itemsAdded;

            if (showNotify) sendNotification(itemName, itemsAdded, true);
                
            // Negative return indicates failure to add (full inventory, etc)
            encounteredError = (itemsAdded < 0); // Stop the loop
            
        } while (overflowAdd && !encounteredError && remainingItems > 0 && slotNumber < 0);

        // We failed to add the item entirely
        if (remainingItems == INT_MAX && encounteredError)
        {
            return -3;
        }
            
        // Drop any remaining items on the ground
        if (overflowDrop)
        {
            // Spawn UActor for pickup item
            // not yet implemented
        }
        return (quantity - remainingItems);
    }
    return -1;
}

int UInventoryComponent::addItemFromExisting(UInventoryComponent* fromInventory,
    int fromSlot, int quantity, bool overflowAdd, bool overflowDrop, bool showNotify)
{
    UE_LOG(LogTemp, Display, TEXT("InventoryComponent: addItemFromExisting"));
    // If pointer is null, default to this inventory (we're moving something within the same inventory)
    if (fromInventory == nullptr) fromInventory = this;
    return addItemFromExistingToSlot(fromInventory, fromSlot, -1, quantity, overflowDrop, showNotify);
}

int UInventoryComponent::addItemFromExistingWithRemainder(UInventoryComponent* fromInventory,
    int fromSlot, int quantity, int& remainder, bool overflowAdd, bool overflowDrop, bool showNotify)
{
    UE_LOG(LogTemp, Display, TEXT("InventoryComponent: addItemFromExistingWithRemainder"));
    // If pointer is null, default to this inventory (we're moving something within the same inventory)
    if (fromInventory == nullptr) fromInventory = this;
    return addItemFromExistingToSlotWithRemainder(fromInventory, fromSlot, -1, quantity, remainder, overflowDrop, showNotify);
}

int UInventoryComponent::addItemFromExisting(FStItemData newItem, int quantity, bool overflowAdd, bool overflowDrop, bool showNotify)
{
    UE_LOG(LogTemp, Display, TEXT("InventoryComponent: addItemFromExisting (overload)"));
    if (!UItemSystem::getItemDataIsValid(newItem)) return false;
    int remainder;
    return addItemFinalWithRemainder(newItem, -1, quantity, remainder, overflowDrop, showNotify);
}

int UInventoryComponent::addItemFromExistingToSlot(UInventoryComponent* fromInventory,
    int fromSlot, int toSlot, int quantity, bool overflowAdd, bool overflowDrop, bool showNotify)
{
    UE_LOG(LogTemp, Display, TEXT("InventoryComponent: addItemFromExistingToSlot"));
    int remainder;
    return addItemFromExistingToSlotWithRemainder(fromInventory, fromSlot, toSlot, quantity, remainder, overflowAdd, overflowDrop, showNotify);
}

int UInventoryComponent::addItemFromExistingToSlot(FStItemData existingItem, int toSlot, int quantity, bool overflowAdd, bool overflowDrop, bool showNotify)
{
    UE_LOG(LogTemp, Display, TEXT("InventoryComponent: addItemFromExistingToSlot (overload)"));
    int remainder;
    return addItemFinalWithRemainder(existingItem, toSlot, quantity, remainder, overflowAdd, overflowDrop, showNotify);
}

int UInventoryComponent::addItemFromExistingToSlotWithRemainder(UInventoryComponent* fromInventory,
    int fromSlot, int toSlot, int quantity, int& remainder, bool overflowAdd, bool overflowDrop, bool showNotify)
{
    UE_LOG(LogTemp, Display, TEXT("InventoryComponent: addItemFromExistingToSlotWithRemainder"));
    if (quantity < 1) quantity = 1;
    
    // If from inventory is not provided (nullptr), then it's this inventory
    if (fromInventory == nullptr) fromInventory = this;

    // In order to come into our inventory, it can't be in an equipment slot, so whether or not it's equipment is irrelevant.
    // Equipment can only be donned/doffed from the inventory of the person using the equipment.
    const int existingQty = fromInventory->getInventorySlot(fromSlot).slotQuantity;
    const FStItemData existingItem = fromInventory->getItemInSlot(fromSlot);
    if (UItemSystem::getItemDataIsValid(existingItem))
    {
        // Simple ternary to ensure new quantity is not greater than the available quantity of the existing item.
        quantity = quantity > existingQty ? existingQty : quantity;
        return addItemFinalWithRemainder(existingItem, toSlot, quantity, remainder, overflowAdd, overflowDrop, showNotify);
        
    }
    return -1;
}

int UInventoryComponent::addItemFinal(FStItemData newItem, int toSlot, int quantity, bool overflowAdd, bool overflowDrop, bool showNotify)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): addItemFinal()"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    int remainder;
    return addItemFinalWithRemainder(newItem, toSlot, quantity, remainder, overflowAdd, overflowDrop, showNotify);
}


int UInventoryComponent::addItemFinalWithRemainder(FStItemData newItem, int toSlot, int quantity, int& remainder, bool overflowAdd, bool overflowDrop, bool showNotify)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): addItemFinalWithRemainder()"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    if (quantity < 1) quantity = 1;
    int remainingItems = quantity;
    bool encounteredError;
    int totalItemsAdded = 0;

    // Continue adding the item until we hit an error or there are no items left to be added
    // This does not loop if 'overflowAdd' is set to FALSE
    do
    {
        // Negative return indicates failure to add (full inventory, etc)
        const int itemsAdded = addItem(newItem, quantity, showNotify, toSlot);
        encounteredError = itemsAdded < 0; // Stop the loop
        
        if (!encounteredError)
        {
            totalItemsAdded += itemsAdded;
            remainingItems = remainingItems - itemsAdded;
            // Add item to pending notifications map
            if (showNotify)
            {
                setNotifyItem(newItem, itemsAdded);
            }
        }
        
    } while (overflowAdd && !encounteredError && remainingItems > 0);

    // We failed to add the item entirely
    if (remainingItems == quantity && encounteredError)
    {
        const FString outMsg = (GetOwner())->GetName() + ": Failed to add item (reference object did not exist)";
        remainder = quantity;
        return -3;
    }
            
    // Drop any remaining items on the ground
    if (overflowDrop)
    {
        // Spawn UActor for pickup item, where quantity is equal to the remaining items after add
    }
    remainder = (quantity - totalItemsAdded);
    return totalItemsAdded;
    
}


bool UInventoryComponent::donEquipment(UInventoryComponent* fromInventory, int fromSlot, EEquipmentSlotType eSlot)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): donEquipment()"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    if (!IsValid(fromInventory)) fromInventory = this;
    
    // Per UE coding convention, "IsValid" should be used when possible over "nullptr" check.
    FStItemData itemData = fromInventory->getItemInSlot(fromSlot, false);
    if (!UItemSystem::getItemDataIsValid(itemData))
        return false;

    // If the parameter was not a specific equipment slot, find one that works.
    EEquipmentSlotType equipSlot = eSlot;
    if (eSlot == EEquipmentSlotType::NONE)
    {
        // Get all slots this item can be equipped into as an array
        // Attempt to add the item to each slot until it fits or fails
        TArray<EEquipmentSlotType> eligibleSlots = UItemSystem::getItemEquipSlots(itemData);
        for (int i = 0; i < eligibleSlots.Num(); i++)
        {
            FStInventorySlot foundSlot = getEquipmentSlot(eligibleSlots[i]);
            // If the found slot is valid and matches, then we equip it.
            if (foundSlot.equipType != EEquipmentSlotType::NONE)
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
        int slotNumber = getEquipmentSlotNumber(eSlot);
        if (!isValidEquipmentSlot(slotNumber))                  return false;
        if (getEquipmentSlot(slotNumber).equipType != eSlot)    return false;

        int itemsRemoved = fromInventory->removeItemFromSlot(fromSlot, INT_MAX, false, false, false, true);
        //addItemFromExistingToSlot(itemData, slotNumber, itemsRemoved, false, false, false);
        m_equipmentSlots[slotNumber].itemData = itemData;
        m_equipmentSlots[slotNumber].slotQuantity = itemsRemoved;
        InventoryUpdate(slotNumber, true);
        return true;
    }
    
    return false;
}

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
                int itemsRemoved = removeItemFromSlot(fromSlot, INT_MAX, true, false, false, true);
                toInventory->addItemFromExistingToSlot(itemData, slotNumber, itemsRemoved, false, false, false);
            }
        }
    }
    return false;
}


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
        if (isEquipment && isValidEquipmentSlot(slotNumber) || !isEquipment && isValidInventorySlot(slotNumber))
        {
            const FStItemData existingItem = getItemInSlot(slotNumber, isEquipment);
            if (UItemSystem::getItemDataIsValid(existingItem))
            {
            
                // Can't remove an item that doesn't exist.
                const FName itemName = UItemSystem::getItemName(existingItem);
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
                        m_equipmentSlots[slotNumber].slotQuantity = 0;
                        m_equipmentSlots[slotNumber].itemData = FStItemData();
                    }
                    else
                    {
                        m_inventorySlots[slotNumber].slotQuantity = 0;
                        m_inventorySlots[slotNumber].itemData = FStItemData();
                    }
                }
                // Only a portion of the items are being removed.
                else
                {
                    if (isEquipment) m_equipmentSlots[slotNumber].slotQuantity -= qty;
                    else             m_inventorySlots[slotNumber].slotQuantity -= qty;
                }
            
                if (showNotify) sendNotification(itemName, qty, false);
            
                // Removes the item from memory
                if (getQuantityInSlot(slotNumber) < 1)
                {
                    if (isEquipment) m_equipmentSlots[slotNumber].itemData = FStItemData();
                    else             m_inventorySlots[slotNumber].itemData = FStItemData();
                }

                InventoryUpdate(slotNumber, isEquipment);
                return qty;
            }
        }
    }
    return -1;
}

int UInventoryComponent::removeItemByQuantity(FName itemName, int quantity, bool dropToGround, bool removeAll)
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
            const int numRemoved = removeItemFromSlot(i, qty, dropToGround, removeAll);
            qty -= numRemoved;

            if (qty < 1) break;
        }
        
        // If current quantity is not equal to the requested quantity, we succeeded
        if (qty != quantity) return qty;
        
    }
    return -1;
}

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
    
    if (!fromInventory->isValidInventorySlot(fromSlotNum)) return false;
    if (!isValidInventorySlot(toSlotNum)) toSlotNum = -1;
    if (quantity < 1) quantity = 1;

    // Remember which items were in which slots
    FStItemData fromItem = fromInventory->getItemInSlot(fromSlotNum, fromEquipmentSlot);
    int fromQuantity     = fromInventory->getQuantityInSlot(fromSlotNum, fromEquipmentSlot);
    FStItemData toItem   = getItemInSlot(toSlotNum, isEquipmentSlot);

    // If the FROM item isn't valid, this is not a valid swap. Otherwise.... wtf are they swapping?
    if (!UItemSystem::getItemDataIsValid(fromItem))
    {
        return false;
    }

    // Ensures the slot matches what the client says it is and that the FROM slot is a valid slot type
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
    bool destinationValid = false;
    if (isEquipmentSlot)
        destinationValid = getEquipmentSlotType(toSlotNum) != EEquipmentSlotType::NONE;
    else
    {
        if (toSlotNum < 0) toSlotNum = getFirstEmptySlot();
        destinationValid = getInventorySlotType(toSlotNum) != EInventorySlotType::NONE;
    }
    if (!destinationValid)
    {
        return false;
    }

    // Check if there's an existing item in the destination slot
    if (UItemSystem::getItemDataIsValid(toItem))
    {

        // Check if items can be stacked. This will be the easiest, fastest way to manage this call.
        if (UItemSystem::isSameItemData(toItem, fromItem))
        {
            if (fromInventory->decreaseQuantityInSlot(fromSlotNum, quantity, showNotify))
            {
                int itemsAdded = increaseQuantityInSlot(toSlotNum, quantity, showNotify);
                remainder = (quantity - itemsAdded);
                return true;   
            }
        }
        
    }

    // We have one last problem to look for; If one of the slots (or both) is an equipment slot, we obviously can't
    // swap something into it that is not a matching equipment type. So we need to check that the types match.
    // i.e
    //   You can't take a sword out of primary, and swap a carrot from the inventory into the primary slot.
    if ((fromEquipmentSlot || isEquipmentSlot) && UItemSystem::getItemDataIsValid(toItem))
    {
        // Gets the eligible slot types from the existing item (the item in the origination slot)
        EEquipmentSlotType ourSlotType = getEquipmentSlotType(toSlotNum);
        const TArray<EEquipmentSlotType> slotTypes = UItemSystem::getItemEquipSlots(toItem);
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
    int moveQuantity = quantity > fromQuantity ? fromQuantity : quantity;

    // Step 2: Remember the item we used to have in this slot.
    FStItemData oldItem = getItemInSlot(toSlotNum, isEquipmentSlot); // Returns -1 if this slot was empty

    // If the items match or the destination slot is empty, it will stack.
    // Otherwise it will swap the two slots.
    bool isSameItem = UItemSystem::isSameItemData(fromItem, toItem); 
    if (isSameItem || !UItemSystem::getItemDataIsValid(toItem))
    {
        if (isEquipmentSlot)
        {
            EEquipmentSlotType eSlot = getEquipmentSlotType(toSlotNum);
            return donEquipment(this, fromSlotNum, eSlot);
        }
        if (fromInventory->decreaseQuantityInSlot(fromSlotNum, moveQuantity, showNotify) > 0)
        {
            int newSlot = addItemFromExistingToSlot(fromItem, toSlotNum, moveQuantity, true, false, showNotify);
            if (newSlot < 0) return false;
            return true;
        }
    }
    else
    {
        if (fromInventory->decreaseQuantityInSlot(fromSlotNum, fromQuantity, fromEquipmentSlot, showNotify) > 0)
        {
            int newSlot = addItemFromExistingToSlot(fromItem, toSlotNum, fromQuantity, true, false, showNotify);
            if (newSlot < 0)
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

void UInventoryComponent::OnRep_InventorySlotUpdated()
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): OnRep_InventorySlotUpdated()"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    FString ownerName = "NULL";
    if (IsValid(GetOwner()->GetOwner()))
        ownerName = GetOwner()->GetOwner()->GetName();
    
    if (bVerboseOutput)
    {
        UE_LOG(LogTemp, Display, TEXT("%s(%s): OnInventoryUpdated.Broadcast(-1, false)"),
            *GetName(), GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    OnInventoryUpdated.Broadcast(-1, false);
}

void UInventoryComponent::OnRep_EquipmentSlotUpdated()
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): OnRep_EquipmentSlotUpdated()"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    FString ownerName = "NULL";
    if (IsValid(GetOwner()->GetOwner()))
        ownerName = GetOwner()->GetOwner()->GetName();
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("%s(%s): OnInventoryUpdated.Broadcast(-1, true)"),
            *GetName(), GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    OnInventoryUpdated.Broadcast(-1, true);
}

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
    
    const FStItemData existingItem = fromInventory->getItemInSlot(fromSlotNum);
    if (UItemSystem::getItemDataIsValid(existingItem))
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
            int itemsAdded;
            if (toSlotNum < 0)
            {
                itemsAdded = addItemFromExisting(existingItem, qty,
                    false, false, showNotify);
            }
            else
            {
                itemsAdded = addItemFinal(existingItem, toSlotNum, qty, false, false, showNotify);
            }
            
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
            else
            {
                // We failed to add the items. Replace the originals.
                fromInventory->increaseQuantityInSlot(fromSlotNum, qty, showNotify);
            }
            
        }
    }
    return false;
}

int UInventoryComponent::addItem(FName itemName, int quantity, bool showNotify, int& slotNumber)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): addItem()"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    if (quantity < 1) quantity = 1;
    const FStItemData newItem = UItemSystem::getItemDataFromItemName(itemName);
    return addItem(newItem, quantity, false, slotNumber);
}

int UInventoryComponent::addItem(FStItemData newItem, int quantity, bool showNotify, int& slotNumber)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): addItem(overload)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    if (quantity < 1) quantity = 1;
    // Create a new object that is a copy of the item being referenced
    // A negative slot number means to add it to the first available slot.
    if (!UItemSystem::getItemDataIsValid(newItem))
    {
        return -1;
    }

    // Insert into first available slot, if slot number given is not valid.
    FName itemName = UItemSystem::getItemName(newItem);
    if (!isValidInventorySlot(slotNumber))
    {
        if (UItemSystem::isItemStackable(newItem))
        {
            TArray<int> existingSlots = getSlotsContainingItem(itemName);
            for (int i = 0; i < existingSlots.Num(); i++)
            {
                if (getQuantityInSlot(existingSlots[i]) < UItemSystem::getMaximumStackSize(itemName))
                {
                    slotNumber = existingSlots[i];
                    break;
                }
            }
        }

        // If slotNum is still -1, we need to find the first empty slot.
        if (slotNumber < 0)
        {
            slotNumber = getFirstEmptySlot();
        }

        // If it is STILL -1, we have no room for this item.
        if (slotNumber < 0)
        {
            return -1;
        }
        
    }

    // Setup our addition quantity
    int maxQuantity = UItemSystem::getItemMaxStackSize(newItem);
    int newQuantity = getQuantityInSlot(slotNumber) + abs(quantity);

    // If an item exists at the destination slot, we will try to add to it.
    FStItemData existingItem = getItemInSlot(slotNumber);
    
    if (UItemSystem::getItemDataIsValid(existingItem))
    {
        FName existingName = UItemSystem::getItemName(existingItem);
        
        // if the item matches, we're good to go.
        if (UItemSystem::isSameItemData(existingItem, newItem))
        {
            if (!UItemSystem::isStackable(existingName))
            {
                UE_LOG(LogTemp, Warning, TEXT("(%s) Item is not stackable. Unable to add item."),
                    GetOwner()->HasAuthority()?TEXT("SRV"):TEXT("CLI"));
                return -1;
            }
        }
        else
        {
            slotNumber = getFirstEmptySlot();
            if (slotNumber < 0)
            {
                UE_LOG(LogTemp, Warning, TEXT("(%s) Unable to find an empty slot."),
                    GetOwner()->HasAuthority()?TEXT("SRV"):TEXT("CLI"));
                return -1;
            }
        }
        
    }
    
    // We are go for the addition
    int addQuantity = newQuantity > maxQuantity ? maxQuantity : newQuantity;
    m_inventorySlots[slotNumber].slotQuantity = addQuantity;
    if (!UItemSystem::getItemDataIsValid(existingItem))
        m_inventorySlots[slotNumber].itemData = newItem;
    if (showNotify)
    {
        sendNotification(UItemSystem::getItemName(newItem), addQuantity, true);
    }
    
    InventoryUpdate(slotNumber);
    return addQuantity;
    
}

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

int UInventoryComponent::increaseQuantityInSlot(int slotNumber, int quantity, int& newQuantity, bool showNotify)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): increaseQuantityInSlot(overload)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    if (quantity < 1) quantity = 1;
    const FStItemData slotItem = getItemInSlot(slotNumber);
    if (UItemSystem::getItemDataIsValid(slotItem))
    {
        const int maxQty = UItemSystem::getMaximumStackSize(UItemSystem::getItemName(slotItem));
        const int startQty = getQuantityInSlot(slotNumber);
        const int newTotal = startQty + abs(quantity);
        newQuantity = (newTotal >= maxQty) ? maxQty : newTotal;
        const int amountAdded = (newQuantity - startQty);
        m_inventorySlots[slotNumber].slotQuantity = newQuantity;
        if (showNotify)
        {
            sendNotification(getItemNameInSlot(slotNumber), amountAdded);
        }
        InventoryUpdate(slotNumber);
        return amountAdded;
    }
    return -1;
}

int UInventoryComponent::decreaseQuantityInSlot(int slotNumber, int quantity, int& remainder, bool isEquipment, bool showNotify)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): decreaseQuantityInSlot(overload)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    if (quantity < 1) quantity = 1;
    const FStItemData slotItem = getItemInSlot(slotNumber);
    if (UItemSystem::getItemDataIsValid(slotItem))
    {
        const int startQty = getQuantityInSlot(slotNumber);
        const int tempQty = startQty - abs(quantity);
        remainder = (tempQty >= 0) ? tempQty : 0;
        
        // Since we may have only removed 'n' amount instead of 'quantity', we need to recalculate
        const int amountRemoved = startQty - remainder;
        
        if (isEquipment) m_equipmentSlots[slotNumber].slotQuantity = remainder;
        else             m_inventorySlots[slotNumber].slotQuantity = remainder;
        
        if (showNotify)
        {
            sendNotification(getItemNameInSlot(slotNumber), amountRemoved, false);
        }

        // If nothing is left, remove it altogether.
        if (remainder < 1)
        {
            if (isEquipment)
            {
                m_equipmentSlots[slotNumber].slotQuantity = remainder;
                m_equipmentSlots[slotNumber].itemData     = FStItemData();
            }
            else
            {
                m_inventorySlots[slotNumber].slotQuantity = remainder;
                m_inventorySlots[slotNumber].itemData     = FStItemData();
            }
        }
        
        InventoryUpdate(slotNumber);
        return amountRemoved;
    }
    return -1;
}

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

void UInventoryComponent::resetInventorySlot(int slotNumber)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): resetInventorySlot(%d)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), slotNumber);
    }
    removeItemFromSlot(slotNumber, 1, false,true);
    InventoryUpdate(slotNumber);
}

void UInventoryComponent::InventoryUpdate(int slotNumber, bool isEquipment, bool isAtomic)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): InventoryUpdate(%d)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), slotNumber);
    }
    // Notify delegate so bound functions can trigger
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("%s(%s): OnInventoryUpdated.Broadcast(%d, %s)"),
            *GetName(), GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"),
            slotNumber, isEquipment?TEXT("TRUE"):TEXT("FALSE"));
    }

    // Fire server delegates
    if (isAtomic)
        OnInventoryUpdated.Broadcast(-1, isEquipment);
    else
        OnInventoryUpdated.Broadcast(slotNumber, isEquipment);

    // Trigger client delegates
    Client_InventoryUpdate(isAtomic ? -1 : slotNumber, isEquipment);
}

void UInventoryComponent::Client_InventoryUpdate_Implementation(int slotNumber, bool isEquipment)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): Client_InventoryUpdate_Implementation()"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    OnInventoryUpdated.Broadcast(slotNumber, isEquipment);
}

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
    FStItemData fromItem = fromInventory->getItemInSlot(fromSlotNum);
    if (UItemSystem::getItemDataIsValid((fromItem)))
    {
        // Does an item exist in the destination slot?
        FStItemData toItem = toInventory->getItemInSlot(toSlotNum);
        if (UItemSystem::getItemDataIsValid(toItem) || toSlotNum < 0)
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
                    int slotNum = toInventory->addItemFromExisting(fromInventory, fromSlotNum, moveQuantity, true, overflowDrop, showNotify);
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

bool UInventoryComponent::activateItemInSlot(int slotNumber, bool isEquipment)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): activateItemInSlot(%d)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), slotNumber);
    }
    if (isValidInventorySlot(slotNumber))
    {

        FStItemData itemInSlot = getItemInSlot(slotNumber, isEquipment);
        if (UItemSystem::getItemDataIsValid(itemInSlot))
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
                swapOrStackSlots(this,slotNumber, destinationSlot, 1,
                false, !isEquipment, isEquipment);
            }
            else
            {
                if (decreaseQuantityInSlot(slotNumber,1,isEquipment) > 0)
                {
                    //TODO - Spawn a UActor and activate the item
                    //       Should spawn owned by *this* inventory's owner.
                    return true;
                }
            }
        }
    }
    return false;
}

bool UInventoryComponent::setNotifyItem(FStItemData itemData, int quantityAffected)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): setNotifyItem(%s, %d)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), *itemData.properName.ToString(), quantityAffected);
    }
    if (UItemSystem::getItemDataIsValid(itemData))
    {
        sendNotification(UItemSystem::getItemName(itemData), quantityAffected, quantityAffected > 0);
        return true;
    }
    return false;
}





//---------------------------------------------------------------------------------------------------------------
//---------------------------------- REPLICATION ----------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------



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
            if (!fromInventory->isValidInventorySlot(fromSlot))
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
            if (!toInventory->isValidInventorySlot(toSlot))
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
        const FStItemData originItem = fromInventory->getItemInSlot(fromSlot, isFromEquipSlot);
        if (UItemSystem::getItemDataIsValid(originItem))
        {
            // Remove the items from the origin inventory
            moveQty = fromInventory->decreaseQuantityInSlot(fromSlot,moveQty,true);
            if (moveQty < 1)
            {
                UE_LOG(LogTemp, Error, TEXT("TransferItems(%s): Failed to remove any items from origin inventory."),
                    (GetOwner()->HasAuthority()?TEXT("SRV"):TEXT("CLI")));
                return;
            }
        }
        
        // If decrease was successful, add it to the destination inventory.
        const int itemsAdded = toInventory->addItemFromExistingToSlot(originItem, toSlot, moveQty, false,false,true);
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
            fromInventory->addItemFromExistingToSlot(originItem,fromSlot,moveQty,true,false,false);
        }
    }
}


void UInventoryComponent::Server_DropItemOnGround_Implementation(UInventoryComponent* fromInventory, int fromSlot,
    int quantity, bool isFromEquipSlot)
{
    UE_LOG(LogTemp, Display, TEXT("InventoryComponent: Server_DropItemOnGround_Implementation"));
    if (!IsValid(fromInventory)) fromInventory = this;

    if (fromInventory->getItemInSlot(fromSlot, isFromEquipSlot).properName == UItemSystem::getInvalidName())
        return;
    
    AActor* thatActor = fromInventory->GetOwner();
    if (!IsValid(thatActor)) return; // Just in case something went terribly wrong.

    AActor* thisActor = Cast<AActor>(GetOwner());
    if (!IsValid(thatActor)) return; // Safety catch
    
    // If the inventories belong to different actors, make sure it's not player v. player
    if (thisActor != thatActor)
    {
        AController* thisController = thatActor->GetInstigatorController();
        AController* thatController = thatActor->GetInstigatorController();

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

    ACharacter* thisCharacter = Cast<ACharacter>(thisActor);
    if (IsValid(thisCharacter))
    {
        //const FVector fwdVector = thisPlayer->GetActorUpVector() * (-1);//thisPlayer->GetActorForwardVector();
        //const FVector spawnPosition = thisPlayer->GetActorLocation() - 88; //(fwdVector * 512.0f);
        USkeletalMeshComponent* thisMesh = thisCharacter->GetMesh();
        if (!IsValid(thisMesh)) return;
        
        FTransform spawnTransform(
            FRotator(FMath::RandRange(0.f,359.f),FMath::RandRange(0.f,359.f),FMath::RandRange(0.f,359.f)),
            thisMesh->GetBoneLocation("Root"));
        FActorSpawnParameters spawnParams;
        spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        
        FStItemData itemCopy    = fromInventory->getItemInSlot(fromSlot, isFromEquipSlot);
        int itemQuantity        = fromInventory->getQuantityInSlot(fromSlot, isFromEquipSlot);
        int tossQuantity        = (quantity > itemQuantity) ? itemQuantity : abs(quantity);

        APickupActorBase* pickupItem = GetWorld()->SpawnActorDeferred<APickupActorBase>(
                                                APickupActorBase::StaticClass(), spawnTransform);
        if (IsValid(pickupItem))
        {
            if (fromInventory->decreaseQuantityInSlot(fromSlot, tossQuantity) > 0)
            {
                pickupItem->SetupItemFromData(itemCopy, tossQuantity);
                pickupItem->FinishSpawning(spawnTransform);
            }
        }
        
    }
    
}

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

void UInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> & OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // Update to owner only. When someone opens something like a storage chest, they'll be set to the owner.
    DOREPLIFETIME_CONDITION(UInventoryComponent, m_inventorySlots, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(UInventoryComponent, m_equipmentSlots, COND_OwnerOnly);
}