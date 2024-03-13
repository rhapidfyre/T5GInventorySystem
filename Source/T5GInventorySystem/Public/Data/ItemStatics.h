// Take Five Games, LLC

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "InventoryTags.h"

#include "ItemStatics.generated.h"


class UItemDataAsset;


USTRUCT(Blueprintable)
struct T5GINVENTORYSYSTEM_API FItemStatics
{
	GENERATED_BODY();
	
	FItemStatics() : Rarity(TAG_Item_Rarity_Trash), Durability(-1.f),
		CraftTimestamp(FDateTime()), bIsEquipped(false) {} ;

	// Used to create a whole new item
	FItemStatics(const UItemDataAsset* DataAsset);

	// The name of the Data Asset for this item
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite) FName			ItemName;
	
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite) FGameplayTag	Rarity;
	
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite) float			Durability;
	
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite) FString		CrafterName;
	
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite) FDateTime		CraftTimestamp;
	
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite) bool			bIsEquipped;


	bool operator==(const FItemStatics& InItemStatics) const
	{
		if (InItemStatics.ItemName != this->ItemName)
		{ return false; }
		if (!InItemStatics.Rarity.MatchesTagExact(this->Rarity))
		{ return false; }
		if (InItemStatics.Durability != this->Durability)
		{ return false; }
		if (!InItemStatics.CrafterName.IsEmpty() || CrafterName.IsEmpty())
		{
			if (InItemStatics.CrafterName != CrafterName)
				{ return false; }
		}
		if (InItemStatics.CraftTimestamp != CraftTimestamp)
		{
			if (InItemStatics.CraftTimestamp != CraftTimestamp)
				{ return false; }
		}
		return true;
	}

	bool operator!=(const FItemStatics& InItemStatics) const
	{
		return !(*this == InItemStatics);
	}
	
};