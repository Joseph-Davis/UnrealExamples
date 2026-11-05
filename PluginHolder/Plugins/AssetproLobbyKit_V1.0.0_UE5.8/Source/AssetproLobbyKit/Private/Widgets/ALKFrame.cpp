// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Widgets/ALKFrame.h"
#include "Rendering/DrawElements.h"
#include "Brushes/SlateNoResource.h"
#include "Widgets/SCompoundWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKFrame)

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

/** Paints a frame (FALKFrameStyle) behind its content, with an optional rise-in. */
class SALKFrame : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SALKFrame) {}
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		ChildSlot[InArgs._Content.Widget];
	}

	void SetStyle(const FALKFrameStyle& InStyle)
	{
		Style = InStyle;
		Invalidate(EInvalidateWidgetReason::Paint);
	}

	void PlayIntro(float Duration)
	{
		IntroStart = FALKEffects::Now();
		IntroDuration = FMath::Max(0.05f, Duration);
		Invalidate(EInvalidateWidgetReason::Paint);
		if (!bAnimating)
		{
			bAnimating = true;
			RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SALKFrame::Animate));
		}
	}

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
	{
		// The intro: everything fades in, and rises a few pixels unless effects are reduced.
		float Alpha = 1.f;
		float Rise = 0.f;
		const float IntroTime = static_cast<float>(FALKEffects::Now() - IntroStart);
		if (IntroStart >= 0.0 && IntroTime < IntroDuration)
		{
			const float T = FMath::Clamp(IntroTime / IntroDuration, 0.f, 1.f);
			const float Eased = 1.f - FMath::Square(1.f - T);
			Alpha = Eased;
			Rise = FALKEffects::AreReduced() ? 0.f : (1.f - Eased) * 10.f;
		}
		const FWidgetStyle FadedStyle = FWidgetStyle(InWidgetStyle).BlendColorAndOpacityTint(FLinearColor(1.f, 1.f, 1.f, Alpha));
		const FVector2f Size = FVector2f(AllottedGeometry.GetLocalSize());
		const FGeometry Geometry = Rise > 0.f ? AllottedGeometry.MakeChild(Size, FSlateLayoutTransform(FVector2f(0.f, Rise))) : AllottedGeometry;

		if (Size.X >= 1.f && Size.Y >= 1.f)
		{
			const FLinearColor Tint = FadedStyle.GetColorAndOpacityTint();
			const FVector2f Min(0.f, 0.f);
			// Under the frame: its shadow and its glow.
			FALKPaint::Shadow(OutDrawElements, LayerId, Geometry, Min, Size, Style.Radius, Style.Shadow * Tint, FVector2f(Style.ShadowOffset), Style.ShadowBlur);
			if (Style.Glow.A > 0.f && Style.GlowSize > 0.f)
			{
				FALKPaint::Glow(OutDrawElements, LayerId, Geometry, Min, Size, Style.Radius, Style.Glow * Tint, Style.GlowSize);
			}
			FALKPaint::RoundedBox(OutDrawElements, LayerId + 1, Geometry, Min, Size, Style.Radius, Style.Fill * Tint, Style.Outline * Tint, Style.OutlineWidth);

			// Over the fill, inside the outline: the tint from the leading edge, the gradient and the lit top edge. The
			// gradient and the edge belong to the fill, so they fade with it (a disabled button, a see-through row).
			const float Solidity = FMath::Clamp(Style.Fill.A, 0.f, 1.f);
			const float Inset = Style.Outline.A > 0.f ? FMath::Max(0.f, Style.OutlineWidth) : 0.f;
			const FVector2f InnerMin(Inset, Inset);
			const FVector2f InnerMax = Size - InnerMin;
			const float InnerRadius = FMath::Max(0.f, Style.Radius - Inset);
			if (Style.Wash.A > 0.f)
			{
				FALKPaint::GradientBox(OutDrawElements, LayerId + 2, Geometry, InnerMin, InnerMax, InnerRadius, EALKGradient::FadeRight, Style.Wash * Tint);
			}
			if (Style.Gradient > 0.f && Solidity > 0.f)
			{
				FALKPaint::GradientBox(OutDrawElements, LayerId + 2, Geometry, InnerMin, InnerMax, InnerRadius, EALKGradient::Depth,
					Tint * FLinearColor(1.f, 1.f, 1.f, FMath::Min(Style.Gradient, 1.f) * Solidity));
			}
			if (Style.TopEdge.A > 0.f && Solidity > 0.f)
			{
				FALKPaint::TopEdge(OutDrawElements, LayerId + 3, Geometry, Min, Size, Style.Radius, Style.TopEdge * Tint * FLinearColor(1.f, 1.f, 1.f, Solidity), Inset);
			}

			if (Style.Tick.A > 0.f && Style.TickWidth > 0.f)
			{
				// A short bar inside the left edge, centred on the frame. On a row it stays short; on a tall band (the
				// results title) it grows with the band instead of sitting there as a stray dash.
				const float TickInset = FMath::Max(4.f, Style.Radius * 0.6f);
				const float Half = FMath::Min(Size.Y * 0.28f, FMath::Max(14.f, Size.Y * 0.5f - 28.f));
				FALKPaint::RoundedBox(OutDrawElements, LayerId + 4, Geometry, FVector2f(TickInset, Size.Y * 0.5f - Half), FVector2f(TickInset + Style.TickWidth, Size.Y * 0.5f + Half),
					Style.TickWidth * 0.5f, Style.Tick * Tint);
			}
			if (Style.Ring.A > 0.f && Style.RingWidth > 0.f)
			{
				const FVector2f Pad(Style.RingPad + Style.RingWidth, Style.RingPad + Style.RingWidth);
				FALKPaint::RoundedBox(OutDrawElements, LayerId + 4, Geometry, Min - Pad, Size + Pad, Style.Radius + Pad.X, FLinearColor::Transparent, Style.Ring * Tint, Style.RingWidth);
			}
		}
		return SCompoundWidget::OnPaint(Args, Geometry, MyCullingRect, OutDrawElements, LayerId + 5, FadedStyle, bParentEnabled);
	}

