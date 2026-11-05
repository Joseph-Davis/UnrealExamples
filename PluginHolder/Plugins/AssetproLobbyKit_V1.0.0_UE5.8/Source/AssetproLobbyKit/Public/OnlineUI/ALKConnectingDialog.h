// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Online/ALKOnlineTypes.h"
#include "Screens/ALKScreen.h"
#include "ALKConnectingDialog.generated.h"

class UALKButton;
class UALKSpinner;
class UBorder;
class UImage;
class UTextBlock;

/**
 * The small dialog shown while the game is busy getting somewhere: "Creating the game...", "Looking for games...",
 * "Joining Friday Night Arena...", "Leaving...". It has a spinner and a Cancel button (also Back), and closes itself as
 * soon as the online subsystem is idle again, in a lobby or in a match. The Online Flow subsystem opens it by itself.
 *
 * Designer widget names: DimImage, Panel, MapBand (the map's picture across the top of the panel while joining a game
 * whose map has one), Spinner, TitleText, DetailText, CancelButton.
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKConnectingDialog : public UALKScreen
{
	GENERATED_BODY()

public:
	UALKConnectingDialog(const FObjectInitializer& ObjectInitializer);

	/** Stops what the dialog waits for: the search while looking for games, else hosting or joining. Does nothing while leaving. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Dialog")
	void CancelConnecting();

	/** The line the dialog shows for an online state. Empty for the states it closes on. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Dialog")
	FText GetStateText(EALKOnlineState OnlineState) const;

protected:
	virtual void NativeBuildDefaultLayout(UWidgetTree& Tree) override;
	virtual void NativeOnLayoutReady() override;
	virtual void NativeApplyTheme(const UALKTheme& Theme) override;
	virtual void NativeOnActivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual bool OnBackRequested_Implementation() override;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Dialog", meta = (BindWidgetOptional))
	TObjectPtr<UImage> DimImage;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Dialog", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Panel;

	/** The picture of the map of the game being joined or created, as a soft band across the top. Hidden when there is none. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Dialog", meta = (BindWidgetOptional))
	TObjectPtr<class UALKThumbnail> MapBand;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Dialog", meta = (BindWidgetOptional))
	TObjectPtr<UALKSpinner> Spinner;

	/** What the game is doing. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Dialog", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	/** A second, smaller line: what Cancel does. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Dialog", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DetailText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Dialog", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> CancelButton;

private:
	/** Shows the line for the state, or closes the dialog when there is nothing left to wait for. */
	void RefreshState(bool bForce);

	EALKOnlineState ShownState = EALKOnlineState::Idle;
};
