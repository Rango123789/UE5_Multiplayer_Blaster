// Fill out your copyright notice in the Description page of Project Settings.


#include "CharacterComponents/CombatComponent.h"
#include "Weapons/Weapon.h"
#include "Weapons/HitScanWeapon_Shotgun.h"
#include "Characters/BlasterCharacter.h"
#include "PlayerController/BlasterPlayerController.h"
#include "HUD/BlasterHUD.h"
#include "Weapons/Projectile.h"

#include "Camera/CameraComponent.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Components/SphereComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h" //for DoLineTrace
#include "Kismet/GameplayStatics.h" //for DeprojectScreenToWorld
//#include "TimerManager.h" //NEWs

//TESTING
//#include "Menu_UserWidget.h" //ONLY work after you add the plugin module here
//#include "MultiplayerSession_GameSubsystem.h" //ONLY work after you add the plugin module here
//#include "../../../../Plugins/MultiplayerSessions/Source/MultiplayerSessions/Public/MultiplayerSession_GameSubsystem.h"
//#include "../../../../Plugins/MultiplayerSessions/Source/MultiplayerSessions/Public/Menu_UserWidget.h"
//#include "../../../../Plugins/MultiplayerSessions/Source/MultiplayerSessions/Public/Menu_UserWidget.h"
//#include "MultiplayerSessions/MultiplayerSessions.h"
//#include "Plugins/MultiplayerSessions/Menu_UserWidget.h"

UCombatComponent::UCombatComponent()
	//: TimerDelegate( FTimerDelegate::CreateUObject(this, &ThisClass::FireTimer_Callback) )
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true; //if you tick later, then turn it on
	// IT WORK TOO! but this is not recommeded, let's do it in HOSTING actor
	SetIsReplicatedByDefault(true);

	//UMenu_UserWidget Key;
	//UMultiplayerSession_GameSubsystem* key22;

	//TimerDelegate.BindUFunction(this, FName("FireTimer_Callback")); 

	/*
	#Puzzle: 
+I didn't set UCombatComponent::SetIsReplicated(true)/LIKE
, how it can still work so well? did I check it from BP_Char?

+Well i set this from the HOSTING actor perspective already
, meaning for UActorComponent created by CreateDefaultSubobject we register the component in the hosting actor's constructor

+There is a thing you must be aware of
(1) Char::Char(){ Combat::SetIsReplicated(true)} and Combat::Combat(){  SetIsReplicated(true)  } in fact change the same underlying data
=you need to only change EITHER of them, if you do both, then the setting in the HOSTING actor will take precendence naturally (Actor's contructor run after its comp's constructor, who ever run after will be the final value)
(2)_[UPDATE] if you did do SetIsReplicated(true) LOCALLY in UCombatComponent
, then it will be the default value of it in the HOSTING actor too
, hell yeah!
(3) However for the built-in UActorComponent, UE5 dont typically set SetIsReplicated(true)
, perhaps because it suit better for the default case of single player
, hence if you use any buil-in Component, you gotta remember call HostingActor::UActorComponent::SetIsReplicated(true) within the Hosting Actor class!

+However a common practice is that you DO NOT do it in LOCAL UActorComponent class (even if it work) so that the default behaviour is NOT replicated
, you follow UE5's practice and only do it in hosting Actgor
	*/
}

void UCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	DOREPLIFETIME(UCombatComponent, EquippedWeapon);
	DOREPLIFETIME(UCombatComponent, SecondWeapon);

	DOREPLIFETIME(UCombatComponent, bIsAiming);
	//DOREPLIFETIME(UCombatComponent, bIsFiring); //just added, BUT didn't affect anyway
	//DOREPLIFETIME(UCombatComponent, CarriedAmmo); //will still work but redudant for non-owning clients to receive it - they dont need it in this game
	DOREPLIFETIME_CONDITION(UCombatComponent, CarriedAmmo, COND_OwnerOnly); //only owning proxy need to receive it
	DOREPLIFETIME_CONDITION(UCombatComponent, ThrowGrenade, COND_OwnerOnly); //only owning proxy need to receive it

	DOREPLIFETIME(UCombatComponent, CharacterState);
}

void UCombatComponent::ClientResetToUnoccupied_Implementation()
{
	CharacterState = ECharacterState::ECS_Unoccupied;
	//fix can't reload after shot-shot-reload-shot  :this will help to reset bLocalRealoding for CD too in worst case that ReloadEnd is interrupted to reach 
	//better off move it into Fire() chain instead, using "let interrupting action to reset the interrupted action itself" technique
	bLocalReloading = false;
	//hitchking-fix: that when ReloadMontage interrupt FireMontage/its chain bCanfire get stuck in false:
	//bCanFire = true; 
}

void UCombatComponent::BeginPlay()
{
	Super::BeginPlay();

//this focus on Aiming, where Crouch speed is handled automatically by UE5 when we press Crouch and UnCrouch
	//assign its value here (or BeginPlay()), so surely all character instances has it
	if (Character && Character->GetCharacterMovement())
	{
		MaxWalkSpeed_Backup = Character->GetCharacterMovement()->MaxWalkSpeed;
		//We have MaxWalkSpeed, [Max]CrouchSpeed , but we dont have AimWalkSpeed lol, hence instead of accessing it, we assign it ourself!
		AimWalkSpeed = 300.f; //you shouldn't do this here, let it be in .h so that you can change from UE5 without being changed back here
	}

	if (Character)
	{
		DefaultPOV = Character->GetCamera()->FieldOfView;
		CurrentPOV = DefaultPOV;

		//I have no idea why he add this HasAuthority(), i thought it is like a table of reference so it is for IsLocallyControlled()/Owner at least? what the heck is going on LOL?
		//Well because CarriedAmmo will be set in Equip which in GOLDEN1 that will always run from the server, and also CarriedAmmo is replicated to COND_Owner only, so the owner get the needed value lol = OKAY!
		if (Character->HasAuthority())
		{
			InitializeCarriedAmmo();
		}
	}
}

void UCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    //UPDATE: because other non-controlling device dont care about Crosshairs and POV of this camera so add IsLocallyContrlled() will save performance for other device
	if (Character && Character->IsLocallyControlled()) //ADDED!
	{
		SetHUDPackageForHUD(DeltaTime);
		SetPOVForCamera(DeltaTime);
	}
}

void UCombatComponent::Input_Reload()
{
	//we only reload if it is Unoccupied and currently not already Readling (avoiding Spam) -- && CharacterState != ECS_Reloading  is redudant
	if (CarriedAmmo > 0 &&
		EquippedWeapon && !EquippedWeapon->IsFull() &&
		CharacterState == ECharacterState::ECS_Unoccupied &&
		bLocalReloading == false) //I think this is optional, reason: currently 'bLocalReloading' will turn false even before 'CharacterState' turn Unoccupied when ReloadEnd reach!
		                          //infact it can even stop you from reloading if bLocalReloading is not set back to false successfully after reloading (that the ReloadMontage is interrupted before it can set)
	{
		//clien-side prediciton part, non-replicated DoAction():
			//CharacterState = ECharacterState::ECS_Reloading; //remove
		Character->PlayReloadMontage(); 
		bLocalReloading = true; //we will go to AnimInstance and make an exception for CD, so that it can use FABRIK back immediately when ReloadEnd without waiting for 'ping'

		//normal replication part: if CD is already the server then we dont need to call this any more, because if so we just DoAction() above in the server already :D :D 
			//if( Character &&Character->HasAuthority() == false) ServerInput_Reload(); //replaced
		ServerInput_Reload();

		//"let interrupting action to reset the interrupted action itself" technique: in fact for fun, because Timerline_fire is set to always reach currently:
		bCanFire = true;
	}
}

void UCombatComponent::ServerInput_Reload_Implementation()
{
	if (Character == nullptr) return;
	//this help trigger OnRep_, if it changes WeaponState from Unoccupied
	CharacterState = ECharacterState::ECS_Reloading; //(*)
	    //called in Reload's AnimNotify
		//UpdateHUD_CarriedAmmo(); //self-replicated, we have OnRep_CarriedAmmo work already

	//if it reach this hosting function and it is CD, then the server always did this already, so no need to repeat it. Either case, we still need to exclude !Character->IsLocallyControlled() in OnRep_CharacterState too, I know this is crazy but 2 stages are independent!
	//the only reason why we dont do it right in TIRE1 but here is because we also need to call (*) above!
	if( !Character->IsLocallyControlled() ) Character->PlayReloadMontage(); //call it in  OnRep_CharacterState too for clients

	//this is the challenge2, but luckily help to solve some of this lesson LOL:
	ClientResetToUnoccupied();
}

