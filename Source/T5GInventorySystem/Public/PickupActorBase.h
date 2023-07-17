#pragma once

#include "CoreMinimal.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "lib/InventorySlot.h"

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
	float SphereRadius = 64.0f;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Actor Settings");
	float ItemDurability = 0.f;

	// Used to set the item that will spawn prior to BeginPlay()
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Actor Settings");
	FName ItemName = FName();

	// Used to set how many items will spawn prior to BeginPlay()
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Actor Settings");
	int SpawnQuantity = 1;

	/*
	 * Sets up a pickup actor containing a new item by the given name.
	 * @param ItemName The name of the item to look up in the data table
	 * @param quantity The number of items in this pickup actor
	 */
	UFUNCTION(BlueprintCallable)
	void SetupItemFromName(FName NewItemName, int NewItemQuantity = 1);

	UFUNCTION(BlueprintCallable)
	void OnPickedUp(AActor* targetActor);
	
protected:
	
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	virtual void CheckOverlapCall(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

private:

	void SetupItemData();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	USphereComponent* PickUpDetection;

private:
	
	// Used to simulate physics without a huge amount of bandwith use
	UPROPERTY(Replicated) FTransform m_WorldTransform;

	// A pseudo-slot for managing durability, quantity, etc
	UPROPERTY(Replicated) FStInventorySlot m_Slot = FStInventorySlot();
	
	FTimerHandle m_WaitTimer;
	
	float m_SphereRadius = 64.0f;

	// Flipped to true while the pickup actor is processing a pickup request
	bool bIsOperating = false;
	
};
