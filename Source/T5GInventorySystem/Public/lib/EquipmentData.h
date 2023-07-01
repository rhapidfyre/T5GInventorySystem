#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"

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

USTRUCT(BlueprintType, Blueprintable)
struct FStEquipmentData : public FTableRowBase
{
	GENERATED_BODY();
	// The values affected by donning this equipment
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int AddMaxHealth = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int AddMaxStamina = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int AddMaxMagic = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ModifyHungerRate = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ModifyThirstRate = 0.f;
	
	// The higher the percentage of enhanced focus, the less like a spell will be interrupted when struck
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float EnhancedFocus = 0;
};