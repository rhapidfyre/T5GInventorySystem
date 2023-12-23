
#include "lib/InventorySave.h"

TArray<FStInventorySlot> UInventorySave::LoadInventorySlots()
{
	return InventorySlots_;
}

void UInventorySave::SaveInventorySlots(const TArray<FStInventorySlot>& ArrayOfCopiedSlots)
{
	InventorySlots_ = ArrayOfCopiedSlots;
}
