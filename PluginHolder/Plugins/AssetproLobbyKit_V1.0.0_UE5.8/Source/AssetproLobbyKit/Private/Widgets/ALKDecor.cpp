// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Widgets/ALKDecor.h"
#include "Widgets/ALKPaint.h"
#include "ALKDeveloperSettings.h"
#include "ALKMenuConfig.h"
#include "Engine/Texture2D.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SLeafWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKDecor)

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

/** Paints rules, value bars, spinners, icons, avatars and switches (see FALKDecorStyle). */
class SALKDecor : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SALKDecor) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
	}

	void SetStyle(const FALKDecorStyle& InStyle, bool bInstant = true)
	{
		const bool bWasSpinner = Style.Kind == EALKDecorKind::Spinner;
		Style = InStyle;
		if (Style.Kind == EALKDecorKind::Spinner && !bWasSpinner)
		{
			RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SALKDecor::Spin));
		}
		if (Style.Kind == EALKDecorKind::Switch)
		{
			if (bInstant || FALKEffects::AreReduced() || SwitchPosition < 0.f)
			{
				SwitchPosition = Style.Value;
			}
			else if (!FMath::IsNearlyEqual(SwitchPosition, Style.Value) && !bSliding)
			{
				bSliding = true;
				RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SALKDecor::Slide));
			}
		}
		Invalidate(EInvalidateWidgetReason::Layout | EInvalidateWidgetReason::Paint);
	}

	virtual FVector2D ComputeDesiredSize(float) const override
	{
		if (Style.Kind == EALKDecorKind::Rule)
		{
			return FVector2D(32.0, Style.Thickness + 1.0);
		}
		return Style.DesiredSize;
	}

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& Out, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
	{
		const FVector2f Size = FVector2f(Geometry.GetLocalSize());
		const FLinearColor Tint = InWidgetStyle.GetColorAndOpacityTint();
		const FLinearColor Color = Style.Color * Tint;
		switch (Style.Kind)
		{
		case EALKDecorKind::Rule:
		{
			const float Y = Size.Y * 0.5f;
			FALKPaint::Lines(Out, LayerId, Geometry, { FVector2f(0.f, Y), FVector2f(Size.X, Y) }, Color, Style.Thickness);
			break;
		}
		case EALKDecorKind::ValueBar:
		{
			const float Value = FMath::Clamp(Style.Value, 0.f, 1.f);
			const float Knob = Style.bKnob ? FMath::Min(Size.Y * 0.5f, 9.f) : 0.f;
			const float Half = FMath::Min(Style.Thickness, Size.Y) * 0.5f;
			const float Left = Knob;
			const float Right = Size.X - Knob;
			const float Y = Size.Y * 0.5f;
			const float X = Left + (Right - Left) * Value;
			FALKPaint::RoundedBox(Out, LayerId, Geometry, FVector2f(Left - Half, Y - Half), FVector2f(Right + Half, Y + Half), Half, Style.SecondaryColor * Tint);
			if (Value > 0.f)
			{
				FALKPaint::RoundedBox(Out, LayerId + 1, Geometry, FVector2f(Left - Half, Y - Half), FVector2f(X + Half, Y + Half), Half, Color);
			}
			if (Style.bKnob)
			{
				FALKPaint::Circle(Out, LayerId + 2, Geometry, FVector2f(X, Y), Knob, Style.TertiaryColor * Tint);
			}
			return LayerId + 3;
		}
		case EALKDecorKind::Spinner:
		{
			// Eight short arcs, the brightest one moving round.
			const FVector2f Center = Size * 0.5f;
			const float Radius = FMath::Min(Size.X, Size.Y) * 0.5f - Style.Thickness;
			const float Turn = FALKEffects::AreReduced() ? 0.f : FMath::Frac(static_cast<float>(FALKEffects::Now()) * 0.9f) * 360.f;
			for (int32 Step = 0; Step < 8; ++Step)
			{
				const float Alpha = 0.15f + 0.85f * Step / 7.f;
				FALKPaint::Arc(Out, LayerId, Geometry, Center, Radius, Turn + Step * 45.f + 7.f, Turn + Step * 45.f + 37.f, Color * FLinearColor(1.f, 1.f, 1.f, Alpha), Style.Thickness);
			}
			break;
		}
		case EALKDecorKind::Icon:
			PaintIcon(Out, LayerId, Geometry, Size, Color);
			break;
		case EALKDecorKind::Avatar:
			PaintAvatar(Out, LayerId, Geometry, Size, Tint);
			return LayerId + 4;
		case EALKDecorKind::Switch:
		{
			const float Position = FMath::Clamp(SwitchPosition, 0.f, 1.f);
			const float Radius = Style.Radius < 0.f ? Size.Y * 0.5f : Style.Radius;
			const float Pad = FMath::Max(2.f, Size.Y * 0.14f);
			const float KnobSize = Size.Y - Pad * 2.f;
			const float X = FMath::Lerp(Pad, Size.X - Pad - KnobSize, Position);
			FALKPaint::RoundedBox(Out, LayerId, Geometry, FVector2f(0.f, 0.f), Size, Radius, FMath::Lerp(Style.SecondaryColor, Style.Color, Position) * Tint);
			FALKPaint::RoundedBox(Out, LayerId + 1, Geometry, FVector2f(X, Pad), FVector2f(X + KnobSize, Pad + KnobSize), FMath::Max(0.f, Radius - Pad),
				FMath::Lerp(Style.QuaternaryColor, Style.TertiaryColor, Position) * Tint);
			return LayerId + 2;
		}
		}
		return LayerId + 1;
	}

