// Fill out your copyright notice in the Description page of Project Settings.


#include "lib/FuelData.h"
#include "lib/ItemData.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Fuel_Wood,		"Item.Fuel.Wood");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Fuel_Oil,		"Item.Fuel.Oil");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Fuel_Generic,	"Item.Fuel.Generic");

FStFuelData::FStFuelData(const UFuelItemAsset* NewData)
	: ItemAsset(NewData)
{
	
}

