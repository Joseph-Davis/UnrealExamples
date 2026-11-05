// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Widgets/ALKGamepadDiagram.h"
#include "Widgets/ALKPaint.h"
#include "ALKDeveloperSettings.h"
#include "ALKTheme.h"
#include "Widgets/ALKInputLabels.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKGamepadDiagram)

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

namespace ALKDiagramPrivate
{
	/**
	 * The drawing is made in design units: the controller is about 400 wide, the whole diagram 1000 x 400, centred on
	 * the widget and scaled to fit it. Text keeps its own size so it stays readable at any scale.
	 */
	static constexpr float DesignWidth = 1000.f;
	static constexpr float DesignHeight = 400.f;

	/** Where an input sits on the controller (Anchor) and where its callout line ends (Label). */
	struct FPlacement
	{
		EALKGamepadInput Input;
		FVector2f Anchor;
		FVector2f Label;
		/** -1: the label is right-aligned to the left of Label; +1: it starts right of Label. */
		int32 Side;
	};

	static const FPlacement Placements[] = {
		{ EALKGamepadInput::LeftTrigger, FVector2f(-150.f, -117.f), FVector2f(-235.f, -150.f), -1 },
		{ EALKGamepadInput::LeftBumper, FVector2f(-166.f, -97.f), FVector2f(-235.f, -110.f), -1 },
		{ EALKGamepadInput::LeftStick, FVector2f(-136.f, -35.f), FVector2f(-235.f, -55.f), -1 },
		{ EALKGamepadInput::LeftStickPress, FVector2f(-128.f, -19.f), FVector2f(-235.f, -15.f), -1 },
		{ EALKGamepadInput::DPad, FVector2f(-77.f, 25.f), FVector2f(-235.f, 40.f), -1 },
		{ EALKGamepadInput::View, FVector2f(-30.f, -48.f), FVector2f(-60.f, -178.f), -1 },
		{ EALKGamepadInput::Menu, FVector2f(30.f, -48.f), FVector2f(60.f, -178.f), 1 },
		{ EALKGamepadInput::RightTrigger, FVector2f(150.f, -117.f), FVector2f(235.f, -150.f), 1 },
		{ EALKGamepadInput::RightBumper, FVector2f(166.f, -97.f), FVector2f(235.f, -110.f), 1 },
		{ EALKGamepadInput::FaceTop, FVector2f(122.f, -62.f), FVector2f(235.f, -72.f), 1 },
		{ EALKGamepadInput::FaceRight, FVector2f(147.f, -37.f), FVector2f(235.f, -35.f), 1 },
		{ EALKGamepadInput::FaceLeft, FVector2f(96.f, -32.f), FVector2f(235.f, 2.f), 1 },
		{ EALKGamepadInput::FaceBottom, FVector2f(122.f, -12.f), FVector2f(235.f, 39.f), 1 },
		{ EALKGamepadInput::RightStick, FVector2f(81.f, 25.f), FVector2f(235.f, 76.f), 1 },
		{ EALKGamepadInput::RightStickPress, FVector2f(73.f, 43.f), FVector2f(235.f, 113.f), 1 },
	};

	/** Body outline as cubic segments (start, control 1, control 2, end), clockwise from the top left. */
	static const FVector2f Outline[][4] = {
		{ FVector2f(-120.f, -95.f), FVector2f(-40.f, -106.f), FVector2f(40.f, -106.f), FVector2f(120.f, -95.f) },
		{ FVector2f(120.f, -95.f), FVector2f(150.f, -100.f), FVector2f(176.f, -90.f), FVector2f(186.f, -70.f) },
		{ FVector2f(186.f, -70.f), FVector2f(202.f, -42.f), FVector2f(207.f, 20.f), FVector2f(200.f, 62.f) },
		{ FVector2f(200.f, 62.f), FVector2f(196.f, 108.f), FVector2f(178.f, 136.f), FVector2f(150.f, 131.f) },
		{ FVector2f(150.f, 131.f), FVector2f(124.f, 126.f), FVector2f(110.f, 86.f), FVector2f(95.f, 70.f) },
		{ FVector2f(95.f, 70.f), FVector2f(40.f, 54.f), FVector2f(-40.f, 54.f), FVector2f(-95.f, 70.f) },
		{ FVector2f(-95.f, 70.f), FVector2f(-110.f, 86.f), FVector2f(-124.f, 126.f), FVector2f(-150.f, 131.f) },
		{ FVector2f(-150.f, 131.f), FVector2f(-178.f, 136.f), FVector2f(-196.f, 108.f), FVector2f(-200.f, 62.f) },
		{ FVector2f(-200.f, 62.f), FVector2f(-207.f, 20.f), FVector2f(-202.f, -42.f), FVector2f(-186.f, -70.f) },
		{ FVector2f(-186.f, -70.f), FVector2f(-176.f, -90.f), FVector2f(-150.f, -100.f), FVector2f(-120.f, -95.f) },
		// Bumpers
		{ FVector2f(-181.f, -85.f), FVector2f(-172.f, -104.f), FVector2f(-140.f, -111.f), FVector2f(-116.f, -106.f) },
		{ FVector2f(181.f, -85.f), FVector2f(172.f, -104.f), FVector2f(140.f, -111.f), FVector2f(116.f, -106.f) },
		// Triggers
		{ FVector2f(-166.f, -101.f), FVector2f(-172.f, -122.f), FVector2f(-136.f, -134.f), FVector2f(-126.f, -110.f) },
		{ FVector2f(166.f, -101.f), FVector2f(172.f, -122.f), FVector2f(136.f, -134.f), FVector2f(126.f, -110.f) },
	};

