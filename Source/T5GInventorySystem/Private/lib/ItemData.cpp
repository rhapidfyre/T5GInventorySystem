
#include "lib/ItemData.h"

#include "lib/InventorySlot.h"

UDataTable* UItemSystem::getItemDataTable()
{
    const FSoftObjectPath itemTable = FSoftObjectPath("/T5GInventorySystem/DataTables/DT_ItemData.DT_ItemData");
    UDataTable* dataTable = Cast<UDataTable>(itemTable.ResolveObject());
    if (IsValid(dataTable)) return dataTable;
    return Cast<UDataTable>(itemTable.TryLoad());
}

FStItemData UItemSystem::getItemDataFromItemName(FName itemName)
{
    if (itemName.IsValid())
    {
        if (itemName != getInvalidName())
        {
            if ( const UDataTable* dt = getItemDataTable() )
            {
                if (IsValid(dt))
                { 
                    const FString errorCaught;
                    FStItemData* itemDataPtr = dt->FindRow<FStItemData>(itemName, errorCaught);
                    if (itemDataPtr != nullptr)
                    {
                        return *itemDataPtr;
                    }
                }
            }
        }
    }
    // Return a default item data struct
    return FStItemData();
}

bool UItemSystem::getItemNameIsValid(FName itemName, bool performLookup)
{
    if (itemName.IsNone())
    {
        if (performLookup)
        {
            const FStItemData ItemData = getItemDataFromItemName(itemName);
            if ( const UDataTable* dt = getItemDataTable() )
            {
                if (IsValid(dt))
                { 
                    const FString errorCaught;
                    const FStItemData* itemDataPtr = dt->FindRow<FStItemData>(itemName, errorCaught);
                    return (itemDataPtr != nullptr);
                }
            }
        }
        else
        {
            return (itemName != getInvalidName());
        }
    }
    return false; // Invalid or not found
}

int UItemSystem::getMaximumStackSize(FName itemName)
{
    if (itemName.IsValid())
    {
        UDataTable* dataTable = getItemDataTable();
        if (dataTable)
        {
            FStItemData* itemData = dataTable->FindRow<FStItemData>(itemName, "");
            if (itemData)
            {
                int stackSize = itemData->maxStackSize;
                return ((stackSize > 0) ? stackSize : 1);
            }
        }//dataTable Valid
    }//name IsValid
    return 1;
}

bool UItemSystem::isSameItemName(FName itemOne, FName itemTwo)
{
    if (itemOne.IsValid() && itemTwo.IsValid())
    {
        if (itemOne == itemTwo)
        {
            if (getItemNameIsValid(itemOne) && getItemNameIsValid(itemTwo))
            {
                return true;
            }
        }
    }
    return false;
}

bool UItemSystem::isItemStackable(const FStItemData& itemData)
{
    return (itemData.maxStackSize > 1);
}

bool UItemSystem::GetIsStackable(FName itemName)
{
    if (itemName == getInvalidName()) return false;
    return isItemStackable( getItemDataFromItemName(itemName) );
}

float UItemSystem::getWeight(FName itemName)
{
    if (itemName == getInvalidName()) return 0.01;
    return getItemWeight(getItemDataFromItemName(itemName));
}

TArray<EEquipmentSlotType> UItemSystem::getEquipmentSlots(FName itemName)
{
    TArray<EEquipmentSlotType> retArray;
    if (itemName == getInvalidName()) return retArray;
    return getItemEquipSlots(getItemDataFromItemName(itemName));
}

bool UItemSystem::isItemActivated(const FStItemData& itemData)
{
    return (itemData.itemActivation != EItemActivation::NONE);
}

float UItemSystem::getItemDurability(const FStItemData& itemData)
{
    // New copy of the same item for comparison
    if (itemData.MaxDurability >= 0.f)
    {
        // Master item has a valid durability value
        if (itemData.MaxDurability > 0.f)
        {
            if (itemData.MaxDurability < itemData.MaxDurability)
                return itemData.MaxDurability;
            return itemData.MaxDurability;
        }
    }
    return -1.f;
}

float UItemSystem::getDurability(FName itemName) 
{
    if (itemName == getInvalidName()) return false;
    return (getItemDurability(getItemDataFromItemName(itemName)));
}

bool UItemSystem::isActivated(FName itemName)
{
    if (itemName == getInvalidName()) return false;
    return isItemActivated( getItemDataFromItemName(itemName) );
}

bool UItemSystem::IsSameItem(FStInventorySlot& SlotOne, FStInventorySlot& SlotTwo)
{
    if (isSameItemName(SlotOne.ItemName, SlotTwo.ItemName))
    {
        if (SlotOne.GetItemData().MaxDurability > 0.f)
        {
            return SlotOne.SlotDurability == SlotTwo.SlotDurability;
        }
        return true;
    }
    return false;
}