private:
	void PaintAvatar(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2f& Size, const FLinearColor& Tint) const
	{
		const float R = FMath::Min(Size.X, Size.Y) * 0.5f;
		const FVector2f Center = Size * 0.5f;
		if (Style.TertiaryColor.A > 0.f)
		{
			FALKPaint::Circle(Out, Layer, Geometry, Center, R + 3.5f, FLinearColor::Transparent, Style.TertiaryColor * Tint, 2.f);
		}
		if (Style.Picture.GetResourceObject())
		{
			FSlateBrush Brush = Style.Picture;
			Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
			Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
			Brush.OutlineSettings.Width = 0.f;
			FSlateDrawElement::MakeBox(Out, Layer + 1, Geometry.ToPaintGeometry(FVector2f(R * 2.f, R * 2.f), FSlateLayoutTransform(Center - FVector2f(R, R))), &Brush, ESlateDrawEffect::None, Tint);
		}
		else
		{
			// The placeholder: a disc with a head and shoulders.
			FALKPaint::Circle(Out, Layer + 1, Geometry, Center, R, Style.Color * Tint);
			const FLinearColor Glyph = Style.SecondaryColor * Tint;
			FALKPaint::Circle(Out, Layer + 2, Geometry, Center + FVector2f(0.f, -0.185f * R), 0.255f * R, Glyph);
			TArray<FVector2f> Shoulders;
			const FVector2f ShoulderCenter = Center + FVector2f(0.f, 0.73f * R);
			for (int32 Step = 0; Step <= 12; ++Step)
			{
				const float Angle = FMath::DegreesToRadians(180.f + Step * 15.f);
				Shoulders.Add(ShoulderCenter + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * 0.54f * R);
			}
			FALKPaint::FillConvex(Out, Layer + 2, Geometry, Shoulders, Glyph);
		}
		if (Style.QuaternaryColor.A > 0.f)
		{
			const FVector2f DotCenter = Center + FVector2f(0.72f * R, 0.72f * R);
			const float DotRadius = FMath::Max(3.5f, 0.26f * R);
			FALKPaint::Circle(Out, Layer + 3, Geometry, DotCenter, DotRadius + 2.f, Style.BackColor * Tint);
			FALKPaint::Circle(Out, Layer + 3, Geometry, DotCenter, DotRadius, Style.QuaternaryColor * Tint);
		}
	}

	void PaintIcon(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2f& Size, const FLinearColor& Color) const
	{
		const float S = FMath::Min(Size.X, Size.Y);
		const FVector2f O = (Size - FVector2f(S, S)) * 0.5f;
		// P: the icon's box from 0 to 1. C: the same box from -0.5 to 0.5, centred.
		auto P = [&](float X, float Y) { return O + FVector2f(X, Y) * S; };
		auto C = [&](float X, float Y) { return O + FVector2f(X + 0.5f, Y + 0.5f) * S; };
		const float W = FMath::Max(1.5f, S / 11.f);
		auto Line = [&](TArray<FVector2f> Points, bool bClosed = false) { FALKPaint::Lines(Out, Layer, Geometry, MoveTemp(Points), Color, W, bClosed); };
		auto Fill = [&](const TArray<FVector2f>& Points) { FALKPaint::FillConvex(Out, Layer, Geometry, Points, Color); };
		auto Box = [&](const FVector2f& Min, const FVector2f& Max, float Radius, bool bFilled)
		{
			FALKPaint::RoundedBox(Out, Layer, Geometry, Min, Max, Radius * S, bFilled ? Color : FLinearColor::Transparent, bFilled ? FLinearColor::Transparent : Color, bFilled ? 0.f : W);
		};
		auto Disc = [&](const FVector2f& Center, float Radius, bool bFilled)
		{
			FALKPaint::Circle(Out, Layer, Geometry, Center, Radius * S, bFilled ? Color : FLinearColor::Transparent, bFilled ? FLinearColor::Transparent : Color, bFilled ? 0.f : W);
		};
		auto Arc = [&](const FVector2f& Center, float Radius, float Start, float End) { FALKPaint::Arc(Out, Layer, Geometry, Center, Radius * S, Start, End, Color, W); };
		auto Ellipse = [&](const FVector2f& Center, float RadiusX, float RadiusY)
		{
			TArray<FVector2f> Points;
			for (int32 Step = 0; Step < 24; ++Step)
			{
				const float Angle = FMath::DegreesToRadians(Step * 15.f);
				Points.Add(Center + FVector2f(FMath::Cos(Angle) * RadiusX, FMath::Sin(Angle) * RadiusY) * S);
			}
			Line(MoveTemp(Points), true);
		};
		auto HalfDisc = [&](const FVector2f& Center, float Radius)
		{
			TArray<FVector2f> Points;
			for (int32 Step = 0; Step <= 12; ++Step)
			{
				const float Angle = FMath::DegreesToRadians(180.f + Step * 15.f);
				Points.Add(Center + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius * S);
			}
			Fill(Points);
		};

		switch (Style.Icon)
		{
		case EALKIcon::Checkpoint:
			Line({ P(0.5f, 0.04f), P(0.96f, 0.5f), P(0.5f, 0.96f), P(0.04f, 0.5f) }, true);
			Fill({ P(0.5f, 0.28f), P(0.72f, 0.5f), P(0.5f, 0.72f), P(0.28f, 0.5f) });
			break;
		case EALKIcon::Unlock:
			Box(P(0.16f, 0.46f), P(0.84f, 0.94f), 0.08f, false);
			Arc(P(0.5f, 0.3f), 0.2f, 180.f, 345.f);
			Line({ P(0.3f, 0.3f), P(0.3f, 0.46f) });
			break;
		case EALKIcon::Save:
			Box(P(0.08f, 0.08f), P(0.92f, 0.92f), 0.12f, false);
			FALKPaint::FillRect(Out, Layer, Geometry, P(0.28f, 0.1f), P(0.62f, 0.32f), Color);
			Box(P(0.26f, 0.56f), P(0.74f, 0.9f), 0.05f, false);
			break;
		case EALKIcon::Alert:
			Line({ P(0.5f, 0.08f), P(0.95f, 0.9f), P(0.05f, 0.9f) }, true);
			FALKPaint::FillRect(Out, Layer, Geometry, P(0.46f, 0.36f), P(0.54f, 0.64f), Color);
			FALKPaint::FillRect(Out, Layer, Geometry, P(0.46f, 0.72f), P(0.54f, 0.8f), Color);
			break;
		case EALKIcon::Info:
			Disc(P(0.5f, 0.5f), 0.42f, false);
			Disc(P(0.5f, 0.3f), 0.055f, true);
			FALKPaint::FillRect(Out, Layer, Geometry, P(0.455f, 0.43f), P(0.545f, 0.74f), Color);
			break;
		case EALKIcon::Signal:
			for (int32 Bar = 0; Bar < 4; ++Bar)
			{
				const float X = 0.08f + Bar * 0.23f;
				FALKPaint::FillRect(Out, Layer, Geometry, P(X, 0.9f - 0.2f * (Bar + 1)), P(X + 0.15f, 0.9f), Color);
			}
			break;
		case EALKIcon::Check:
			Line({ C(-0.3f, 0.02f), C(-0.08f, 0.24f), C(0.32f, -0.22f) });
			break;
		case EALKIcon::Cross:
			Line({ C(-0.25f, -0.25f), C(0.25f, 0.25f) });
			Line({ C(-0.25f, 0.25f), C(0.25f, -0.25f) });
			break;
		case EALKIcon::Plus:
			Line({ C(-0.3f, 0.f), C(0.3f, 0.f) });
			Line({ C(0.f, -0.3f), C(0.f, 0.3f) });
			break;
		case EALKIcon::ChevronRight:
			Line({ C(-0.12f, -0.28f), C(0.16f, 0.f), C(-0.12f, 0.28f) });
			break;
		case EALKIcon::ChevronLeft:
			Line({ C(0.12f, -0.28f), C(-0.16f, 0.f), C(0.12f, 0.28f) });
			break;
		case EALKIcon::ChevronDown:
			Line({ C(-0.28f, -0.12f), C(0.f, 0.16f), C(0.28f, -0.12f) });
			break;
		case EALKIcon::ChevronUp:
			Line({ C(-0.28f, 0.12f), C(0.f, -0.16f), C(0.28f, 0.12f) });
			break;
		case EALKIcon::Crown:
			Fill({ C(-0.46f, -0.2f), C(-0.2f, 0.04f), C(-0.43f, 0.04f) });
			Fill({ C(-0.2f, 0.04f), C(0.f, -0.34f), C(0.2f, 0.04f) });
			Fill({ C(0.2f, 0.04f), C(0.46f, -0.2f), C(0.43f, 0.04f) });
			Fill({ C(-0.43f, 0.03f), C(0.43f, 0.03f), C(0.4f, 0.3f), C(-0.4f, 0.3f) });
			break;
		case EALKIcon::Star:
		case EALKIcon::StarOutline:
		{
			TArray<FVector2f> Outer, Inner, All;
			for (int32 Index = 0; Index < 5; ++Index)
			{
				const float A0 = FMath::DegreesToRadians(-90.f + Index * 72.f);
				const float A1 = A0 + FMath::DegreesToRadians(36.f);
				Outer.Add(C(FMath::Cos(A0) * 0.46f, FMath::Sin(A0) * 0.46f));
				Inner.Add(C(FMath::Cos(A1) * 0.2f, FMath::Sin(A1) * 0.2f));
				All.Add(Outer.Last());
				All.Add(Inner.Last());
			}
			if (Style.Icon == EALKIcon::StarOutline)
			{
				FALKPaint::Lines(Out, Layer, Geometry, All, Color, FMath::Max(1.2f, W * 0.7f), true);
			}
			else
			{
				Fill(Inner);
				for (int32 Index = 0; Index < 5; ++Index)
				{
					Fill({ Outer[Index], Inner[Index], Inner[(Index + 4) % 5] });
				}
			}
			break;
		}
		case EALKIcon::Lock:
			Box(C(-0.3f, -0.04f), C(0.3f, 0.4f), 0.08f, true);
			Arc(C(0.f, -0.06f), 0.2f, 180.f, 360.f);
			break;
		case EALKIcon::Microphone:
			Box(C(-0.14f, -0.44f), C(0.14f, 0.1f), 0.14f, true);
			Arc(C(0.f, -0.02f), 0.28f, 0.f, 180.f);
			Line({ C(0.f, 0.26f), C(0.f, 0.44f) });
			break;
		case EALKIcon::Speaker:
		case EALKIcon::SpeakerMuted:
			FALKPaint::FillRect(Out, Layer, Geometry, C(-0.42f, -0.12f), C(-0.23f, 0.12f), Color);
			Fill({ C(-0.24f, -0.12f), C(-0.02f, -0.34f), C(-0.02f, 0.34f), C(-0.24f, 0.12f) });
			if (Style.Icon == EALKIcon::Speaker)
			{
				Arc(C(0.02f, 0.f), 0.2f, -50.f, 50.f);
				Arc(C(0.02f, 0.f), 0.38f, -48.f, 48.f);
			}
			else
			{
				Line({ C(0.14f, -0.16f), C(0.44f, 0.16f) });
				Line({ C(0.14f, 0.16f), C(0.44f, -0.16f) });
			}
			break;
		case EALKIcon::User:
			Disc(C(0.f, -0.19f), 0.17f, true);
			HalfDisc(C(0.f, 0.42f), 0.36f);
			break;
		case EALKIcon::Users:
			Disc(C(-0.2f, -0.16f), 0.14f, true);
			HalfDisc(C(-0.2f, 0.36f), 0.28f);
			Disc(C(0.2f, -0.1f), 0.12f, true);
			HalfDisc(C(0.21f, 0.36f), 0.24f);
			break;
		case EALKIcon::Search:
			Disc(C(-0.1f, -0.1f), 0.24f, false);
			Line({ C(0.08f, 0.08f), C(0.36f, 0.36f) });
			break;
		case EALKIcon::Refresh:
			Arc(C(0.f, 0.f), 0.32f, 30.f, 320.f);
			Fill({ C(0.12f, -0.44f), C(0.44f, -0.3f), C(0.18f, -0.08f) });
			break;
		case EALKIcon::PingBars:
			for (int32 Bar = 0; Bar < 4; ++Bar)
			{
				const float Height = 0.2f + 0.2f * Bar;
				const float X = -0.36f + Bar * 0.2f;
				FALKPaint::FillRect(Out, Layer, Geometry, C(X, 0.36f - Height), C(X + 0.13f, 0.36f), Bar < Style.Level ? Color : Color * FLinearColor(1.f, 1.f, 1.f, 0.22f));
			}
			break;
		case EALKIcon::Platform:
			Box(C(-0.36f, -0.36f), C(0.36f, 0.36f), 0.16f, false);
			Disc(C(0.f, 0.f), 0.1f, true);
			break;
		case EALKIcon::Eye:
			Ellipse(C(0.f, 0.f), 0.42f, 0.24f);
			Disc(C(0.f, 0.f), 0.12f, true);
			break;
		case EALKIcon::Flag:
			Line({ C(-0.26f, -0.4f), C(-0.26f, 0.42f) });
			Fill({ C(-0.26f, -0.38f), C(0.36f, -0.2f), C(-0.26f, -0.02f) });
			break;
		case EALKIcon::Bolt:
			Fill({ C(0.1f, -0.46f), C(-0.3f, 0.06f), C(-0.02f, 0.06f) });
			Fill({ C(0.1f, -0.46f), C(-0.02f, 0.06f), C(0.02f, -0.1f) });
			Fill({ C(-0.02f, 0.06f), C(-0.12f, 0.46f), C(0.3f, -0.1f) });
			Fill({ C(-0.02f, 0.06f), C(0.3f, -0.1f), C(0.02f, -0.1f) });
			break;
		case EALKIcon::Server:
			Box(C(-0.38f, -0.36f), C(0.38f, -0.04f), 0.07f, false);
			Box(C(-0.38f, 0.06f), C(0.38f, 0.38f), 0.07f, false);
			Disc(C(-0.22f, -0.2f), 0.05f, true);
			Disc(C(-0.22f, 0.22f), 0.05f, true);
			break;
		case EALKIcon::List:
			for (const float Y : { -0.26f, 0.f, 0.26f })
			{
				Line({ C(-0.38f, Y), C(-0.28f, Y) });
				Line({ C(-0.12f, Y), C(0.38f, Y) });
			}
			break;
		case EALKIcon::Hash:
			Line({ C(-0.12f, -0.38f), C(-0.2f, 0.38f) });
			Line({ C(0.2f, -0.38f), C(0.12f, 0.38f) });
			Line({ C(-0.38f, -0.14f), C(0.38f, -0.14f) });
			Line({ C(-0.38f, 0.14f), C(0.38f, 0.14f) });
			break;
		case EALKIcon::Play:
			Fill({ C(-0.22f, -0.32f), C(0.34f, 0.f), C(-0.22f, 0.32f) });
			break;
		case EALKIcon::Pause:
			FALKPaint::FillRect(Out, Layer, Geometry, C(-0.24f, -0.3f), C(-0.07f, 0.3f), Color);
			FALKPaint::FillRect(Out, Layer, Geometry, C(0.07f, -0.3f), C(0.24f, 0.3f), Color);
			break;
		case EALKIcon::Globe:
			Disc(C(0.f, 0.f), 0.4f, false);
			Ellipse(C(0.f, 0.f), 0.18f, 0.4f);
			Line({ C(-0.4f, 0.f), C(0.4f, 0.f) });
			break;
		case EALKIcon::Clock:
			Disc(C(0.f, 0.f), 0.4f, false);
			Line({ C(0.f, -0.22f), C(0.f, 0.f), C(0.16f, 0.1f) });
			break;
		case EALKIcon::Copy:
			Box(C(-0.32f, -0.38f), C(0.12f, 0.12f), 0.07f, false);
			Box(C(-0.1f, -0.14f), C(0.34f, 0.38f), 0.07f, false);
			break;
		case EALKIcon::Target:
			Disc(C(0.f, 0.f), 0.26f, false);
			Line({ C(-0.46f, 0.f), C(-0.12f, 0.f) });
			Line({ C(0.12f, 0.f), C(0.46f, 0.f) });
			Line({ C(0.f, -0.46f), C(0.f, -0.12f) });
			Line({ C(0.f, 0.12f), C(0.f, 0.46f) });
			break;
		case EALKIcon::Dot:
			Disc(C(0.f, 0.f), 0.3f, true);
			break;
		case EALKIcon::More:
			for (const float X : { -0.3f, 0.f, 0.3f })
			{
				Disc(C(X, 0.f), 0.08f, true);
			}
			break;
		default:
			break;
		}
	}

	EActiveTimerReturnType Spin(double InCurrentTime, float InDeltaTime)
	{
		Invalidate(EInvalidateWidgetReason::Paint);
		return Style.Kind == EALKDecorKind::Spinner ? EActiveTimerReturnType::Continue : EActiveTimerReturnType::Stop;
	}

	EActiveTimerReturnType Slide(double InCurrentTime, float InDeltaTime)
	{
		SwitchPosition = FMath::FInterpConstantTo(SwitchPosition, Style.Value, InDeltaTime, 8.f);
		Invalidate(EInvalidateWidgetReason::Paint);
		bSliding = !FMath::IsNearlyEqual(SwitchPosition, Style.Value);
		return bSliding ? EActiveTimerReturnType::Continue : EActiveTimerReturnType::Stop;
	}

	FALKDecorStyle Style;
	float SwitchPosition = -1.f;
	bool bSliding = false;
};

