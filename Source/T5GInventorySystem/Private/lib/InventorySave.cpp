
#include "lib/InventorySave.h"

TArray<FStInventorySlot> UInventorySave::LoadInventorySlots()
{
	return InventorySlots_;
}

void UInventorySave::SaveInventorySlots(const TArray<FStInventorySlot> ArrayOfCopiedSlots)
{
	InventorySlots_.Empty( ArrayOfCopiedSlots.Num() );
	for (int i = 0; i < ArrayOfCopiedSlots.Num(); i++)
	{
		InventorySlots_.Add(ArrayOfCopiedSlots[i]);
	}
}
