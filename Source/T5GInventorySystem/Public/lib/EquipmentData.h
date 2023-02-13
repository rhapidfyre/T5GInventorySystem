#pragma once

#include "CoreMinimal.h"

#include "EquipmentData.generated.h"


/**
 * WARNING - If you CHANGE, ADD or REMOVE any of these enums,
 * you will need to go to     Content/AZ_Assets/DataTables
 * and update the             DT_SlotIcons
 */
UENUM(BlueprintType)
enum class EEquipmentSlotType : uint8
{
	NONE, PRIMARY, SECONDARY, HELMET, FACE, NECK, SHOULDERS, BACK, TORSO,
	WAIST, HANDS, LEGS, ANKLET, FEET, SLEEVES, COSMETIC, EARRINGLEFT, EARRINGRIGHT,
	RINGLEFT, RINGRIGHT, WRISTLEFT, WRISTRIGHT
};