//these versions are not used: reason, try to change 'Replicated Enum state var' of out Replication pattern pose a risk.
//void UCombatComponent::Input_Reload()
//{
//	//we only reload if it is Unoccupied and currently not already Readling (avoiding Spam) -- && CharacterState != ECS_Reloading  is redudant
//	if (CarriedAmmo > 0 &&
//		EquippedWeapon && !EquippedWeapon->IsFull() &&
//		CharacterState == ECharacterState::ECS_Unoccupied)
//	{
//		//clien-side prediciton part, DoAction():
//			//not sure we should to this LOL, because we can simply Play ReloadMontage() indepent from setting CharacterState to Reloading LOL, this is onlly helpful if there is other code also rely on Characterstate and need quick responsive too:
//			//I test it it doesn't hurt so far (nor I see any different it make LOL)
//			//In fact if you put 'HasAuthority()' below this line is A MUST (unless you remove that condition below)
//		CharacterState = ECharacterState::ECS_Reloading;
//		//as always we should add IsLocallycontrolled in OnRep_characterstate to exclude it! 
//		Character->PlayReloadMontage();
//
//		//normal replication part: if CD is already the server then we dont need to call this any more, because if so we just DoAction() above in the server already :D :D 
//		if (Character && Character->HasAuthority() == false) ServerInput_Reload();
//	}
//}
//void UCombatComponent::ServerInput_Reload_Implementation()
//{
//	if (Character == nullptr) return;
//	//this help trigger OnRep_, if it changes WeaponState from Unoccupied
//	CharacterState = ECharacterState::ECS_Reloading; //self-replicated, and trigger its OnRep_X
//	//Call this in ReloadEnd() with HasAuthority() if you want it to update HUD that time = everyone like this :)
//		//UpdateHUD_CarriedAmmo(); //self-replicated, we have OnRep_CarriedAmmo work already
//
////all code bellow will be copied and pasted to OnRep_, STEPHEN factorize all of them into ReloadHandle() so that when we add more stuff to ReloadHandle() we dont need to go and paste to OnRep_ again LOL, BUT I say we way LOL. Anyway HandleReload() didn't include the line 'CharacterState = ECharacterState::ECS_Reloading above - of course, this is only for non-replicated code LOL
//	//you must go and use AimNotify to reset it back to UnOccupied, otherwise you get stuck in state :D :D
//	Character->PlayReloadMontage(); //call it in  OnRep_CharacterState too for clients
//}

//I haven't call this anywhere yet, as this i meant for when Reloading
//this will be called in RealoadEnd Notify, giving the sense of rewarding after wating for the anim animation to finish, except Shotgun will use the specialized version below this one:
//Change Ammo + CarriedAmmo should be done in the server first, this function will be  wrapped inside ReloadEnd() and checked there, dont worry:
void UCombatComponent::UpdateHUD_CarriedAmmo()
{
	if (EquippedWeapon == nullptr) return;

	//change CarriedAmmo's value here when Reload, say when Reload, so it will be different a bit LOL, load X_max - x to get max or load y if y < X_max - x:
	int32 LoadAmount = FMath::Min(EquippedWeapon->GetMagCapacity() - EquippedWeapon->GetAmmo(), CarriedAmmo);

	//Change Ammo here will trigger OnRep_Ammo ->SetHUDAmmo work automatically in clients!
	EquippedWeapon->SetAmmo(EquippedWeapon->GetAmmo() + LoadAmount);

	//Stephen swap order of them for sematic reason, but I prefer this, changing CarriedAmmo here ill trigger OnRep_CarriedAmmo  ->SetHUDCarriedAmmo work automatically in clients!
	CarriedAmmo = CarriedAmmo - LoadAmount;
	   //this line not affect this lesson but update the author_Char::Combat::CarriedAmmoMap
	if (CarriedAmmoMap.Contains(EquippedWeapon->GetWeaponType()))
	{
		CarriedAmmoMap[EquippedWeapon->GetWeaponType()] = CarriedAmmo;
	}
//you can move these 2 lines into Char::ReloadEnd() so that it wont update before that point, however the SetHUDAmmo trigger in Client already, so you have to move them all together :D :D. 
//So the final idea is to Call this HOSTING function in ReloadEnd() instead of in ServerInput_Reload()
//So because it is called in the server, so to compensate we add 'HasAuthority()' if call from there! 
// So you must understand that once we add it for Do_Action, the clients wont pass it and wont excecute it, so you must rely on OnRep (if any)
	
	CheckAndSetHUD_CarriedAmmo();

	//still need to help HUDAmmo in the server update to, I choose to do it here too, rather than outside:
	EquippedWeapon->CheckAndSetHUD_Ammo();
}

//For PlayReloadAnimation ~>Shotgun section, we call this instead:
//if it reach this function for first time, it must pass the condition < MagCapacity I guess
//so for now I dont check it full when I start to Load :D :D
//TO MAKE it consistent, I will create 'ReloadOneShell()' and wrap around this function and then check HasAuthorioty() from there too - simply make a copy of 'ReloadEnd()' and adjust it!
void UCombatComponent::UpdateHUD_CarriedAmmo_SpecializedForShotgun()
{
//PART1: almost the same UpdateHUD_CarriedAmmo()
	if (EquippedWeapon == nullptr) return;

	//this time it will much more easier LOL:
	int32 LoadAmount = 1;

	//Change Ammo here will trigger OnRep_Ammo ->SetHUDAmmo work automatically in clients!
	EquippedWeapon->SetAmmo(EquippedWeapon->GetAmmo() + LoadAmount);

	//Stephen swap order of them for sematic reason, but I prefer this, changing CarriedAmmo here ill trigger OnRep_CarriedAmmo  ->SetHUDCarriedAmmo work automatically in clients!
	CarriedAmmo = CarriedAmmo - LoadAmount;
	//this line not affect this lesson but update the author_Char::Combat::CarriedAmmoMap
	if (CarriedAmmoMap.Contains(EquippedWeapon->GetWeaponType()))
	{
		CarriedAmmoMap[EquippedWeapon->GetWeaponType()] = CarriedAmmo;
	}
	//you can move these 2 lines into Char::ReloadEnd() so that it wont update before that point, however the SetHUDAmmo trigger in Client already, so you have to move them all together :D :D. 
	//So the final idea is to Call this HOSTING function in ReloadEnd() instead of in ServerInput_Reload()
	//So because it is called in the server, so to compensate we add 'HasAuthority()' if call from there! 
	// So you must understand that once we add it for Do_Action, the clients wont pass it and wont excecute it, so you must rely on OnRep (if any)

	CheckAndSetHUD_CarriedAmmo();

	//still need to help HUDAmmo in the server update to, I choose to do it here too, rather than outside:
	EquippedWeapon->CheckAndSetHUD_Ammo();

//PART2: check if it full or out of CarriedAmmo and jump directly to sub section 'Shotgun End':
	//this only jump in the server within ReloadOneAmmo(), hence also check and call it in Weapon::OnRep_Ammo() is perfect! but from there we could only check 'EquippedWeapon->IsFull()' ? Well it has 'Ownercharacter' you can use :D 
	if (EquippedWeapon->IsFull() || CarriedAmmo == 0)
	{
		CharacterState = ECharacterState::ECS_Unoccupied; //moved from LoadOneAmmo(); so not any other React can intterupt it after first reload
		if (Character) Character->JumpToShotgunEndSection();//helper from Character
	}
}

void UCombatComponent::UpdateHUD_ThrowGrenade()
{
	if (ThrowGrenade <= 0) return;
	
	ThrowGrenade--;

	CheckAndSetHUD_ThrowGrenade();
}

//It first change when Equipped, this is auto-triggered
void UCombatComponent::OnRep_CarriedAmmo()
{
	CheckAndSetHUD_CarriedAmmo();
}

void UCombatComponent::CheckAndSetHUD_CarriedAmmo()
{
	if (Character == nullptr) return;
	BlasterPlayerController = BlasterPlayerController == nullptr ? Character->GetController<ABlasterPlayerController>() : BlasterPlayerController;

	if (BlasterPlayerController) BlasterPlayerController->SetHUDCarriedAmmo(CarriedAmmo);
}

void UCombatComponent::InitializeCarriedAmmo()
{
	CarriedAmmoMap.Emplace(EWeaponType::EWT_AssaultRifle, StartCarriedAmmo_AR);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_RocketLauncher, StartCarriedAmmo_Rocket);

	CarriedAmmoMap.Emplace(EWeaponType::EWT_Pistol, StartCarriedAmmo_Pistol);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_SMG, StartCarriedAmmo_SMG);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_Shotgun, StartCarriedAmmo_Shotgun);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_SniperRifle, StartCarriedAmmo_SniperRifle);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_GrenadeLauncher, StartCarriedAmmo_GrenadeLauncher);
}

