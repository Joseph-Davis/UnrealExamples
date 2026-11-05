// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/ALKUserWidget.h"
#include "ALKMatchHUDBase.generated.h"

/**
 * What the Online Flow asks of the match overlay: show the scoreboard while its key is held, open the chat line, step
 * the spectator camera, show an announcement. ALK Match HUD is the kit's overlay; a match overlay of your own that
 * should react to the same keys derives from this class (C++ or Blueprint) and is set in the menu config's Online Screens.
 */
UCLASS(Abstract, Blueprintable)
class ASSETPROLOBBYKIT_API UALKMatchHUDBase : public UALKUserWidget
{
	GENERATED_BODY()

public:
	/** Shows or hides the scoreboard. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|HUD")
	void SetScoreboardVisible(bool bVisible);
	virtual void SetScoreboardVisible_Implementation(bool bVisible) {}

	/** Opens the chat line for typing: to everyone, or to the team. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|HUD")
	void OpenChat(bool bTeamChat);
	virtual void OpenChat_Implementation(bool bTeamChat) {}

	/** True while the player is typing a chat line. */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Assetpro Lobby|HUD")
	bool IsChatOpen() const;
	virtual bool IsChatOpen_Implementation() const { return false; }

	/** While spectating: watch the previous (-1) or the next (+1) player. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|HUD")
	void SpectateStep(int32 Direction);
	virtual void SpectateStep_Implementation(int32 Direction) {}

	/** A large line in the upper middle of the screen for a few seconds ("Match point", "Final minute"). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|HUD")
	void ShowAnnouncement(const FText& Title, const FText& Subtitle, float Seconds);
	virtual void ShowAnnouncement_Implementation(const FText& Title, const FText& Subtitle, float Seconds) {}
};
