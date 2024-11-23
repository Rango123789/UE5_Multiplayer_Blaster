// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/BlasterCharacter.h"
#include "CharacterComponents/CombatComponent.h"
#include "PlayerController/BlasterPlayerController.h"
#include "GameModes/BlasterGameMode.h"
#include "Weapons/Weapon.h"
#include "Components/CapsuleComponent.h"

//#include "Curves/CurveFloat.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Components/WidgetComponent.h"
#include "HUD/Overhead_UserWidget.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/SkeletalMeshSocket.h"//test
#include "Blaster/Blaster.h"
#include <Kismet/GameplayStatics.h>
#include <PlayerStates/PlayerState_Blaster.h>
#include <HUD/BlasterHUD.h>
#include "HUD/CharacterOverlay_UserWidget.h"
//#include "HUD/BlasterHUD.h"
//#include "HUD/CharacterOverlay_UserWidget.h"

// Sets default values
ABlasterCharacter::ABlasterCharacter()
{
	if (GetWorld()) UE_LOG(LogTemp, Warning, TEXT("Character,  Constructor Time: %f "), GetWorld()->GetTimeSeconds())

	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true; //it is true by default, no need
	//Setup SpringArm and Camera: 
	  //normally we attach SpringArm too 'RootComponent' (which is capsule here), but LATER ON we will use 'Crouch' function - that we need to scale the capsule/root component DOWN, so if a parent is scaled its attached childs will scaled too, including relative location to the parent LOL, hence the location of SpringArm to its parent change ABNORMALLY, we dont want it so perhaps GetMesh() is the choice here
	  //But scale Capsule/RootComponent will also Scale GetMesh() and hence scale SpringArm as well LOL, so what next to solve this? well we'll see dont worry
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArmComponent"));
	SpringArm->SetupAttachment(GetMesh());
	SpringArm->TargetArmLength = 600; //the default is 300
	SpringArm->bUsePawnControlRotation = true; //the default is surprisingly 'false' LOL

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName); //the Socket name is optional I think, whether or not you specify it it will au-to attacj to that 'SocketName' by default behaviour LOL
	Camera->bUsePawnControlRotation = false; //optional because it already attach to SpringArm that is set to use it already above, but if not do this 2 same code 'MAY' run twice in the same frame, not good.

	//Setup UWidgetComponent:
	Overhead_WidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("Overhead_WidgetComponent"));
	Overhead_WidgetComponent->SetupAttachment(RootComponent);

	//Setup UCombatComponent: We will replicate this Component[/pointer object] ; it is not SceneComponent so...
	CombatComponent = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
	CombatComponent->SetIsReplicated(true); // || ->SetIsReplicatedByDefault(true) || ->bReplicates = true || directly set it from the local class AWeapon is also fine - I refer to do it where the local class is LOL. But it may set back false if it is within the other class?, It could be LOL.

	//make sure you can
	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;

	GetMesh()->SetCollisionObjectType(ECC_SkeletalMesh);
	GetMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);
	GetMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_WorldDynamic, ECollisionResponse::ECR_Block);

	//GetMesh()->SetNotifyRigidBodyCollision(true); //no need, only need to check where Hit Event need to occur

	//setup TimelineComp:
	TimelineComponent = CreateDefaultSubobject<UTimelineComponent>("TimelineComp");

}

void ABlasterCharacter::BeginPlay()
{
	if (GetWorld()) UE_LOG(LogTemp, Warning, TEXT("Character,  BeginPlay Time: %f "), GetWorld()->GetTimeSeconds())

	Super::BeginPlay();
//stephen dont have these code, as he uses old input system:
	//Create UEnhancedInputLocalPlayerSubsystem_object associate with ULocalPlayer+APlayerController controlling this pawn:
	BlasterPlayerController = Cast<ABlasterPlayerController>(GetController());
	PlayerState_Blaster = GetPlayerState<APlayerState_Blaster>();
	if (BlasterPlayerController)
	{

		BlasterPlayerController->SetHUDHealth(Health, MaxHealth); 	//this is for GameStart
		//OPTION1 to delay HUD to be shown during WarmUpTime:
			//ABlasterHUD* BlasterHUD = BlasterPlayerController->GetHUD<ABlasterHUD>();
			//if (BlasterHUD && BlasterHUD->GetCharacterOverlay_UserWidget())
			//{
			//	BlasterHUD->GetCharacterOverlay_UserWidget()->AddToViewport();
			//}
	}

	if (PlayerState_Blaster && BlasterPlayerController)
	{ 

		BlasterPlayerController->SetHUDScore(PlayerState_Blaster->GetScore()); 	//medicine1
		BlasterPlayerController->SetHUDDefeat(PlayerState_Blaster->GetDefeat()); 	//medicine1
	}
	
	////Only the server device can get a valid reference to GameMode as it is only created in the server device, not sure it is a good idea to create a member of this where this is only meaningful to the server device
	//BlasterGameMode = Cast<ABlasterGameMode>(GetWorld()->GetAuthGameMode());

	//If you the server pawn isn't spawned with the system naturally for first time, then you must set it up in PC::OnPosses (JUST like the way you fix HUD widget GameStart + Respawn)
	if (BlasterPlayerController && IMC_Blaster)
	{
		//create __ object and associate it with the LocalPlayer object, hence PlayerController, currently controlling this Character: 
		UEnhancedInputLocalPlayerSubsystem* EISubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(BlasterPlayerController->GetLocalPlayer());

		//the priority=0 is just for fun, there is only one IMC we're gonna add to our Blaster Character: 
		if (EISubsystem) EISubsystem->AddMappingContext(IMC_Blaster, 0);
	}

	//call the helper function to change text of the underlying widget of the widget component:
	if (Overhead_WidgetComponent)
	{
		UOverhead_UserWidget* Overhead_UserWidget = Cast<UOverhead_UserWidget>(Overhead_WidgetComponent->GetUserWidgetObject());
		if (Overhead_UserWidget) Overhead_UserWidget->ShowPlayerNetRole(this);
	}

	BaseAimRotation_SinceStopMoving = GetBaseAimRotation();

	if (HasAuthority() )
	{
		OnTakeAnyDamage.AddDynamic(this, &ThisClass::ReceiveDamage);
	}
}