	struct FCircle
	{
		FVector2f Center;
		float Radius;
		EALKGamepadInput Input;
		/** Print the button's letter inside (face buttons). */
		bool bLetter;
		/** Light up when the input has an action (false for the guide button). */
		bool bShowsBinding = true;
	};

	static const FCircle Circles[] = {
		{ FVector2f(-110.f, -35.f), 26.f, EALKGamepadInput::LeftStick, false },
		{ FVector2f(-110.f, -35.f), 16.f, EALKGamepadInput::LeftStick, false },
		{ FVector2f(55.f, 25.f), 26.f, EALKGamepadInput::RightStick, false },
		{ FVector2f(55.f, 25.f), 16.f, EALKGamepadInput::RightStick, false },
		{ FVector2f(110.f, -62.f), 12.f, EALKGamepadInput::FaceTop, true },
		{ FVector2f(135.f, -37.f), 12.f, EALKGamepadInput::FaceRight, true },
		{ FVector2f(85.f, -37.f), 12.f, EALKGamepadInput::FaceLeft, true },
		{ FVector2f(110.f, -12.f), 12.f, EALKGamepadInput::FaceBottom, true },
		{ FVector2f(-30.f, -40.f), 7.f, EALKGamepadInput::View, false },
		{ FVector2f(30.f, -40.f), 7.f, EALKGamepadInput::Menu, false },
		{ FVector2f(0.f, -66.f), 14.f, EALKGamepadInput::Menu, false, false },
	};

	static TArray<FVector2f> MakeCirclePoints(const FVector2f& Center, float Radius, int32 Segments = 40)
	{
		TArray<FVector2f> Points;
		Points.Reserve(Segments + 1);
		for (int32 Index = 0; Index <= Segments; ++Index)
		{
			const float Angle = 2.f * PI * Index / Segments;
			Points.Add(Center + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
		}
		return Points;
	}

	static FVector2f MeasureText(const FText& Text, const FSlateFontInfo& Font)
	{
		if (!FSlateApplication::IsInitialized() || !FSlateApplication::Get().GetRenderer())
		{
			return FVector2f(Text.ToString().Len() * Font.Size * 0.55f, Font.Size * 1.3f);
		}
		const TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		return FVector2f(FontMeasure->Measure(Text, Font));
	}
}

void UALKGamepadDiagram::SetCallouts(const TArray<FALKGamepadCallout>& InCallouts)
{
	Callouts = InCallouts;
	InvalidateLayoutAndVolatility();
}

FText UALKGamepadDiagram::GetInputLabel(EALKGamepadInput Input) const
{
	const ULocalPlayer* Player = GetOwningLocalPlayer();
	switch (Input)
	{
	case EALKGamepadInput::LeftTrigger: return FALKInputLabels::GetKeyLabel(EKeys::Gamepad_LeftTrigger, Player);
	case EALKGamepadInput::LeftBumper: return FALKInputLabels::GetKeyLabel(EKeys::Gamepad_LeftShoulder, Player);
	case EALKGamepadInput::LeftStick: return LOCTEXT("GlyphLeftStick", "L");
	case EALKGamepadInput::LeftStickPress: return FALKInputLabels::GetKeyLabel(EKeys::Gamepad_LeftThumbstick, Player);
	case EALKGamepadInput::View: return FALKInputLabels::GetKeyLabel(EKeys::Gamepad_Special_Left, Player);
	case EALKGamepadInput::DPad: return LOCTEXT("GlyphDPad", "D-Pad");
	case EALKGamepadInput::RightTrigger: return FALKInputLabels::GetKeyLabel(EKeys::Gamepad_RightTrigger, Player);
	case EALKGamepadInput::RightBumper: return FALKInputLabels::GetKeyLabel(EKeys::Gamepad_RightShoulder, Player);
	case EALKGamepadInput::Menu: return FALKInputLabels::GetKeyLabel(EKeys::Gamepad_Special_Right, Player);
	case EALKGamepadInput::FaceTop: return FALKInputLabels::GetKeyLabel(EKeys::Gamepad_FaceButton_Top, Player);
	case EALKGamepadInput::FaceRight: return FALKInputLabels::GetKeyLabel(EKeys::Gamepad_FaceButton_Right, Player);
	case EALKGamepadInput::FaceLeft: return FALKInputLabels::GetKeyLabel(EKeys::Gamepad_FaceButton_Left, Player);
	case EALKGamepadInput::FaceBottom: return FALKInputLabels::GetKeyLabel(EKeys::Gamepad_FaceButton_Bottom, Player);
	case EALKGamepadInput::RightStick: return LOCTEXT("GlyphRightStick", "R");
	case EALKGamepadInput::RightStickPress: return FALKInputLabels::GetKeyLabel(EKeys::Gamepad_RightThumbstick, Player);
	default: return FText::GetEmpty();
	}
}

int32 UALKGamepadDiagram::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	using namespace ALKDiagramPrivate;

