// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "BlasterGameMode.generated.h"


//add more stuff to MatchState namespace from AGameMode:
namespace MatchState
{
	//extern BLASTER_API is optional if you assign value right here:
	extern BLASTER_API const FName CoolDown;//Match Duration has been reached. Diplay winner and begin cooldown timer.
}

/**
 * 
 */
UCLASS()
class BLASTER_API ABlasterGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	//set it to false by default, in case child::constructor dont change it (because child isn't selected) then this will be the default, otherwise it will change to child new value - this is what we want.
	bool bIsTeamMatch = false;

	ABlasterGameMode();
	virtual void Tick(float DeltaTime) override;

	virtual void PlayerEliminated(class ABlasterCharacter* ElimininatedCharacter, class ABlasterPlayerController* EliminatedController, class ABlasterPlayerController* AttackerController);
	virtual void RequestRespawn(ABlasterCharacter* ElimininatedCharacter, ABlasterPlayerController* EliminatedController);

	//parameter can be either Char || PC || PS, but PS is what our function directly need so yeah:
	void HandleLeaveGameRequest(class APlayerState_Blaster* LeavingPlayerState);

	//custom virtual function:
	virtual float CalculateDamage(AController* AttackController, AController* VictimController, const float& BaseDamage);


protected:
	void virtual BeginPlay() override;

	virtual void OnMatchStateSet() override;

	//New1, it should ORIGINATE from GM instead LOL:
	UPROPERTY(EditAnywhere)
	float MatchTime = 120.f;

	//New2, we should also create PC::WarmUpTime to be propogate to from here
	UPROPERTY(EditAnywhere)
	float WarmUpTime = 10.f;

	float CountingDownTime = WarmUpTime;//with this you dont even need BeginPlay

	UPROPERTY(EditAnywhere)
	float CoolDownTime = 10.f;  //ForNextLesson
	//StarterMap -> LobbyMap ->     GameMap
	//    0                      BlasterLevelStartingTime
	float LevelStartingTime =0.f; //to be set in BeginPlay()


public:
	float GetLevelStartingTime() { return LevelStartingTime; }
	float GetWarmUpTime() { return WarmUpTime; }
	float GetMatchTime() { return MatchTime; }
	float GetCoolDownTime() { return CoolDownTime; }

	float GetCountingDownTime() { return CountingDownTime; }
	FName GetMatchState() { return MatchState; }
};
