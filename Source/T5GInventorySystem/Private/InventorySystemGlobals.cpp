
#include "InventorySystemGlobals.h"


/**
 * 
 * @param ItemOne 
 * @param ItemTwo 
 * @return 
 */
bool UInventorySystem::GetIsSameItem(const FStItemData& ItemOne, const FStItemData& ItemTwo) const
{
	if (IsValid(ItemOne.Data) && IsValid(ItemTwo.Data))
	{
		return (ItemOne.Data == ItemTwo.Data);
	}
	return false;
}

/**
 * 
 * @param ItemOne 
 * @param ItemTwo 
 * @return 
 */
bool UInventorySystem::GetIsSameExactItem(const FStItemData& ItemOne, const FStItemData& ItemTwo) const
{
	return ItemOne == ItemTwo;
}

/**
 * 
 * @param SlotOne 
 * @param SlotTwo 
 * @return 
 */
bool UInventorySystem::GetIsSameSlot(const FStInventorySlot& SlotOne, const FStInventorySlot& SlotTwo) const
{
	if (SlotOne.ContainsValidItem())
	{
		if (SlotTwo.ContainsValidItem())
		{
			if (SlotOne.ContainsItem(SlotTwo.SlotItemData))
			{
				return (SlotOne.SlotItemData == SlotTwo.SlotItemData);
			}
		}
	}
	// Slot one is empty, slot two is not.
	else if (SlotTwo.ContainsValidItem())
	{
		// Can't possibly be the same slot.
		return false;
	}
	// Both slots are empty
	else
	{
		return (SlotOne == SlotTwo);
	}
	return false;
}


/**
 * 
 * @param SlotOne 
 * @param SlotTwo 
 * @return 
 */
bool UInventorySystem::GetIsSameExactSlot(const FStInventorySlot& SlotOne, const FStInventorySlot& SlotTwo) const
{
	return (SlotOne == SlotTwo);
}
