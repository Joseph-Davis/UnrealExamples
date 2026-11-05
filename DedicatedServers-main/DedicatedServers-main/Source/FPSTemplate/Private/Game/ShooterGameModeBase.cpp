// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/ShooterGameModeBase.h"

#include "Game/MatchGameState.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Player/DSPlayerController.h"
#include "Player/MatchPlayerState.h"

AShooterGameModeBase::AShooterGameModeBase()
{
	RespawnTime = 2.f;
}

void AShooterGameModeBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
}

void AShooterGameModeBase::StartPlayerElimination(float ElimTime, ACharacter* ElimmedCharacter,
	APlayerController* VictimController, APlayerController* AttackerController)
{
	FTimerDelegate ElimTimerDelegate;
	FTimerHandle TimerHandle;
	Timers.Add(VictimController, TimerHandle);
	ElimTimerDelegate.BindLambda([this, ElimmedCharacter, VictimController, AttackerController]()
	{
		PlayerEliminated(ElimmedCharacter, VictimController, AttackerController);
		GetWorldTimerManager().ClearTimer(Timers[VictimController]);
		Timers.Remove(VictimController);
		
	});
	GetWorldTimerManager().SetTimer(TimerHandle, ElimTimerDelegate, ElimTime + RespawnTime, false);
}

void AShooterGameModeBase::PlayerEliminated(ACharacter* ElimmedCharacter, APlayerController* VictimController,
                                            APlayerController* AttackerController)
{
	RequestRespawn(ElimmedCharacter, VictimController);
}

void AShooterGameModeBase::RequestRespawn(ACharacter* ElimmedCharacter, AController* ElimmedController)
{
	if (ElimmedCharacter)
	{
		ElimmedCharacter->Reset();
		ElimmedCharacter->Destroy();
	}
	if (ElimmedController)
	{
		TArray<AActor*> PlayerStarts;
		UGameplayStatics::GetAllActorsOfClass(this, APlayerStart::StaticClass(), PlayerStarts);
		int32 Selection = FMath::RandRange(0, PlayerStarts.Num() - 1);
		RestartPlayerAtPlayerStart(ElimmedController, PlayerStarts[Selection]);
	}
}

void AShooterGameModeBase::OnMatchEnded()
{
	Super::OnMatchEnded();

	TArray<FString> LeaderIds;
	if (AMatchGameState* MatchGameState = GetGameState<AMatchGameState>(); IsValid(MatchGameState))
	{
		TArray<AMatchPlayerState*> Leaders = MatchGameState->GetLeaders();
		for (AMatchPlayerState* Leader : Leaders)
		{
			if (ADSPlayerController* LeaderPC = Cast<ADSPlayerController>(Leader->GetPlayerController()); IsValid(LeaderPC))
			{
				LeaderIds.Add(LeaderPC->Username);
			}
		}
	}
	UpdateLeaderboard(LeaderIds);
}