// ---------------------------------------------------------------- Rule

TSharedRef<SWidget> UALKRule::RebuildWidget()
{
	MyDecor = SNew(SALKDecor);
	SynchronizeProperties();
	return MyDecor.ToSharedRef();
}

void UALKRule::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	MyDecor.Reset();
}

void UALKRule::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	if (MyDecor.IsValid())
	{
		FALKDecorStyle Style;
		Style.Kind = EALKDecorKind::Rule;
		Style.Color = Color;
		Style.Thickness = Thickness;
		MyDecor->SetStyle(Style);
	}
}

void UALKRule::SetRuleColor(const FLinearColor& InColor)
{
	Color = InColor;
	SynchronizeProperties();
}

#if WITH_EDITOR
const FText UALKRule::GetPaletteCategory()
{
	return LOCTEXT("PaletteCategory", "Assetpro Lobby Kit");
}
#endif

// ---------------------------------------------------------------- Value bar

TSharedRef<SWidget> UALKValueBar::RebuildWidget()
{
	MyDecor = SNew(SALKDecor);
	Push();
	return MyDecor.ToSharedRef();
}

void UALKValueBar::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	MyDecor.Reset();
}

void UALKValueBar::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	Push();
}

void UALKValueBar::SetValue(float InValue)
{
	const float Clamped = FMath::Clamp(InValue, 0.f, 1.f);
	if (!FMath::IsNearlyEqual(Clamped, Value))
	{
		Value = Clamped;
		Push();
	}
}

