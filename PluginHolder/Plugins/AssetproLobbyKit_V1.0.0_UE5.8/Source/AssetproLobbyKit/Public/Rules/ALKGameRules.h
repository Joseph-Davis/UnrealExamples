// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ALKTypes.h"
#include "ALKGameRules.generated.h"

/** How a match rule is edited on the host screen. */
UENUM(BlueprintType)
enum class EALKRuleType : uint8
{
	/** On or off. The value is "1" or "0". */
	Toggle,
	/** One of a list of choices. The value is the choice's index ("0", "1"...). */
	Options,
	/** A number between Min Value and Max Value. */
	Slider,
	/** Free text, for example a map seed. */
	Text
};

/**
 * One match rule the host can set (time limit, score limit, friendly fire...), defined purely in data.
 * Add yours to the Online Config's Rule Definitions; read the chosen value in the match with Get Rule Int / Bool / Float.
 */
USTRUCT(BlueprintType)
struct ASSETPROLOBBYKIT_API FALKRuleDefinition
{
	GENERATED_BODY()

	/** The name your game code reads the rule by, for example "TimeLimit". Letters, digits and underscores. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
	FName Id;

	/** The rule's name on the host screen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
	FText DisplayName;

	/** Shown as the rule's hint on the host screen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule", meta = (MultiLine = true))
	FText Description;

	/** How the rule is edited: a switch, a list of options, a slider or a line of text. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
	EALKRuleType Type = EALKRuleType::Toggle;

	/** The choices of an Options rule, in order. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule", meta = (EditCondition = "Type == EALKRuleType::Options", EditConditionHides))
	TArray<FText> Options;

	/** The slider's lowest value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule", meta = (EditCondition = "Type == EALKRuleType::Slider", EditConditionHides))
	float MinValue = 0.f;

	/** The slider's highest value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule", meta = (EditCondition = "Type == EALKRuleType::Slider", EditConditionHides))
	float MaxValue = 1.f;

	/** The slider moves in steps of this size. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule", meta = (EditCondition = "Type == EALKRuleType::Slider", EditConditionHides))
	float StepSize = 1.f;

	/** How a slider value is printed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule", meta = (EditCondition = "Type == EALKRuleType::Slider", EditConditionHides))
	EALKValueFormat Format = EALKValueFormat::Integer;

	/** Printed after a slider value, for example "min" or "s". Optional. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule", meta = (EditCondition = "Type == EALKRuleType::Slider", EditConditionHides))
	FText Unit;

	/** Shown instead of the number when a slider is at 0, for example "Unlimited" or "Off". Optional. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule", meta = (EditCondition = "Type == EALKRuleType::Slider", EditConditionHides))
	FText ZeroText;

	/** Toggle: "0" or "1". Options: the choice's index. Slider: the number. Text: the text. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
	FString DefaultValue;

	/** Offered for every game mode. Turn off for a rule that only the modes listing it in their Extra Rules use. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
	bool bAllModes = true;

	static FALKRuleDefinition MakeToggle(FName InId, const FText& InName, const FText& InDescription, bool bDefault);
	static FALKRuleDefinition MakeSlider(FName InId, const FText& InName, const FText& InDescription, float InMin, float InMax, float InStep, float InDefault,
		EALKValueFormat InFormat = EALKValueFormat::Integer, const FText& InUnit = FText::GetEmpty(), const FText& InZeroText = FText::GetEmpty());
	static FALKRuleDefinition MakeOptions(FName InId, const FText& InName, const FText& InDescription, const TArray<FText>& InOptions, int32 DefaultIndex);
	static FALKRuleDefinition MakeText(FName InId, const FText& InName, const FText& InDescription, const FString& InDefault);

	/** A value made valid for this rule: "0"/"1" (toggle), an existing index (options), clamped and on a step (slider), trimmed (text). */
	FString SanitizeValue(const FString& Value) const;

	/** A value as the player reads it: "On", the choice's name, "10 min", "Unlimited"... */
	FText GetValueText(const FString& Value) const;
};

/** One rule's chosen value. */
USTRUCT(BlueprintType)
struct ASSETPROLOBBYKIT_API FALKRuleValue
{
	GENERATED_BODY()

	/** The rule's Id. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
	FName Id;

	/** Its value as text: "1", "10", "0.5", a seed... */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
	FString Value;

