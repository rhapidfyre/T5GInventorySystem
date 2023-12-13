
#include "InventorySystemGlobals.h"



/**
 * Checks if the slots contain the same item with the same durability
 * @param SlotOne The first inventory slot to compare
 * @param SlotTwo The second inventory slot being compared
 * @return True if the items have the same durability and name
 */
bool UInventorySystem::IsSameItem(FStInventorySlot& SlotOne, FStInventorySlot& SlotTwo)
{
	if (UItemSystem::isSameItemName(SlotOne.ItemName, SlotTwo.ItemName))
	{
		if (SlotOne.GetItemData().MaxDurability > 0.f)
		{
			return SlotOne.SlotDurability == SlotTwo.SlotDurability;
		}
		return true;
	}
	return false;
}

/**
 * Like 'IsSameItem', but checks for slot quantity and custom stats
 * @param SlotOne The first inventory slot to compare
 * @param SlotTwo The second inventory slot being compared
 * @return True if the items have the same durability, quantity, name, and stats
 */
bool UInventorySystem::IsExactSameItem(FStInventorySlot& SlotOne, FStInventorySlot& SlotTwo)
{
	if (UItemSystem::isSameItemName(SlotOne.ItemName, SlotTwo.ItemName))
	{
		if (SlotOne.GetItemData().MaxDurability > 0.f)
		{
			return SlotOne.SlotDurability == SlotTwo.SlotDurability;
		}
		return true;
	}
	return false;
}