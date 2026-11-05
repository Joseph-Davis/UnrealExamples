// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "ALKDeveloperSettings.h"
#include "ALKTheme.h"
#include "ALKMenuConfig.h"
#include "Online/ALKOnlineConfig.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKDeveloperSettings)

UALKDeveloperSettings::UALKDeveloperSettings()
{
}

FName UALKDeveloperSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}

const UALKTheme* UALKDeveloperSettings::GetTheme()
{
	const UALKDeveloperSettings* Settings = GetDefault<UALKDeveloperSettings>();
	if (Settings->RuntimeTheme)
	{
		return Settings->RuntimeTheme;
	}

	if (!Settings->Theme.IsNull())
	{
		if (!Settings->LoadedTheme || Settings->LoadedTheme != Settings->Theme.Get())
		{
			Settings->LoadedTheme = Settings->Theme.LoadSynchronous();
		}
		if (Settings->LoadedTheme)
		{
			return Settings->LoadedTheme;
		}
	}

	if (!Settings->ThemeClass.IsNull())
	{
		if (UClass* Class = Settings->ThemeClass.LoadSynchronous())
		{
			return Class->GetDefaultObject<UALKTheme>();
		}
	}

	return GetDefault<UALKTheme>();
}

const UALKMenuConfig* UALKDeveloperSettings::GetMenuConfig()
{
	const UALKDeveloperSettings* Settings = GetDefault<UALKDeveloperSettings>();
	if (!Settings->MenuConfig.IsNull())
	{
		if (!Settings->LoadedMenuConfig || Settings->LoadedMenuConfig != Settings->MenuConfig.Get())
		{
			Settings->LoadedMenuConfig = Settings->MenuConfig.LoadSynchronous();
		}
		if (Settings->LoadedMenuConfig)
		{
			return Settings->LoadedMenuConfig;
		}
	}

	if (!Settings->MenuConfigClass.IsNull())
	{
		if (UClass* Class = Settings->MenuConfigClass.LoadSynchronous())
		{
			return Class->GetDefaultObject<UALKMenuConfig>();
		}
	}

	return GetDefault<UALKMenuConfig>();
}

const UALKOnlineConfig* UALKDeveloperSettings::GetOnlineConfig()
{
	const UALKDeveloperSettings* Settings = GetDefault<UALKDeveloperSettings>();
	if (!Settings->OnlineConfig.IsNull())
	{
		if (!Settings->LoadedOnlineConfig || Settings->LoadedOnlineConfig != Settings->OnlineConfig.Get())
		{
			Settings->LoadedOnlineConfig = Settings->OnlineConfig.LoadSynchronous();
		}
		if (Settings->LoadedOnlineConfig)
		{
			return Settings->LoadedOnlineConfig;
		}
	}

	if (!Settings->OnlineConfigClass.IsNull())
	{
		if (UClass* Class = Settings->OnlineConfigClass.LoadSynchronous())
		{
			return Class->GetDefaultObject<UALKOnlineConfig>();
		}
	}

	return GetDefault<UALKOnlineConfig>();
}

void UALKDeveloperSettings::SetRuntimeTheme(UALKTheme* NewTheme)
{
	UALKDeveloperSettings* Settings = GetMutableDefault<UALKDeveloperSettings>();
	if (Settings->RuntimeTheme != NewTheme)
	{
		Settings->RuntimeTheme = NewTheme;
		UALKTheme::OnAnyThemeChanged().Broadcast();
	}
}

#if WITH_EDITOR
void UALKDeveloperSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	LoadedTheme = nullptr;
	LoadedMenuConfig = nullptr;
	LoadedOnlineConfig = nullptr;
	UALKTheme::OnAnyThemeChanged().Broadcast();
}
#endif