//this is meant to be called from BP::OnPosses, avoiding cluding all input-relevant types from there
//again you can directly do this from there, but must include relevant types
void ABlasterCharacter::SetupEnhancedInput_IMC()
{
	//add this extra line:
	BlasterPlayerController = Cast<ABlasterPlayerController>(GetController());

	if (BlasterPlayerController && IMC_Blaster)
	{
		//create __ object and associate it with the LocalPlayer object, hence PlayerController, currently controlling this Character: 
		UEnhancedInputLocalPlayerSubsystem* EISubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(BlasterPlayerController->GetLocalPlayer());

		//the priority=0 is just for fun, there is only one IMC we're gonna add to our Blaster Character: 
		if (EISubsystem) EISubsystem->AddMappingContext(IMC_Blaster, 0);
	}
}

//Stephen way, for now I use my way already
void ABlasterCharacter::PollInit()
{
	//we only do these if it is currently null
	if (PlayerState_Blaster == nullptr)
	{
		BlasterPlayerController = Cast<ABlasterPlayerController>(GetController());

		PlayerState_Blaster = GetPlayerState<APlayerState_Blaster>(); //could be optional

		if (PlayerState_Blaster && BlasterPlayerController)
		{
			BlasterPlayerController->SetHUDScore(PlayerState_Blaster->GetScore()); 	//this is for GameStart
			BlasterPlayerController->SetHUDDefeat(PlayerState_Blaster->GetDefeat());
		}
	}

	//if (BlasterHUD == nullptr)
	//{
	//	BlasterPlayerController = Cast<ABlasterPlayerController>(GetController());
	//	if (BlasterPlayerController)
	//	{
	//		BlasterHUD = BlasterPlayerController->GetHUD<ABlasterHUD>();
	//		if (BlasterHUD) BlasterHUD->SetupBlasterHUD();
	//	}

	//}
}

void ABlasterCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ABlasterCharacter, OverlappingWeapon , COND_OwnerOnly);
	//DOREPLIFETIME(ABlasterCharacter, OverlappingWeapon);
	// 
	//I dont remember stephen did this, but I add this line according the warning, as I see we set CombatComponent replicated in this char::char() above -->no it gives a crash, regardless the warning suggest lol, this is "UActorComponent", not regular vars
	//DOREPLIFETIME_CONDITION(ABlasterCharacter, CombatComponent, COND_OwnerOnly);

	DOREPLIFETIME(ABlasterCharacter, Health);
	DOREPLIFETIME(ABlasterCharacter, bDisableMostInput);
}

void ABlasterCharacter::PostInitializeComponents()
{
	//supposedly all components should be initialized and not null before this Post, but still I want to check as good practice:
	Super::PostInitializeComponents();
	if (CombatComponent) CombatComponent->Character = this;
}

void ABlasterCharacter::Destroyed()
{
	Super::Destroyed();

	//ONLY WHEN you're in CoolDown state you need to destroy the weapon with the character lol
	if (BlasterPlayerController &&
		BlasterPlayerController->GetMatchState() == MatchState::CoolDown &&
		GetEquippedWeapon())
	{
		GetEquippedWeapon()->Destroy();
	}
}

void ABlasterCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
    
	//PollInit(); //Currently I use medicine 1,2,3

	HideCharacterIfCameraClose();

//this block (in combination with ABP) make auto proxy do AimOffset when within [-90,90], TurnInPlace when goes beyond it
// ; make non-auto proxies play turn animation All the time (everytime you delta rotation > threshold)
	AimOffsetAndTurnInPlace_GLOBAL(DeltaTime);
}

void ABlasterCharacter::AimOffsetAndTurnInPlace_GLOBAL(float DeltaTime)
{
	//We dont do any of them when bDisableMostInput=true (say during CoolDownTime)
	if (bDisableMostInput)
	{
		//make sure non-auto proxies wont get stuck and play turn anim all the time, while its TurningInPlace != NoTurning at the same time CoolDownTime start - like stop firing logic:
		TurningInPlace = ETurningInPlace::RTIP_NoTurning;
		//You can set this in PC::OnMatchStateSet_Pro()~>CoolDown~>HandleCoolDown() as well, but why do we just do her together with TurningInPlace too right? :)
		bUseControllerRotationYaw = false; //since you can't move during b__ = true, so bOrientRotationWithMovement is irrelevant.
		AO_Yaw = 0;

		return;
	}

	//autonomous proxy will have its AO_Yaw set (and AO_Pitch)
	if (IsLocallyControlled())
	{
		SetupAimOffsetVariables(DeltaTime);
	}
	//non-auto proxies will not have its AO_Yaw set, bRotateRoot = false (but AO_Pitch set normally)
	else
	{
		//we should only need to accumilating time for SimulateProxy
		AccumilatingTime += DeltaTime;

		//we MANUALLY call OnRep_ReplicatedMovement() after every TimeThreshold, you can reduce it to a small number, say 0.1s - the smaller it is, the smoother you have , but with less performance!
		if (AccumilatingTime > TimeThreshold)
		{
			OnRep_ReplicatedMovement();
			//you dd NOT do this instead, because there is 'super:: ' that need to be run too
			//Turn_ForSimProxyOnly();
			//AccumilatingTime = 0.f;
		}
	}
	//UE_LOG(LogTemp, Warning, TEXT("ProxyDeltaYaw: %f"), DeltaTime);
}