void UALKValueBar::SetColors(const FLinearColor& InFill, const FLinearColor& InTrack)
{
	if (!FillColor.Equals(InFill) || !TrackColor.Equals(InTrack))
	{
		FillColor = InFill;
		TrackColor = InTrack;
		Push();
	}
}

void UALKValueBar::SetKnobColor(const FLinearColor& InKnob)
{
	if (!KnobColor.Equals(InKnob))
	{
		KnobColor = InKnob;
		Push();
	}
}

void UALKValueBar::Push()
{
	if (MyDecor.IsValid())
	{
		FALKDecorStyle Style;
		Style.Kind = EALKDecorKind::ValueBar;
		Style.Color = FillColor;
		Style.SecondaryColor = TrackColor;
		Style.TertiaryColor = KnobColor;
		Style.Value = Value;
		Style.bKnob = bShowKnob;
		Style.Thickness = TrackHeight;
		Style.DesiredSize = BarSize;
		MyDecor->SetStyle(Style);
	}
}

#if WITH_EDITOR
const FText UALKValueBar::GetPaletteCategory()
{
	return LOCTEXT("PaletteCategory", "Assetpro Lobby Kit");
}
#endif

// ---------------------------------------------------------------- Spinner

TSharedRef<SWidget> UALKSpinner::RebuildWidget()
{
	MyDecor = SNew(SALKDecor);
	SynchronizeProperties();
	return MyDecor.ToSharedRef();
}

