#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ItemData.h"

#include "EquipmentData.generated.h"


/**
 * Equipment items are subsets (children) of Item Data that contain information
 * specifically regarding equipment, such as what mesh is used when it is worn
 */
UCLASS(BlueprintType)
class T5GINVENTORYSYSTEM_API UEquipmentItemData : public UItemDataAsset
{
	GENERATED_BODY()


public:

	UEquipmentItemData() {};

	UFUNCTION(BlueprintType)
	bool GetCanEquipInSlot(const FGameplayTag& EquipmentSlotTag) const
	{
		return EquippableSlots.HasTag(EquipmentSlotTag);
	}

	FGameplayTagContainer GetItemEquippableSlots() const
	{
		return EquippableSlots;
	}

	virtual void GetItemTagOptions(FGameplayTagContainer& TagOptions) const override;
	
	// The mesh worn by typically masculine wearers. This is the default if feminine is nullptr.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)	USkeletalMesh* MeshMasculine = nullptr;

	// The mesh used by typically feminine wearers.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)	USkeletalMesh* MeshFeminine  = nullptr;
	
	// What body part this mesh is associated with (body part "slot")
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTagContainer EquippableSlots = {};
	
	// Which part parts will be hidden if this mesh is equipped. Overridden by "ShowsBodyParts"
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTagContainer HidesBodyParts = {};
	
	// Which body parts MUST be visible if this mesh is equipped. Overrules "ShowsBodyParts".
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTagContainer ShowsBodyParts = {};
	
	// Effect(s) to apply as long as the item is equipped
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<UGameplayEffect*> EffectsEquipped = {};
	
	// Effect(s) to apply to the character who has this item in their inventory
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<UGameplayEffect*> EffectsPassive = {};
};