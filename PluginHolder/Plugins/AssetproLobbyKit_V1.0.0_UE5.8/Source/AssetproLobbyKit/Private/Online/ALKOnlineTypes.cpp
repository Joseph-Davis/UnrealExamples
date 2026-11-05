// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Online/ALKOnlineTypes.h"
#include "Lobby/ALKLobbyGameState.h"
#include "Engine/GameInstance.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Misc/Char.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKOnlineTypes)

#define LOCTEXT_NAMESPACE "ALKOnline"

// ---------------------------------------------------------------- Connection errors

void FALKConnectionErrors::GetText(EALKConnectionError Error, FText& OutTitle, FText& OutMessage)
{
	switch (Error)
	{
	case EALKConnectionError::TimedOut:
		OutTitle = LOCTEXT("ErrTimedOutTitle", "Connection Timed Out");
		OutMessage = LOCTEXT("ErrTimedOutMessage", "The game did not answer in time. Check your connection and try again.");
		break;
	case EALKConnectionError::ServerFull:
		OutTitle = LOCTEXT("ErrServerFullTitle", "Game Full");
		OutMessage = LOCTEXT("ErrServerFullMessage", "There is no free player slot in this game.");
		break;
	case EALKConnectionError::WrongVersion:
		OutTitle = LOCTEXT("ErrWrongVersionTitle", "Version Mismatch");
		OutMessage = LOCTEXT("ErrWrongVersionMessage", "This game runs a different version. Update your game and try again.");
		break;
	case EALKConnectionError::WrongPassword:
		OutTitle = LOCTEXT("ErrWrongPasswordTitle", "Wrong Password");
		OutMessage = LOCTEXT("ErrWrongPasswordMessage", "The password for this game is not correct.");
		break;
	case EALKConnectionError::HostLeft:
		OutTitle = LOCTEXT("ErrHostLeftTitle", "Host Left");
		OutMessage = LOCTEXT("ErrHostLeftMessage", "The host closed the game.");
		break;
	case EALKConnectionError::Kicked:
		OutTitle = LOCTEXT("ErrKickedTitle", "Removed From Game");
		OutMessage = LOCTEXT("ErrKickedMessage", "The host removed you from the game.");
		break;
	case EALKConnectionError::Banned:
		OutTitle = LOCTEXT("ErrBannedTitle", "Banned");
		OutMessage = LOCTEXT("ErrBannedMessage", "The host has banned you from this game.");
		break;
	case EALKConnectionError::NetworkError:
		OutTitle = LOCTEXT("ErrNetworkTitle", "Network Error");
		OutMessage = LOCTEXT("ErrNetworkMessage", "The connection to the game was lost.");
		break;
	case EALKConnectionError::SessionNotFound:
		OutTitle = LOCTEXT("ErrNotFoundTitle", "Game Not Found");
		OutMessage = LOCTEXT("ErrNotFoundMessage", "This game is no longer available.");
		break;
	case EALKConnectionError::Cancelled:
		OutTitle = LOCTEXT("ErrCancelledTitle", "Cancelled");
		OutMessage = LOCTEXT("ErrCancelledMessage", "The connection was cancelled.");
		break;
	case EALKConnectionError::Unknown:
		OutTitle = LOCTEXT("ErrUnknownTitle", "Connection Failed");
		OutMessage = LOCTEXT("ErrUnknownMessage", "Something went wrong while connecting. Please try again.");
		break;
	case EALKConnectionError::None:
	default:
		OutTitle = FText::GetEmpty();
		OutMessage = FText::GetEmpty();
		break;
	}
}