void UCombatComponent::Input_Throw()
{
	if (Character == nullptr) return;
	//so that it wont spam itself and any other montages:
	if (CharacterState != ECharacterState::ECS_Unoccupied) return;
	if (ThrowGrenade <= 0) return;

//i start to feel this STUPID lol, if you bother to make it happen in server first, why still bother to restrict it to be called in the server and then give up and make it work in the controlling device first?
	//Stephen said, that he want to play Montage in controlling device immediately to see it immediatly, and then send RPC to the server and other clients later:  FIRST TIME!
	// However in OnRep_CharState we will include the IsLocallycontroll case to avoid playing twice LOL: FIRST TIME!
	//Consequencely you must change the state for it to (even before it receive LEGAL value from server later)
	CharacterState = ECharacterState::ECS_Throwing; // I forget this line LOL

	Character->PlayThrowMontage(); //too see immediate effect in controlling device
	AttachEquippedWeaponToLeftHandSocket();

	UpdateHUD_ThrowGrenade(); //in case it is already the server and can't pass next check

	//the second consequence, because you do the 2 line above, so you can in fact OPTIONALLY put the 'ServerInput_Throw()' inside if(!HasAuthority())), because in worst case it is called in the server it skip this RPC it still has the 2 line above back it up! :D :D
	    //usual option:
	//ServerInput_Throw(); //this solve server part, CharacterState_will solve in OnRep_CharState
		//better option: as we already have 2 line above back up in worst case
	if (Character->HasAuthority() == false)
	{
		ServerInput_Throw();
	}
}

void UCombatComponent::ServerInput_Throw_Implementation()
{
	if (Character == nullptr) return;

	CharacterState = ECharacterState::ECS_Throwing; //trigger OnRep_ -->paste code for clients

	Character->PlayThrowMontage();
	AttachEquippedWeaponToLeftHandSocket(); //this is self-replicated, no need to call it in OnRep_CharacterState::Throwing at all _VERFIED - I test it!

	UpdateHUD_ThrowGrenade();
}


void UCombatComponent::OnRep_ThrowGrenade()
{
	CheckAndSetHUD_ThrowGrenade();
}

void UCombatComponent::CheckAndSetHUD_ThrowGrenade()
{
	if (Character == nullptr) return;
	BlasterPlayerController = BlasterPlayerController == nullptr ? Character->GetController<ABlasterPlayerController>() : BlasterPlayerController;
	if (BlasterPlayerController) BlasterPlayerController->SetHUDThrowGrenade(ThrowGrenade);
}

void UCombatComponent::OnRep_CharacterState()
{
	if (Character == nullptr) return;
	switch (CharacterState)
	{
	case ECharacterState::ECS_Reloading :
		//We did play it for the DC already so we exclude it avoiding double playing it , start the Reload montage TWICE, you may not notice it because the later play on top of the first and also have 'interpolating' as well:
		if (!Character->IsLocallyControlled())
		{
			Character->PlayReloadMontage(); //can put it into 'ReloadHanle()'
		}
		break;
	case ECharacterState::ECS_Throwing :
		//the controlling device already played from Combat::Input_Throw for seeing immediate effect, so we dont want to double-play it (may cause side effect if any), so we exclude it: FIRST TIME
		if( !Character->IsLocallyControlled() ) Character->PlayThrowMontage();

		break;

	case ECharacterState::ECS_Swapping:
		//Swap(){ Equip(1) + Equip(2) } ear self-handled, hence in client we need only PlayAnimation only
		//Character->PlaySwapMontage(); //for now
		if (!Character->IsLocallyControlled()) Character->PlaySwapMontage(); //will change into
		break;

	case ECharacterState::ECS_Unoccupied : 
		//if (bIsFiring)
		//{
		//	Input_Fire(bIsFiring); 
		//}
		break;
	}
}


void UCombatComponent::OnRep_IsAiming()
{
	////UPDATE: because 'bLocalIsAiming = InIsAming' is done in Locally controlled device ONLY, so you must add IsLocallyControlled in OnRep_ too, otherwise other clients will experience weird behaviour :D :D that is always false
	//Meaning we accept the fact that other-client will have DOUBLE zoom-in/out ; even so, they wouldn't know when the CD is the one controlling the character right? hell yeah!
	//i TEST it, it is true!
	if (Character && Character->IsLocallyControlled())
	{
		bIsAiming = bLocalIsAiming;
	}
}


void UCombatComponent::SetPOVForCamera(float DeltaTime)
{
	if (EquippedWeapon == nullptr) return;
	if (Character == nullptr || Character->GetCamera() == nullptr) return;

	if (bIsAiming)
	{
		CurrentPOV = FMath::FInterpTo(CurrentPOV, EquippedWeapon->GetPOV(), DeltaTime, EquippedWeapon->GetPOVInterpSpeed());
	}
	else
	{
		CurrentPOV = FMath::FInterpTo(CurrentPOV, DefaultPOV, DeltaTime, EquippedWeapon->GetPOVInterpSpeed());
	}

	Character->GetCamera()->SetFieldOfView(CurrentPOV);
} 

//Stephen call it SetHUDCrosshairs, this is to be put in Tick so we need to optimize it
void UCombatComponent::SetHUDPackageForHUD(float DeltaTime)
{
	if (Character == nullptr || Character->GetController() == nullptr ) return;

//DEMO-ready: can also use if (__ ==nullptr) __ = Cast<();

	BlasterPlayerController = BlasterPlayerController == nullptr ? Cast<ABlasterPlayerController>(Character->GetController()) : BlasterPlayerController;
	if (BlasterPlayerController)
	{
		BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(BlasterPlayerController->GetHUD()) : BlasterHUD;
	}

//DEMO-setup
	if (BlasterHUD == nullptr) return;

	FHUDPackage HUDPackage;

	if (EquippedWeapon) //theriocically this is enough
	{
		HUDPackage.CrosshairsCenter = EquippedWeapon->CrosshairsCenter;
		HUDPackage.CrosshairsRight = EquippedWeapon->CrosshairsRight;
		HUDPackage.CrosshairsLeft = EquippedWeapon->CrosshairsLeft;
		HUDPackage.CrosshairsTop = EquippedWeapon->CrosshairsTop;
		HUDPackage.CrosshairsBottom = EquippedWeapon->CrosshairsBottom;

		//expanding and shinking 
		if (Character->GetCharacterMovement()->IsFalling()) AdditionalJumpFactor = FMath::FInterpTo(AdditionalJumpFactor, JumpFactorMax, DeltaTime, 2.25f); 
		else AdditionalJumpFactor = FMath::FInterpTo(AdditionalJumpFactor, 0, DeltaTime, 10.f); 

		//I dont interp [0->max] because I like the sudden effect when I shoot/fire :D :D 
		if (bIsFiring) AdditinalFireFactor = FireFactorMax;
		else AdditinalFireFactor = FMath::FInterpTo(AdditinalFireFactor, 0, DeltaTime, 5.f);

		// 0.65f or equal to the inital 0.5f below are the matter of preference
		if (bIsAiming)	SubtractiveAimFactor = FMath::FInterpTo(SubtractiveAimFactor, AimFactorMax , DeltaTime, 5.f);
		else SubtractiveAimFactor = FMath::FInterpTo(SubtractiveAimFactor, 0 , DeltaTime, 5.f);


		//Velocity is interloated behind the scene so no need FMath::FInterpTo
		HUDPackage.ExpandFactor = Character->GetVelocity().Size2D() / 600.f 
			+ 0.5f  //give it an inital expan of 0.5f (rather than let 0) , so that you aim, it can shink.
			+ AdditionalJumpFactor 
			+ AdditinalFireFactor
			- SubtractiveAimFactor; 
		HUDPackage.Color = CrosshairsColor;
	}
	else //but in case you lose your weapon even if you did have one (out of bullets+) 
	{
		HUDPackage.CrosshairsCenter = nullptr;
		HUDPackage.CrosshairsRight = nullptr;
		HUDPackage.CrosshairsLeft = nullptr;
		HUDPackage.CrosshairsTop = nullptr;
		HUDPackage.CrosshairsBottom = nullptr;
	}

	BlasterHUD->SetHUDPackage(HUDPackage);
}

void UCombatComponent::Input_Fire(bool InIsFiring)
{
	bIsFiring = InIsFiring;

	if (CanFire())
	{
		bCanFire = false;

		//no need to check EquippedWeapon, you just check it in Canfire() LOL:
		switch (EquippedWeapon->GetFireType()) 
		{
		case EFireType::EFT_Projectile:
			FireProjectileWeapon(InIsFiring);
			break;

		case EFireType::EFT_HitScan:
			FireHitScanWeapon(InIsFiring);
			break;

		case EFireType::EFT_Shotgun:
			FireShotgunWeapon(InIsFiring);
			break;
		}

		//"let interrupting action to reset the interrupted action itself" technique:
		bLocalReloading = false;

	}
	//RecursiveTimer for automatic fire: must move it out here LOL,  to guarantee that bCanFire can be set to true at least!
	Start_FireTimer(); //this is the right place to call .SetTimer (which will be recursive very soon)
}