//this function auto-trigger itself when ReplicatedMovement is changed and  replicated
void ABlasterCharacter::OnRep_ReplicatedMovement()
{
	Super::OnRep_ReplicatedMovement();

//I don't add if condition because I want both simulated and authoritive copies having this new behaviour!
	//if (GetLocalRole() == ENetRole::ROLE_SimulatedProxy)
	//{
		Turn_ForSimProxyOnly();
		//AccumilatingTime = 0.f;
	//}
	AccumilatingTime = 0.f; //why stephen let it out here?
}

//currently only the server can trigger this
void ABlasterCharacter::ReceiveDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	Health = FMath::Clamp(Health - Damage, 0, MaxHealth);

	//to be replicated to clients via OnRep_Health as well
	if(Health > 0.f)  PlayHitReactMontage();
	UpdateHUD_Health();

	//only when Health ==0 we should summon the gamemode to do its very job
	if (Health <= 0.f) 
	{
		//Only the server device can get a valid reference to GameMode as it is only created in the server 
		   // OPTION1
		ABlasterGameMode* BlasterGameMode = Cast<ABlasterGameMode>( GetWorld()->GetAuthGameMode() );
		if (BlasterGameMode)
		{
			ABlasterPlayerController* AttackerController = Cast<ABlasterPlayerController>(InstigatedBy);
			
			//you miss this line: 
			BlasterPlayerController = BlasterPlayerController == nullptr ? Cast<ABlasterPlayerController>(GetController()) : BlasterPlayerController;

			BlasterGameMode->PlayerEliminated(this, BlasterPlayerController, AttackerController); 
		}
		//  //OPTION2
		//Elim();
	}
}

void ABlasterCharacter::Elim()
{
	//CHAIN1: seen to all devices
	MulticastElim();

	//CHAIN2: other things happen ONLY in server (and expected to be self-replicated, otherwise must let it go with Multicast above)
	      //disable most input:
	bDisableMostInput = true;
	
	      //respawn after 3s:
	GetWorldTimerManager().SetTimer(TimerHandle_Elim, this , &ThisClass::TimerCallback_Elim,  DelayTime_Elim);

           //you can simply create AWeapon::Dropped to contain these code, and then call a single code, you access through so many tires for the same local destination LOL :d :d
	       //drop the weapon && so on:
	if (GetCombatComponent() && GetCombatComponent()->EquippedWeapon)
	{
		GetCombatComponent()->EquippedWeapon->Drop();
	}
}

void ABlasterCharacter::TimerCallback_Elim()
{
	//when this TimerCallback reached, we re-spawn the Character 
	//so the idea is we create yet another ABlasterGameMode::RequestRespawn() hadling all the Reset, Destroy and respawn from there
	//and then call it back here! 
	//what the heck why it is so 'around' why dont we just call directly from ABlasterGameMode::PlayerEliminated() instead?
	//also why you must call Char::Elim from GameMode instead of directly here in char::ReceiveDamage? 

	ABlasterGameMode* BlasterGameMode = Cast<ABlasterGameMode>(GetWorld()->GetAuthGameMode());
	//you miss this line: 
	BlasterPlayerController = BlasterPlayerController == nullptr ? Cast<ABlasterPlayerController>(GetController()) : BlasterPlayerController;

	if(BlasterGameMode)	BlasterGameMode->RequestRespawn(this, BlasterPlayerController);
}

void ABlasterCharacter::MulticastElim_Implementation()
{
	//In worst case when Sniper rifle is Aming and get elimmed:
	//this will help to override the current frame (either end or middle frame) and play backward and help you to reach "first frame" with Scale = 0, Opacity=0
    //also no need to do this when it is not sniper rifle nor not currently IsAiming, doing this in these cases only bring stupid side effect :D :D
	if (IsLocallyControlled() && 
		CombatComponent &&
		CombatComponent->EquippedWeapon&&
		CombatComponent->EquippedWeapon->GetWeaponType() == EWeaponType::EWT_SniperRifle &&
		CombatComponent->bIsAiming == true)
	{
		ShowScopeWidget(false);
	}
	//update HUD_Ammo to zero: (can't re-use CheckAndSetHUD_Ammo here, it has no parameter)
	BlasterPlayerController = BlasterPlayerController == nullptr ? Cast<ABlasterPlayerController>(GetController()) : BlasterPlayerController;
	if (BlasterPlayerController) BlasterPlayerController->SetHUDAmmo(0);

	//For animation
	bIsEliminated = true; //that help it switch ABP chain1 to chain2_elim first
	PlayElimMontage();    //and so now can play on ElimSlot in that chain2_elim

	//ready material before run Timeline:
	if(SkeletalMesh_Optimized) GetMesh()->SetSkeletalMeshAsset(SkeletalMesh_Optimized);
	
	if (MaterialInstance)
	{
		MaterialInstanceDynamic = UMaterialInstanceDynamic::Create(MaterialInstance, this);
		GetMesh()->SetMaterial(0, MaterialInstanceDynamic);

		MaterialInstanceDynamic->SetScalarParameterValue(TEXT("DissolveAdding"), -0.5f); 
		MaterialInstanceDynamic->SetScalarParameterValue(TEXT("Glow"), 80.f); //implicit construction
	}
	//start Dissolve process: it will start immediately , unlike respawn with Timer in Chain2
	StartTimeline_Dissolve();

	//disable ollision for char::capsule and getmesh() in all devices:
	if(GetCapsuleComponent()) GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if(GetMesh()) GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	//disable movement:
	GetCharacterMovement()->DisableMovement(); //can move, but still can rotate
	GetCharacterMovement()->StopMovementImmediately(); //can't move, can't rotate

	//disable Input: if you can't move and can't rotate above already, this is REDUNDANT? No you still need to disable other things like Fire, Reload...input as well LOL:
		//DisableInput(BlasterPlayerController);

	//play UParticleSystem(a bot) and its sound:
	FVector SpawnLocation = { GetActorLocation().X ,GetActorLocation().Y , GetActorLocation().Z + 200.f };
	//I dont do BotParticleComp = --: 
	UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), BotParticle, SpawnLocation, GetActorRotation(), true); //I pass in bAutoDestroy=true to make sure LOL
	UGameplayStatics::PlaySoundAtLocation(this, BotSound, SpawnLocation);
}