	const int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	const UALKTheme* Theme = UALKDeveloperSettings::GetTheme();
	if (!Theme)
	{
		return BaseLayer;
	}

	const FVector2f Size = FVector2f(AllottedGeometry.GetLocalSize());
	const float Scale = FMath::Min(Size.X / DesignWidth, Size.Y / DesignHeight);
	if (Scale <= KINDA_SMALL_NUMBER)
	{
		return BaseLayer;
	}
	// Slightly below centre: the top labels need the room.
	const FVector2f Origin(Size.X * 0.5f, Size.Y * 0.5f + 14.f * Scale);
	auto ToLocal = [&Origin, Scale](const FVector2f& Design) { return Origin + Design * Scale; };

	const FPaintGeometry PaintGeometry = AllottedGeometry.ToPaintGeometry();
	const ESlateDrawEffect Effects = ESlateDrawEffect::None;
	const float Opacity = InWidgetStyle.GetColorAndOpacityTint().A;
	auto Faded = [Opacity](const FLinearColor& Color, float Alpha = 1.f) { return Color.CopyWithNewOpacity(Color.A * Alpha * Opacity); };

	const int32 LineLayer = BaseLayer + 1;
	const int32 GlyphLayer = BaseLayer + 2;
	const int32 TextLayer = BaseLayer + 3;

	auto FindAction = [this](EALKGamepadInput Input) -> const FText*
	{
		const FALKGamepadCallout* Callout = Callouts.FindByPredicate([Input](const FALKGamepadCallout& Entry) { return Entry.Input == Input; });
		return Callout && !Callout->Action.IsEmpty() ? &Callout->Action : nullptr;
	};

	// ---------------------------------------------------------------- the controller
	const FLinearColor BodyColor = Faded(Theme->TextSecondary, 0.6f);
	const float BodyThickness = FMath::Max(1.5f, 2.f * Scale);
	for (const FVector2f (&Segment)[4] : Outline)
	{
		FSlateDrawElement::MakeCubicBezierSpline(OutDrawElements, LineLayer, PaintGeometry,
			ToLocal(Segment[0]), ToLocal(Segment[1]), ToLocal(Segment[2]), ToLocal(Segment[3]), BodyThickness, Effects, BodyColor);
	}

	FSlateFontInfo LetterFont = Theme->CaptionFont;
	LetterFont.TypefaceFontName = FName(TEXT("Bold"));
	LetterFont.Size = FMath::Max(7.f, FMath::RoundToFloat(9.f * Scale));
	LetterFont.LetterSpacing = 0;
	for (const FCircle& Circle : Circles)
	{
		const bool bBound = Circle.bShowsBinding && FindAction(Circle.Input) != nullptr;
		const FLinearColor Color = bBound ? Faded(Theme->TextPrimary, 0.85f) : BodyColor;
		FSlateDrawElement::MakeLines(OutDrawElements, LineLayer, PaintGeometry, MakeCirclePoints(ToLocal(Circle.Center), Circle.Radius * Scale), Effects, Color, true, BodyThickness * 0.8f);

		if (Circle.bLetter)
		{
			const FText Letter = GetInputLabel(Circle.Input);
			if (Letter.ToString().Len() == 1)
			{
				const FVector2f LetterSize = MeasureText(Letter, LetterFont);
				const FVector2f Position = ToLocal(Circle.Center) - LetterSize * 0.5f;
				FSlateDrawElement::MakeText(OutDrawElements, TextLayer, AllottedGeometry.ToPaintGeometry(LetterSize, FSlateLayoutTransform(Position)), Letter, LetterFont, Effects, Color);
			}
		}
	}

