// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/APITest/RecorMatchStatsWidget.h"

#include "Components/Button.h"
#include "UI/GameStats/GameStatsManager.h"
#include "UI/HTTP/HTTPRequestTypes.h"

void URecorMatchStatsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	GameStatsManager = NewObject<UGameStatsManager>(this, GameStatsManagerClass);
	Button_RecordMatchStats->OnClicked.AddDynamic(this, &URecorMatchStatsWidget::RecordMatchStatsButtonClicked);
}

void URecorMatchStatsWidget::RecordMatchStatsButtonClicked()
{
	check(GameStatsManager);

	const FString Username = TEXT("TestUser");
	FDSRecordMatchStatsInput RecordMatchStatsInput;
	RecordMatchStatsInput.username = Username;
	RecordMatchStatsInput.matchStats.hits = 1;
	RecordMatchStatsInput.matchStats.headShotElims = 5;

	GameStatsManager->RecordMatchStats(RecordMatchStatsInput);
}