//this is currently called CD ONLY:
bool UCombatComponent::CanFire()
{
	//bLocalReloading is for client-side prection purpose, we take it into account to, so that we can't fire after we press Reload button, potentially fix relevant bugs:
	if (bLocalReloading) return false;

	return
		(bCanFire &&
			EquippedWeapon && EquippedWeapon->GetAmmo() > 0 &&
			//this make Fire can't interrupt any montage generally:
			CharacterState == ECharacterState::ECS_Unoccupied)
		||
		(bCanFire &&
			EquippedWeapon && EquippedWeapon && EquippedWeapon->GetAmmo() > 0 &&
			//this is an exception for Shotgun, even if it is Reloading:
			EquippedWeapon->GetWeaponType() == EWeaponType::EWT_Shotgun &&
			CharacterState == ECharacterState::ECS_Reloading
			);
}


void UCombatComponent::FireProjectileWeapon(bool InIsFiring)
{
	if (EquippedWeapon == nullptr) return;

	//ready the correct HitTarget
	FHitResult HitResult;
	DoLineTrace_UnderCrosshairs(HitResult);
	//GOLDEN2X:
	DoAction_Fire(InIsFiring, HitResult.ImpactPoint); //GOOD SPOT! the DC will have it locally first
	ServerInput_Fire(InIsFiring, HitResult.ImpactPoint, EquippedWeapon->GetFireDelay()); //~>MultiRPC~>All devices:: Weapon::Fire + ...
}

void UCombatComponent::FireHitScanWeapon(bool InIsFiring)
{
	if (EquippedWeapon == nullptr) return; 

	//ready the correct HitTarget
	FHitResult HitResult;
	DoLineTrace_UnderCrosshairs(HitResult);

	//this is the only different between Projectile and HitScan (except shotgun)
	FVector FinalHitTarget = EquippedWeapon->GetUseScatter() ? EquippedWeapon->RandomEndWithScatter(HitResult.ImpactPoint) : HitResult.ImpactPoint;

	//GOLDEN2X:
	DoAction_Fire(InIsFiring, FinalHitTarget); //GOOD SPOT! the DC will have it locally first
	ServerInput_Fire(InIsFiring, FinalHitTarget, EquippedWeapon->GetFireDelay()); //~>MultiRPC~>All devices:: Weapon::Fire + ...
}

void UCombatComponent::FireShotgunWeapon(bool InIsFiring)
{
	if (EquippedWeapon == nullptr) return;
	AHitScanWeapon_Shotgun* HitScanWeapon_Shotgun = Cast<AHitScanWeapon_Shotgun>(EquippedWeapon);
	if (HitScanWeapon_Shotgun == nullptr) return;

	//ready the correct HitTarget1
	FHitResult HitResult;
	DoLineTrace_UnderCrosshairs(HitResult);
	//ready an array of HitTarget2 (specialized for shotgun):
	TArray<FVector_NetQuantize> HitTargets;
	HitScanWeapon_Shotgun->RandomEndsWithScatter_Shotgun(HitResult.ImpactPoint, HitTargets);

	//GOLDEN2X: call version for shotgun:
	DoAction_Fire_Shotgun(InIsFiring, HitTargets, HitScanWeapon_Shotgun); //GOOD SPOT! the DC will have it locally first
	ServerInput_Fire_Shotgun(InIsFiring, HitTargets, HitScanWeapon_Shotgun,EquippedWeapon->GetFireDelay()); //~>MultiRPC~>All devices:: Weapon::Fire + ...
}

void UCombatComponent::ServerInput_Fire_Implementation(bool InIsFiring, const FVector_NetQuantize& Target, float FireDelay)
{
	////move up to serverRPC
	//bIsFiring = InIsFiring; //We can't remove it here as this is for replication perupose :)

	MulticastInput_Fire(InIsFiring, Target);

	//I decide to call it here, Hell no! if you call it here, Ammo will be subtracted even before the PlayFireMontage happen - which we're not even sure it will pass it next condition to be up there :D :D :
	//if (EquippedWeapon) EquippedWeapon->UpdateHUD_Ammo();
}

//when it reaches this TIRE it must pass CanFire() and have Ammo > 0, So we dont need to check it again I suppose:
void UCombatComponent::MulticastInput_Fire_Implementation(bool InIsFiring, const FVector_NetQuantize& Target)
{
	//exclude the CD, as it is done locally in TIRE already!
	if (Character && !Character->IsLocallyControlled())
	{
		DoAction_Fire(InIsFiring, Target);
	}
}

void UCombatComponent::DoAction_Fire(bool InIsFiring, const FVector_NetQuantize& Target)
{
	//note that because the machine to be called is different, so put this line here or in the HOSTING function 'could' make a difference generally lol:
	if (Character == nullptr || EquippedWeapon == nullptr) return;

	//move up to serverRPC
	bIsFiring = InIsFiring; //We can't remove it here as this is for replication perupose :)

	//Option1 to fix Fire intterupt Reload back when timer reach:
	//when it reaches this TIRE it must pass TIRE1::CanFire() and have Ammo > 0, So we dont need to check it again I suppose:
	if (
		(bIsFiring &&
			//this make Fire can't interrupt any montage generally:
			CharacterState == ECharacterState::ECS_Unoccupied)
		||
		(bIsFiring &&
			//this is an exception for Shotgun, even if it is Reloading:
			EquippedWeapon->GetWeaponType() == EWeaponType::EWT_Shotgun &&
			CharacterState == ECharacterState::ECS_Reloading))
	{
		Character->PlayFireMontage();

		EquippedWeapon->Fire(Target); //instead of member HitTarget, now you can remove it!

		//it should be here: well stephen even call it in Weapon::Fire instead, i think it make more sense if we subtract the ammo after we see the FireSound+Effect in Weapon::Fire right (in fact Fire via FireAnimation_InWeapon)?
		// /So optionally you can move it into the Weapon::Fire() , put it at bottom if you want.
		//UPDATE: this has been move into Weapon::Fire, also run in all devices at this TIRE to match stephen
			//if (Character->HasAuthority()) EquippedWeapon->UpdateHUD_Ammo();

		/*
		Stepehen directly set it back to Unoccupied right in "Combat::MulticastInput_Fire"
		, this is the reason:
		(1) the fire montage is "TOO short"
		(2) "Combat::MulticastInput_Fire" is the ONLY play trigger the PlayFireMontage()
		=hell yeah!
		*/
		if (Character->HasAuthority()) CharacterState = ECharacterState::ECS_Unoccupied;
	}
}

//for shotgun:
void UCombatComponent::ServerInput_Fire_Shotgun_Implementation(bool InIsFiring, const TArray<FVector_NetQuantize>& HitTargets, AHitScanWeapon_Shotgun* HitScanWeapon_Shotgun, float FireDelay)
{
	MulticastInput_Fire_Shotgun(InIsFiring, HitTargets, HitScanWeapon_Shotgun);
}

void UCombatComponent::MulticastInput_Fire_Shotgun_Implementation(bool InIsFiring, const TArray<FVector_NetQuantize>& HitTargets, AHitScanWeapon_Shotgun* HitScanWeapon_Shotgun)
{
	//exclude the CD, as it is done locally in TIRE already!
	if (Character && !Character->IsLocallyControlled())
	{
		DoAction_Fire_Shotgun(InIsFiring, HitTargets, HitScanWeapon_Shotgun);
	}
}

void UCombatComponent::DoAction_Fire_Shotgun(bool InIsFiring, const TArray<FVector_NetQuantize>& HitTargets, AHitScanWeapon_Shotgun* HitScanWeapon_Shotgun)
{
	//note that because the machine to be called is different, so put this line here or in the HOSTING function 'could' make a difference generally lol:
	if (Character == nullptr || HitScanWeapon_Shotgun == nullptr) return;

	//move up to serverRPC
	bIsFiring = InIsFiring; //We can't remove it here as this is for replication perupose :)

	//Option1 to fix Fire intterupt Reload back when timer reach:
	//when it reaches this TIRE it must pass TIRE1::CanFire() and have Ammo > 0, So we dont need to check it again I suppose:
	if (
		(bIsFiring &&
			//this make Fire can't interrupt any montage generally:
			CharacterState == ECharacterState::ECS_Unoccupied)
		||
		(bIsFiring &&
			//this is an exception for Shotgun, even if it is Reloading:
			HitScanWeapon_Shotgun->GetWeaponType() == EWeaponType::EWT_Shotgun &&
			CharacterState == ECharacterState::ECS_Reloading))
	{
		Character->PlayFireMontage();

		//This is the KEY different lines:
		HitScanWeapon_Shotgun->ShotgunFire(HitTargets); //instead of member HitTarget, now you can remove it!

		//it should be here: well stephen even call it in Weapon::Fire instead, i think it make more sense if we subtract the ammo after we see the FireSound+Effect in Weapon::Fire right (in fact Fire via FireAnimation_InWeapon)?
		// /So optionally you can move it into the Weapon::Fire() , put it at bottom if you want.
		if (Character->HasAuthority()) HitScanWeapon_Shotgun->UpdateHUD_Ammo();
		/*
		Stepehen directly set it back to Unoccupied right in "Combat::MulticastInput_Fire"
		, this is the reason:
		(1) the fire montage is "TOO short"
		(2) "Combat::MulticastInput_Fire" is the ONLY play trigger the PlayFireMontage()
		=hell yeah!
		*/
		if (Character->HasAuthority()) CharacterState = ECharacterState::ECS_Unoccupied;
	}
}

