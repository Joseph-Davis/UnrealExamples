// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "OnlineUI/ALKOnlineUITypes.h"
#include "Blueprint/UserWidget.h"
#include "CommonActivatableWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKOnlineUITypes)

namespace ALKOnlineUITypesPrivate
{
	/** The kit's own class by name; soft, so this file needs none of the screens' headers. */
	template <typename TBase>
	static TSoftClassPtr<TBase> KitClass(const TCHAR* ClassName)
	{
		return TSoftClassPtr<TBase>(FSoftObjectPath(FString::Printf(TEXT("/Script/AssetproLobbyKit.%s"), ClassName)));
	}
}

FALKOnlineScreenClasses::FALKOnlineScreenClasses()
{
	using namespace ALKOnlineUITypesPrivate;
	PlayHub = KitClass<UCommonActivatableWidget>(TEXT("ALKPlayHubScreen"));
	HostGame = KitClass<UCommonActivatableWidget>(TEXT("ALKHostGameScreen"));
	ServerBrowser = KitClass<UCommonActivatableWidget>(TEXT("ALKServerBrowserScreen"));
	Social = KitClass<UCommonActivatableWidget>(TEXT("ALKSocialScreen"));
	Lobby = KitClass<UCommonActivatableWidget>(TEXT("ALKLobbyScreen"));
	ConnectingDialog = KitClass<UCommonActivatableWidget>(TEXT("ALKConnectingDialog"));
	MatchFoundDialog = KitClass<UCommonActivatableWidget>(TEXT("ALKMatchFoundDialog"));
	TextEntryDialog = KitClass<UCommonActivatableWidget>(TEXT("ALKTextEntryDialog"));
	PlayerOptionsDialog = KitClass<UCommonActivatableWidget>(TEXT("ALKPlayerOptionsDialog"));
	ProfileDialog = KitClass<UCommonActivatableWidget>(TEXT("ALKProfileDialog"));
	ReportDialog = KitClass<UCommonActivatableWidget>(TEXT("ALKReportDialog"));
	Results = KitClass<UCommonActivatableWidget>(TEXT("ALKResultsScreen"));
	Leaderboards = KitClass<UCommonActivatableWidget>(TEXT("ALKLeaderboardScreen"));
	Loadout = KitClass<UCommonActivatableWidget>(TEXT("ALKLoadoutScreen"));
	AdminPanel = KitClass<UCommonActivatableWidget>(TEXT("ALKAdminScreen"));
	MatchHUD = KitClass<UUserWidget>(TEXT("ALKMatchHUD"));
}