void UALKSpinner::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	MyDecor.Reset();
}

void UALKSpinner::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	if (MyDecor.IsValid())
	{
		FALKDecorStyle Style;
		Style.Kind = EALKDecorKind::Spinner;
		Style.Color = Color;
		Style.Thickness = FMath::Max(2.f, Diameter / 9.f);
		Style.DesiredSize = FVector2D(Diameter, Diameter);
		MyDecor->SetStyle(Style);
	}
}

void UALKSpinner::SetSpinnerColor(const FLinearColor& InColor)
{
	Color = InColor;
	SynchronizeProperties();
}

#if WITH_EDITOR
const FText UALKSpinner::GetPaletteCategory()
{
	return LOCTEXT("PaletteCategory", "Assetpro Lobby Kit");
}
#endif

// ---------------------------------------------------------------- Icon

TSharedRef<SWidget> UALKIconWidget::RebuildWidget()
{
	MyDecor = SNew(SALKDecor);
	SynchronizeProperties();
	return MyDecor.ToSharedRef();
}

void UALKIconWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	MyDecor.Reset();
}

void UALKIconWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	if (MyDecor.IsValid())
	{
		FALKDecorStyle Style;
		Style.Kind = EALKDecorKind::Icon;
		Style.Icon = Icon;
		Style.Color = Color;
		Style.Level = Level;
		Style.DesiredSize = FVector2D(IconSize, IconSize);
		MyDecor->SetStyle(Style);
	}
}

