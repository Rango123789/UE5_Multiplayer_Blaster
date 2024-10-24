// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapons/ProjectileWeapon.h"
#include "Weapons/Projectile.h"

void AProjectileWeapon::Fire(const FVector& HitTarget)
{
	Super::Fire(HitTarget);

	//this block spawns ACasing for costmetic effeft
	{

	}

	//this HOSTING function is called in all machine, but we only let the server spawn the Projectile, this block only run in the server
	if (HasAuthority()) 
	{

		FTransform MuzzleFlashSocket_Transform_InWeapon = WeaponMesh->GetSocketTransform(FName("MuzzleFlash"));

		FVector SpawnLocation = MuzzleFlashSocket_Transform_InWeapon.GetLocation();
	
		FVector FacingDirection = (HitTarget - SpawnLocation);

		FRotator SpawnRotation = FacingDirection.Rotation(); //accept ROLL = 0 -> YAW & PTICH

		FActorSpawnParameters SpawnParams;

		//stephen: SpawnParams.Owner = GetOwner() too, what the heck? is that sensible? :D :D
		SpawnParams.Owner = this; //the Owner of the to-be-spawn projectile is the ProectileWeapon - make sense!

		SpawnParams.Instigator = Cast<APawn>(GetOwner()); 

		GetWorld()->SpawnActor<AProjectile>(
			ProjectileClass,
			SpawnLocation,SpawnRotation,SpawnParams
		);
	}
}
