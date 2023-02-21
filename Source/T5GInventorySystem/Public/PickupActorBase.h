#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "lib/ItemData.h"

#include "PickupActorBase.generated.h"

UCLASS(BlueprintType)
class T5GINVENTORYSYSTEM_API APickupActorBase : public AStaticMeshActor
{
	GENERATED_BODY()
public:

	APickupActorBase();

	virtual void Tick(float DeltaTime) override;

	virtual void OnConstruction(const FTransform& Transform) override;

	virtual void PostLoad() override;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Actor Settings");
	float m_sphereRadius = 64.0f;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Actor Settings");
	FName m_itemName = UItemSystem::getInvalidName();

	UPROPERTY(Replicated)
	FStItemData m_itemData;

	/*
	 * Sets up a pickup actor containing a new item by the given name.
	 * @param itemName The name of the item to look up in the data table
	 * @param quantity The number of items in this pickup actor
	 */
	UFUNCTION(BlueprintCallable)
	void SetupItemData(FName itemName, int quantity = 1);

	/*
	 * Used when the pickup actor is being made from an existing item.
	 * Such as, a player dropping it from their inventory.
	 * @param itemData The itemData this actor will represent
	 * @param quantity The number of items in this pickup actor
	 */
	UFUNCTION(BlueprintCallable)
	void SetupItemFromData(FStItemData itemData, int quantity = 1);
	
	UFUNCTION(BlueprintCallable)
	void OnPickedUp(AActor* targetActor);
	
protected:
	
	// If the actor fails to validate, this function is called to try again.
	void CheckForValidity();

	void SetupItemData();
	
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	bool bSetupComplete = false;

	UPROPERTY(Replicated)
	int m_itemQuantity = 1;

	bool m_isOperating = false; // Sets to true to avoid duplication exploits

	int m_retryCount = 0;
	
	FTimerHandle m_waitTimer;
	
};
