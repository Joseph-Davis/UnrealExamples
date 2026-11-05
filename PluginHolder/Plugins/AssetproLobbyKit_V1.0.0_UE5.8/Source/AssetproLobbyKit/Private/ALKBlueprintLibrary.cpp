// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "ALKBlueprintLibrary.h"
#include "HUD/ALKHUDSubsystem.h"
#include "Save/ALKSaveSubsystem.h"
#include "ALKDeveloperSettings.h"
#include "ALKMenuSubsystem.h"
#include "Settings/ALKSettingsSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKBlueprintLibrary)

UALKMenuSubsystem* UALKBlueprintLibrary::GetMenuSubsystem(const UObject* WorldContextObject, APlayerController* PlayerController)
{
	const ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
	if (!LocalPlayer && GEngine)
	{
		if (const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
		{
			LocalPlayer = World->GetFirstLocalPlayerFromController();
		}
	}
	return LocalPlayer ? LocalPlayer->GetSubsystem<UALKMenuSubsystem>() : nullptr;
}

UALKSaveSubsystem* UALKBlueprintLibrary::GetSaveSubsystem(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UALKSaveSubsystem>() : nullptr;
}

UALKHUDSubsystem* UALKBlueprintLibrary::GetHUD(const UObject* WorldContextObject, APlayerController* PlayerController)
{
	const ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
	if (!LocalPlayer && GEngine)
	{
		if (const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
		{
			LocalPlayer = World->GetFirstLocalPlayerFromController();
		}
	}
	return LocalPlayer ? LocalPlayer->GetSubsystem<UALKHUDSubsystem>() : nullptr;
}

UALKSettingsSubsystem* UALKBlueprintLibrary::GetSettingsSubsystem(const UObject* WorldContextObject)
{
	if (GEngine)
	{
		if (const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
		{
			if (const UGameInstance* GameInstance = World->GetGameInstance())
			{
				return GameInstance->GetSubsystem<UALKSettingsSubsystem>();
			}
		}
	}
	return nullptr;
}

const UALKTheme* UALKBlueprintLibrary::GetActiveTheme()
{
	return UALKDeveloperSettings::GetTheme();
}

void UALKBlueprintLibrary::SetRuntimeTheme(UALKTheme* NewTheme)
{
	UALKDeveloperSettings::SetRuntimeTheme(NewTheme);
}

const UALKMenuConfig* UALKBlueprintLibrary::GetActiveMenuConfig()
{
	return UALKDeveloperSettings::GetMenuConfig();
}

UALKConfirmDialog* UALKBlueprintLibrary::ShowConfirmationDialog(const UObject* WorldContextObject, APlayerController* PlayerController, FText Title, FText Message, FText ConfirmText, FText CancelText, bool bDestructive, const FALKDialogResultDelegate& OnResult)
{
	UALKMenuSubsystem* Menu = GetMenuSubsystem(WorldContextObject, PlayerController);
	if (!Menu)
	{
		return nullptr;
	}

	FALKDialogDescriptor Descriptor;
	Descriptor.Title = Title;
	Descriptor.Message = Message;
	Descriptor.ConfirmText = ConfirmText;
	Descriptor.CancelText = CancelText;
	Descriptor.bDestructive = bDestructive;
	return Menu->ShowConfirmDialog(Descriptor, OnResult);
}
