// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "HUD/ALKHUDSubsystem.h"
#include "HUD/ALKHUDLayer.h"
#include "ALKDeveloperSettings.h"
#include "ALKMenuConfig.h"
#include "ALKMenuSubsystem.h"
#include "Settings/ALKSettingsSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UObjectGlobals.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKHUDSubsystem)

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

void UALKHUDSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UALKMenuSubsystem* Menus = Collection.InitializeDependency<UALKMenuSubsystem>())
	{
		Menus->OnChapterStarted.AddDynamic(this, &ThisClass::HandleChapterStarted);
		UnlockedHandle = Menus->OnChapterUnlockedNative.AddUObject(this, &ThisClass::HandleChapterUnlocked);
	}
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostLoadMap);
}

void UALKHUDSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	if (const ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UALKMenuSubsystem* Menus = LocalPlayer->GetSubsystem<UALKMenuSubsystem>())
		{
			Menus->OnChapterStarted.RemoveDynamic(this, &ThisClass::HandleChapterStarted);
			Menus->OnChapterUnlockedNative.Remove(UnlockedHandle);
		}
	}
	if (OverlayLayer)
	{
		OverlayLayer->RemoveFromParent();
		OverlayLayer = nullptr;
	}
	if (Layer)
	{
		Layer->RemoveFromParent();
		Layer = nullptr;
	}
	Super::Deinitialize();
}

UALKHUDLayer* UALKHUDSubsystem::GetHUDLayer()
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	APlayerController* PlayerController = LocalPlayer ? LocalPlayer->GetPlayerController(LocalPlayer->GetWorld()) : nullptr;
	if (!PlayerController)
	{
		return nullptr;
	}

	// A layer belongs to one world: a new level gets a new one.
	if (Layer && (Layer->GetWorld() != PlayerController->GetWorld() || !Layer->IsInViewport()))
	{
		Layer->RemoveFromParent();
		Layer = nullptr;
	}
	if (!Layer)
	{
		TSubclassOf<UALKHUDLayer> LayerClass = UALKDeveloperSettings::GetMenuConfig()->HUDLayerClass;
		if (!LayerClass)
		{
			LayerClass = UALKHUDLayer::StaticClass();
		}
		Layer = CreateWidget<UALKHUDLayer>(PlayerController, LayerClass);
		if (Layer)
		{
			// Under the menus (the root layout sits at 1000).
			Layer->AddToPlayerScreen(500);
			Layer->SetHidden(bHUDHidden);
			Layer->SetNotificationDrop(NotificationDrop);
		}
	}
	return Layer;
}

void UALKHUDSubsystem::ShowInteractionPrompt(const FALKInteractionPrompt& Prompt)
{
	if (UALKHUDLayer* HUD = GetHUDLayer())
	{
		HUD->ShowPrompt(Prompt);
	}
}

void UALKHUDSubsystem::HideInteractionPrompt()
{
	if (Layer)
	{
		Layer->HidePrompt();
	}
}

void UALKHUDSubsystem::SetInteractionProgress(float Progress)
{
	if (Layer)
	{
		Layer->SetPromptProgress(Progress);
	}
}

bool UALKHUDSubsystem::IsInteractionPromptShown() const
{
	return Layer && Layer->IsPromptShown();
}

void UALKHUDSubsystem::ShowSubtitle(const FText& Speaker, const FText& Line, float Duration)
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	const UGameInstance* GameInstance = LocalPlayer ? LocalPlayer->GetGameInstance() : nullptr;
	const UALKSettingsSubsystem* Settings = GameInstance ? GameInstance->GetSubsystem<UALKSettingsSubsystem>() : nullptr;
	if (Settings && !Settings->AreSubtitlesEnabled())
	{
		return;
	}
	// Long enough to read: about 18 letters a second, never under 2 seconds.
	const float Shown = Duration > 0.f ? Duration : FMath::Clamp(1.5f + Line.ToString().Len() * 0.055f, 2.f, 9.f);
	if (UALKHUDLayer* HUD = GetHUDLayer())
	{
		HUD->AddSubtitle(Speaker, Line, Shown);
	}
}

void UALKHUDSubsystem::ClearSubtitles()
{
	if (Layer)
	{
		Layer->ClearSubtitles();
	}
}

void UALKHUDSubsystem::ShowNotification(const FText& Title, const FText& Detail, EALKIcon Icon, float Duration)
{
	// The HUD sits under the menus: with one open, the notification goes to the layer above them.
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	const UALKMenuSubsystem* Menus = LocalPlayer ? LocalPlayer->GetSubsystem<UALKMenuSubsystem>() : nullptr;
	if (Menus && Menus->IsAnyMenuOpen())
	{
		ShowMenuNotification(Title, Detail, Icon, Duration);
		return;
	}
	if (UALKHUDLayer* HUD = GetHUDLayer())
	{
		HUD->AddNotification(Title, Detail, Icon, Duration);
	}
}

