
#include "lib/ItemData.h"

UDataTable* UItemSystem::getItemDataTable()
{
    const FSoftObjectPath itemTable = FSoftObjectPath("/T5GInventorySystem/DataTables/DT_ItemData.DT_ItemData");
    UDataTable* dataTable = Cast<UDataTable>(itemTable.ResolveObject());
    if (dataTable) return dataTable;
    return Cast<UDataTable>(itemTable.TryLoad());
}

FStItemData UItemSystem::getItemDataFromItemName(FName itemName)
{
    if (itemName.IsValid())
    {
        if (itemName != UItemSystem::getInvalidName())
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
    if (itemName.IsValid())
    {
        if (performLookup)
        {
            const FStItemData itemData = (UItemSystem::getItemDataFromItemName(itemName));
            return (itemData.properName != getInvalidName());
        }
        return (itemName != getInvalidName());
    }
    return false; // Invalid or not found
}

bool UItemSystem::getItemDataIsValid(FStItemData itemData)
{
    return getItemNameIsValid(itemData.properName, false);
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

bool UItemSystem::isItemStackable(FStItemData itemData)
{
    return (itemData.maxStackSize > 1);
}

bool UItemSystem::isStackable(FName itemName)
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

bool UItemSystem::isItemActivated(FStItemData itemData)
{
    return (itemData.itemActivation != EItemActivation::NONE);
}

float UItemSystem::getItemDurability(FStItemData itemData)
{
    if (itemData.currentDurability > 0.0f)
        return (itemData.currentDurability >= 1.0f ? 1.0f : itemData.currentDurability);
    return (itemData.currentDurability >= 0.0f ? itemData.currentDurability : -1.0f);
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

bool UItemSystem::isSameItemData(FStItemData itemOne, FStItemData itemTwo)
{
    if (isSameItemName(itemOne.properName, itemTwo.properName))
    {
        // We'll have to do more verification later but this works for now
        // Check durability, weight, craft quality, etc.
        return true;
    }
    return false;
}