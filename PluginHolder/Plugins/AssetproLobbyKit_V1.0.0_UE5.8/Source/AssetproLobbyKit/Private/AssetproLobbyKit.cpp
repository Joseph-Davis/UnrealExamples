// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "AssetproLobbyKit.h"
#include "Widgets/ALKPointerTracker.h"
#include "Misc/CoreDelegates.h"
#include "Misc/EngineVersionComparison.h"

DEFINE_LOG_CATEGORY(LogALK);

void FAssetproLobbyKitModule::StartupModule()
{
	// Slate exists by the time the engine finishes initializing (never in commandlets or dedicated servers).
#if UE_VERSION_OLDER_THAN(5, 8, 0)
	FSimpleMulticastDelegate& OnPostEngineInit = FCoreDelegates::OnPostEngineInit;
#else
	FSimpleMulticastDelegate& OnPostEngineInit = FCoreDelegates::GetOnPostEngineInit();
#endif
	OnPostEngineInit.AddLambda([]()
	{
		FALKPointerTracker::Register();
	});
}

void FAssetproLobbyKitModule::ShutdownModule()
{
	FALKPointerTracker::Unregister();
}

IMPLEMENT_MODULE(FAssetproLobbyKitModule, AssetproLobbyKit)