void ABlasterCharacter::StartTimeline_Dissolve()
{
	if (TimelineComponent == nullptr) return;

	//this is non-sparse DYNAMIC delegate, it has .BindDynamic instead (NOT .AddDynamic). 
	// Do not worry, this line is NOT inside Timeline_Callback so it wont trigger many times at all
	OnTimelineFloat_Delegate_Dissolve.BindDynamic(this, &ThisClass::OnTimelineFloat_Callback_Dissolve);

	TimelineComponent->AddInterpFloat(CurveFloat_Dissolve, OnTimelineFloat_Delegate_Dissolve);
	TimelineComponent->Play();

}

//all code in this function will be repeated constantly during Timeline:
void ABlasterCharacter::OnTimelineFloat_Callback_Dissolve(float DissolveAdding)
{
	if (MaterialInstanceDynamic == nullptr) return;
	MaterialInstanceDynamic->SetScalarParameterValue(TEXT("DissolveAdding"), DissolveAdding);
}

//this will be called on all clients 
void ABlasterCharacter::OnRep_Health()
{
	if (Health > 0.f) PlayHitReactMontage();
	UpdateHUD_Health();
}

void ABlasterCharacter::UpdateHUD_Health()
{
	//Read Appendix28, to know why this line of code is more than necessary!
	BlasterPlayerController = BlasterPlayerController == nullptr ? Cast<ABlasterPlayerController>(GetController()) : BlasterPlayerController;
	if (BlasterPlayerController) BlasterPlayerController->SetHUDHealth(Health, MaxHealth);
}

void ABlasterCharacter::Turn_ForSimProxyOnly()
{
	if (CombatComponent == nullptr || CombatComponent->EquippedWeapon == nullptr) return;

	bShouldRotateRootBone = false;

	AO_Pitch = GetBaseAimRotation().GetNormalized().Pitch; //copy from SetupAimOffsetVariables()

	//not moving && not jumping (not sure not jumping is overkill here lol, as it can be solved enternally alread, but it helps to reduce code!!!)
	if (GetVelocity().Size() > 0.f)
	{
		TurningInPlace = ETurningInPlace::RTIP_NoTurning;
		return; //important!
	}

//START TO IMPLEMENT the sim behaivour:
	ProxyRotation_LastFrame = ProxyRotation;
	ProxyRotation = GetActorRotation();

	ProxyDeltaYaw = (ProxyRotation - ProxyRotation_LastFrame).GetNormalized().Yaw; // /DeltaTime

	//UE_LOG(LogTemp, Warning, TEXT("ProxyDeltaYaw: %f"), ProxyDeltaYaw);

	//ABP only care the TurningInPlace and it will play mactching animation, I think we should add additional condition that when the turning In Place finish, it should auto go back to idle state?
	//If you can keep the turning state as long as you keep rotating (keep playing turning animation)
	//but when you stop rotating or rotating fast enough that -x < ProxyDeltaYaw < x
	//TurningInPlace = NoTurning And it will stop playing Turning animation from ABP
	if (ProxyDeltaYaw > TurnThreshold)
	{
		TurningInPlace = ETurningInPlace::RTIP_TurnRight;
		return;
	}
	else if (ProxyDeltaYaw < -TurnThreshold)
	{
		TurningInPlace = ETurningInPlace::RTIP_TurnLeft;
		return;
	}
	else
	{
		TurningInPlace = ETurningInPlace::RTIP_NoTurning;
	}
}

void ABlasterCharacter::SetupAimOffsetVariables(float DeltaTime)
{
	//UPDATE: this setup is meant for when you have a weapon, and you can skip this if you didn't have any
	//this does improve performance, so why not:
	if (CombatComponent == nullptr || CombatComponent->EquippedWeapon == nullptr)  return;
    
	//these lines are just for readibility:
	float GroundSpeed = GetVelocity().Size2D();
	bool IsInAir = GetCharacterMovement()->IsFalling();

	//BLOCK0: GLOBAL else if about "moving/jumping" or not
	if (GroundSpeed == 0.f && !IsInAir  ) //not moving, not jumping
	{
		bShouldRotateRootBone = true; //NEW1

		bUseControllerRotationYaw = true; //from false in last lesson

		FRotator DeltaRotation = (GetBaseAimRotation() - BaseAimRotation_SinceStopMoving);

		AO_Yaw = DeltaRotation.GetNormalized().Yaw;

		TurnInPlace_ForAutoProxyOnly(DeltaTime);

	}
	else //running or jumping - this is "GLOBAL else", hence when you move+ "all AimOffset->TurningInPlace" effect will be removed
	{
		bShouldRotateRootBone = false; //NEW2

		bUseControllerRotationYaw = true; //already true from last lesson ->now you can remove both lines

		BaseAimRotation_SinceStopMoving = GetBaseAimRotation();
		
		AO_Yaw = 0; 

		SetTurningInPlace(ETurningInPlace::RTIP_NoTurning); //This is another way to stop the TurningInPlace process, if happening.
	}

	//add .GetNormalized() fix it!
	AO_Pitch = GetBaseAimRotation().GetNormalized().Pitch; 

	//UE_LOG(LogTemp, Warning, TEXT("AO_Yaw: %f"), AO_Yaw); 
}