//define predicate (function retturn bool) for 'FUNCTION( , WithValidation)' , no need to re-declare it in .h:
//it has the same signature as the ServerRPC, same name but with suffix _Validate:
bool UCombatComponent::ServerInput_Fire_Validate(bool InIsFiring, const FVector_NetQuantize& Target, float FireDelay)
{
	//careful you dont want to kick players when they dont have a weapon :D 
	if (EquippedWeapon == nullptr) true; 

	//me: floating point type can have a small amount of discrepency/inaccurcy, so doing like this is absolutely stupid, especially you compare them in different devices:

	//return FireDelay == EquippedWeapon->GetFireDelay(); //this is stupid
	//return (abs(EquippedWeapon->GetFireDelay() - FireDelay) <= 0.001f); //this is the orgin of FMath functions

	//stephen: you can return either of them
	bool bIsNearlyZero = FMath::IsNearlyZero(EquippedWeapon->GetFireDelay() - FireDelay, 0.001f); //10^-3
	bool bIsNearlyEqual = FMath::IsNearlyEqual(EquippedWeapon->GetFireDelay(), FireDelay); //10^-8 by default
	return bIsNearlyEqual; // || return bIsNearlyZero;
}

bool UCombatComponent::ServerInput_Fire_Shotgun_Validate(bool InIsFiring, const TArray<FVector_NetQuantize>& HitTargets, class AHitScanWeapon_Shotgun* HitScanWeapon_Shotgun, float FireDelay)
{
	//careful you dont want to kick players when they dont have a weapon :D 
	if (EquippedWeapon == nullptr) true;

	//me: floating point type can have a small amount of discrepency/inaccurcy, so doing like this is absolutely stupid, especially you compare them in different devices:

	//return FireDelay == EquippedWeapon->GetFireDelay(); //this is stupid
	//return (abs(EquippedWeapon->GetFireDelay() - FireDelay) <= 0.001f); //this is better

	//stephen: you can return either of them
	//bool bIsNearlyZero = FMath::IsNearlyZero(EquippedWeapon->GetFireDelay() - FireDelay, 0.001f); //10^-3
	bool bIsNearlyEqual = FMath::IsNearlyEqual(EquippedWeapon->GetFireDelay(), FireDelay, 0.001f); //10^-8 by default
	return bIsNearlyEqual;

}

void UCombatComponent::Start_FireTimer()
{
	if (EquippedWeapon == nullptr) return;
	GetWorld()->GetTimerManager().SetTimer(TimeHandle, this, &ThisClass::FireTimer_Callback, EquippedWeapon->GetFireDelay());
}

void UCombatComponent::FireTimer_Callback()
{
	//We must do this before (*), as we need it to be true in case we actually release the fire button
	bCanFire = true;

	if (EquippedWeapon == nullptr) return;
	if (!bIsFiring || !EquippedWeapon->GetIsAutomatic()) return; //(*)

	//I move it up here, shouldn't put any code below a RECURSIVE code:
	//this is the best place to reload, Stephen said, as this reach after DelayTime and eveything has been settle correctly! it is true I didn't have side effect as I release the Fire button and lose one more button without playing FireSound lol

	//Option2 to fix Fire intterupt Reload back when timer reach:
	if(CharacterState == ECharacterState::ECS_Unoccupied) Input_Fire(bIsFiring);

	if (EquippedWeapon->IsEmpty() && CarriedAmmo > 0)
	{
		Input_Reload();
	}
}

void UCombatComponent::Input_Fire_WithoutAssingmentLine()
{
	//if (bIsFiring == false) GetWorld()->GetTimerManager().ClearTimer(TimeHandle);

	//can factorize these in to Combat::Fire() to be used instead of Input_Fire itself in timer_callback
	//you may wan to factorize this block into CanFire(), but I say no, I let it be here
	{
		//news:
		//if (InIsFiring == false) return;
		//this is a part of stopping Gun can't stop firing:
		if (bCanFire == false) return;
		//extra condition about ammo:
		if (EquippedWeapon == nullptr || EquippedWeapon->GetAmmo() <= 0 || CharacterState != ECharacterState::ECS_Unoccupied) return;
	}

	//set it back to false as a part of preventing we spam the fire button during WaitTime to reach Timer callback
	bCanFire = false;

	FHitResult HitResult;
	DoLineTrace_UnderCrosshairs(HitResult);

	ServerInput_Fire(bIsFiring, HitResult.ImpactPoint, EquippedWeapon->GetFireDelay()); //rather than member HitPoint

	Start_FireTimer(); //this is the right place to call .SetTimer (which will be recursive very soon)
}

//Instead of doing it from AWeapon::Equip() we do it here, hence it should do all the stuff we usually did here
void UCombatComponent::Equip(AWeapon* InWeapon)
{
	if (InWeapon == nullptr || Character == nullptr) return;
	//I decide to not let it equip if we're currenting do whatever (say Reloading/Throwing)
	if (CharacterState != ECharacterState::ECS_Unoccupied) return;
    
	//ONLY when we already have a weapon amd SecondWeapon is empty should we attach it to backpack instead:
	//can factorize it into EquipSecondWeapon()
	if (EquippedWeapon != nullptr && SecondWeapon == nullptr)
	{
		EquipSecondWeaponToBackpack(InWeapon);
	}
	//otherwise: In case we dont have any weapon (equip to hand) or have both weapon (drop from hand and equip to hand) we handle like in the last lesson:
	//can factorize it into EquipPrimaryWeapon
	else
	{
		DropCurrentWeaponIfAny();

		//this is new weapon:
		EquippedWeapon = InWeapon;  // -->trigger OnRep_EquippedWeapon, why my OnRep_ didn't trigger?
		                            // since we already check on InWeapon above so it can't be nullptr after this line
		EquippedWeapon->PlayEquipSound(Character); //just for cosmetic

		//I move this on top with the hope that it is replicated before OnRep_WeaponState
		EquippedWeapon->SetOwner(Character);

		EquippedWeapon->SetWeaponState(EWeaponState::EWS_Equipped) ;

		//this better after SetOwner:
		ExtractCarriedAmmoFromMap_UpdateHUDAmmos_ReloadIfEmpty();

		//these 2 lines are optional, so I will remove it anyway
		//bIsAutomatic = EquippedWeapon->GetIsAutomatic();
		//FireDelay = EquippedWeapon->GetFireDelay();

	//factorize this into 'Ready Collision and physics setup before Attachment or Simulation' function:
		//Our SMG dont need these to locally similated, move it out fix the bug:
		EquippedWeapon->GetWeaponMesh()->SetSimulatePhysics(false); //TIRE1
		EquippedWeapon->GetWeaponMesh()->SetEnableGravity(false);   //TIRE3 - no need nor should you do this LOL

		if (EquippedWeapon->GetWeaponType() == EWeaponType::EWT_SMG)
		{
			EquippedWeapon->GetWeaponMesh()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		}
		else
		{
			EquippedWeapon->GetWeaponMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}

		AttachEquippedWeaponToRightHandSocket();

	//we want after we have a weapon on hand, we want Actor facing in the same direction as Camera!
	//this need only to be set ONCE, so we dont need to repeat this in EquipSecondWeapon(), so I only need it be here:
		Character->GetCharacterMovement()->bOrientRotationToMovement = false; //at first it is true
		Character->bUseControllerRotationYaw = true; //at first it is false
	}
}

//it is exactly the same Equip_SpecializedforSwap, except we remove 'if (CharacterState != ECharacterState::ECS_Unoccupied) return;' that we already check at first and then change CharacaterState early then the point we actually do the Equip:
void UCombatComponent::Equip_SpecializedforSwap(AWeapon* InWeapon)
{
	if (InWeapon == nullptr || Character == nullptr) return;

	//if (CharacterState != ECharacterState::ECS_Unoccupied) return; //REMOVE!

	//ONLY when we already have a weapon amd SecondWeapon is empty should we attach it to backpack instead:
	//can factorize it into EquipSecondWeapon()
	if (EquippedWeapon != nullptr && SecondWeapon == nullptr)
	{
		EquipSecondWeaponToBackpack(InWeapon);
	}
	//otherwise: In case we dont have any weapon (equip to hand) or have both weapon (drop from hand and equip to hand) we handle like in the last lesson:
	//can factorize it into EquipPrimaryWeapon
	else
	{
		DropCurrentWeaponIfAny();

		//this is new weapon:
		EquippedWeapon = InWeapon;  // -->trigger OnRep_EquippedWeapon, why my OnRep_ didn't trigger?
		// since we already check on InWeapon above so it can't be nullptr after this line
		EquippedWeapon->PlayEquipSound(Character); //just for cosmetic

		//I move this on top with the hope that it is replicated before OnRep_WeaponState
		EquippedWeapon->SetOwner(Character);

		EquippedWeapon->SetWeaponState(EWeaponState::EWS_Equipped);

		//this better after SetOwner:
		ExtractCarriedAmmoFromMap_UpdateHUDAmmos_ReloadIfEmpty();

		//these 2 lines are optional, so I will remove it anyway
		//bIsAutomatic = EquippedWeapon->GetIsAutomatic();
		//FireDelay = EquippedWeapon->GetFireDelay();

	//factorize this into 'Ready Collision and physics setup before Attachment or Simulation' function:
		//Our SMG dont need these to locally similated, move it out fix the bug:
		EquippedWeapon->GetWeaponMesh()->SetSimulatePhysics(false); //TIRE1
		EquippedWeapon->GetWeaponMesh()->SetEnableGravity(false);   //TIRE3 - no need nor should you do this LOL

		if (EquippedWeapon->GetWeaponType() == EWeaponType::EWT_SMG)
		{
			EquippedWeapon->GetWeaponMesh()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		}
		else
		{
			EquippedWeapon->GetWeaponMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}

		AttachEquippedWeaponToRightHandSocket();

		//we want after we have a weapon on hand, we want Actor facing in the same direction as Camera!
		//this need only to be set ONCE, so we dont need to repeat this in EquipSecondWeapon(), so I only need it be here:
		Character->GetCharacterMovement()->bOrientRotationToMovement = false; //at first it is true
		Character->bUseControllerRotationYaw = true; //at first it is false
	}
}


