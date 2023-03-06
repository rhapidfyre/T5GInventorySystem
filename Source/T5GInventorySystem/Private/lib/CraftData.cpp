
#include "lib/CraftData.h"

#include "lib/ItemData.h"

UDataTable* UCraftSystem::getRecipeDataTable()
{
	const FSoftObjectPath itemTable = FSoftObjectPath(
			"/T5GInventorySystem/DataTables/DT_RecipeData.DT_RecipeData");
	UDataTable* dataTable = Cast<UDataTable>(itemTable.ResolveObject());
	if (IsValid(dataTable)) return dataTable;
	return Cast<UDataTable>(itemTable.TryLoad());
}

FStCraftingRecipe UCraftSystem::getRecipeFromItemName(FName itemName)
{
	if (itemName.IsValid())
	{
		if (itemName != UItemSystem::getInvalidName())
		{
			if ( const UDataTable* dt = getRecipeDataTable() )
			{
				if (IsValid(dt))
				{
					const FString errorCaught;
					FStCraftingRecipe* recipePtr =
						dt->FindRow<FStCraftingRecipe>(itemName, errorCaught);
					if (recipePtr != nullptr)
					{
						return *recipePtr;
					}
				}
			}
		}
	}
	// Return a default item data struct
	return FStCraftingRecipe();
}

bool UCraftSystem::getRecipeIsValid(FStCraftingRecipe recipeData)
{
	return UItemSystem::getItemNameIsValid(recipeData.itemName);
}

bool UCraftSystem::getItemHasRecipe(FName itemName)
{
	if (itemName.IsValid())
	{
		return getRecipeIsValid( getRecipeFromItemName(itemName) );
	}
	return false; // Invalid or not found
}