void ABlasterCharacter::TurnInPlace_ForAutoProxyOnly(float DeltaTime)
{
	//BLOCK1,2 is for "Turning In Place" feature, Without them, AO_Yaw and AimOffset will still work (but remember to set bUseYaw = false back LOL.
	//NEW BLOCK1: 
	if (TurningInPlace == ETurningInPlace::RTIP_NoTurning)
	{
		AO_Yaw_SinceNoTurning_ToBeInterpolatedBackToZero = AO_Yaw;
	}
	else //either TurnLeft or TurnRight
	{
		AO_Yaw_SinceNoTurning_ToBeInterpolatedBackToZero = FMath::FInterpTo(AO_Yaw_SinceNoTurning_ToBeInterpolatedBackToZero, 0.f, DeltaTime, InterpSpeed_Turning); //NO NEED ? -> need during turn process

		AO_Yaw = AO_Yaw_SinceNoTurning_ToBeInterpolatedBackToZero; //NO NEED ? -> need during turn process

		//it is up to up to decide when to stop interpolate, x = 15 -->stop short at 90-15 = 75 degree, accepting the skip 15 degree. 
		//this local Block is absolutely important so that you can escape either TurnLeft or TurnRight naturally and automatically, otherwise no way to escape it naturally even when the AO_Yaw_SinceNoTurning reach ZERO in interpolation, unless you move again to escape it :D :D
		if (abs(AO_Yaw) <= X_stop) //Stephen < 15.f, I update this!
		{
			TurningInPlace = ETurningInPlace::RTIP_NoTurning;

			BaseAimRotation_SinceStopMoving = GetBaseAimRotation(); //we can't forget this
		}
	}

	//NEW BLOCK2: this is for animation, and also trigger "else" of BLOCK1
	if (AO_Yaw > 90.f)
	{
		SetTurningInPlace(ETurningInPlace::RTIP_TurnRight);
	}
	if (AO_Yaw < -90.f)
	{
		SetTurningInPlace(ETurningInPlace::RTIP_TurnLeft);
	}
}

//return true if EquippedWeapon is NOT null
bool ABlasterCharacter::IsWeaponEquipped()
{
	return (CombatComponent && CombatComponent->EquippedWeapon);
}

bool ABlasterCharacter::IsAming()
{
	return (CombatComponent && CombatComponent->bIsAiming);
}

bool ABlasterCharacter::IsAFiring()
{
	return (CombatComponent && CombatComponent->bIsFiring);
}

void ABlasterCharacter::ResetCharacterStateToUnoccupied()
{
	//because the inner code is self-replicated, so add HasAuthority is optional. if you wan the server first, then add HasAuthority, if you want all copies run over, then remove it (for cosmetic action) 
	if (HasAuthority())
	{
		if (CombatComponent) CombatComponent->CharacterState = ECharacterState::ECS_Unoccupied;
	}
}

void ABlasterCharacter::ReloadEnd1()
{
	if (CombatComponent == nullptr) return;
	//this may cause "LEGENDARYcase", if so, simpl remove 'HasAuthority()' will overcome it LOL = no it's not!
		//because the inner code is self-replicated, so add HasAuthority is optional. if you wan the server first, then add HasAuthority, if you want all copies run over, then remove it (for cosmetic action) 
	if (HasAuthority())
	{
		CombatComponent->CharacterState = ECharacterState::ECS_Unoccupied;
		//this make my clients can't stop firing, it is because of 'late replication', currently bIsFiring isn't setup very clean in GOLDEN2, it is like a shit LOL, nor should we replicateit LOL
		//continue to fire if bFiring is still true (still holding the Fire key):
		// FOR NOW, I comment it out:
		// the code inside the is meant to be run in the server only, you can't let it run in the client nor should you also call this in OnRep!
			if (CombatComponent->bIsFiring)
			{
				//you may be tempted to pass in "true", but it can still be changed right?
				CombatComponent->Input_Fire(CombatComponent->bIsFiring); 
			}
	}
	//If you want..., you call this either here or in Combat::ServerInput_Reload, not both:
	if(HasAuthority()) CombatComponent->UpdateHUD_CarriedAmmo();
}

//no need this LOL, this is BPImplementableEvent that is ONLY allowed to define in BP, not from C++ lol
// NOT BPNativeEvent or like
//void ABlasterCharacter::ShowScopeWidget_Implementation(bool bShowScope)
//{
//}


//this can't only be called on client copies: adding param/using it or not doesn't matter
void ABlasterCharacter::OnRep_OverlappingWeapon(AWeapon* LastWeapon)
{
	//Reacheaching this OnRep boby the OverlappingWeapon must be assigned new value already
	//hence either of them will be executed, but not both I'm sure :D :D
	if (OverlappingWeapon)
	{
		OverlappingWeapon->ShowPickupWidget(true);
	}
	else if (LastWeapon)
	{
		LastWeapon->ShowPickupWidget(false);
	}
}