void UCombatComponent::EquipSecondWeaponToBackpack(AWeapon* InWeapon)
{
	if (InWeapon == nullptr) return;

	//there are a lot of code that we dont need for SecondWeapon, there is only what we need for it:
	SecondWeapon = InWeapon; //trigger OnRep_SecondWeapon
	SecondWeapon->PlayEquipSound(Character); //just for cosmetic

	SecondWeapon->SetOwner(Character);

	//new state, go to SetWeaponState and OnRep_WeaponState to UPDATE and adapt them:
	SecondWeapon->SetWeaponState(EWeaponState::EWS_EquippedSecond); //WORK1

	//physics part:
	SecondWeapon->GetWeaponMesh()->SetSimulatePhysics(false); //TIRE1
	SecondWeapon->GetWeaponMesh()->SetEnableGravity(false);   //TIRE3 - no need nor should you do this
	//I feel like I want SMG simulate even if it is on the backack:
	if (SecondWeapon->GetWeaponType() == EWeaponType::EWT_SMG)
	{
		SecondWeapon->GetWeaponMesh()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
	else
	{
		SecondWeapon->GetWeaponMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	//Adapt this into 'ToSecondWeaponSocket/Backpack' instead
	AttachSecondWeaponToSecondWeaponSocket(); //WORK2 = done
}

//like Combat::Equip(), this currently run the server only, OnRep_EquippedWeapon + OnRep_WeaponState handle next for client parts
void UCombatComponent::SwapWeapons()
{
	if (CharacterState != ECharacterState::ECS_Unoccupied) return;

	//Now it has Swapping State, and has animation to be played (as soon as possible):
	CharacterState = ECharacterState::ECS_Swapping; //WORK1 -> create more case in OnRep_CharacterState and handle it for client parts.
	Character->PlaySwapMontage();

//these code NOW must be done at some point of the AM_SwapMontage:
		//AWeapon* TempWeapon = EquippedWeapon;
	//this make  EquippedWeapon = SecondWeapon, because the 'else' kicks in
	//but after this SecondWeapon = itself still, it still have value value!
		//Equip(SecondWeapon); //self-handled
	//OPTION1: this work like a charm
		//EquipSecondWeaponToBackpack(TempWeapon); //self-handled
	//OPTION2: this also work too, AT LEAST in the server, I believe:
	//if you forget this line, you can't get the 'if' and it keep drop and then attach back the same weapon LOL, because it need SecondWeapon to be nullptr, where currently SecondWeapon and EquippedWeapon are shared the same weapon address
		//SecondWeapon = nullptr;
		//Equip(TempWeapon);
}

//becareful: as long as the montage anim is playing in all devices, all of these code are run in all devices directly, not just in the server like the code above
//so you may want to put 'if(HasAuthority()) for them:
void UCombatComponent::StartSwapAttachment()
{
	//We dont want it to be delayed for these action so we comment it out, it is fine when the server reach back and does the same thing (these code below are 'double-immue doaction'
		//if (Character && !Character->HasAuthority()) return; 

	//these code must be done at some point of the AM_SwapMontage:
	AWeapon* TempWeapon = EquippedWeapon;

	//this make  EquippedWeapon = SecondWeapon, because the 'else' kicks in
	//but after this SecondWeapon = itself still, it still have value value!
	//Equip(SecondWeapon); //self-handled
	Equip_SpecializedforSwap(SecondWeapon);

	//OPTION1: this work like a charm
	EquipSecondWeaponToBackpack(TempWeapon); //self-handled

	//OPTION2: this also work too, AT LEAST in the server, I believe:
	//if you forget this line, you can't get the 'if' and it keep drop and then attach back the same weapon LOL, because it need SecondWeapon to be nullptr, where currently SecondWeapon and EquippedWeapon are shared the same weapon address
		//SecondWeapon = nullptr;
		//Equip(TempWeapon);
}

//becareful: as long as the montage anim is playing in all devices, all of these code are run in all devices directly, not just in the server like the code above
//so you may want to put 'if(HasAuthority()) for them:
void UCombatComponent::SwapEnd()
{
	if (Character == nullptr) return;

	//perhaps this will make the CD to use back FRABRIK a little bit late? no I test it, the condition bUseFrabrik in last lesson luckily solve this problem here too!? but HOW LOl?
	//well it is because currently the 'StartSwapAttachment()' is currently subject to the same delay LOL
	// So if we remove the HasAuthority(), which we will, we will have that problem back!

	//if it doesn't work simply create yet another extra bool for CD, the step is the same!!!
	if (Character && Character->HasAuthority())
	{
		CharacterState = ECharacterState::ECS_Unoccupied;//self-replicated
	}
	//this to be done in all devices or at least in the CD (because only the CD have problem4: FRABRIK working all the time), but anyway overkill is good so LOL:
	Character->bIsLocalSwapping = false; //go to AnimInstance and add this restriction for bUseFABRIK!

}


//call this specialized function with ThrowEnd():
void UCombatComponent::AttachEquippedWeaponToRightHandSocket()
{
	if (EquippedWeapon == nullptr || Character == nullptr || Character->GetMesh() == nullptr) return;
	const USkeletalMeshSocket* RightHandSocket = Character->GetMesh()->GetSocketByName(FName("RightHandSocket"));
	if (RightHandSocket) RightHandSocket->AttachActor(EquippedWeapon, Character->GetMesh());
}

//can call this function with PlayThrowMontage/Input_Throw, make sure it is called in server for replication:
void UCombatComponent::AttachEquippedWeaponToLeftHandSocket()
{
	if (EquippedWeapon == nullptr || Character == nullptr || Character->GetMesh() == nullptr) return;

	//Add "LeftHandSocket" for the rest of weapon:
	FName SocketName("LeftHandSocket");
	
	//Add a socket 'LeftHandSocket_Pistol' for Pistol and Pistol, SMG 
	if (EquippedWeapon->GetWeaponType() == EWeaponType::EWT_Pistol ||
		EquippedWeapon->GetWeaponType() == EWeaponType::EWT_SMG)
	{
		SocketName = FName("LeftHandSocket_Pistol");
	}

	const USkeletalMeshSocket* LeftHandSocket = Character->GetMesh()->GetSocketByName(SocketName);
	if (LeftHandSocket) LeftHandSocket->AttachActor(EquippedWeapon, Character->GetMesh());
}

//This is for SecondWeapon: go and add 'SecondWeaponSocket' in SKM_inChar
void UCombatComponent::AttachSecondWeaponToSecondWeaponSocket()
{
	if (SecondWeapon == nullptr || Character == nullptr || Character->GetMesh() == nullptr) return;
	const USkeletalMeshSocket* RightHandSocket = Character->GetMesh()->GetSocketByName(FName("SecondWeaponSocket"));
	if (RightHandSocket) RightHandSocket->AttachActor(SecondWeapon, Character->GetMesh());
}

void UCombatComponent::ExtractCarriedAmmoFromMap_UpdateHUDAmmos_ReloadIfEmpty()
{
	//you must update CarriedAmmo from the map first before you check on it LOL
	if (CarriedAmmoMap.Contains(EquippedWeapon->GetWeaponType()))
	{
		//map[KEY] = value of that KEY, remeMber? 
		CarriedAmmo = CarriedAmmoMap[EquippedWeapon->GetWeaponType()];
		//CarriedAmmo = *(CarriedAmmoMap.Find(EquippedWeapon->GetWeaponType())) ;
	}
	CheckAndSetHUD_CarriedAmmo();

	//checking if Ammo is 0 and CarriedAmmo to Auto-Reload it:

	//if (EquippedWeapon->GetAmmo() <= 0 && CarriedAmmo <= 0) return; //this line is stupid
	if (EquippedWeapon->GetAmmo() <= 0 && CarriedAmmo > 0)
	{
		Input_Reload(); //self-organized (i.e self replicated in some way)
	}

	//UPDATE: this will help to fix other client NEW owner update its Weapon::Ammo when pick it up: you can also do it locally inside Weapon::CheckAndSetHUD_Ammo(), but I say no, it make it less flexibility, currently Equip()::ThisHostingfunction trigger in the server only:
	EquippedWeapon->ClientSetAmmo(EquippedWeapon->GetAmmo());

	//I decide to call it here, safe enough from SetOwner above LOL:
	EquippedWeapon->CheckAndSetHUD_Ammo(); //no need, Input_Reload is self-handled already? no in case it can't pass the if check, then this will be needed LOL
}

void UCombatComponent::DropCurrentWeaponIfAny()
{
	//News: drop current weapon (if any) before you pick a new one
	if (EquippedWeapon)
	{
		EquippedWeapon->Drop();
		//turn on CustomDepth back here or in AWeapon::Drop() = recommended!
	}
}

//this is to fix the owning client can't update these on itself (weird case, can't explain :D )
void UCombatComponent::OnRep_EquippedWeapon()
{
	if (EquippedWeapon == nullptr || EquippedWeapon->GetWeaponMesh() ==nullptr || Character == nullptr) return;

	EquippedWeapon->PlayEquipSound(Character);
	//the condition is optional, but since I know only that owning client have problem, so I only need to let this code run on that client LOL, hell yeah!

	//Note that I've been thinking about extra the code, setting WeaponState, Setting physics, but unfortunately WeaponState is not public member nor did I want to move it to public sesson to break my UNIVERSAL pattern :)
	//Hence the only choice1: is to follow stephen, focus on case=Equipped only
	//Choice2: create another exclusive setter SetWeaponStateOnly()

	////choice1:
	//EquippedWeapon->SetWeaponState(EWeaponState::EWS_Equipped); //go and restrict SetWeaponState::case_Equipped that the local can't touch Sphere collision -->Go and add If(HasAuthority()), but you dont have to setup physics here.
	
	//choice2: 
	EquippedWeapon->SetWeaponState_Only(EWeaponState::EWS_Equipped); //without needing to break the GLOBAL pattern, but you will have to setup physics here

	//Our SMG dont need these to locally similated, move it out fix the bug:
	EquippedWeapon->GetWeaponMesh()->SetSimulatePhysics(false); //TIRE1
	EquippedWeapon->GetWeaponMesh()->SetEnableGravity(false);   //TIRE3 - no need nor should you do this LOL
	if (EquippedWeapon->GetWeaponType() != EWeaponType::EWT_SMG)
	{
		EquippedWeapon->GetWeaponMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (EquippedWeapon->GetWeaponType() == EWeaponType::EWT_SMG)
	{
		EquippedWeapon->GetWeaponMesh()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}

	//even if Attachment action is replicated we call it here to make sure it works
	const USkeletalMeshSocket* RightHandSocket = Character->GetMesh()->GetSocketByName(FName("RightHandSocket"));
	if (RightHandSocket) RightHandSocket->AttachActor(EquippedWeapon, Character->GetMesh());

	//Why only owning device? well because Stephen want controlling device to have AimOffset and Simulated proxy to have 'TurnInPlace' behaviour remember?
	//however the AUTHO proxy should be the same too, hence we should go back to Equip() and NOT let it to be these value if not controlling. Anyway it is auto-fixed anway in Tick so dont worry LOL
	if (Character->IsLocallyControlled())
	{
		Character->GetCharacterMovement()->bOrientRotationToMovement = false; //at first it is true
		Character->bUseControllerRotationYaw = true; //at first it is false

	}

}

//we only need to repeat whatever needed for secondweapon:
void UCombatComponent::OnRep_SecondWeapon()
{
	if (SecondWeapon == nullptr || SecondWeapon->GetWeaponMesh() == nullptr || Character == nullptr) return;

	SecondWeapon->PlayEquipSound(Character);
	
	SecondWeapon->SetWeaponState_Only(EWeaponState::EWS_EquippedSecond); 

	//Our SMG dont need these to locally similated, move it out fix the bug:
	SecondWeapon->GetWeaponMesh()->SetSimulatePhysics(false); //TIRE1
	SecondWeapon->GetWeaponMesh()->SetEnableGravity(false);   //TIRE3 - no need nor should you do this LOL
	if (SecondWeapon->GetWeaponType() != EWeaponType::EWT_SMG)
	{
		SecondWeapon->GetWeaponMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (SecondWeapon->GetWeaponType() == EWeaponType::EWT_SMG)
	{
		SecondWeapon->GetWeaponMesh()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}

	//even if Attachment action is replicated we call it here to make sure it works
	AttachSecondWeaponToSecondWeaponSocket();
}


//Pickup::Overlap trigger in server -->this Combat::PickupAmmo also trigger in the server first
void UCombatComponent::PickupAmmo(EWeaponType InWeaponType, uint32 InAmmoAmmount)
{
	//Increase Ammo: both of them are self-replicated if called in the server, yes currently is
	if (CarriedAmmoMap.Contains(InWeaponType))
	{
		//this only update the map (to be retrieved later) but doesn't in fact update the CarriedAmmo in case it match the current EquippedWeapon's Type at all:
		CarriedAmmoMap[InWeaponType] = FMath::Clamp(CarriedAmmoMap[InWeaponType] + InAmmoAmmount, 0, MaxCarriedAmmo);
		//This will actually change the Carried Ammo in worst case:
		if (EquippedWeapon && InWeaponType == EquippedWeapon->GetWeaponType())
		{
			CarriedAmmo = CarriedAmmoMap[InWeaponType]; //OnRep_CarriedAmmo will trigger for client part
		}
		//it check on current CarriedAmmo and update HUD, if the WeaponType's Pickup doesn't match WeaponType's EquippedWeapon then this wont cause any visual effect :D :D
		CheckAndSetHUD_CarriedAmmo(); //this for the server part ONLY
	}

	//If it matches the type of EquippedWeapon and EquippedWeapon::Ammo = 0 then auto-reload it;
	if (EquippedWeapon  && EquippedWeapon->IsEmpty() && InWeaponType == EquippedWeapon->GetWeaponType())
	{
		Input_Reload(); //this is self-handled, no matter where you call it
	}
}

//currently this could only call in IsLocallyControlled() /autonompus proxy - hence 'if(IsLocallyControlled()' become redudant!
void UCombatComponent::SetIsAiming(bool InIsAiming)
{
	if (Character == nullptr || EquippedWeapon == nullptr) return;

	//for client-side prediction purpose:
		//this OFFICAL value subject to replication and lag:
	bIsAiming = InIsAiming;
		//this LOCAL value subject ONLY to 'local input', indepently from replication and never being corrected, it is responsive and change when you press and release key and nothing more:
		//UPDATE: because this is done in Locally controlled device, so you must add IsLocallyControlled in OnRep_ too, otherwise other clients will experience weird behaviour :D :D that is always false
	bLocalIsAiming = InIsAiming; //extra bPressed for CD to use locally

	ServerSetIsAiming(InIsAiming);

	//need ONLY for locally controlled proxy:
		//option2: call BP_function(InIsAiming) here
	if (Character->IsLocallyControlled() && EquippedWeapon->GetWeaponType() == EWeaponType::EWT_SniperRifle)
	{
		Character->ShowScopeWidget(InIsAiming);
	}
}

void UCombatComponent::ServerSetIsAiming_Implementation(bool InIsAiming) //REPLACE
{
	if (Character)
	{
		bIsAiming = InIsAiming;
	//self-replicated code 
		//UCharacterMovemenComponent::MaxWalkSpeed is self-replicated by UE5?
		//well if we change MaxWalkSpeed in local machines and it is being corrected back by UE5
		//, then it suggest that MaxWalkSpeed is self- replicated
		//, meaning calling 'MaxWalkSpeed = new value' in server will update it back to client proxies as well!
		//also this is the ONLY place I find this code, so the evidence proves it LOL:
		if (Character->GetCharacterMovement()) Character->GetCharacterMovement()->MaxWalkSpeed = InIsAiming ? AimWalkSpeed : MaxWalkSpeed_Backup;
	//non-replicated code must be put inside extra MulticastRPC - if any:
		//no code
	}
}

FVector UCombatComponent::DoLineTrace_UnderCrosshairs(FHitResult& LineHitResult)
{
	if (GetWorld() == nullptr || GEngine == nullptr) return FVector{};
	
	FVector2D ViewportSize;
	if(GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}
	FVector2D ViewportCenterLocation = FVector2D(ViewportSize.X / 2.f, ViewportSize.Y / 2.f); //Stephen call it "CrosshairsLocation", this is relative to Screen 2D coordinates 

    //WAY1: me, not sure it is exactly like way2 I test them both, either of them are working the same, in multiplayer test, each device has its own trace line and draw its own sphere, so one see the other no matter WAY1 or WAY2 (which is good)
	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	//WAY2: stephen, he said player index 0 here will be always the controller controlling the controlled pawn in the local device, even if it is multiplayer game! so index is also relative huh? :D :D
	APlayerController* PlayerController1 = UGameplayStatics::GetPlayerController(this, 0);
	
	FVector WorldLocation;
	FVector WorldDirection; //it will be modified and normalized 

	bool bIsSuccessful = 
		UGameplayStatics::DeprojectScreenToWorld(
			PlayerController1, //test both
			ViewportCenterLocation,
			WorldLocation,
			WorldDirection
	    );
	
	if (!bIsSuccessful) return FVector{}; //this is not the reason that why a device only see its own DebugSphere

	FVector Start = WorldLocation; //Start is Exactly Camera location, I didn't realize it LOL

	//Stephen: (Character->GetActorLocation() - Start).Size() + StartTraceOffset
	float StartOffset = (Character->GetActorLocation() - Start).Size2D() / abs(FMath::Cos(Character->GetAO_Pitch() * 3.14f / 180.f ) ) + ExtraStartOffset;

	Start += WorldDirection * StartOffset;

	FVector End = Start + WorldDirection * 80000; //Direction is current a vector unit with length=1 only, so yeah!

	//DrawDebugSphere(GetWorld(), Start, SphereRadius, 12.f, FColor::Green, bDrawConsistentLine);

	//GEngine->AddOnScreenDebugMessage(-1, 2, FColor::Blue, FString::Printf(TEXT("TraceOffset: %f , Pitch: %f , Cos: %f "), StartOffset, Character->GetAO_Pitch(), FMath::Cos(Character->GetAO_Pitch() * 3.14f / 180.f)));	
	
	GetWorld()->LineTraceSingleByChannel(
		LineHitResult,
		Start,
		End,
		ECollisionChannel::ECC_Visibility //almost all things block visibility by default
	);

	if (LineHitResult.bBlockingHit == false)
	{
		LineHitResult.ImpactPoint = End;
	}

	HitTarget = LineHitResult.ImpactPoint;

	//HitTarget = LineHitResult.ImpactPoint; //ImpactPoint now can be relied on in either case after the if!

	//DrawDebugSphere(GetWorld(), LineHitResult.ImpactPoint, SphereRadius, 12.f, FColor::Red, bDrawConsistentLine);

	return End;
}

void UCombatComponent::ReloadEnd()
{
	if (Character == nullptr) return;
	//this may cause "LEGENDARYcase", if so, simpl remove 'HasAuthority()' will overcome it LOL = no it's not!
		//because the inner code is self-replicated, so add HasAuthority is optional. if you wan the server first, then add HasAuthority, if you want all copies run over, then remove it (for cosmetic action) 
	//for client-side prediction you can remove 'HasAuthority()' and it solve side problem immediately (Unoccupied arrive late for FABRIK to snap the hand back) , but as universal rules, it is not recommended for 'Replicated enum state var' at all! 

	//only CD's bLocalReloading change to true from Input_X, only CD need to make a change back here: (I dont know why stephen didn't add if(CD) :d :d
	//later on stephen add 'bLocalReloading' to CanFire()
	if (Character->IsLocallyControlled()) bLocalReloading = false;

	//client must wait 'ping' after reach ReloadEnd() to do other actions, say fire, again
	//so stephen idea is that we also send ClientRPC( __ = ECS_Unoccupied) AFTER we play ReloadAnimation in the server
	if (Character->HasAuthority())
	{
		CharacterState = ECharacterState::ECS_Unoccupied;
		//this make my clients can't stop firing, it is because of 'late replication', currently bIsFiring isn't setup very clean in GOLDEN2, it is like a shit LOL, nor should we replicateit LOL
		//continue to fire if bFiring is still true (still holding the Fire key):
		// FOR NOW, I comment it out:
		// the code inside the is meant to be run in the server only, you can't let it run in the client nor should you also call this in OnRep!
	}

	//it should originate from the Owning client , NOT the server, this fix!!!:
	if(Character->IsLocallyControlled())
	{
		if (bIsFiring)
		{
			//you may be tempted to pass in "true", but it can still be changed right?
			Input_Fire(bIsFiring);
		}
	}
	//If you want..., you call this either here or in Combat::ServerInput_Reload, not both:
	
	//if (Character->HasAuthority()) UpdateHUD_CarriedAmmo();
	UpdateHUD_CarriedAmmo(); //try achieve client-side prediction when ReloadEnd reach locally, go and check if the code insde is self-handled or DIA! - it works like a charm
}

void UCombatComponent::ReloadOneAmmo()
{
	if (Character == nullptr) return;

	//Not sure if this is still appropriate? this can be called 4 times lol, it will even set to Unoccupied after first load, and any other Montage can intterupt it (including Fire(wanted), React(depdens): 
	//if you dont like this behaviour simply move it into if(CarriedAmmo =0 || IsFull()) together with JumpToShotgunEnd() in the UpdateHUD_CarriedAmmo_SpecializedForShotgun() bellow! and its hosting function is currently having HasAuthority() here anyway LOL!
		//if (Character->HasAuthority())
		//{
		//	CharacterState = ECharacterState::ECS_Unoccupied;
		//}

	//Optional for shotgun if you want it to continue to fire if Fire button is still press after reload at least one ammo!
	if (Character->IsLocallyControlled())
	{
		if (bIsFiring) Input_Fire(bIsFiring);
	}

	//If you want..., you call this either here or in Combat::ServerInput_Reload, not both:
	if (Character->HasAuthority())
	{
		UpdateHUD_CarriedAmmo_SpecializedForShotgun();
	}
}

//to be called when in ThrowEnd AnimNotify , no condition mean call in all devices (if previous end up PlayThrowAnimation in all devices)
void UCombatComponent::ThrowEnd()
{
	CharacterState = ECharacterState::ECS_Unoccupied;
	//Snap the weapon back to where it begin:
	AttachEquippedWeaponToRightHandSocket();
}

void UCombatComponent::ShowGrenadeMesh()
{
	if (Character) Character->ShowGrenadeMesh();
}

void UCombatComponent::HideGrenadeMesh_SpawnActualProjectileGrenade()
{
	if (Character == nullptr) return;
//cosmetic part: do it for all devices
	Character->HideGrenadeMesh();

//main part: Spawn Projectile_ThrowGrenade: GrenadeSocket_inChar -> DoLineTracce_cross::HitTarget
	//step1: need HitTarget from Controlling device:
	if (Character->IsLocallyControlled())
	{
		FHitResult HitResult;
		DoLineTrace_UnderCrosshairs(HitResult); //if hit, HitResult.Impoint = whatever Hit, if not we did set HitResult.Impoint = End; remember , hell yeah!

		//step2: propogate this value to the server -> clients:
		ServerSpawnGrenade(HitResult.ImpactPoint);
	}

}

//Spawn the AProjectile_Grenade here to be replicated across all devices:
//go and copy the code in AProjectilWeapon::Fire code and adapt: GrenadeSocket_InChar -> HitTarget:
void UCombatComponent::ServerSpawnGrenade_Implementation(const FVector& Target)
{
	if (Character == nullptr || Character->GetMesh() ==nullptr ||ProjectileClass == nullptr)  return;
	//now starting location is socket in Char:
	//you can in fact Character->GetGrenadeMesh()->GetComponentLocation() as well LOL
	FTransform GrenadeSocketTransform_InChar = Character->GetMesh()->GetSocketTransform(FName("GrenadeSocket"));

	FVector SpawnLocation = GrenadeSocketTransform_InChar.GetLocation();

	FVector FacingDirection = (Target - SpawnLocation);

	FRotator SpawnRotation = FacingDirection.Rotation(); //accept ROLL = 0 -> YAW & PTICH

	FActorSpawnParameters SpawnParams;

	//now we dont have weapon, so leave it be nullptr (rather then set it be Character causing inconsistency with other AProjectile_X spawned by Weapon)
	SpawnParams.Owner = nullptr; 

	//UActorComponent::GetOwner() will be naturally its hosting actor, according to the last test remember, however I will simply enter 'Character':
	SpawnParams.Instigator = Cast<APawn>(Character); //GetOwner() will also work

	GetWorld()->SpawnActor<AProjectile>(
		ProjectileClass,
		SpawnLocation, SpawnRotation, SpawnParams
	);
}

////I separate it here to avoid, replication late:
//void UCombatComponent::EndReload_ContinueFiringIf()
//{
//	//this is INCCORECT, the ORIGIN of Input_Callback is from the OWNING client, not HasAuthority()
//	//if (Character && Character->HasAuthority()) 
//	
//	//this is perfect, that stop the issue can't stop firing after reloading!
//	if (Character  && Character->IsLocallyControlled() )
//	{
//		if (bIsFiring)
//		{
//			//you may be tempted to pass in "true", but it can still be changed right?
//			Start_FireTimer();
//		}
//	}
//}
//
//







