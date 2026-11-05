// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/ALKButton.h"
#include "ALKCardButton.generated.h"

class UALKFrame;
class UALKThumbnail;
class UTextBlock;
class UTexture2D;
class UALKCardButton;

/**
 * A card you can pick: a tile on the play hub ("Host Game"), a playlist, a map. It shows an icon or a picture, a
 * title, a line of small text, and optionally a badge over the picture, a tag beside the title, a status line with a
 * dot and a call-to-action pill. The whole card is the button; focus draws the theme's ring round it.
 *
 * Give it a size with a Size Box (or a fill slot). Designer widget names: Background, Picture, IconBox, CardIcon, Label
 * (the title), SubtitleText, BadgeChip, CornerChip, FooterText, FooterDot, ActionPill.
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKCardButton : public UALKButton
{
	GENERATED_BODY()

public:
	UALKCardButton(const FObjectInitializer& ObjectInitializer);

	/** The title, the small line under it and the icon in the corner (None hides the icon). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Card")
	void SetCard(const FText& Title, const FText& Subtitle, EALKIcon InIcon = EALKIcon::None);

	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Card")
	void SetSubtitle(const FText& Subtitle);

	/**
	 * Shows a picture across the top of the card instead of the icon. An empty picture shows generated placeholder art
	 * for PlaceholderId (see ALKThumbnail).
	 */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Card")
	void SetPicture(UTexture2D* InPicture, FName PlaceholderId);

	/** Hides the picture again. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Card")
	void ClearPicture();

	/** A small label over the top left of the picture ("Featured playlist"). Empty hides it. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Card")
	void SetBadge(const FText& Text);

	/** A tag at the right of the title ("4v4"). Empty hides it. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Card")
	void SetCornerChip(const FText& Text);

	/** A status line under the subtitle with a coloured dot ("1,284 playing"). Empty hides it. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Card")
	void SetFooter(const FText& Text, FLinearColor DotColor);

	/** A filled pill at the lower right that names what picking the card does ("Find match"). Empty hides it. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Card")
	void SetActionLabel(const FText& Text, EALKIcon InIcon = EALKIcon::None);

	/** Larger title and padding (the featured tile). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assetpro Menu|Card")
	bool bLarge = false;

	/** Fired when the card gets keyboard / gamepad / mouse focus. */
	TMulticastDelegate<void(UALKCardButton*)> OnCardFocused;

protected:
	virtual void NativeBuildDefaultLayout(UWidgetTree& Tree) override;
	virtual void NativeApplyTheme(const UALKTheme& Theme) override;
	virtual void NativeApplyVisualState(const UALKTheme& Theme, float Highlight, bool bEnabled) override;
	virtual void NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent) override;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Card", meta = (BindWidgetOptional))
	TObjectPtr<UALKThumbnail> Picture;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Card", meta = (BindWidgetOptional))
	TObjectPtr<UALKFrame> IconBox;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Card", meta = (BindWidgetOptional))
	TObjectPtr<UALKIconWidget> CardIcon;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Card", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SubtitleText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Card", meta = (BindWidgetOptional))
	TObjectPtr<UALKFrame> BadgeChip;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Card", meta = (BindWidgetOptional))
	TObjectPtr<UALKFrame> CornerChip;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Card", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> FooterText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Card", meta = (BindWidgetOptional))
	TObjectPtr<UALKIconWidget> FooterDot;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Card", meta = (BindWidgetOptional))
	TObjectPtr<UALKFrame> ActionPill;

private:
	void RefreshVisibility();

	EALKIcon CardIconType = EALKIcon::None;
	bool bHasPicture = false;
	FLinearColor FooterDotColor = FLinearColor::Transparent;
};