EALKConnectionError FALKConnectionErrors::FromReason(const FString& Reason)
{
	if (Reason.IsEmpty())
	{
		return EALKConnectionError::None;
	}
	if (Reason.Contains(ALKNetReason::ServerFull))
	{
		return EALKConnectionError::ServerFull;
	}
	if (Reason.Contains(ALKNetReason::WrongPassword))
	{
		return EALKConnectionError::WrongPassword;
	}
	if (Reason.Contains(ALKNetReason::WrongVersion))
	{
		return EALKConnectionError::WrongVersion;
	}
	if (Reason.Contains(ALKNetReason::Banned))
	{
		return EALKConnectionError::Banned;
	}
	if (Reason.Contains(ALKNetReason::Kicked))
	{
		return EALKConnectionError::Kicked;
	}
	if (Reason.Contains(ALKNetReason::HostLeft))
	{
		return EALKConnectionError::HostLeft;
	}

	// The engine's own wording (game session: "Server full."; net driver: "... timed out ...").
	if (Reason.Contains(TEXT("full"), ESearchCase::IgnoreCase))
	{
		return EALKConnectionError::ServerFull;
	}
	if (Reason.Contains(TEXT("timed out"), ESearchCase::IgnoreCase) || Reason.Contains(TEXT("timeout"), ESearchCase::IgnoreCase))
	{
		return EALKConnectionError::TimedOut;
	}
	if (Reason.Contains(TEXT("banned"), ESearchCase::IgnoreCase))
	{
		return EALKConnectionError::Banned;
	}
	if (Reason.Contains(TEXT("kicked"), ESearchCase::IgnoreCase))
	{
		return EALKConnectionError::Kicked;
	}
	if (Reason.Contains(TEXT("outdated"), ESearchCase::IgnoreCase) || Reason.Contains(TEXT("incompatible"), ESearchCase::IgnoreCase))
	{
		return EALKConnectionError::WrongVersion;
	}
	return EALKConnectionError::Unknown;
}

FString FALKConnectionErrors::ToReason(EALKConnectionError Error)
{
	switch (Error)
	{
	case EALKConnectionError::ServerFull: return ALKNetReason::ServerFull;
	case EALKConnectionError::WrongPassword: return ALKNetReason::WrongPassword;
	case EALKConnectionError::WrongVersion: return ALKNetReason::WrongVersion;
	case EALKConnectionError::Banned: return ALKNetReason::Banned;
	case EALKConnectionError::Kicked: return ALKNetReason::Kicked;
	case EALKConnectionError::HostLeft: return ALKNetReason::HostLeft;
	default: return FString();
	}
}

// ---------------------------------------------------------------- Sessions

bool FALKSessionFilter::Matches(const FALKSessionInfo& Session) const
{
	if (bHideFull && Session.IsFull())
	{
		return false;
	}
	if (bHideEmpty && Session.IsEmpty())
	{
		return false;
	}
	if (bHidePasswordProtected && Session.bPasswordProtected)
	{
		return false;
	}
	if (bLANOnly && !Session.bLAN)
	{
		return false;
	}
	if (!ModeId.IsNone() && Session.ModeId != ModeId)
	{
		return false;
	}
	if (!Region.IsEmpty() && !Session.Region.IsEmpty() && !Session.Region.Equals(Region, ESearchCase::IgnoreCase))
	{
		return false;
	}
	const FString Needle = SearchText.TrimStartAndEnd();
	if (!Needle.IsEmpty()
		&& !Session.ServerName.Contains(Needle, ESearchCase::IgnoreCase)
		&& !Session.HostName.Contains(Needle, ESearchCase::IgnoreCase)
		&& !Session.ModeId.ToString().Contains(Needle, ESearchCase::IgnoreCase)
		&& !Session.MapId.ToString().Contains(Needle, ESearchCase::IgnoreCase))
	{
		return false;
	}
	return true;
}

TArray<FALKSessionInfo> FALKSessionFilter::Apply(const FALKSessionFilter& Filter, const TArray<FALKSessionInfo>& Sessions)
{
	TArray<FALKSessionInfo> Result;
	Result.Reserve(Sessions.Num());
	for (const FALKSessionInfo& Session : Sessions)
	{
		if (Filter.Matches(Session))
		{
			Result.Add(Session);
		}
	}
	return Result;
}