	FALKRuleValue() = default;
	FALKRuleValue(FName InId, const FString& InValue) : Id(InId), Value(InValue) {}

	bool operator==(const FALKRuleValue& Other) const { return Id == Other.Id && Value == Other.Value; }
	bool operator!=(const FALKRuleValue& Other) const { return !(*this == Other); }
};

/**
 * The rules of one match: a small list of Id / value pairs. It replicates (the lobby game state carries it), travels to
 * the match in the level URL ("?ALKRules=TimeLimit=10,ScoreLimit=50") and is advertised with the session.
 *
 * In your match code: Rules.GetInt("TimeLimit"), Rules.GetBool("FriendlyFire")... (Blueprint: Get Rule Int / Bool / Float / String).
 */
USTRUCT(BlueprintType)
struct ASSETPROLOBBYKIT_API FALKGameRules
{
	GENERATED_BODY()

	/** One entry per rule. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rules", meta = (TitleProperty = "Id"))
	TArray<FALKRuleValue> Values;

	bool Has(FName Id) const;
	FString GetString(FName Id, const FString& Default = FString()) const;
	bool GetBool(FName Id, bool bDefault = false) const;
	int32 GetInt(FName Id, int32 Default = 0) const;
	float GetFloat(FName Id, float Default = 0.f) const;

	void SetString(FName Id, const FString& Value);
	void SetBool(FName Id, bool bValue);
	void SetInt(FName Id, int32 Value);
	void SetFloat(FName Id, float Value);
	bool Remove(FName Id);
	void Reset() { Values.Reset(); }
	int32 Num() const { return Values.Num(); }

	/** Adds every rule that has no value yet, at its default. Values already set are kept. */
	void ApplyDefaults(const TArray<FALKRuleDefinition>& Definitions);

	/** Every rule back to its default; rules without a definition are dropped. */
	void ResetToDefaults(const TArray<FALKRuleDefinition>& Definitions);

	/** Makes every value valid for its definition, adds missing rules at their default and drops rules without a definition. */
	void Sanitize(const TArray<FALKRuleDefinition>& Definitions);

	/** A random valid value for every rule. The same seed always gives the same rules. */
	void Randomize(const TArray<FALKRuleDefinition>& Definitions, int32 Seed);

	/** "TimeLimit=10,ScoreLimit=50,FriendlyFire=0": safe in a travel URL option and in a session setting. */
	FString ToOptionsString() const;

	/** Reads To Options String back. Also accepts a whole URL options string that has an "ALKRules=" option in it. */
	static FALKGameRules FromOptionsString(const FString& Options);

	/** The rules as a map, for display. */
	TMap<FName, FString> ToMap() const;

	/** A number as rule text: "10" for whole numbers, up to three decimals otherwise, always with a dot. */
	static FString NumberToString(float Value);

	bool operator==(const FALKGameRules& Other) const { return Values == Other.Values; }
	bool operator!=(const FALKGameRules& Other) const { return !(*this == Other); }
};

/** A named set of rules the player saved on the host screen. */
USTRUCT(BlueprintType)
struct ASSETPROLOBBYKIT_API FALKRulePreset
{
	GENERATED_BODY()

	/** The preset's name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rules")
	FString Name;

	/** The game mode the preset was saved for (None = any). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rules")
	FName ModeId;

	/** The saved rules. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rules")
	FALKGameRules Rules;
};

/**
 * Helpers for values carried in level URLs ("?Key=Value?Key2=Value2") and session settings.
 * Escaped text only uses letters, digits and . _ - + ~ so it survives Server Travel, the command line and LAN packets.
 */
struct ASSETPROLOBBYKIT_API FALKOptionString
{
	/** Any text as URL-safe ASCII: a space becomes "+", other special characters "~XX" or "~uXXXX". */
	static FString Escape(const FString& Text);

	/** Reads Escape back. */
	static FString Unescape(const FString& Text);

	/** "?Key=EscapedValue", to append to a level URL. */
	static FString MakeOption(const TCHAR* Key, const FString& RawValue);

	/** Finds "Key=" in a URL options string and gives its unescaped value. False when the option is not there. */
	static bool GetOption(const FString& Options, const TCHAR* Key, FString& OutValue);

	static int32 GetIntOption(const FString& Options, const TCHAR* Key, int32 Default);
};
