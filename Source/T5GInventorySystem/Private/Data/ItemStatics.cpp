// Take Five Games, LLC


#include "Data/ItemStatics.h"
#include "lib/ItemData.h"


FItemStatics::FItemStatics(const UItemDataAsset* DataAsset)
	: Rarity(TAG_Item_Rarity_Trash), Durability(-1.f),
		CraftTimestamp(FDateTime()), bIsEquipped(false)
{
	if (IsValid(DataAsset))
	{
		ItemName	= DataAsset->GetPrimaryAssetId().PrimaryAssetName;
		Rarity		= DataAsset->GetItemRarity();
		Durability	= DataAsset->GetItemMaxDurability();

		const UEquipmentDataAsset* EquipmentDataAsset = Cast<UEquipmentDataAsset>(DataAsset);
		if (IsValid(EquipmentDataAsset))
		{
			bIsEquipped = EquipmentDataAsset->bStartEquipped;
		}
	}
}
