// Fill out your copyright notice in the Description page of Project Settings.


#include "lib/FuelData.h"
#include "lib/ItemData.h"

UDataTable* UFuelSystem::getFuelItemDataTable()
{
	const FSoftObjectPath itemTable = FSoftObjectPath("/T5GInventorySystem/DataTables/DT_FuelData.DT_FuelData");
	UDataTable* dataTable = Cast<UDataTable>(itemTable.ResolveObject());
	if (IsValid(dataTable)) return dataTable;
	return Cast<UDataTable>(itemTable.TryLoad());
}

FStFuelData UFuelSystem::getFuelItemFromName(FName itemName)
{
	if (itemName.IsValid())
	{
		if (itemName != UItemSystem::getInvalidName())
		{
			if ( const UDataTable* dt = getFuelItemDataTable() )
			{
				if (IsValid(dt))
				{
					const FString errorCaught;
					FStFuelData* fuelPtr = dt->FindRow<FStFuelData>(itemName, errorCaught);
					if (fuelPtr != nullptr)
					{
						return *fuelPtr;
					}
				}
			}
		}
	}
	// Return a default item data struct
	return FStFuelData();
}

bool UFuelSystem::getFuelNameIsValid(FName itemName, bool performLookup)
{
	if (performLookup)
	{
		return UItemSystem::getItemNameIsValid(itemName);
	}
	return (!itemName.IsNone());
}

bool UFuelSystem::getFuelItemIsValid(FStFuelData itemData)
{
    return getFuelNameIsValid(itemData.properName, false);
}

FTimespan UFuelSystem::getFuelBurnTimeByName(FName itemName)
{
	if (UItemSystem::getItemNameIsValid(itemName))
	{
		const FStFuelData fuelData = getFuelItemFromName(itemName);
		return FTimespan::FromSeconds(fuelData.burnTime);
	}
	return FTimespan();
}

FTimespan UFuelSystem::getFuelBurnTime(FStFuelData fuelData)
{
	if (UItemSystem::getItemNameIsValid(fuelData.properName))
		return getFuelBurnTimeByName(fuelData.properName);
	return FTimespan(0,0,0);
}
