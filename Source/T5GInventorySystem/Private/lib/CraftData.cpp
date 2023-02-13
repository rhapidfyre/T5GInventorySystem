
#include "CraftData.h"

bool UCraftSystem::isSameCrafter(FStCrafterData craftOne, FStCrafterData craftTwo)
{
	if (craftOne.crafterName.EqualTo(craftTwo.crafterName))
	{
		/*
		// Returns true if the TMap from item one has the exact same keys and mapped values as two
		TArray<FStCraftQuality> arrayOne = craftOne.itemQuality;
		TArray<FStCraftQuality> arrayTwo = craftTwo.itemQuality;
		// Come back to this later
		if (arrayOne == arrayTwo) return true;
		*/
		return true;
	}
	return false;
}