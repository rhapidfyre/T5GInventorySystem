#include "lib/InventoryData.h"

FStStartingItem::FStStartingItem(FName NewName, int NewQuantity, bool startEquipped)
{
	startingItem = NewName;
	quantity = NewQuantity;
	bStartEquipped = startEquipped;
}
