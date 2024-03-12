// Take Five Games, LLC
#include "Data/InventoryTags.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_Item,						"Item");
UE_DEFINE_GAMEPLAY_TAG(TAG_Inventory,					"Inventory");
UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment,					"Equipment");

UE_DEFINE_GAMEPLAY_TAG(TAG_Inventory_Slot,				"Inventory.SlotType");
UE_DEFINE_GAMEPLAY_TAG(TAG_Inventory_Slot_Uninit,		"Inventory.SlotType.Uninitialized");
UE_DEFINE_GAMEPLAY_TAG(TAG_Inventory_Slot_Generic,		"Inventory.SlotType.Generic");
UE_DEFINE_GAMEPLAY_TAG(TAG_Inventory_Slot_Equipment,	"Inventory.SlotType.Equipment");
UE_DEFINE_GAMEPLAY_TAG(TAG_Inventory_Slot_Locked,		"Inventory.SlotType.Locked");
UE_DEFINE_GAMEPLAY_TAG(TAG_Inventory_Slot_Mirrored, 	"Inventory.SlotType.Mirrored");
UE_DEFINE_GAMEPLAY_TAG(TAG_Inventory_Slot_Hidden,		"Inventory.SlotType.Hidden");

UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot,				"Equipment.SlotType");
UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Uninit,		"Equipment.SlotType.Uninitialized");
UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Primary,		"Equipment.SlotType.Primary");
UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Secondary,	"Equipment.SlotType.Secondary");
UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Ranged,		"Equipment.SlotType.Ranged");
UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Ammunition,	"Equipment.SlotType.Ammunition");
UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Head,			"Equipment.SlotType.Head");
UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Face,			"Equipment.SlotType.Face");
UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Neck,			"Equipment.SlotType.Neck");
UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Torso,		"Equipment.SlotType.Torso");
UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Shoulders,	"Equipment.SlotType.Shoulders");
UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Arms,			"Equipment.SlotType.Arms");
UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Wrists,		"Equipment.SlotType.Wrist");
UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Wrists_Left,	"Equipment.SlotType.Wrist.Left");
UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Wrists_Right,	"Equipment.SlotType.Wrist.Right");
UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Ring,			"Equipment.SlotType.Ring");
UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Ring_Left,	"Equipment.SlotType.Ring.Left");
UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Ring_Right,	"Equipment.SlotType.Ring.Right");
UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Waist,		"Equipment.SlotType.Waist");
UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Legs,			"Equipment.SlotType.Legs");
UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Anklet,		"Equipment.SlotType.Anklet");
UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Feet,			"Equipment.SlotType.Feet");

UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Category_Equipment, 	"Item.Category.Equipment");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Category_QuestItem, 	"Item.Category.QuestItem");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Category_Drinkable, 	"Item.Category.Drinkable");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Category_Edible, 		"Item.Category.Edible");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Category_Weapon,		"Item.Category.Weapon");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Category_Weapon_Melee,	"Item.Category.Weapon.Melee");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Category_Weapon_Ranged,	"Item.Category.Weapon.Ranged");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Category_Placeable, 	"Item.Category.Placeable");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Category_Currency, 		"Item.Category.Currency");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Category_Fuel, 			"Item.Category.Fuel");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Category_Utility, 		"Item.Category.Utility");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Category_Ingredient, 	"Item.Category.Ingredient");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Category_Component, 	"Item.Category.Component");

UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Generic,				"Item.Generic");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Equipment,				"Item.Equipment");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Edible,					"Item.Food");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Drink,					"Item.Drink");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Focus,					"Item.Focus");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Scroll,					"Item.Scroll");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Money,					"Item.Money");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Fuel,					"Item.Fuel");

UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Rarity_Trash,			"Item.Rarity.Trash");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Rarity_Common,			"Item.Rarity.Common");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Rarity_Uncommon,		"Item.Rarity.Uncommon");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Rarity_Rare,			"Item.Rarity.Rare");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Rarity_Legendary,		"Item.Rarity.Legendary");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Rarity_Divine,			"Item.Rarity.Divine");

UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Activates,				"Item.Activation");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Activation_Trigger,		"Item.Activation.Trigger");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Activation_Equip,		"Item.Activation.Equip");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Activation_Drink,		"Item.Activation.Drink");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Activation_Eat,			"Item.Activation.Eat");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Activation_Emplace,		"Item.Activation.Emplace");
