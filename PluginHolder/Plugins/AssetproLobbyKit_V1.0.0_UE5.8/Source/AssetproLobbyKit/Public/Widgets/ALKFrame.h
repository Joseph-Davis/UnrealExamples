// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/Border.h"
#include "Widgets/ALKPaint.h"
#include "ALKFrame.generated.h"

class SALKFrame;

/**
 * How a frame is drawn: a rounded panel with an outline, a shadow, an accent bar and a focus ring, and optionally a
 * soft gradient, a lit top edge, a tint from its leading edge and a glow.
 */
USTRUCT(BlueprintType)
struct ASSETPROLOBBYKIT_API FALKFrameStyle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frame")
	FLinearColor Fill = FLinearColor::Transparent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frame")
	FLinearColor Outline = FLinearColor::Transparent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frame", meta = (ClampMin = "0"))
	float OutlineWidth = 1.f;

	/** Corner radius in pixels. 0 gives square corners. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frame", meta = (ClampMin = "0"))
	float Radius = 10.f;

	/** Shadow under the frame. Alpha 0 hides it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frame|Shadow")
	FLinearColor Shadow = FLinearColor::Transparent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frame|Shadow")
	FVector2D ShadowOffset = FVector2D(0.0, 8.0);

	/** How far the shadow spreads. 0 gives a hard copy of the frame (a chunky, printed look). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frame|Shadow", meta = (ClampMin = "0"))
	float ShadowBlur = 16.f;

	/** A short rounded bar inside the left edge (the focused menu entry). Alpha 0 hides it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frame|Tick")
	FLinearColor Tick = FLinearColor::Transparent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frame|Tick", meta = (ClampMin = "0"))
	float TickWidth = 4.f;

	/** A ring just outside the frame (the focused card or button). Alpha 0 hides it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frame|Ring")
	FLinearColor Ring = FLinearColor::Transparent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frame|Ring", meta = (ClampMin = "0"))
	float RingWidth = 2.f;

	/** Gap between the frame and its ring. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frame|Ring")
	float RingPad = 3.f;

	/**
	 * A soft top-to-bottom gradient over the fill: a little lighter at the top, a little darker at the bottom, so the
	 * frame reads as a lit surface instead of a flat colour. 0 keeps the fill flat; about 0.05 is subtle, 0.2 is strong.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frame|Depth", meta = (ClampMin = "0", ClampMax = "1"))
	float Gradient = 0.f;

	/** A one pixel lit line along the top edge, inside the outline. Alpha 0 hides it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frame|Depth")
	FLinearColor TopEdge = FLinearColor::Transparent;

	/** A tint laid over the fill, strongest at the left edge and thinning out to the right (the focused row). Alpha 0 hides it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frame|Depth")
	FLinearColor Wash = FLinearColor::Transparent;

	/** A soft glow round the frame (the focused card, row or main button). Alpha 0 hides it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frame|Glow")
	FLinearColor Glow = FLinearColor::Transparent;

	/** How far the glow spreads, in pixels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frame|Glow", meta = (ClampMin = "0"))
	float GlowSize = 14.f;
};

/**
 * A border drawn as a rounded panel (see FALKFrameStyle) instead of a brush: menus, panels, focused entries, dialogs,
 * cards and the HUD pieces all use it. It holds one child like any border and can rise in when it appears.
 */
UCLASS()
class ASSETPROLOBBYKIT_API UALKFrame : public UBorder
{
	GENERATED_BODY()

public:
	UALKFrame(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Frame")
	FALKFrameStyle FrameStyle;

	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Frame")
	void SetFrameStyle(const FALKFrameStyle& InStyle);

	const FALKFrameStyle& GetFrameStyle() const { return FrameStyle; }

	/** The frame and its content fade in while rising a few pixels. With Reduce Effects on they only fade. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Frame")
	void PlayIntro(float Duration = 0.2f);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void SynchronizeProperties() override;
#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

private:
	TSharedPtr<SALKFrame> MyFrame;
};
