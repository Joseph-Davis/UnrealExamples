// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Screens/ALKMenuListScreen.h"
#include "ALKMainMenuScreen.generated.h"

class UALKSocialStrip;
class UUserWidget;

/**
 * Main menu: the game title (or logo) and the menu config's Main Menu Entries over the atmosphere, the version in a
 * corner, and in the right column the menu config's Main Menu Side Widget (the party and the friends who are online).
 * Right, or the Party key, moves into the column; Left comes back. Back offers to quit when the menu config says so.
 *
 * Designer widget names: BackgroundImage, LogoImage, TitleText, SubtitleText, EntriesBox, FooterNote, SideWidget.
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKMainMenuScreen : public UALKMenuListScreen
{
	GENERATED_BODY()

public:
	UALKMainMenuScreen(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeBuildDefaultLayout(UWidgetTree& Tree) override;
	virtual void NativeOnLayoutReady() override;
	virtual void NativeApplyTheme(const UALKTheme& Theme) override;
	virtual TArray<FALKMenuEntry> NativeGetConfigEntries() const override;
	virtual bool OnBackRequested_Implementation() override;
	virtual bool NativeHandleMenuKey(const FKey& Key) override;
	virtual FNavigationReply NativeOnNavigation(const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent, const FNavigationReply& InDefaultReply) override;

	/** The right column (menu config: Main Menu Side Widget Class), or null. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Main Menu", meta = (BindWidgetOptional))
	TObjectPtr<UUserWidget> SideWidget;

private:
	/** The side widget when it is the kit's social strip. */
	UALKSocialStrip* GetSocialStrip() const;
	/** Moves the focus into the side column. False when there is nothing to focus there. */
	bool FocusSideColumn();
	void HandleSideNavigateOut();

	/** The entry that had the focus when the player moved into the side column: Left brings them back to it. */
	TWeakObjectPtr<UALKButton> EntryBeforeSide;
};
