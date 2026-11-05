// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Types/SlateEnums.h"
#include "Widgets/ALKUserWidget.h"
#include "ALKSocialStrip.generated.h"

class UALKFriendsPanel;
class UALKPartyPanel;
class USizeBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FALKOnSocialStripNavigateOut);

/**
 * The social column of the main menu: the party panel on top and, under it, the friends panel in its short form (friends
 * who are online, a few lines, an "All friends" button that opens the friends screen). Place it at the right of a menu,
 * send the focus into it with Focus First and take it back in On Navigate Out, which fires when the player presses Left
 * on the column's left edge. It shows nothing while online play is turned off.
 *
 * Designer widget names: StripSize (a Size Box), PartyPanel, FriendsPanel.
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKSocialStrip : public UALKUserWidget
{
	GENERATED_BODY()

public:
	/** Puts the focus on the column's first button or line. False when there is nothing to focus (online play is off). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Social")
	bool FocusFirst();

	/** True while the focus is somewhere in the column. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Social")
	bool HasFocusInside() const;

	/** The party panel on top. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Social")
	UALKPartyPanel* GetPartyPanel() const { return PartyPanel; }

	/** The friends panel under it. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Social")
	UALKFriendsPanel* GetFriendsPanel() const { return FriendsPanel; }

	/** Changes how wide the column is. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Social")
	void SetStripWidth(float InWidth);

	/** How wide the column is. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assetpro Lobby|Social", meta = (ClampMin = "240"))
	float StripWidth = 340.f;

	/** Left was pressed on the column's left edge: move the focus back to your menu here. */
	UPROPERTY(BlueprintAssignable, Category = "Assetpro Lobby|Social")
	FALKOnSocialStripNavigateOut OnNavigateOutEvent;

	/** The same for C++ (AddUObject / AddLambda). */
	FSimpleMulticastDelegate OnNavigateOut;

protected:
	virtual void NativeBuildDefaultLayout(UWidgetTree& Tree) override;
	virtual void NativeOnLayoutReady() override;
	virtual void NativeConstruct() override;

	/** Gives the column its width. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Social", meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> StripSize;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Social", meta = (BindWidgetOptional))
	TObjectPtr<UALKPartyPanel> PartyPanel;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Social", meta = (BindWidgetOptional))
	TObjectPtr<UALKFriendsPanel> FriendsPanel;

private:
	/** Left found nothing more inside the column. */
	UFUNCTION()
	UWidget* HandleNavigateLeft(EUINavigation Direction);

	bool IsOnlineEnabled() const;
};