void UALKIconWidget::SetIcon(EALKIcon InIcon, const FLinearColor& InColor)
{
	Icon = InIcon;
	Color = InColor;
	SynchronizeProperties();
}

void UALKIconWidget::SetLevel(int32 InLevel)
{
	Level = FMath::Clamp(InLevel, 0, 4);
	SynchronizeProperties();
}

#if WITH_EDITOR
const FText UALKIconWidget::GetPaletteCategory()
{
	return LOCTEXT("PaletteCategory", "Assetpro Lobby Kit");
}
#endif

// ---------------------------------------------------------------- Avatar

FLinearColor UALKAvatar::GetPlaceholderColor(const FString& InPlayerName)
{
	static const TCHAR* Colors[] = { TEXT("5B8DEF"), TEXT("E5734A"), TEXT("9B6DE3"), TEXT("3FB8AF"), TEXT("D9A441"), TEXT("D65A8E"), TEXT("6FB04F"), TEXT("8891A6") };
	uint32 Sum = 0;
	for (const TCHAR Character : InPlayerName)
	{
		Sum += static_cast<uint32>(Character);
	}
	return FLinearColor(FColor::FromHex(Colors[Sum % UE_ARRAY_COUNT(Colors)]));
}

TSharedRef<SWidget> UALKAvatar::RebuildWidget()
{
	MyDecor = SNew(SALKDecor);
	SynchronizeProperties();
	return MyDecor.ToSharedRef();
}

