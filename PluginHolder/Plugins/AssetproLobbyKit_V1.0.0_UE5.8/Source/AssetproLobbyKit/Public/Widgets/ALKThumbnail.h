// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "ALKThumbnail.generated.h"

class SALKThumbnail;
class UTexture2D;

/**
 * A picture with rounded corners that always covers its box (it is cropped, never squashed): map and playlist
 * pictures, save screenshots, chapter stills. Without a picture it shows generated placeholder art, the same every
 * time for the same Placeholder Id, so lists look finished before any art exists.
 */
UCLASS()
class ASSETPROLOBBYKIT_API UALKThumbnail : public UWidget
{
	GENERATED_BODY()

public:
	/** The picture. Empty shows the placeholder art. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thumbnail")
	TObjectPtr<UTexture2D> Picture;

	/** Picks the placeholder art (a map Id, a playlist Id...). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thumbnail")
	FName PlaceholderId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thumbnail", meta = (ClampMin = "0"))
	float CornerRadius = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thumbnail")
	FLinearColor Tint = FLinearColor::White;

	/** Size the thumbnail asks for (a stretched slot or a size box overrides it). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thumbnail")
	FVector2D ThumbnailSize = FVector2D(320.0, 180.0);

	/**
	 * A veil over the lower part of the picture: it fades into this colour (the panel under the picture), so a title on
	 * or right under it reads and the picture does not end in a hard edge. Alpha sets the strength; 0 shows no veil.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thumbnail")
	FLinearColor Veil = FLinearColor::Transparent;

	/**
	 * Takes the veil from the active theme instead of Veil: its panel colour at its Picture Veil strength. For pictures
	 * at the top of a panel with their title right under them.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thumbnail")
	bool bThemeVeil = false;

	/** Shows a picture, or the placeholder art for InPlaceholderId when InPicture is empty. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Decor")
	void SetPicture(UTexture2D* InPicture, FName InPlaceholderId = NAME_None);

	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Decor")
	void SetCornerRadius(float InRadius);

	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Decor")
	void SetTint(const FLinearColor& InTint);

	/** Sets the veil over the lower part of the picture (see Veil) and stops following the theme's. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Decor")
	void SetVeil(const FLinearColor& InVeil);

	/** Follows the theme's veil (see Theme Veil), or stops following it. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Decor")
	void SetThemeVeil(bool bInThemeVeil);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void SynchronizeProperties() override;
	virtual void BeginDestroy() override;
#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

private:
	void HandleThemeChanged();

	TSharedPtr<SALKThumbnail> MyThumbnail;
	FDelegateHandle ThemeChangedHandle;

	/** Keeps the generated placeholder alive while it is shown. */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> Placeholder;
};
