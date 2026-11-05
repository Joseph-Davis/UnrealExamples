// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Screens/ALKScreen.h"
#include "Types/SlateEnums.h"
#include "ALKSocialScreen.generated.h"

class UALKFriendsPanel;
class UALKPartyPanel;
class UImage;
class UTextBlock;
class UVerticalBox;

/**
 * Friends and party on one screen: the friends list at the left (friends and recent players, with Join and Invite), the
 * party at the right and, under it, a button that opens the platform's own invite window. Left and Right move between
 * the two panels; Q / E (LB / RB) switch between friends and recent players.
 *
 * Open it with Open Social (Online Flow subsystem) or the Open Social menu action.
 *
 * Designer widget names: BackgroundImage, SystemLabelText, TitleText, FriendsPanel, PartyColumn, PartyPanel,
 * PlatformInviteButton.
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKSocialScreen : public UALKScreen
{
	GENERATED_BODY()

public:
	UALKSocialScreen(const FObjectInitializer& ObjectInitializer);

	/** Moves the focus to the friends list. With no friends to show it opens the platform's invite window instead. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Social")
	void FocusFriends();

	/** Opens the platform's own invite window, when the online backend has one. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Social")
	void ShowPlatformInvite();

protected:
	virtual void NativeBuildDefaultLayout(UWidgetTree& Tree) override;
	virtual void NativeOnLayoutReady() override;
	virtual void NativeApplyTheme(const UALKTheme& Theme) override;
	virtual bool NativeHandleMenuKey(const FKey& Key) override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Social", meta = (BindWidgetOptional))
	TObjectPtr<UImage> BackgroundImage;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Social", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SystemLabelText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Social", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	/** The friends list, at the left. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Social", meta = (BindWidgetOptional))
	TObjectPtr<UALKFriendsPanel> FriendsPanel;

	/** Holds the party panel and the platform invite button, at the right. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Social", meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> PartyColumn;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Social", meta = (BindWidgetOptional))
	TObjectPtr<UALKPartyPanel> PartyPanel;

	/** "Invite through your platform". */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Social", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> PlatformInviteButton;

private:
	/** Right at the friends panel's edge goes to the party; Left at the party column's edge goes back to the friends. */
	UFUNCTION()
	UWidget* HandleNavigateToParty(EUINavigation Direction);

	UFUNCTION()
	UWidget* HandleNavigateToFriends(EUINavigation Direction);

	void ShowFriendsTab(bool bRecent);
};
