
#include "lib/InventorySlot.h"
#include "lib/ItemData.h"

FStInventorySlot::FStInventorySlot(FName InitItemName, int InitQuantity)
{
	ItemName		= InitItemName;
	SlotQuantity	= InitQuantity;
	SlotDurability	= 0.f;
}

FStItemData FStInventorySlot::GetItemData() const
{
	return UItemSystem::getItemDataFromItemName(ItemName);
}

bool FStInventorySlot::IsSlotEmpty() const
{
	return (SlotQuantity < 1 || ItemName.IsNone());
}

void FStInventorySlot::EmptyAndResetSlot()
{
	ItemName		= FName();
	SlotQuantity	= 0;
	SlotDurability	= 0.f;
}