//this will be called in Weapon::Overlap which could only happen in-server copy
//hence MANUAL work condition can count on it!
void ABlasterCharacter::SetOverlappingWeapon(AWeapon* InWeapon)
{
	//before the time we assign it a new value, check if it is not null
	//then should turn it OFF, before it receive new value that is surely null :D :D
	//if it is nullptr, then it will pass this line, without turning it off it is surely already OFF from last time LOL
	//if it is nullptr, it will pass this line and to be turn ON in the final block!
	if (OverlappingWeapon ) //&& IsLocallyControlled() , &&InWeapon == nullptr - no need
	{
		OverlappingWeapon->ShowPickupWidget(false);
	}

	OverlappingWeapon = InWeapon;

    //if it is assigned new value, but it is not null, then I think we have to turn it ON
	//if it is assigned new value, but it is null, then it will pass this line, it is OFF by first block above, which is exactly what we want.
	if (OverlappingWeapon &&  IsLocallyControlled())
	{
		OverlappingWeapon->ShowPickupWidget(true);
	}
}

AWeapon* ABlasterCharacter::GetEquippedWeapon()
{
	if (CombatComponent == nullptr) return nullptr;
	else return CombatComponent->EquippedWeapon;
}

void ABlasterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedPlayerInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);

	if (EnhancedPlayerInputComponent)
	{
 		EnhancedPlayerInputComponent->BindAction(IA_Move, ETriggerEvent::Triggered, this, &ThisClass::Input_Move );
		EnhancedPlayerInputComponent->BindAction(IA_Look, ETriggerEvent::Triggered, this, &ThisClass::Input_Look);
		EnhancedPlayerInputComponent->BindAction(IA_Jump, ETriggerEvent::Triggered, this, &ThisClass::Input_Jump);
		EnhancedPlayerInputComponent->BindAction(IA_EKeyPressed, ETriggerEvent::Triggered, this, &ThisClass::Input_EKeyPressed);
		EnhancedPlayerInputComponent->BindAction(IA_Crouch, ETriggerEvent::Triggered, this, &ThisClass::Input_Crouch);

		EnhancedPlayerInputComponent->BindAction(IA_Aim_Pressed, ETriggerEvent::Triggered, this, &ThisClass::Input_Aim_Pressed);
		EnhancedPlayerInputComponent->BindAction(IA_Aim_Released, ETriggerEvent::Triggered, this, &ThisClass::Input_Aim_Released);

		EnhancedPlayerInputComponent->BindAction(IA_Fire_Pressed, ETriggerEvent::Triggered, this, &ThisClass::Input_Fire_Pressed);
		EnhancedPlayerInputComponent->BindAction(IA_Fire_Released, ETriggerEvent::Triggered, this, &ThisClass::Input_Fire_Released);

		EnhancedPlayerInputComponent->BindAction(IA_Reload, ETriggerEvent::Triggered, this, &ThisClass::Input_Reload);
	}
}

void ABlasterCharacter::PlayMontage_SpecificSection(UAnimMontage* InMontage, FName InName)
{
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && InMontage)
	{
		AnimInstance->Montage_Play(InMontage);

		AnimInstance->Montage_JumpToSection(InName, InMontage);
	}
}

void ABlasterCharacter::PlayFireMontage()
{
	if (CombatComponent == nullptr || AM_FireMontage == nullptr) return;

	//from AM_ asset from BP, you must name its sections to match these below:
	FName SectionName = CombatComponent->bIsAiming ? FName("RifleAim") : FName("RifleHip");

	PlayMontage_SpecificSection(AM_FireMontage, SectionName);
}

void ABlasterCharacter::PlayHitReactMontage()
{
	if(AM_FireMontage == nullptr) return;

	FName SectionName("FromFront"); //for now

	PlayMontage_SpecificSection(AM_HitReact, SectionName);

}

void ABlasterCharacter::PlayElimMontage()
{
	PlayMontage_SpecificSection(AM_ElimMontage);
}

void ABlasterCharacter::PlayReloadMontage()
{
	if (CombatComponent == nullptr || CombatComponent->EquippedWeapon == nullptr || AM_ReloadMontage == nullptr) return;

	//from AM_ asset from BP, you must name its sections to match these below:
	FName SectionName;

	EWeaponType Type = CombatComponent->EquippedWeapon->GetWeaponType();

	switch (Type)
	{
	case EWeaponType::EWT_AssaultRifle:
		SectionName = FName("AssaultRifle");
		break;
	//more case will be added as we have more weapon types
	case EWeaponType::EWT_RocketLauncher:
		SectionName = FName("RocketLauncher"); //for now
		break;
	case EWeaponType::EWT_Pistol:
		SectionName = FName("Pistol"); //for now
		break;
	case EWeaponType::EWT_SMG:
		SectionName = FName("Pistol"); //for now
		break;
	case EWeaponType::EWT_Shotgun:
		SectionName = FName("Shotgun"); //for now
		break;
	case EWeaponType::EWT_SniperRifle:
		SectionName = FName("SniperRifle"); //for now
		break;
	case EWeaponType::EWT_GrenadeLauncher:
		SectionName = FName("GrenadeLauncher"); //for now
		break;
	}

	PlayMontage_SpecificSection(AM_ReloadMontage, SectionName);
}

//not sure this will cause any side effect on currently playing 'Realoading Montage'?
//I guess as long as it is the same AM_ReloadMontage assset it know how to behave right?
//[UPDATE] this work like a charm, however if you know that it current playing AM_ReloadMontage (no matter what section is played) you can skip the line "Montage_Play" and directly call "JumpToSection(AM_ , NewSection)" 
void ABlasterCharacter::JumpToShotgunEndSection()
{
	PlayMontage_SpecificSection(AM_ReloadMontage, FName("ShotgunEnd"));
}

