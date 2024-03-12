
#include "lib/EquipmentData.h"

#include "lib/InventorySlot.h"

void UEquipmentDataAsset::GetItemTagOptions(FGameplayTagContainer& TagOptions) const
{
	Super::GetItemTagOptions(TagOptions);
	for (const FGameplayTag& gameplayTag : EquippableSlots) { TagOptions.AddTag(gameplayTag); }
	for (const FGameplayTag& gameplayTag : HidesBodyParts)  { TagOptions.AddTag(gameplayTag); }
	for (const FGameplayTag& gameplayTag : ShowsBodyParts)  { TagOptions.AddTag(gameplayTag); }
	TagOptions.AddTag(TAG_Item_Category_Equipment);
}
