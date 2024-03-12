
#include "lib/InventorySave.h"

#include "lib/InventorySlot.h"

TArray<FStInventorySlot> UInventorySave::LoadInventorySlots()
{
	return InventorySlots_;
}

void UInventorySave::SaveInventorySlots(TArray<FStInventorySlot> ArrayOfCopiedSlots)
{
	InventorySlots_.Empty( ArrayOfCopiedSlots.Num() );
	for (int i = 0; i < ArrayOfCopiedSlots.Num(); i++)
	{
		if (ArrayOfCopiedSlots[i].ContainsValidItem())
			{ InventorySlots_.Add( ArrayOfCopiedSlots[i] ); }
		else
			{ InventorySlots_.Add( FStItemData() ); }
	}
}