void UALKAvatar::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	MyDecor.Reset();
}

void UALKAvatar::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	// No picture of the player's own: one of the menu config's default avatars, the same one every time for the name.
	DefaultPicture = nullptr;
	if (!Picture && !bAnonymous && bUseDefaultAvatars)
	{
		if (const UALKMenuConfig* Config = UALKDeveloperSettings::GetMenuConfig())
		{
			DefaultPicture = Config->GetDefaultAvatar(PlayerName);
		}
	}
	UTexture2D* Shown = Picture ? Picture.Get() : DefaultPicture.Get();

	if (MyDecor.IsValid())
	{
		const FLinearColor Base = bAnonymous ? FLinearColor(0.25f, 0.27f, 0.31f, 1.f) : GetPlaceholderColor(PlayerName);
		FALKDecorStyle Style;
		Style.Kind = EALKDecorKind::Avatar;
		Style.Color = FMath::Lerp(Base, BackColor, 0.3f).CopyWithNewOpacity(1.f);
		Style.SecondaryColor = FMath::Lerp(Base, FLinearColor::White, 0.7f).CopyWithNewOpacity(1.f);
		Style.TertiaryColor = RingColor;
		Style.QuaternaryColor = StatusColor;
		Style.BackColor = BackColor.CopyWithNewOpacity(1.f);
		if (Shown && !bAnonymous)
		{
			Style.Picture.SetResourceObject(Shown);
			Style.Picture.ImageSize = FVector2D(Diameter, Diameter);
		}
		Style.DesiredSize = FVector2D(Diameter, Diameter);
		MyDecor->SetStyle(Style);
	}
}

