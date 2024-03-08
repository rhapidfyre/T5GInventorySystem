
#include "lib/InventoryData.h"
#include "lib/ItemData.h"
#include "Logging/StructuredLog.h"

/**
* Performs a die roll, returning true if it succeeded
 * @param dieRoll The resulting die roll
 * @param winChance The final chance to win
 * @param minChance The minimum winning chance
 * @param maxChance The maximum winning chance
 * @return True if roll wins, false if roll loses
 */
static bool RollTheDice(double& dieRoll, double& winChance, double minChance, double maxChance)
{
	// winChance is 'maxChance', but after validation
	winChance = (maxChance >= minChance) ? maxChance : minChance + 0.01;
	
	double randomMin = (minChance >= 0.f) ? minChance : 0.f; // Min chance must be at least 0%
	// Max chance must be at least Minimum Chance plus .001%
	double randomMax = (winChance >= minChance) ? maxChance : minChance + 0.00001; 
	
	// Impossible to win
	if (maxChance > winChance)
		{return false;}
	
	// Determines the win chance between the minimum and maximum chance
	// So if min is 30%, and max is 40%, if randomChance is 33%, the roll must be 0-33 to win.
	const double highestChance = FMath::RandRange(randomMin, randomMax);
	
	// The chance to win is either the random chance, or the win chance, whichever is highest
	const double numberToBeat = winChance > highestChance ? highestChance : winChance;
	return FMath::RandRange(0.f, 1.f) <= winChance;
}

static int DetermineQuantity(int minRolls = 1, int maxRolls = 1)
{
	const int minimumRolls = minRolls > 0 ? minRolls : 1;
	const int maximumRolls = maxRolls >= minRolls ? maxRolls  : minRolls;
	return FMath::RandRange(minimumRolls, maximumRolls);
}

/**
 * Returns an array of all starting items, after all data has been generated,
 * such as durability, quantity, rarity and spawn chances.
 * @return Array of items to start with, pre-generated.
 */
TArray<FStItemData> UInventoryDataAsset::GetStartingItems() const
{
	TArray<FStItemData> stStartingItems = {};
	if (StartingItems.Num() > 0)
	{
		for (FStartingItem startItem : StartingItems)
		{
			// Continue iterating if the reference is invalid
			if (!IsValid(startItem.ItemReference)) { continue; }

			const int validatedQuantity = startItem.QuantityMaximum > 0 ? startItem.QuantityMaximum : 1;
			const int itemQuantity = !startItem.bRandomQuantity
				? validatedQuantity : DetermineQuantity(startItem.QuantityMinimum, startItem.QuantityMaximum);

			for (int i = 0; i < itemQuantity; i++)
			{
				double rollResult, winChance;
				const bool winningRoll = RollTheDice(
						rollResult, winChance,
						startItem.ChanceMinimum,
						startItem.ChanceMaximum);
			
				UE_LOGFMT(LogTemp, Display, "GetStartingItems(): "
					"Rolled '{DieRoll}', Needed <= {WinChance}", rollResult, winChance);
			
				if (winningRoll)
				{
					FStItemData NewItem(startItem.ItemReference);
					NewItem.DurabilityNow = startItem.ItemReference->GetItemMaxDurability();
					NewItem.bIsEquipped   = startItem.bEquipOnStart;
					stStartingItems.Add(NewItem);
				}
			}
		}
	}
	return stStartingItems;
}
