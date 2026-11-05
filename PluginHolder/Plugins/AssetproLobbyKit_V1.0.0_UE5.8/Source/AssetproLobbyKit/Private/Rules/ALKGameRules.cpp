// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Rules/ALKGameRules.h"
#include "Math/RandomStream.h"
#include "Misc/CString.h"
#include "Misc/Char.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKGameRules)

#define LOCTEXT_NAMESPACE "ALKRules"

namespace ALKRulesPrivate
{
	static const TCHAR* RulesOptionKey = TEXT("ALKRules");

	static bool IsPlainChar(TCHAR Char)
	{
		return (Char >= TEXT('A') && Char <= TEXT('Z')) || (Char >= TEXT('a') && Char <= TEXT('z')) || (Char >= TEXT('0') && Char <= TEXT('9'))
			|| Char == TEXT('.') || Char == TEXT('_') || Char == TEXT('-');
	}

	static int32 HexValue(TCHAR Char)
	{
		if (Char >= TEXT('0') && Char <= TEXT('9'))
		{
			return Char - TEXT('0');
		}
		if (Char >= TEXT('A') && Char <= TEXT('F'))
		{
			return 10 + (Char - TEXT('A'));
		}
		if (Char >= TEXT('a') && Char <= TEXT('f'))
		{
			return 10 + (Char - TEXT('a'));
		}
		return INDEX_NONE;
	}

	/** Reads Count hex digits at Index. Returns false when they are not all there. */
	static bool ReadHex(const FString& Text, int32 Index, int32 Count, uint32& OutValue)
	{
		if (Index + Count > Text.Len())
		{
			return false;
		}
		OutValue = 0;
		for (int32 Offset = 0; Offset < Count; ++Offset)
		{
			const int32 Digit = HexValue(Text[Index + Offset]);
			if (Digit == INDEX_NONE)
			{
				return false;
			}
			OutValue = (OutValue << 4) | static_cast<uint32>(Digit);
		}
		return true;
	}

