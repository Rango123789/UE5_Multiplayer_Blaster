// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BlasterCharacter.generated.h"

UCLASS()
class BLASTER_API ABlasterCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	/***functions***/
//category1: auto-generated functions:
	ABlasterCharacter();
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

//category2: virtual functions:
	/**<Actor>*/

	 /**</Actor>*/

	/**<X>*/

	 /**</X>*/

//category3: regular functions: 
	//montages:

	   //sound and effects:

	//bool functions:

	//BP-callale functions:
//category4: callbacks 



protected:
	/***functions***/
//category1: auto-generated functions:
	virtual void BeginPlay() override;
//category2: virtual functions:
	/**<Actor>*/

	 /**</Actor>*/

	/**<X>*/

	 /**</X>*/

//category3: regular functions 

//category4: callbacks

/***data members****/
//Category1: Enums , arrays, pointers to external classes
	//enum states:

	//pointer to external classes:

	//arrays:

	//class type:

//category2: UActorComponents   
	UPROPERTY(EditAnywhere)
	class USpringArmComponent* SpringArm; //CameraBoom
	UPROPERTY(EditAnywhere)
	class UCameraComponent* Camera;       //FollowCamera

//category3: Engine types      
	//montages:

	//sound and effects:

//category4: basic and primitive types



private: 
	/***functions***/
//category1: auto-generated functions:

//category2: virtual functions:

//category3: regular functions 

//category4: callbacks
	void Input_Move(const struct FInputActionValue & Value); //in UE5.2 need to forward-declare this struct
	void Input_Look(const FInputActionValue & Value);
	void Input_Jump(const FInputActionValue & Value);

/***data members****/
//Category1: Enums , arrays, pointers to external classes

//category2: UActorComponents   

//category3: Engine types      
    //input:
	UPROPERTY(EditAnywhere)
	class UInputMappingContext* IMC_Blaster;

	UPROPERTY(EditAnywhere)
	class UInputAction* IA_Move;
	UPROPERTY(EditAnywhere)
	class UInputAction* IA_Look;
	UPROPERTY(EditAnywhere)
	class UInputAction* IA_Jump;


	//montages:

	//sound and effects:

//category4: basic and primitive types

public:	
	/***Setters and Getters***/




};
