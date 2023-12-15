
#include "lib/InventorySlot.h"
#include "lib/ItemData.h"
#include "Logging/StructuredLog.h"

// Generic Constructor for initializing an un-usable inventory slot
FStInventorySlot::FStInventorySlot()
{}

// Overload for initializing the struct as an inventory slot
FStInventorySlot::FStInventorySlot(EInventorySlotType InvSlotType)
{
	EquipType	= EEquipmentSlotType::NONE;
	SlotType	= InvSlotType;
}

// Overload for initializing the struct as an equipment slot
FStInventorySlot::FStInventorySlot(EEquipmentSlotType EquipSlotType)
{
	EquipType	= EquipSlotType;
	SlotType	= EInventorySlotType::EQUIP;
}

// Overload for initializing the struct as an inventory slot with an item in it
FStInventorySlot::FStInventorySlot(FName InitItemName, int InitQuantity)
{
	ItemName		= InitItemName;
	SlotQuantity	= InitQuantity;
	SlotDurability	= 0.f;
}

void FStInventorySlot::DamageDurability(float DamageAmount)
{
	const float newDurability = SlotDurability - abs(DamageAmount);
	SlotDurability = newDurability < 0 ? 0.f : newDurability;
}

void FStInventorySlot::RepairDurability(float DamageAmount)
{
	const FStItemData ItemData = GetItemData();
	if (SlotDurability >= 0.f)
	{
		const float newDurability = SlotDurability + abs(DamageAmount);
		SlotDurability = newDurability > ItemData.MaxDurability ? ItemData.MaxDurability : newDurability;
	}
}

bool FStInventorySlot::AddQuantity(int AddAmount)
{
	const FStItemData ItemData = GetItemData();
	const float nQty = SlotQuantity + abs(AddAmount);
	if (nQty <= ItemData.maxStackSize) SlotQuantity = nQty;
	// The addition worked if the new quantity is the current slot quantity
	return nQty == SlotQuantity;
}

bool FStInventorySlot::RemoveQuantity(int RemoveAmount)
{
	const FStItemData ItemData = GetItemData();
	const float nQty = SlotQuantity - abs(RemoveAmount);
	SlotQuantity = (nQty < 1) ? 0 : nQty;
	// The removal worked if the new quantity is the current slot quantity
	return (nQty == SlotQuantity);
}

// Returns the data for the item in the slot
// If the item is empty, it will return default item data
FStItemData FStInventorySlot::GetItemData() const
{
	return UItemSystem::getItemDataFromItemName(ItemName);
}

// Returns TRUE if the slot is empty (qty < 1 or invalid name)
bool FStInventorySlot::IsSlotEmpty() const
{
	return (SlotQuantity < 1 || ItemName.IsNone());
}

/**
 * Returns whether or not this slot contains an item. If the optional parameter
 * is provided, returns true if this slot contains the item argument.
 * @param itemName (optional) If provided, checks if this slot contains given item
 * @return	With itemName, true on match false on mismatch.
 *			Without itemName, true on occupied, false on empty
 */
bool FStInventorySlot::ContainsItem(FName itemName) const
{
	if (UItemSystem::getItemNameIsValid(itemName, false))
	{
		return UItemSystem::isSameItemName(itemName, ItemName);
	}
	return UItemSystem::getItemNameIsValid(ItemName, false);
}

// Sets the slot to vacant without changing it's inventory or equip slot types
void FStInventorySlot::EmptyAndResetSlot()
{
	ItemName		= FName();
	SlotQuantity	= 0;
	SlotDurability	= 0.f;
}

int FStInventorySlot::GetMaxStackAllowance() const
{
	return UItemSystem::getMaximumStackSize(ItemName);
}

void FStInventorySlot::Activate()
{
	OnSlotActivated.Broadcast(SlotNumber, SlotType);
}