	static bool IsTruthy(const FString& Value)
	{
		return Value == TEXT("1") || Value.Equals(TEXT("true"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("on"), ESearchCase::IgnoreCase)
			|| Value.Equals(TEXT("yes"), ESearchCase::IgnoreCase);
	}
}

// ---------------------------------------------------------------- FALKOptionString

FString FALKOptionString::Escape(const FString& Text)
{
	FString Result;
	Result.Reserve(Text.Len() + 8);
	for (const TCHAR Char : Text)
	{
		if (ALKRulesPrivate::IsPlainChar(Char))
		{
			Result.AppendChar(Char);
		}
		else if (Char == TEXT(' '))
		{
			Result.AppendChar(TEXT('+'));
		}
		else
		{
			const uint32 Code = static_cast<uint32>(static_cast<uint16>(Char));
			if (Code < 0x100)
			{
				Result += FString::Printf(TEXT("~%02X"), Code);
			}
			else
			{
				Result += FString::Printf(TEXT("~u%04X"), Code);
			}
		}
	}
	return Result;
}

FString FALKOptionString::Unescape(const FString& Text)
{
	FString Result;
	Result.Reserve(Text.Len());
	for (int32 Index = 0; Index < Text.Len(); ++Index)
	{
		const TCHAR Char = Text[Index];
		if (Char == TEXT('+'))
		{
			Result.AppendChar(TEXT(' '));
		}
		else if (Char == TEXT('~'))
		{
			uint32 Code = 0;
			if (Index + 1 < Text.Len() && Text[Index + 1] == TEXT('u') && ALKRulesPrivate::ReadHex(Text, Index + 2, 4, Code))
			{
				Result.AppendChar(static_cast<TCHAR>(Code));
				Index += 5;
			}
			else if (ALKRulesPrivate::ReadHex(Text, Index + 1, 2, Code))
			{
				Result.AppendChar(static_cast<TCHAR>(Code));
				Index += 2;
			}
			// A stray "~" is dropped.
		}
		else
		{
			Result.AppendChar(Char);
		}
	}
	return Result;
}

FString FALKOptionString::MakeOption(const TCHAR* Key, const FString& RawValue)
{
	return FString::Printf(TEXT("?%s=%s"), Key, *Escape(RawValue));
}

bool FALKOptionString::GetOption(const FString& Options, const TCHAR* Key, FString& OutValue)
{
	const FString Prefix = FString(Key) + TEXT("=");
	int32 SearchFrom = 0;
	while (SearchFrom < Options.Len())
	{
		const int32 Found = Options.Find(Prefix, ESearchCase::IgnoreCase, ESearchDir::FromStart, SearchFrom);
		if (Found == INDEX_NONE)
		{
			return false;
		}
		// The key has to start the string or follow a "?", so "XALKName=" does not match "ALKName=".
		if (Found == 0 || Options[Found - 1] == TEXT('?'))
		{
			const int32 ValueStart = Found + Prefix.Len();
			int32 ValueEnd = Options.Find(TEXT("?"), ESearchCase::CaseSensitive, ESearchDir::FromStart, ValueStart);
			if (ValueEnd == INDEX_NONE)
			{
				ValueEnd = Options.Len();
			}
			OutValue = Unescape(Options.Mid(ValueStart, ValueEnd - ValueStart));
			return true;
		}
		SearchFrom = Found + 1;
	}
	return false;
}

int32 FALKOptionString::GetIntOption(const FString& Options, const TCHAR* Key, int32 Default)
{
	FString Value;
	return GetOption(Options, Key, Value) && Value.IsNumeric() ? FCString::Atoi(*Value) : Default;
}

// ---------------------------------------------------------------- FALKRuleDefinition

FALKRuleDefinition FALKRuleDefinition::MakeToggle(FName InId, const FText& InName, const FText& InDescription, bool bDefault)
{
	FALKRuleDefinition Rule;
	Rule.Id = InId;
	Rule.DisplayName = InName;
	Rule.Description = InDescription;
	Rule.Type = EALKRuleType::Toggle;
	Rule.DefaultValue = bDefault ? TEXT("1") : TEXT("0");
	return Rule;
}

FALKRuleDefinition FALKRuleDefinition::MakeSlider(FName InId, const FText& InName, const FText& InDescription, float InMin, float InMax, float InStep, float InDefault,
	EALKValueFormat InFormat, const FText& InUnit, const FText& InZeroText)
{
	FALKRuleDefinition Rule;
	Rule.Id = InId;
	Rule.DisplayName = InName;
	Rule.Description = InDescription;
	Rule.Type = EALKRuleType::Slider;
	Rule.MinValue = InMin;
	Rule.MaxValue = InMax;
	Rule.StepSize = InStep;
	Rule.Format = InFormat;
	Rule.Unit = InUnit;
	Rule.ZeroText = InZeroText;
	Rule.DefaultValue = FALKGameRules::NumberToString(InDefault);
	return Rule;
}

FALKRuleDefinition FALKRuleDefinition::MakeOptions(FName InId, const FText& InName, const FText& InDescription, const TArray<FText>& InOptions, int32 DefaultIndex)
{
	FALKRuleDefinition Rule;
	Rule.Id = InId;
	Rule.DisplayName = InName;
	Rule.Description = InDescription;
	Rule.Type = EALKRuleType::Options;
	Rule.Options = InOptions;
	Rule.DefaultValue = FString::FromInt(FMath::Clamp(DefaultIndex, 0, FMath::Max(0, InOptions.Num() - 1)));
	return Rule;
}

FALKRuleDefinition FALKRuleDefinition::MakeText(FName InId, const FText& InName, const FText& InDescription, const FString& InDefault)
{
	FALKRuleDefinition Rule;
	Rule.Id = InId;
	Rule.DisplayName = InName;
	Rule.Description = InDescription;
	Rule.Type = EALKRuleType::Text;
	Rule.DefaultValue = InDefault;
	return Rule;
}

FString FALKRuleDefinition::SanitizeValue(const FString& Value) const
{
	switch (Type)
	{
	case EALKRuleType::Toggle:
		return ALKRulesPrivate::IsTruthy(Value) ? TEXT("1") : TEXT("0");

	case EALKRuleType::Options:
	{
		const int32 Index = Value.IsNumeric() ? FCString::Atoi(*Value) : 0;
		return FString::FromInt(FMath::Clamp(Index, 0, FMath::Max(0, Options.Num() - 1)));
	}

	case EALKRuleType::Slider:
	{
		const float Low = FMath::Min(MinValue, MaxValue);
		const float High = FMath::Max(MinValue, MaxValue);
		float Number = Value.IsNumeric() ? FCString::Atof(*Value) : Low;
		Number = FMath::Clamp(Number, Low, High);
		if (StepSize > KINDA_SMALL_NUMBER)
		{
			Number = FMath::Clamp(Low + FMath::RoundToFloat((Number - Low) / StepSize) * StepSize, Low, High);
		}
		return FALKGameRules::NumberToString(Number);
	}

	case EALKRuleType::Text:
	default:
		return Value.TrimStartAndEnd().Left(64);
	}
}

FText FALKRuleDefinition::GetValueText(const FString& Value) const
{
	switch (Type)
	{
	case EALKRuleType::Toggle:
		return ALKRulesPrivate::IsTruthy(Value) ? LOCTEXT("RuleOn", "On") : LOCTEXT("RuleOff", "Off");

	case EALKRuleType::Options:
	{
		const int32 Index = Value.IsNumeric() ? FCString::Atoi(*Value) : 0;
		return Options.IsValidIndex(Index) ? Options[Index] : FText::GetEmpty();
	}

	case EALKRuleType::Slider:
	{
		const float Number = Value.IsNumeric() ? FCString::Atof(*Value) : 0.f;
		if (!ZeroText.IsEmpty() && FMath::IsNearlyZero(Number))
		{
			return ZeroText;
		}
		FText NumberText;
		switch (Format)
		{
		case EALKValueFormat::Percent:
			NumberText = FText::AsPercent(Number);
			break;
		case EALKValueFormat::OneDecimal:
		{
			FNumberFormattingOptions Formatting;
			Formatting.SetMinimumFractionalDigits(1).SetMaximumFractionalDigits(1);
			NumberText = FText::AsNumber(Number, &Formatting);
			break;
		}
		case EALKValueFormat::TwoDecimals:
		{
			FNumberFormattingOptions Formatting;
			Formatting.SetMinimumFractionalDigits(2).SetMaximumFractionalDigits(2);
			NumberText = FText::AsNumber(Number, &Formatting);
			break;
		}
		case EALKValueFormat::Integer:
		default:
			NumberText = FText::AsNumber(FMath::RoundToInt(Number));
			break;
		}
		return Unit.IsEmpty() ? NumberText : FText::Format(LOCTEXT("RuleValueWithUnit", "{0} {1}"), NumberText, Unit);
	}

	case EALKRuleType::Text:
	default:
		return FText::FromString(Value);
	}
}

// ---------------------------------------------------------------- FALKGameRules

bool FALKGameRules::Has(FName Id) const
{
	return Values.ContainsByPredicate([Id](const FALKRuleValue& Each) { return Each.Id == Id; });
}

FString FALKGameRules::GetString(FName Id, const FString& Default) const
{
	const FALKRuleValue* Found = Values.FindByPredicate([Id](const FALKRuleValue& Each) { return Each.Id == Id; });
	return Found ? Found->Value : Default;
}

bool FALKGameRules::GetBool(FName Id, bool bDefault) const
{
	const FALKRuleValue* Found = Values.FindByPredicate([Id](const FALKRuleValue& Each) { return Each.Id == Id; });
	return Found ? ALKRulesPrivate::IsTruthy(Found->Value) : bDefault;
}

int32 FALKGameRules::GetInt(FName Id, int32 Default) const
{
	const FALKRuleValue* Found = Values.FindByPredicate([Id](const FALKRuleValue& Each) { return Each.Id == Id; });
	return Found && Found->Value.IsNumeric() ? FMath::RoundToInt(FCString::Atof(*Found->Value)) : Default;
}

float FALKGameRules::GetFloat(FName Id, float Default) const
{
	const FALKRuleValue* Found = Values.FindByPredicate([Id](const FALKRuleValue& Each) { return Each.Id == Id; });
	return Found && Found->Value.IsNumeric() ? FCString::Atof(*Found->Value) : Default;
}

void FALKGameRules::SetString(FName Id, const FString& Value)
{
	if (Id.IsNone())
	{
		return;
	}
	if (FALKRuleValue* Found = Values.FindByPredicate([Id](const FALKRuleValue& Each) { return Each.Id == Id; }))
	{
		Found->Value = Value;
	}
	else
	{
		Values.Emplace(Id, Value);
	}
}

void FALKGameRules::SetBool(FName Id, bool bValue)
{
	SetString(Id, bValue ? TEXT("1") : TEXT("0"));
}

void FALKGameRules::SetInt(FName Id, int32 Value)
{
	SetString(Id, FString::FromInt(Value));
}

void FALKGameRules::SetFloat(FName Id, float Value)
{
	SetString(Id, NumberToString(Value));
}

bool FALKGameRules::Remove(FName Id)
{
	return Values.RemoveAll([Id](const FALKRuleValue& Each) { return Each.Id == Id; }) > 0;
}

void FALKGameRules::ApplyDefaults(const TArray<FALKRuleDefinition>& Definitions)
{
	for (const FALKRuleDefinition& Definition : Definitions)
	{
		if (!Definition.Id.IsNone() && !Has(Definition.Id))
		{
			Values.Emplace(Definition.Id, Definition.SanitizeValue(Definition.DefaultValue));
		}
	}
}

void FALKGameRules::ResetToDefaults(const TArray<FALKRuleDefinition>& Definitions)
{
	Values.Reset();
	ApplyDefaults(Definitions);
}

void FALKGameRules::Sanitize(const TArray<FALKRuleDefinition>& Definitions)
{
	TArray<FALKRuleValue> Clean;
	Clean.Reserve(Definitions.Num());
	for (const FALKRuleDefinition& Definition : Definitions)
	{
		if (Definition.Id.IsNone() || Clean.ContainsByPredicate([&Definition](const FALKRuleValue& Each) { return Each.Id == Definition.Id; }))
		{
			continue;
		}
		const FString Raw = Has(Definition.Id) ? GetString(Definition.Id) : Definition.DefaultValue;
		Clean.Emplace(Definition.Id, Definition.SanitizeValue(Raw));
	}
	Values = MoveTemp(Clean);
}

void FALKGameRules::Randomize(const TArray<FALKRuleDefinition>& Definitions, int32 Seed)
{
	FRandomStream Stream(Seed);
	Values.Reset();
	for (const FALKRuleDefinition& Definition : Definitions)
	{
		if (Definition.Id.IsNone() || Has(Definition.Id))
		{
			continue;
		}
		FString Value;
		switch (Definition.Type)
		{
		case EALKRuleType::Toggle:
			Value = Stream.RandRange(0, 1) == 1 ? TEXT("1") : TEXT("0");
			break;
		case EALKRuleType::Options:
			Value = FString::FromInt(Stream.RandRange(0, FMath::Max(0, Definition.Options.Num() - 1)));
			break;
		case EALKRuleType::Slider:
		{
			const float Low = FMath::Min(Definition.MinValue, Definition.MaxValue);
			const float High = FMath::Max(Definition.MinValue, Definition.MaxValue);
			if (Definition.StepSize > KINDA_SMALL_NUMBER)
			{
				const int32 Steps = FMath::Max(0, FMath::FloorToInt((High - Low) / Definition.StepSize + 0.001f));
				Value = NumberToString(Low + Stream.RandRange(0, Steps) * Definition.StepSize);
			}
			else
			{
				Value = NumberToString(Stream.FRandRange(Low, High));
			}
			break;
		}
		case EALKRuleType::Text:
		default:
		{
			// A short seed-like word, so "Random" also works for seed rules.
			static const TCHAR Alphabet[] = TEXT("ABCDEFGHJKMNPQRSTVWXYZ23456789");
			const int32 AlphabetLen = UE_ARRAY_COUNT(Alphabet) - 1;
			for (int32 Index = 0; Index < 8; ++Index)
			{
				Value.AppendChar(Alphabet[Stream.RandRange(0, AlphabetLen - 1)]);
			}
			break;
		}
		}
		Values.Emplace(Definition.Id, Definition.SanitizeValue(Value));
	}
}

FString FALKGameRules::ToOptionsString() const
{
	FString Result;
	for (const FALKRuleValue& Each : Values)
	{
		if (Each.Id.IsNone())
		{
			continue;
		}
		if (!Result.IsEmpty())
		{
			Result.AppendChar(TEXT(','));
		}
		FString Key = FALKOptionString::Escape(Each.Id.ToString());
		// The engine picks the game mode by looking for "game=" anywhere in a level URL's options: a rule Id that ends
		// in "Game" must not read that way, so its last letter is written escaped.
		if (Key.EndsWith(TEXT("game"), ESearchCase::IgnoreCase))
		{
			const TCHAR Last = Key[Key.Len() - 1];
			Key.LeftChopInline(1);
			Key += FString::Printf(TEXT("~%02X"), static_cast<uint32>(static_cast<uint16>(Last)));
		}
		Result += Key;
		Result.AppendChar(TEXT('='));
		Result += FALKOptionString::Escape(Each.Value);
	}
	return Result;
}

FALKGameRules FALKGameRules::FromOptionsString(const FString& Options)
{
	FALKGameRules Rules;

	// A whole URL options string: read its ALKRules option. Escaped text never has "?" in it, so this cannot misfire.
	FString List = Options;
	if (Options.Contains(TEXT("?")) || Options.StartsWith(FString(ALKRulesPrivate::RulesOptionKey) + TEXT("="), ESearchCase::IgnoreCase))
	{
		List.Reset();
		// Get Option unescapes, which would turn the "+" of a value into a space before the list is split: take the raw text.
		const FString Prefix = FString(ALKRulesPrivate::RulesOptionKey) + TEXT("=");
		int32 SearchFrom = 0;
		while (SearchFrom < Options.Len())
		{
			const int32 Found = Options.Find(Prefix, ESearchCase::IgnoreCase, ESearchDir::FromStart, SearchFrom);
			if (Found == INDEX_NONE)
			{
				break;
			}
			if (Found == 0 || Options[Found - 1] == TEXT('?'))
			{
				const int32 ValueStart = Found + Prefix.Len();
				int32 ValueEnd = Options.Find(TEXT("?"), ESearchCase::CaseSensitive, ESearchDir::FromStart, ValueStart);
				if (ValueEnd == INDEX_NONE)
				{
					ValueEnd = Options.Len();
				}
				List = Options.Mid(ValueStart, ValueEnd - ValueStart);
				break;
			}
			SearchFrom = Found + 1;
		}
	}

	TArray<FString> Pairs;
	List.ParseIntoArray(Pairs, TEXT(","), true);
	for (const FString& Pair : Pairs)
	{
		FString Key;
		FString Value;
		if (!Pair.Split(TEXT("="), &Key, &Value, ESearchCase::CaseSensitive))
		{
			Key = Pair;
		}
		Key = FALKOptionString::Unescape(Key);
		if (!Key.IsEmpty())
		{
			Rules.SetString(FName(*Key), FALKOptionString::Unescape(Value));
		}
	}
	return Rules;
}

TMap<FName, FString> FALKGameRules::ToMap() const
{
	TMap<FName, FString> Map;
	for (const FALKRuleValue& Each : Values)
	{
		Map.Add(Each.Id, Each.Value);
	}
	return Map;
}

FString FALKGameRules::NumberToString(float Value)
{
	const float Rounded = FMath::RoundToFloat(Value);
	if (FMath::IsNearlyEqual(Value, Rounded, 0.0005f))
	{
		return FString::FromInt(static_cast<int32>(Rounded));
	}
	FString Text = FString::Printf(TEXT("%.3f"), Value);
	while (Text.EndsWith(TEXT("0")))
	{
		Text.LeftChopInline(1);
	}
	if (Text.EndsWith(TEXT(".")))
	{
		Text.LeftChopInline(1);
	}
	return Text;
}

#undef LOCTEXT_NAMESPACE
