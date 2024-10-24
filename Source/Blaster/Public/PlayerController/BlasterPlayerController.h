// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BlasterPlayerController.generated.h"

/**
 * 
 */
UCLASS()
class BLASTER_API ABlasterPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	void SetHUDHealth(float Health, float MaxHealth);
protected:
	virtual void BeginPlay() override;

	//HUD and its Overlay widget (move from Character)
	class ABlasterHUD* BlasterHUD;
	class UCharacterOverlay_UserWidget* CharacterOverlay_UserWidget;
public:

};