void ABlasterCharacter::Input_Move(const FInputActionValue& Value)
{
	if (bDisableMostInput) return;
	FVector2D Input = Value.Get<FVector2D>();
	if (!Controller || Input.Size() == 0) return; //Input.Size() == 0 improve performance, where Controller are optional I think

	//FVector direction1 = GetActorRightVector(); - this move in curent Actor direcion, not Camera direction
        //FVector direction2 = GetActorForwardVector();

	//the key to solve confustion is: Value.X match Right vector, Value.Y match Forward vector:
	FVector direction1 = FRotationMatrix(GetControlRotation()).GetUnitAxis(EAxis::Y); //right vector, but match X of value
	FVector direction2 = FRotationMatrix(GetControlRotation()).GetUnitAxis(EAxis::X); //forware vector, match Y of value

	//FVector direction1 = UKismetMathLibrary::GetRightVector (GetControlRotation() ) - also work
	//FVector direction2 = UKismetMathLibrary::GetForwardVector (GetControlRotation() )

	//FVector direction1 = SpringArm->GetRightVector; - also work
	//FVector direction2 = SpringArm->GetForwardVector; 

	//FVector direction1 = Camera->GetRightVector; - also work
	//FVector direction2 = Camera->GetForwardVector; 

	//for movement: align with Camera direction
	AddMovementInput(direction1, Input.X);
	AddMovementInput(direction2, Input.Y);

}

void ABlasterCharacter::Input_Look(const FInputActionValue& Value)
{
	FVector2D Input = Value.Get<FVector2D>();
	if (!Controller || Input.Size() == 0) return;

	//yaw to turn around horizontally, pitch to turn up and down; you dont want to 'roll' LOL: 
	AddControllerYawInput(Input.X);
	AddControllerPitchInput(Input.Y);
}

void ABlasterCharacter::Input_Jump(const FInputActionValue& Value)
{
	if (bDisableMostInput) return;
	//Super::Jump(); //you just override it, so call the new version instead

	Jump();
}

void ABlasterCharacter::Input_EKeyPressed(const FInputActionValue& Value)
{
	if (bDisableMostInput) return;
	if (CombatComponent == nullptr || OverlappingWeapon == nullptr) return;
	//Without "HasAuthority()" both server add clients can pick. Server pick, clients see it. 
	// where clients pick the server dont see it - but WHY? 
	// where one client pick, other client dont see it as well - by WHY?
	// Because the COLLISION in clients doesn't exist? well but they has reference to OverlappingWeapon decently right? 
	// Because replication only from Server to Clients, or because ReplicatedUsing?
	// Because CombatComponent->SetIsReplicated() indirectly here in this hosting class? But We didn't mark CombatComponent with 'Replicated' from hosting class perspective yet right? So this is NOT yet relevant! it is only self-replicated so far.
	//With "Hasauthority()" only server can pick, clients see it - but HOW? because "Aweapon && Char::OverlappingWeapon" are set to replicated? 
	// where Clients can't even pick - make sense
	if (HasAuthority())
	{
		CombatComponent->Equip(OverlappingWeapon); // (*)
	}
	else
	{
		ServerEKeyPressed(); //its body is purposely indeptical with (*), for clear reason 
	}
}

void ABlasterCharacter::ServerEKeyPressed_Implementation()
{
	if (bDisableMostInput) return;
	if (CombatComponent) CombatComponent->Equip(OverlappingWeapon);
}

void ABlasterCharacter::Input_Crouch(const FInputActionValue& Value)
{
	if (bDisableMostInput) return;
	if (!bIsCrouched) Crouch();
	else UnCrouch();
}

//use 'else' if you want press TWICE to jump, dont use 'else' if you want press ONCE to jump from Crouch state - no , either case lead to the same end: press TWICE to jump.
void ABlasterCharacter::Jump()
{
	if (bIsCrouched) UnCrouch();
	else Super::Jump();
}

void ABlasterCharacter::Input_Aim_Pressed(const FInputActionValue& Value)
{
	if (bDisableMostInput) return;
	if (CombatComponent == nullptr) return;
	CombatComponent->SetIsAiming(true);
	//option1: call BP_function(true) here && must call BP_function(false) in Input_Aim_Released as well LOL
		//ShowScopeWidget(true);
}

void ABlasterCharacter::Input_Aim_Released(const FInputActionValue& Value)
{
	if (bDisableMostInput) return;
	if (CombatComponent == nullptr) return;
	CombatComponent->SetIsAiming(false);
	//option1:
		//ShowScopeWidget(false);
}

void ABlasterCharacter::Input_Fire_Pressed(const FInputActionValue& Value)
{
	if (bDisableMostInput) return;
	if (CombatComponent == nullptr) return;
	CombatComponent->Input_Fire(true);
}

void ABlasterCharacter::Input_Fire_Released(const FInputActionValue& Value)
{
	//this one is uqniue to this Input :)
	if (bDisableMostInput)
	{
		//in case elimmed when firing and can't release the key one time to escape firing:
		CombatComponent->bCanFire = false; //will be reset back to true when respawned, dont worry
		return;
	}

	if (CombatComponent == nullptr) return;
	CombatComponent->Input_Fire(false);
}

void ABlasterCharacter::Input_Reload(const FInputActionValue& Value)
{
	if (bDisableMostInput) return;
	if(CombatComponent) CombatComponent->Input_Reload();
}

