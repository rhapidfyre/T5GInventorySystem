// Take Five Games, LLC
/**
 * Gameplay tags used for the inventory system
 */
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "NativeGameplayTags.h"

#include "InventoryTags.generated.h"


UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Inventory);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment);

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Inventory_Slot);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Inventory_Slot_Uninit);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Inventory_Slot_Generic);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Inventory_Slot_Equipment);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Inventory_Slot_Locked);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Inventory_Slot_Mirrored);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Inventory_Slot_Hidden);

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Uninit);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Primary);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Secondary);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Ranged);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Ammunition);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Head);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Face);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Neck);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Torso);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Shoulders);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Arms);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Wrists);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Wrists_Left);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Wrists_Right);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Ring);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Ring_Left);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Ring_Right);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Waist);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Legs);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Anklet);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Feet);

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Category_None);			// Uncategorized
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Category_Equipment);	// Used as equipment
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Category_QuestItem);	// Used in a quest
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Category_Drink);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Category_Food);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Category_Weapon);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Category_Weapon_Melee);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Category_Weapon_Ranged);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Category_Placeable);	// Can be placed like furniture
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Category_Currency);		// Used as a type of currency
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Category_Fuel);			// Used as fuel for a forge or fire
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Category_Utility);		// An item that provide a function when in possession
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Category_Ingredient);	// Used as an ingredient to a crafting recipe
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Category_Component);	// Used as part of a final product, like a battery

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Money); // An item of monetary value, currency (Aethryst/Aethrym)

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Rarity_Trash);		// An item so common it is generally worthless
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Rarity_Common);		// An item that stands out to no-one
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Rarity_Uncommon);	// Impressive item, usually crafted
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Rarity_Rare);		// There may be only a few like it
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Rarity_Legendary);	// So unique, it passes between generations
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Rarity_Epic);		// An
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Rarity_Divine);		// An item made by the gods

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Activates);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Activation_Trigger); // Default behavior
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Activation_Equip);   // Activating equips it
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Activation_Drink);   // Activating drinks it
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Activation_Eat);     // Activating eats it
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Activation_Emplace); // Starts the placement system