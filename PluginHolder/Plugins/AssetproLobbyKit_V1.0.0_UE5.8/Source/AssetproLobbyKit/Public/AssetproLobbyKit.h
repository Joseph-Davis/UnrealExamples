// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

ASSETPROLOBBYKIT_API DECLARE_LOG_CATEGORY_EXTERN(LogALK, Log, All);

class FAssetproLobbyKitModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