void FALKSessionFilter::Sort(TArray<FALKSessionInfo>& Sessions, EALKSessionSort SortBy, bool bAscending)
{
	// -1, 0 or 1 for the chosen column only.
	auto Compare = [SortBy](const FALKSessionInfo& A, const FALKSessionInfo& B) -> int32
	{
		switch (SortBy)
		{
		case EALKSessionSort::Mode:
			return FMath::Clamp(A.ModeId.ToString().Compare(B.ModeId.ToString(), ESearchCase::IgnoreCase), -1, 1);
		case EALKSessionSort::Map:
			return FMath::Clamp(A.MapId.ToString().Compare(B.MapId.ToString(), ESearchCase::IgnoreCase), -1, 1);
		case EALKSessionSort::Players:
			return A.CurrentPlayers == B.CurrentPlayers ? FMath::Clamp(A.MaxPlayers - B.MaxPlayers, -1, 1) : (A.CurrentPlayers < B.CurrentPlayers ? -1 : 1);
		case EALKSessionSort::Ping:
		{
			// An unknown ping sorts as the worst one.
			const int32 PingA = A.PingMs < 0 ? MAX_int32 : A.PingMs;
			const int32 PingB = B.PingMs < 0 ? MAX_int32 : B.PingMs;
			return PingA == PingB ? 0 : (PingA < PingB ? -1 : 1);
		}
		case EALKSessionSort::Name:
		default:
			return FMath::Clamp(A.ServerName.Compare(B.ServerName, ESearchCase::IgnoreCase), -1, 1);
		}
	};

	Sessions.StableSort([&Compare, bAscending](const FALKSessionInfo& A, const FALKSessionInfo& B)
	{
		const int32 Order = Compare(A, B);
		if (Order != 0)
		{
			return bAscending ? Order < 0 : Order > 0;
		}
		// Ties: by name, then by id, whatever the direction, so a refresh never shuffles equal rows.
		const int32 NameOrder = A.ServerName.Compare(B.ServerName, ESearchCase::IgnoreCase);
		return NameOrder != 0 ? NameOrder < 0 : A.SessionId.Compare(B.SessionId, ESearchCase::CaseSensitive) < 0;
	});
}

FString FALKHostSettings::ToTravelOptions() const
{
	FString Options;
	Options += FALKOptionString::MakeOption(ALKNetOption::ServerName, ServerName);
	Options += FALKOptionString::MakeOption(ALKNetOption::Mode, ModeId.IsNone() ? FString() : ModeId.ToString());
	Options += FALKOptionString::MakeOption(ALKNetOption::Map, MapId.IsNone() ? FString() : MapId.ToString());
	Options += FString::Printf(TEXT("?%s=%d"), ALKNetOption::MaxPlayers, MaxPlayers);
	Options += FString::Printf(TEXT("?%s=%d"), ALKNetOption::Bots, BotCount);
	// The rules list is already escaped pair by pair.
	Options += FString::Printf(TEXT("?%s=%s"), ALKNetOption::Rules, *Rules.ToOptionsString());
	return Options;
}

bool FALKHostSettings::ApplyTravelOptions(const FString& Options)
{
	bool bAny = false;
	FString Value;
	if (FALKOptionString::GetOption(Options, ALKNetOption::ServerName, Value))
	{
		ServerName = Value;
		bAny = true;
	}
	if (FALKOptionString::GetOption(Options, ALKNetOption::Mode, Value))
	{
		ModeId = Value.IsEmpty() ? NAME_None : FName(*Value);
		bAny = true;
	}
	if (FALKOptionString::GetOption(Options, ALKNetOption::Map, Value))
	{
		MapId = Value.IsEmpty() ? NAME_None : FName(*Value);
		bAny = true;
	}
	if (FALKOptionString::GetOption(Options, ALKNetOption::MaxPlayers, Value) && Value.IsNumeric())
	{
		MaxPlayers = FCString::Atoi(*Value);
		bAny = true;
	}
	if (FALKOptionString::GetOption(Options, ALKNetOption::Bots, Value) && Value.IsNumeric())
	{
		BotCount = FCString::Atoi(*Value);
		bAny = true;
	}
	if (FALKOptionString::GetOption(Options, ALKNetOption::Rules, Value))
	{
		Rules = FALKGameRules::FromOptionsString(Options);
		bAny = true;
	}
	return bAny;
}

// ---------------------------------------------------------------- Net stats