void UALKHUDSubsystem::ShowMenuNotification(const FText& Title, const FText& Detail, EALKIcon Icon, float Duration)
{
	if (UALKHUDLayer* Overlay = GetOverlayLayer())
	{
		Overlay->AddNotification(Title, Detail, Icon, Duration);
	}
}

UALKHUDLayer* UALKHUDSubsystem::GetOverlayLayer()
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	APlayerController* PlayerController = LocalPlayer ? LocalPlayer->GetPlayerController(LocalPlayer->GetWorld()) : nullptr;
	if (!PlayerController)
	{
		return nullptr;
	}
	if (OverlayLayer && (OverlayLayer->GetWorld() != PlayerController->GetWorld() || !OverlayLayer->IsInViewport()))
	{
		OverlayLayer->RemoveFromParent();
		OverlayLayer = nullptr;
	}
	if (!OverlayLayer)
	{
		TSubclassOf<UALKHUDLayer> LayerClass = UALKDeveloperSettings::GetMenuConfig()->HUDLayerClass;
		if (!LayerClass)
		{
			LayerClass = UALKHUDLayer::StaticClass();
		}
		OverlayLayer = CreateWidget<UALKHUDLayer>(PlayerController, LayerClass);
		if (OverlayLayer)
		{
			// Above the menus (the root layout sits at 1000).
			OverlayLayer->AddToPlayerScreen(1500);
		}
	}
	return OverlayLayer;
}

void UALKHUDSubsystem::ShowCheckpointNotification()
{
	ShowNotification(LOCTEXT("HUDCheckpoint", "Checkpoint"), LOCTEXT("HUDProgressSaved", "Progress saved"), EALKIcon::Checkpoint, 3.f);
	ShowSavingIndicator(1.5f);
}

void UALKHUDSubsystem::ShowChapterTitle(int32 ChapterNumber, const FText& ChapterName)
{
	if (UALKHUDLayer* HUD = GetHUDLayer())
	{
		HUD->ShowChapterCard(ChapterNumber, ChapterName);
	}
}

bool UALKHUDSubsystem::IsChapterTitleActive() const
{
	return PendingChapterNumber > 0 || (Layer && Layer->IsChapterCardShown());
}

void UALKHUDSubsystem::ShowSavingIndicator(float MinimumSeconds, bool bUntilHidden)
{
	if (UALKHUDLayer* HUD = GetHUDLayer())
	{
		HUD->ShowSaving(FMath::Max(0.f, MinimumSeconds), bUntilHidden);
	}
}

void UALKHUDSubsystem::HideSavingIndicator()
{
	if (Layer)
	{
		Layer->HideSaving();
	}
}

void UALKHUDSubsystem::SetNotificationDrop(float Drop)
{
	NotificationDrop = FMath::Max(0.f, Drop);
	if (Layer)
	{
		Layer->SetNotificationDrop(NotificationDrop);
	}
}

void UALKHUDSubsystem::SetHUDHidden(bool bHidden)
{
	bHUDHidden = bHidden;
	if (Layer)
	{
		Layer->SetHidden(bHidden);
	}
}

void UALKHUDSubsystem::HandleChapterStarted(FName ChapterId)
{
	const UALKMenuConfig* Config = UALKDeveloperSettings::GetMenuConfig();
	const FALKChapter* Chapter = Config->FindChapter(ChapterId);
	if (!Config->bShowChapterTitleOnStart || !Chapter)
	{
		return;
	}
	const int32 Number = Config->Chapters.IndexOfByPredicate([ChapterId](const FALKChapter& Each) { return Each.Id == ChapterId; }) + 1;
	if (Chapter->Level.IsNull())
	{
		ShowChapterTitle(Number, Chapter->Name);
	}
	else
	{
		// The card waits for the chapter's level.
		PendingChapterNumber = Number;
		PendingChapterName = Chapter->Name;
	}
}

void UALKHUDSubsystem::HandlePostLoadMap(UWorld* World)
{
	if (PendingChapterNumber <= 0 || !World || !World->IsGameWorld())
	{
		return;
	}
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer || !LocalPlayer->GetPlayerController(World))
	{
		return;
	}
	if (UALKHUDLayer* HUD = GetHUDLayer())
	{
		// A moment after the level appears.
		HUD->ShowChapterCard(PendingChapterNumber, PendingChapterName, 0.8f);
		PendingChapterNumber = 0;
		PendingChapterName = FText::GetEmpty();
	}
}

void UALKHUDSubsystem::HandleChapterUnlocked(FName ChapterId)
{
	const UALKMenuConfig* Config = UALKDeveloperSettings::GetMenuConfig();
	const FALKChapter* Chapter = Config->FindChapter(ChapterId);
	if (Config->bNotifyChapterUnlocks && Chapter)
	{
		ShowNotification(LOCTEXT("HUDChapterUnlocked", "Chapter Unlocked"), Chapter->Name, EALKIcon::Unlock, 4.f);
	}
}

#undef LOCTEXT_NAMESPACE
