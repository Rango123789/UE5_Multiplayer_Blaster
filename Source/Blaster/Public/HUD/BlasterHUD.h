// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "BlasterHUD.generated.h"


USTRUCT(BlueprintType)
struct FHUDPackage
{
	GENERATED_BODY()

	UTexture2D* CrosshairsCenter;
	UTexture2D* CrosshairsLeft;
	UTexture2D* CrosshairsRight;
	UTexture2D* CrosshairsTop;
	UTexture2D* CrosshairsBottom;
	float ExpandFactor;
	FLinearColor Color;
};


UCLASS()
class BLASTER_API ABlasterHUD : public AHUD
{
	GENERATED_BODY()
public:
	ABlasterHUD(); //for testing
	void virtual DrawHUD() override;
	void DrawCrosshair(UTexture2D* InTexture, FVector2D ViewportSize, FVector2D ExpandOffset);
	void SetupBlasterHUD();
	void PollInit_HUD();
	void virtual Tick(float DeltaTime) override;

	void CreateAndAddElimAnnounce(const FString& AttackPlayer, const FString& ElimmedPlayer);

	UFUNCTION()
	void TimerCallback_RemoveElimAnnounce(class UUserWidget_ElimAnnounce* Message, bool TestBool);

protected:
	/***functions***/
	virtual void BeginPlay() override;

/***data members****/
	UPROPERTY(EditAnywhere)
	TSubclassOf<class UCharacterOverlay_UserWidget> CharacterOverlay_Class;
	UPROPERTY()
	UCharacterOverlay_UserWidget* CharacterOverlay_UserWidget = nullptr;

	UPROPERTY(EditAnywhere)
	TSubclassOf<class UUserWidget_Announcement> Announcement_Class;
	UPROPERTY()
	UUserWidget_Announcement* UserWidget_Announcement = nullptr;

	UPROPERTY(EditAnywhere)
	TSubclassOf<class UUserWidget_ReturnToMainMenu> ReturnToMainMenu_Class;
	UPROPERTY()
	UUserWidget_ReturnToMainMenu* UserWidget_ReturnToMainMenu;

	UPROPERTY(EditAnywhere)
	TSubclassOf<class UUserWidget_ElimAnnounce> ElimAnnounce_Class;
	
	//this is optional, we can create a local variable in the method: 
	// UPDATE: we must create LOCAL variable. not the permanent variable LOL, because the member variable subject to change as new player are killed! 
	// UPDATE2: no in fact the value has been passed into the .BindFunction in the same frame, so trust me, LOCAL or MEMBER wont call any differences!
		//UPROPERTY()
		//UUserWidget_ElimAnnounce* UserWidget_ElimAnnounce;
	
	//this is needed for our purpose:
	UPROPERTY()
	TArray<UUserWidget_ElimAnnounce* > UserWidget_ElimAnnounce_Array;

	UPROPERTY(EditAnywhere)
	float TimerDelay_RemoveElimAnnounce = 8.f;
	
	FHUDPackage HUDPackage; //this will be assigned AWeapon::values via Character::Combat (as this is where you can access both EquippedWeapon and PlayerController()->GetHUD();

	UPROPERTY(EditAnywhere)
	float MaxExpand = 10.f;
private:

public:
	void SetHUDPackage(const FHUDPackage& InHUDPackage){ HUDPackage = InHUDPackage; }

	UCharacterOverlay_UserWidget* GetCharacterOverlay_UserWidget() { return CharacterOverlay_UserWidget; }
	UUserWidget_Announcement* GetUserWidget_Announcement() { return UserWidget_Announcement; }
	UUserWidget_ReturnToMainMenu* GetUserWidget_ReturnToMainMenu() { return UserWidget_ReturnToMainMenu; }
	//UUserWidget_ElimAnnounce* GetUserWidget_ElimAnnounce() { return UserWidget_ElimAnnounce; }
};