FALKNetStats FALKNetStats::FromWorld(const UWorld* World)
{
	FALKNetStats Stats;
	if (!World || World->GetNetMode() == NM_Standalone)
	{
		return Stats;
	}
	Stats.bConnected = true;

	const UGameInstance* GameInstance = World->GetGameInstance();
	const APlayerController* LocalController = GameInstance ? GameInstance->GetFirstLocalPlayerController(World) : nullptr;
	const APlayerState* LocalState = LocalController ? LocalController->GetPlayerState<APlayerState>() : nullptr;
	if (LocalState && World->GetNetMode() == NM_Client)
	{
		Stats.PingMs = FMath::RoundToInt(LocalState->GetPingInMilliseconds());
	}

	if (const UNetDriver* Driver = World->GetNetDriver())
	{
		if (const UNetConnection* Server = Driver->ServerConnection)
		{
			// A client: its one connection to the server.
			Stats.InKbps = Server->InBytesPerSecond * 8.f / 1000.f;
			Stats.OutKbps = Server->OutBytesPerSecond * 8.f / 1000.f;
			Stats.PacketLossInPercent = FMath::Clamp(Server->GetInLossPercentage().GetAvgLossPercentage() * 100.f, 0.f, 100.f);
			Stats.PacketLossOutPercent = FMath::Clamp(Server->GetOutLossPercentage().GetAvgLossPercentage() * 100.f, 0.f, 100.f);
		}
		else
		{
			// A server: the traffic of every client together, the worst loss of any.
			for (const UNetConnection* Client : Driver->ClientConnections)
			{
				if (Client)
				{
					Stats.InKbps += Client->InBytesPerSecond * 8.f / 1000.f;
					Stats.OutKbps += Client->OutBytesPerSecond * 8.f / 1000.f;
					Stats.PacketLossInPercent = FMath::Max(Stats.PacketLossInPercent, FMath::Clamp(Client->GetInLossPercentage().GetAvgLossPercentage() * 100.f, 0.f, 100.f));
					Stats.PacketLossOutPercent = FMath::Max(Stats.PacketLossOutPercent, FMath::Clamp(Client->GetOutLossPercentage().GetAvgLossPercentage() * 100.f, 0.f, 100.f));
				}
			}
		}
	}

	if (const AALKLobbyGameState* GameState = World->GetGameState<AALKLobbyGameState>())
	{
		Stats.ServerTickRate = GameState->ServerTickRate;
	}
	return Stats;
}

// ---------------------------------------------------------------- Lobby codes

namespace ALKLobbyCodePrivate
{
	/** Crockford base 32: no I, L, O or U. */
	static const TCHAR Alphabet[] = TEXT("0123456789ABCDEFGHJKMNPQRSTVWXYZ");

	static constexpr int32 ShortDataDigits = 8;   // 40 bits: address 32 + port offset 8
	static constexpr int32 LongDataDigits = 11;   // 55 bits: address 32 + port 16 (+ 7 spare)
	static constexpr uint64 MixA = 0x9E3779B97F4A7C15ull;
	static constexpr uint64 MixB = 0xC2B2AE3D27D4EB4Full;

	static uint64 MaskBits(int32 Bits)
	{
		return (1ull << Bits) - 1ull;
	}

	/** The number that undoes a multiplication by an odd number, modulo 2^64 (and so modulo any smaller power of two). */
	static uint64 InverseOdd(uint64 Odd)
	{
		uint64 Inverse = Odd;
		for (int32 Step = 0; Step < 6; ++Step)
		{
			Inverse *= 2ull - Odd * Inverse;
		}
		return Inverse;
	}

	/** Spreads the address over every character, so two hosts on one network do not get look-alike codes. Reversible. */
	static uint64 Scramble(uint64 Value, int32 Bits)
	{
		const uint64 Mask = MaskBits(Bits);
		const int32 Shift = (Bits + 1) / 2;
		Value = (Value * MixA) & Mask;
		Value ^= Value >> Shift;
		Value = (Value * MixB) & Mask;
		Value ^= Value >> Shift;
		return Value;
	}

	static uint64 Unscramble(uint64 Value, int32 Bits)
	{
		const uint64 Mask = MaskBits(Bits);
		const int32 Shift = (Bits + 1) / 2;
		Value ^= Value >> Shift;
		Value = (Value * InverseOdd(MixB)) & Mask;
		Value ^= Value >> Shift;
		Value = (Value * InverseOdd(MixA)) & Mask;
		return Value;
	}