void UALKAvatar::SetPlayer(const FString& InPlayerName, UTexture2D* InPicture)
{
	PlayerName = InPlayerName;
	Picture = InPicture;
	SynchronizeProperties();
}

void UALKAvatar::SetRingColor(const FLinearColor& InColor)
{
	RingColor = InColor;
	SynchronizeProperties();
}

void UALKAvatar::SetStatusColor(const FLinearColor& InColor)
{
	StatusColor = InColor;
	SynchronizeProperties();
}

void UALKAvatar::SetBackColor(const FLinearColor& InColor)
{
	BackColor = InColor;
	SynchronizeProperties();
}

void UALKAvatar::SetAnonymous(bool bInAnonymous)
{
	bAnonymous = bInAnonymous;
	SynchronizeProperties();
}

#if WITH_EDITOR
const FText UALKAvatar::GetPaletteCategory()
{
	return LOCTEXT("PaletteCategory", "Assetpro Lobby Kit");
}
#endif

// ---------------------------------------------------------------- Switch

TSharedRef<SWidget> UALKSwitch::RebuildWidget()
{
	MyDecor = SNew(SALKDecor);
	Push(true);
	return MyDecor.ToSharedRef();
}

void UALKSwitch::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	MyDecor.Reset();
}

void UALKSwitch::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	Push(true);
}

void UALKSwitch::SetOn(bool bInOn, bool bInstant)
{
	if (bOn != bInOn)
	{
		bOn = bInOn;
		Push(bInstant);
	}
}

void UALKSwitch::SetColors(const FLinearColor& InOn, const FLinearColor& InOff, const FLinearColor& InKnobOn, const FLinearColor& InKnobOff)
{
	OnColor = InOn;
	OffColor = InOff;
	KnobOnColor = InKnobOn;
	KnobOffColor = InKnobOff;
	Push(true);
}

void UALKSwitch::Push(bool bInstant)
{
	if (MyDecor.IsValid())
	{
		FALKDecorStyle Style;
		Style.Kind = EALKDecorKind::Switch;
		Style.Color = OnColor;
		Style.SecondaryColor = OffColor;
		Style.TertiaryColor = KnobOnColor;
		Style.QuaternaryColor = KnobOffColor;
		Style.Value = bOn ? 1.f : 0.f;
		Style.Radius = CornerRadius;
		Style.DesiredSize = SwitchSize;
		MyDecor->SetStyle(Style, bInstant);
	}
}

#if WITH_EDITOR
const FText UALKSwitch::GetPaletteCategory()
{
	return LOCTEXT("PaletteCategory", "Assetpro Lobby Kit");
}
#endif

#undef LOCTEXT_NAMESPACE