private:
	EActiveTimerReturnType Animate(double InCurrentTime, float InDeltaTime)
	{
		Invalidate(EInvalidateWidgetReason::Paint);
		const bool bBusy = IntroStart >= 0.0 && FALKEffects::Now() - IntroStart < IntroDuration + 0.05;
		bAnimating = bBusy;
		return bBusy ? EActiveTimerReturnType::Continue : EActiveTimerReturnType::Stop;
	}

	FALKFrameStyle Style;
	double IntroStart = -1.0;
	float IntroDuration = 0.2f;
	bool bAnimating = false;
};

UALKFrame::UALKFrame(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The frame draws itself; the border's own brush stays empty.
	SetBrush(FSlateNoResource());
	SetPadding(FMargin(0.f));
}

TSharedRef<SWidget> UALKFrame::RebuildWidget()
{
	// The gradient, the lit edge and the focus tint are drawn with small generated textures.
	FALKPaint::PrepareGradients();
	const TSharedRef<SWidget> Border = Super::RebuildWidget();
	MyFrame = SNew(SALKFrame)[Border];
	MyFrame->SetStyle(FrameStyle);
	return MyFrame.ToSharedRef();
}

void UALKFrame::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	MyFrame.Reset();
}

void UALKFrame::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	if (MyFrame.IsValid())
	{
		MyFrame->SetStyle(FrameStyle);
	}
}

void UALKFrame::SetFrameStyle(const FALKFrameStyle& InStyle)
{
	FrameStyle = InStyle;
	if (MyFrame.IsValid())
	{
		MyFrame->SetStyle(FrameStyle);
	}
}

void UALKFrame::PlayIntro(float Duration)
{
	if (MyFrame.IsValid())
	{
		MyFrame->PlayIntro(Duration);
	}
}

#if WITH_EDITOR
const FText UALKFrame::GetPaletteCategory()
{
	return LOCTEXT("PaletteCategory", "Assetpro Lobby Kit");
}
#endif

#undef LOCTEXT_NAMESPACE