	/** 0..31 for a code character, with the look-alikes mapped (O = 0, I and L = 1). -1 for anything else. */
	static int32 DigitValue(TCHAR Char)
	{
		Char = FChar::ToUpper(Char);
		if (Char == TEXT('O'))
		{
			return 0;
		}
		if (Char == TEXT('I') || Char == TEXT('L'))
		{
			return 1;
		}
		for (int32 Index = 0; Index < 32; ++Index)
		{
			if (Alphabet[Index] == Char)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	/** The check digit: every position has its own odd weight, so one wrong character always changes it. */
	static int32 CheckDigit(const TArray<int32>& Digits)
	{
		int32 Sum = 13;
		for (int32 Index = 0; Index < Digits.Num(); ++Index)
		{
			Sum += Digits[Index] * (Index * 2 + 1);
		}
		return Sum & 31;
	}

	static FString DigitsToCode(uint64 Value, int32 DataDigits)
	{
		TArray<int32> Digits;
		Digits.SetNumUninitialized(DataDigits);
		for (int32 Index = DataDigits - 1; Index >= 0; --Index)
		{
			Digits[Index] = static_cast<int32>(Value & 31ull);
			Value >>= 5;
		}
		Digits.Add(CheckDigit(Digits));

		FString Code;
		for (int32 Index = 0; Index < Digits.Num(); ++Index)
		{
			if (Index > 0 && Index % 3 == 0)
			{
				Code.AppendChar(TEXT('-'));
			}
			Code.AppendChar(Alphabet[Digits[Index]]);
		}
		return Code;
	}

	/** The code's characters as digits, separators dropped. False when a character is not a code character. */
	static bool CodeToDigits(const FString& Code, TArray<int32>& OutDigits)
	{
		OutDigits.Reset();
		for (const TCHAR Char : Code)
		{
			if (Char == TEXT('-') || Char == TEXT(' ') || Char == TEXT('_'))
			{
				continue;
			}
			const int32 Digit = DigitValue(Char);
			if (Digit == INDEX_NONE)
			{
				return false;
			}
			OutDigits.Add(Digit);
		}
		return true;
	}

	static FString MakeAddress(uint32 Address, int32 Port)
	{
		return FString::Printf(TEXT("%u.%u.%u.%u:%d"), (Address >> 24) & 0xFF, (Address >> 16) & 0xFF, (Address >> 8) & 0xFF, Address & 0xFF, Port);
	}
}

bool FALKLobbyCode::ParseIPv4(const FString& Address, uint32& OutAddress, int32& OutPort)
{
	const FString Text = Address.TrimStartAndEnd();
	FString Host = Text;
	FString PortText;
	OutPort = BasePort;
	if (Text.Split(TEXT(":"), &Host, &PortText, ESearchCase::CaseSensitive))
	{
		if (PortText.IsEmpty() || PortText.Len() > 5)
		{
			return false;
		}
		for (const TCHAR Char : PortText)
		{
			if (!FChar::IsDigit(Char))
			{
				return false;
			}
		}
		OutPort = FCString::Atoi(*PortText);
		if (OutPort < 1 || OutPort > 65535)
		{
			return false;
		}
	}

	TArray<FString> Parts;
	Host.ParseIntoArray(Parts, TEXT("."), false);
	if (Parts.Num() != 4)
	{
		return false;
	}
	OutAddress = 0;
	for (const FString& Part : Parts)
	{
		if (Part.IsEmpty() || Part.Len() > 3)
		{
			return false;
		}
		for (const TCHAR Char : Part)
		{
			if (!FChar::IsDigit(Char))
			{
				return false;
			}
		}
		const int32 Byte = FCString::Atoi(*Part);
		if (Byte > 255)
		{
			return false;
		}
		OutAddress = (OutAddress << 8) | static_cast<uint32>(Byte);
	}
	return true;
}

FString FALKLobbyCode::EncodeIPv4(uint32 Address, int32 Port)
{
	using namespace ALKLobbyCodePrivate;
	if (Port < 1 || Port > 65535)
	{
		return FString();
	}
	if (Port >= BasePort && Port < BasePort + 256)
	{
		const uint64 Payload = (static_cast<uint64>(Address) << 8) | static_cast<uint64>(Port - BasePort);
		return DigitsToCode(Scramble(Payload, ShortDataDigits * 5), ShortDataDigits);
	}
	const uint64 Payload = (static_cast<uint64>(Address) << 16) | static_cast<uint64>(Port);
	return DigitsToCode(Scramble(Payload, LongDataDigits * 5), LongDataDigits);
}

FString FALKLobbyCode::Encode(const FString& Address)
{
	uint32 IPv4 = 0;
	int32 Port = BasePort;
	return ParseIPv4(Address, IPv4, Port) ? EncodeIPv4(IPv4, Port) : FString();
}

bool FALKLobbyCode::Decode(const FString& Code, FString& OutAddress)
{
	using namespace ALKLobbyCodePrivate;
	TArray<int32> Digits;
	if (!CodeToDigits(Code, Digits) || (Digits.Num() != ShortDataDigits + 1 && Digits.Num() != LongDataDigits + 1))
	{
		return false;
	}
	const int32 Check = Digits.Pop();
	if (CheckDigit(Digits) != Check)
	{
		return false;
	}

	uint64 Value = 0;
	for (const int32 Digit : Digits)
	{
		Value = (Value << 5) | static_cast<uint64>(Digit);
	}
	const uint64 Payload = Unscramble(Value, Digits.Num() * 5);

	uint32 Address = 0;
	int32 Port = 0;
	if (Digits.Num() == ShortDataDigits)
	{
		Address = static_cast<uint32>(Payload >> 8);
		Port = BasePort + static_cast<int32>(Payload & 0xFFull);
	}
	else
	{
		// The spare top bits are always zero in a real code.
		if ((Payload >> 48) != 0)
		{
			return false;
		}
		Address = static_cast<uint32>(Payload >> 16);
		Port = static_cast<int32>(Payload & 0xFFFFull);
	}
	if (Address == 0 || Port < 1)
	{
		return false;
	}
	OutAddress = MakeAddress(Address, Port);
	return true;
}

bool FALKLobbyCode::ResolveJoinTarget(const FString& CodeOrAddress, FString& OutAddress)
{
	const FString Text = CodeOrAddress.TrimStartAndEnd();
	if (Text.IsEmpty())
	{
		return false;
	}

	// A raw IPv4 address, with or without a port.
	uint32 IPv4 = 0;
	int32 Port = BasePort;
	if (ParseIPv4(Text, IPv4, Port))
	{
		OutAddress = ALKLobbyCodePrivate::MakeAddress(IPv4, Port);
		return true;
	}

	// A lobby code never has a dot or a colon in it; anything with one is a host name ("play.example.com:7777").
	FString Host = Text;
	FString PortText;
	const bool bHasPort = Text.Split(TEXT(":"), &Host, &PortText, ESearchCase::CaseSensitive);
	const bool bLocalHost = Host.Equals(TEXT("localhost"), ESearchCase::IgnoreCase);
	if (!bHasPort && !bLocalHost && !Host.Contains(TEXT(".")))
	{
		return Decode(Text, OutAddress);
	}

	if (bHasPort)
	{
		if (PortText.IsEmpty() || PortText.Len() > 5)
		{
			return false;
		}
		for (const TCHAR Char : PortText)
		{
			if (!FChar::IsDigit(Char))
			{
				return false;
			}
		}
		const int32 HostPort = FCString::Atoi(*PortText);
		if (HostPort < 1 || HostPort > 65535)
		{
			return false;
		}
	}
	if (!bLocalHost && !Host.Contains(TEXT(".")))
	{
		return false;
	}
	// Letters, digits, dots and dashes, with at least one letter (all digits would be a broken IPv4 address).
	bool bHasLetter = false;
	for (const TCHAR Char : Host)
	{
		const bool bLetter = (Char >= TEXT('A') && Char <= TEXT('Z')) || (Char >= TEXT('a') && Char <= TEXT('z'));
		bHasLetter |= bLetter;
		if (!bLetter && !FChar::IsDigit(Char) && Char != TEXT('.') && Char != TEXT('-'))
		{
			return false;
		}
	}
	if (!bHasLetter || Host.StartsWith(TEXT(".")) || Host.EndsWith(TEXT(".")) || Host.Contains(TEXT("..")))
	{
		return false;
	}
	OutAddress = Text;
	return true;
}

FString FALKLobbyCode::Normalize(const FString& Code)
{
	TArray<int32> Digits;
	if (!ALKLobbyCodePrivate::CodeToDigits(Code, Digits))
	{
		return Code.TrimStartAndEnd().ToUpper();
	}
	FString Result;
	for (int32 Index = 0; Index < Digits.Num(); ++Index)
	{
		if (Index > 0 && Index % 3 == 0)
		{
			Result.AppendChar(TEXT('-'));
		}
		Result.AppendChar(ALKLobbyCodePrivate::Alphabet[Digits[Index]]);
	}
	return Result;
}

FString FALKLobbyCode::Mask(const FString& Code)
{
	FString Result;
	Result.Reserve(Code.Len());
	for (const TCHAR Char : Code)
	{
		Result.AppendChar(Char == TEXT('-') || Char == TEXT(' ') ? Char : TEXT('*'));
	}
	return Result;
}

#undef LOCTEXT_NAMESPACE