	// D-pad cross
	{
		const FVector2f Center(-55.f, 25.f);
		const float Arm = 22.f;
		const float Half = 8.f;
		const FVector2f Cross[] = {
			{ -Half, -Arm }, { Half, -Arm }, { Half, -Half }, { Arm, -Half }, { Arm, Half }, { Half, Half }, { Half, Arm },
			{ -Half, Arm }, { -Half, Half }, { -Arm, Half }, { -Arm, -Half }, { -Half, -Half }, { -Half, -Arm }
		};
		TArray<FVector2f> Points;
		for (const FVector2f& Point : Cross)
		{
			Points.Add(ToLocal(Center + Point));
		}
		const FLinearColor Color = FindAction(EALKGamepadInput::DPad) ? Faded(Theme->TextPrimary, 0.85f) : BodyColor;
		FSlateDrawElement::MakeLines(OutDrawElements, LineLayer, PaintGeometry, Points, Effects, Color, true, BodyThickness * 0.8f);
	}

	// ---------------------------------------------------------------- callouts
	FSlateFontInfo LabelFont = Theme->BodyFont;
	LabelFont.Size = FMath::Max(10.f, LabelFont.Size - 2.f);
	FSlateFontInfo GlyphFont = Theme->CaptionFont;
	GlyphFont.TypefaceFontName = FName(TEXT("Bold"));
	GlyphFont.Size = FMath::Max(8.f, GlyphFont.Size - 1.f);
	GlyphFont.LetterSpacing = 0;
	const FText Unassigned = LOCTEXT("Unassigned", "Unassigned");
	const float GlyphHeight = 22.f;
	const float Gap = 8.f;

	for (const FPlacement& Placement : Placements)
	{
		const FText* Action = FindAction(Placement.Input);
		const bool bBound = Action != nullptr;
		const FText& Text = bBound ? *Action : Unassigned;
		const FText Glyph = GetInputLabel(Placement.Input);

		const FVector2f Anchor = ToLocal(Placement.Anchor);
		const FVector2f LineEnd = ToLocal(Placement.Label);

		// Line from the button to the label, with a small dot on the button.
		const FLinearColor LineColor = bBound ? Faded(Theme->TextSecondary, 0.55f) : Faded(Theme->TextMuted, 0.45f);
		FSlateDrawElement::MakeLines(OutDrawElements, LineLayer, PaintGeometry, TArray<FVector2f>{ Anchor, LineEnd }, Effects, LineColor, true, 1.f);
		FSlateDrawElement::MakeLines(OutDrawElements, LineLayer, PaintGeometry, MakeCirclePoints(Anchor, 2.f, 12), Effects, LineColor, true, 2.f);

		// [Glyph] Action, right-aligned on the left side, left-aligned on the right side.
		const FVector2f GlyphTextSize = MeasureText(Glyph, GlyphFont);
		const FVector2f GlyphSize(FMath::Max(GlyphHeight, GlyphTextSize.X + 14.f), GlyphHeight);
		const FVector2f LabelSize = MeasureText(Text, LabelFont);
		const float GroupWidth = GlyphSize.X + Gap + LabelSize.X;
		const float GroupLeft = Placement.Side < 0 ? LineEnd.X - 8.f - GroupWidth : LineEnd.X + 8.f;
		const float CenterY = LineEnd.Y;

		// Draw elements take their colour from the tint, not from the brush.
		const FVector2f GlyphPosition(GroupLeft, CenterY - GlyphSize.Y * 0.5f);
		// Button names on small rounded chips.
		FALKPaint::RoundedBox(OutDrawElements, GlyphLayer, AllottedGeometry, GlyphPosition, GlyphPosition + GlyphSize, FMath::Min(Theme->CornerRadius, 6.f),
			bBound ? Faded(Theme->Accent) : Faded(Theme->TextMuted, 0.45f));
		const FVector2f GlyphTextPosition = GlyphPosition + (GlyphSize - GlyphTextSize) * 0.5f;
		FSlateDrawElement::MakeText(OutDrawElements, TextLayer, AllottedGeometry.ToPaintGeometry(GlyphTextSize, FSlateLayoutTransform(GlyphTextPosition)), Glyph, GlyphFont, Effects,
			bBound ? Faded(Theme->AccentContrast) : Faded(Theme->AccentContrast, 0.8f));

		const FVector2f LabelPosition(GroupLeft + GlyphSize.X + Gap, CenterY - LabelSize.Y * 0.5f);
		FSlateDrawElement::MakeText(OutDrawElements, TextLayer, AllottedGeometry.ToPaintGeometry(LabelSize, FSlateLayoutTransform(LabelPosition)), Text, LabelFont, Effects,
			bBound ? Faded(Theme->TextPrimary) : Faded(Theme->TextMuted));
	}

	return TextLayer;
}

#undef LOCTEXT_NAMESPACE