void ABlasterCharacter::HideCharacterIfCameraClose()
{
	//We must not hide in other machines, as the still need to see you LOL
	if (IsLocallyControlled() == false) return;
	if (CombatComponent == nullptr || CombatComponent->EquippedWeapon == nullptr || 
		CombatComponent->EquippedWeapon->GetWeaponMesh() == nullptr) return;

	float Distance = (GetActorLocation() - Camera->GetComponentLocation()).Size();
	
	if (Distance < CameraThreshold)
	{
	//For Chararacter itself:
		//SetActorHiddenInGame(true);    //Replicated, do de-effect IsLocallyControlled(), NOT wanted
		GetMesh()->SetVisibility(false); // NOT Replicated, hence we need it here. 
	//For EquippedWeapon:
		//CombatComponent->EquippedWeapon->SetActorHiddenInGame(true); //Replicated, do de-effect IsLocallyControlled(), NOT wanted
		//CombatComponent->EquippedWeapon->GetWeaponMesh()->SetVisibility(false); // NOT Replicated, hence we need it here. 
		CombatComponent->EquippedWeapon->GetWeaponMesh()->bOwnerNoSee = true;   // WAY2 for WeaponMesh, only work when you did set the charater is onwer of this, and this will affect the contorlling device ONLY
	}
	else
	{
		//SetActorHiddenInGame(false); // || GetMesh()->SetVisibility(true)
		GetMesh()->SetVisibility(true);
		//CombatComponent->EquippedWeapon->SetActorHiddenInGame(true);
		//CombatComponent->EquippedWeapon->GetWeaponMesh()->SetVisibility(true);
		CombatComponent->EquippedWeapon->GetWeaponMesh()->bOwnerNoSee = false;
	}
}

ABlasterPlayerController* ABlasterCharacter::GetBlasterPlayerController()
{
	BlasterPlayerController = BlasterPlayerController == nullptr ? GetController<ABlasterPlayerController>() : BlasterPlayerController;

	return BlasterPlayerController;
}

ECharacterState ABlasterCharacter::GetCharacterState() {
	if (CombatComponent == nullptr)  return ECharacterState::ECS_MAX; //return whatever LOL
	return CombatComponent->CharacterState;
}

////Stephen create this in UActorComponent instead.
//void ABlasterCharacter::SetIsAiming(bool InIsAiming) //REPLACE
//{
//	if (CombatComponent == nullptr) return;
//
//	if (HasAuthority())
//	{
//		CombatComponent->bIsAiming = InIsAiming;
//		if (GetCharacterMovement()) GetCharacterMovement()->MaxWalkSpeed = InIsAiming ? AimWalkSpeed : MaxWalkSpeed_Backup;
//	}
//	else
//		ServerSetIsAiming(InIsAiming);
//}

//void ABlasterCharacter::ServerSetIsAiming_Implementation(bool InIsAiming) //REPLACE
//{
//	if (CombatComponent)
//	{
//		CombatComponent->bIsAiming = InIsAiming;
//		if (GetCharacterMovement()) GetCharacterMovement()->MaxWalkSpeed = InIsAiming ? AimWalkSpeed : MaxWalkSpeed_Backup;
//	}
//}

/* Seperate SetAimOffsetVars() from SetTurnInPlaceVars()
-----------------------
1. Tick()
{
   Super::Tick(DeltaTime);
   SetAimOffsetVariables(DeltaTime);
   SetTurnInPlaceVariables(DeltaTime);
}

@@NOTE:
+'SetTurnInPlace' must be called after 'SetAimOffsetVariable'

-----------------------
2. SetAimOffsetVariables(DeltaTime)
{
  if (CombatComponent == nullptr)  return;
  if (CombatComponent->EquippedWeapon == nullptr) return;

  if (GetVelocity().Size2D() > 0.f && GetCharacterMovement()->IsFalling()==true )  //stand still, not jumping
  {
	bUseControllerRotationYaw = true;

	BaseAimRotation_SinceStopMoving = GetBaseAimRotation();

	AO_Yaw = 0;
  }
  else //moving | jumping
  {
	bUseControllerRotationYaw = false; //need to change to true if using 'TurnInPlace'

	FRotator DeltaRotation = GetBaseAimRotation() - BaseAimRotation_SinceStopMoving;

	AO_Yaw = DeltaRotation.GetNormalized().Yaw;
  }

  AO_Pitch = GetBaseAimRotation().GetNormalized().Pitch;

  UE_LOG(LogTemp, Warning, TEXT("AO_Yaw: %f"), AO_Yaw);
}

-----------------------
3. SetTurnInPlaceVariables(DeltaTime)
{
  if (CombatComponent == nullptr)  return;
  if (CombatComponent->EquippedWeapon == nullptr) return;

  //change it back to true, from false by SetAimOffsetVariables(DeltaTime):
  bUseControllerRotationYaw = true;

  if (GetVelocity().Size2D() > 0 && GetCharacterMovement()->IsFalling()==true ) //moving | jumping
  {
	SetTurningInPlace(ETurningInPlace::RTIP_NoTurning);
  }
  else  //not moving, not jumping
  {
	if (TurningInPlace == ETurningInPlace::RTIP_NoTurning)
	{
	  AO_Yaw_SinceNoTurning_ToBeInterpolatedBackToZero = AO_Yaw;
	}
	else //either TurnLeft or TurnRight
	{
	  AO_Yaw_SinceNoTurning_ToBeInterpolatedBackToZero = FMath::FInterpTo(
		  AO_Yaw_SinceNoTurning_ToBeInterpolatedBackToZero,
		  0.f,
		  DeltaTime,
		  InterpSpeed_Turning
	  );

	  AO_Yaw = AO_Yaw_SinceNoTurning_ToBeInterpolatedBackToZero;

	  if (abs(AO_Yaw) <= X_stop){
		 TurningInPlace = ETurningInPlace::RTIP_NoTurning;

		 BaseAimRotation_SinceStopMoving = GetBaseAimRotation();
	  }
	}

	if (AO_Yaw > 90.f){
	  SetTurningInPlace(ETurningInPlace::RTIP_TurnRight);
	}

	if (AO_Yaw < -90.f){
	  SetTurningInPlace(ETurningInPlace::RTIP_TurnLeft);
	}
  }
}
*/