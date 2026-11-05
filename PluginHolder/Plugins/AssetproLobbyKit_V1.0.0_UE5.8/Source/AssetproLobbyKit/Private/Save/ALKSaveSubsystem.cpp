// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Save/ALKSaveSubsystem.h"
#include "Save/ALKSaveGame.h"
#include "AssetproLobbyKit.h"
#include "ALKDeveloperSettings.h"
#include "ALKMenuConfig.h"
#include "ALKMenuSubsystem.h"
#include "HUD/ALKHUDSubsystem.h"
#include "Settings/ALKSettingsSubsystem.h"
#include "Widgets/ALKDecor.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "UnrealClient.h"
#include "UObject/Package.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKSaveSubsystem)

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

namespace ALKSavePrivate
{
	/** Screenshot size kept with each save (16:9). */
	static constexpr int32 ThumbWidth = 480;
	static constexpr int32 ThumbHeight = 270;
	static constexpr int32 ThumbQuality = 85;
	/** Seconds to wait for a screenshot before saving without one. */
	static constexpr double CaptureTimeout = 1.0;
	/** Seconds to wait for the loaded level's player before giving up on putting them back in place. */
	static constexpr double LoadTimeout = 15.0;

	static FString GetIndexSlotName()
	{
		return UALKDeveloperSettings::GetMenuConfig()->SaveSlotPrefix + TEXT("_SaveIndex");
	}

	static FString GetWorldPackageName(const UWorld* World)
	{
		return World ? UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()) : FString();
	}
}

void UALKSaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadIndex();
	SessionTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::TickSession));
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostLoadMap);
}

void UALKSaveSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(SessionTicker);
	FTSTicker::GetCoreTicker().RemoveTicker(ApplyLoadTicker);
	SessionTicker.Reset();
	ApplyLoadTicker.Reset();
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	if (ScreenshotHandle.IsValid())
	{
		UGameViewportClient::OnScreenshotCaptured().Remove(ScreenshotHandle);
		ScreenshotHandle.Reset();
	}
	Super::Deinitialize();
}

bool UALKSaveSubsystem::IsEnabled() const
{
	return UALKDeveloperSettings::GetMenuConfig()->bEnableSaveSlots;
}

// ---------------------------------------------------------------- Saves

TArray<FALKSaveSlotInfo> UALKSaveSubsystem::GetSaves() const
{
	TArray<FALKSaveSlotInfo> Saves = Index ? Index->Slots : TArray<FALKSaveSlotInfo>();
	Saves.Sort([](const FALKSaveSlotInfo& A, const FALKSaveSlotInfo& B) { return A.SavedAt > B.SavedAt; });
	return Saves;
}

bool UALKSaveSubsystem::HasAnySave() const
{
	return IsEnabled() && Index && Index->Slots.Num() > 0;
}

bool UALKSaveSubsystem::GetNewestSave(FALKSaveSlotInfo& OutInfo) const
{
	const TArray<FALKSaveSlotInfo> Saves = GetSaves();
	if (Saves.Num() == 0)
	{
		return false;
	}
	OutInfo = Saves[0];
	return true;
}

bool UALKSaveSubsystem::FindSave(const FString& SlotName, FALKSaveSlotInfo& OutInfo) const
{
	const FALKSaveSlotInfo* Found = Index ? Index->Slots.FindByPredicate([&SlotName](const FALKSaveSlotInfo& Info) { return Info.SlotName == SlotName; }) : nullptr;
	if (Found)
	{
		OutInfo = *Found;
	}
	return Found != nullptr;
}

int32 UALKSaveSubsystem::GetFreeManualSlot() const
{
	const int32 Count = UALKDeveloperSettings::GetMenuConfig()->ManualSlots;
	for (int32 Number = 1; Number <= Count; ++Number)
	{
		FALKSaveSlotInfo Existing;
		if (!FindSave(MakeSlotName(EALKSaveSlotType::Manual, Number), Existing))
		{
			return Number;
		}
	}
	return INDEX_NONE;
}

int32 UALKSaveSubsystem::GetManualSaveCount() const
{
	int32 Count = 0;
	for (const FALKSaveSlotInfo& Info : Index ? Index->Slots : TArray<FALKSaveSlotInfo>())
	{
		Count += Info.Type == EALKSaveSlotType::Manual ? 1 : 0;
	}
	return Count;
}

UTexture2D* UALKSaveSubsystem::GetThumbnail(const FString& SlotName)
{
	if (const TObjectPtr<UTexture2D>* Cached = ThumbnailCache.Find(SlotName))
	{
		return *Cached;
	}
	const FALKSaveThumbnail* Thumb = Index ? Index->Thumbnails.FindByPredicate([&SlotName](const FALKSaveThumbnail& Each) { return Each.SlotName == SlotName; }) : nullptr;
	UTexture2D* Texture = Thumb && Thumb->Jpeg.Num() > 0 ? FImageUtils::ImportBufferAsTexture2D(Thumb->Jpeg) : nullptr;
	if (Texture)
	{
		ThumbnailCache.Add(SlotName, Texture);
	}
	return Texture;
}

UTexture2D* UALKSaveSubsystem::GetCurrentViewThumbnail()
{
	if (PendingThumbnail.Num() == 0)
	{
		return nullptr;
	}
	if (!CurrentViewTexture || CurrentViewTextureTime != PendingThumbnailTime)
	{
		CurrentViewTexture = FImageUtils::ImportBufferAsTexture2D(PendingThumbnail);
		CurrentViewTextureTime = PendingThumbnailTime;
	}
	return CurrentViewTexture;
}

FString UALKSaveSubsystem::MakeSlotName(EALKSaveSlotType Type, int32 Number)
{
	const FString& Prefix = UALKDeveloperSettings::GetMenuConfig()->SaveSlotPrefix;
	switch (Type)
	{
	case EALKSaveSlotType::Auto: return FString::Printf(TEXT("%s_Auto_%d"), *Prefix, Number);
	case EALKSaveSlotType::Quick: return FString::Printf(TEXT("%s_Quick"), *Prefix);
	default: return FString::Printf(TEXT("%s_Manual_%02d"), *Prefix, Number);
	}
}

// ---------------------------------------------------------------- Saving

bool UALKSaveSubsystem::SaveToManualSlot(int32 Number)
{
	if (Number < 1 || Number > UALKDeveloperSettings::GetMenuConfig()->ManualSlots)
	{
		UE_LOG(LogALK, Warning, TEXT("Save To Manual Slot: slot %d is outside 1..Manual Slots."), Number);
		return false;
	}
	return StartSave(EALKSaveSlotType::Manual, Number);
}

bool UALKSaveSubsystem::SaveToNewSlot()
{
	const int32 Number = GetFreeManualSlot();
	return Number != INDEX_NONE && StartSave(EALKSaveSlotType::Manual, Number);
}

bool UALKSaveSubsystem::Autosave()
{
	const int32 Slots = UALKDeveloperSettings::GetMenuConfig()->AutosaveSlots;
	if (Slots <= 0 || !Index)
	{
		return false;
	}
	return StartSave(EALKSaveSlotType::Auto, FMath::Clamp(Index->NextAutosave, 1, Slots));
}

bool UALKSaveSubsystem::QuickSave()
{
	if (!IsSavingAllowed())
	{
		ShowHUDNotice(LOCTEXT("CannotSaveTitle", "Cannot Save Now"), GetSavingBlockedReason(), false);
		return false;
	}
	return StartSave(EALKSaveSlotType::Quick, 1);
}

bool UALKSaveSubsystem::StartSave(EALKSaveSlotType Type, int32 Number)
{
	if (!IsSavingAllowed())
	{
		return false;
	}
	const UALKMenuConfig* Config = UALKDeveloperSettings::GetMenuConfig();
	UClass* SaveClass = Config->SaveGameClass ? Config->SaveGameClass.Get() : UALKSaveGame::StaticClass();
	UALKSaveGame* Save = NewObject<UALKSaveGame>(this, SaveClass);

	FALKSaveSlotInfo& Info = Save->Info;
	Info.SlotName = MakeSlotName(Type, Number);
	Info.Type = Type;
	Info.Number = Number;
	Info.ChapterId = CurrentChapter;
	if (const FALKChapter* Chapter = Config->FindChapter(CurrentChapter))
	{
		Info.ChapterName = Chapter->Name;
	}
	Info.LevelName = ALKSavePrivate::GetWorldPackageName(GetGameWorld());
	Info.Summary = SaveSummary;
	Info.PlaytimeSeconds = PlaytimeSeconds;
	Info.SavedAt = FDateTime::UtcNow();
	if (const UALKSettingsSubsystem* Settings = GetGameInstance()->GetSubsystem<UALKSettingsSubsystem>())
	{
		Info.Difficulty = Settings->GetDifficulty();
	}

	const APlayerController* PlayerController = GetFirstPlayerController();
	const APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	if (Config->bSavePlayerTransform && Pawn)
	{
		Save->PlayerTransform = Pawn->GetActorTransform();
		Save->ControlRotation = PlayerController->GetControlRotation();
		Save->bHasPlayerTransform = true;
	}

	OnGatherSaveData.Broadcast(Save);

	PendingSave = Save;
	SaveStartTime = FPlatformTime::Seconds();
	SetSavingIndicator(true);

	// Manual saves are made from the pause menu: use the view taken as it opened (the game behind the menu is dimmed and
	// blurred now). Quick saves and autosaves happen in play: take the view now.
	const UWorld* World = GetGameWorld();
	bUsePauseThumbnail = Type == EALKSaveSlotType::Manual && PendingThumbnail.Num() > 0 && World && World->IsPaused();
	if (bUsePauseThumbnail)
	{
		WritePendingSave();
		return true;
	}
	bWriteAfterCapture = true;
	CaptureThumbnail();
	if (!bCaptureRequested)
	{
		// No screenshot possible (no viewport, rendering off): save without one.
		WritePendingSave();
	}
	return true;
}

void UALKSaveSubsystem::CaptureThumbnail()
{
	if (bCaptureRequested || !FApp::CanEverRender() || !GEngine || !GEngine->GameViewport)
	{
		return;
	}
	bCaptureRequested = true;
	CaptureRequestTime = FPlatformTime::Seconds();
	// Bound only while waiting: while it is bound, the engine hands every screenshot to it instead of writing a file.
	ScreenshotHandle = UGameViewportClient::OnScreenshotCaptured().AddUObject(this, &ThisClass::HandleScreenshot);
	FScreenshotRequest::RequestScreenshot(false);
}

void UALKSaveSubsystem::HandleScreenshot(int32 Width, int32 Height, const TArray<FColor>& Colors)
{
	UGameViewportClient::OnScreenshotCaptured().Remove(ScreenshotHandle);
	ScreenshotHandle.Reset();
	bCaptureRequested = false;

	if (Width > 0 && Height > 0 && Colors.Num() == Width * Height)
	{
		// The middle 16:9 of the view, scaled down and kept as a JPEG.
		const int32 CropWidth = FMath::Min(Width, FMath::RoundToInt(Height * 16.f / 9.f));
		const int32 CropHeight = FMath::Min(Height, FMath::RoundToInt(CropWidth * 9.f / 16.f));
		const int32 X0 = (Width - CropWidth) / 2;
		const int32 Y0 = (Height - CropHeight) / 2;
		TArray<FColor> Cropped;
		Cropped.SetNumUninitialized(CropWidth * CropHeight);
		for (int32 Y = 0; Y < CropHeight; ++Y)
		{
			FMemory::Memcpy(&Cropped[Y * CropWidth], &Colors[(Y0 + Y) * Width + X0], CropWidth * sizeof(FColor));
		}
		TArray<FColor> Small;
		Small.SetNumUninitialized(ALKSavePrivate::ThumbWidth * ALKSavePrivate::ThumbHeight);
		FImageUtils::ImageResize(CropWidth, CropHeight, Cropped, ALKSavePrivate::ThumbWidth, ALKSavePrivate::ThumbHeight, Small, false, true);
		TArray64<uint8> Jpeg;
		if (FImageUtils::CompressImage(Jpeg, TEXT("jpg"), FImageView(Small.GetData(), ALKSavePrivate::ThumbWidth, ALKSavePrivate::ThumbHeight), ALKSavePrivate::ThumbQuality))
		{
			PendingThumbnail = TArray<uint8>(Jpeg.GetData(), static_cast<int32>(Jpeg.Num()));
			PendingThumbnailTime = FPlatformTime::Seconds();
		}
	}
	if (bWriteAfterCapture)
	{
		WritePendingSave();
	}
}

void UALKSaveSubsystem::WritePendingSave()
{
	bWriteAfterCapture = false;
	if (!PendingSave)
	{
		return;
	}
	const bool bFreshThumbnail = PendingThumbnail.Num() > 0 && (bUsePauseThumbnail || PendingThumbnailTime >= SaveStartTime);
	WrittenThumbnail = bFreshThumbnail ? PendingThumbnail : TArray<uint8>();
	PendingSave->Info.bHasThumbnail = WrittenThumbnail.Num() > 0;
	UGameplayStatics::AsyncSaveGameToSlot(PendingSave, PendingSave->Info.SlotName, 0,
		FAsyncSaveGameToSlotDelegate::CreateUObject(this, &ThisClass::HandleSaveWritten));
}

void UALKSaveSubsystem::HandleSaveWritten(const FString& SlotName, int32 UserIndex, bool bSuccess)
{
	UALKSaveGame* Save = PendingSave;
	PendingSave = nullptr;
	SetSavingIndicator(false);
	if (!Save)
	{
		return;
	}
	const FALKSaveSlotInfo Info = Save->Info;
	if (bSuccess && Index)
	{
		Index->Slots.RemoveAll([&SlotName](const FALKSaveSlotInfo& Each) { return Each.SlotName == SlotName; });
		Index->Slots.Add(Info);
		Index->Thumbnails.RemoveAll([&SlotName](const FALKSaveThumbnail& Each) { return Each.SlotName == SlotName; });
		if (WrittenThumbnail.Num() > 0)
		{
			FALKSaveThumbnail& Thumb = Index->Thumbnails.AddDefaulted_GetRef();
			Thumb.SlotName = SlotName;
			Thumb.Jpeg = WrittenThumbnail;
		}
		ThumbnailCache.Remove(SlotName);
		if (Info.Type == EALKSaveSlotType::Auto)
		{
			const int32 Slots = FMath::Max(1, UALKDeveloperSettings::GetMenuConfig()->AutosaveSlots);
			Index->NextAutosave = Info.Number % Slots + 1;
		}
		SaveIndex();
		NotifySavesChanged();
		if (Info.Type == EALKSaveSlotType::Quick)
		{
			ShowHUDNotice(LOCTEXT("GameSavedTitle", "Game Saved"), LOCTEXT("QuicksaveDetail", "Quicksave"), true);
		}
	}
	else
	{
		UE_LOG(LogALK, Warning, TEXT("Could not write the save '%s'."), *SlotName);
		ShowHUDNotice(LOCTEXT("SaveFailedTitle", "Save Failed"), LOCTEXT("SaveFailedDetail", "The game could not be saved."), false);
	}
	WrittenThumbnail.Reset();
	OnSaveFinished.Broadcast(Info, bSuccess);
}

bool UALKSaveSubsystem::DeleteSave(const FString& SlotName)
{
	if (!Index || !Index->Slots.ContainsByPredicate([&SlotName](const FALKSaveSlotInfo& Each) { return Each.SlotName == SlotName; }))
	{
		return false;
	}
	UGameplayStatics::DeleteGameInSlot(SlotName, 0);
	Index->Slots.RemoveAll([&SlotName](const FALKSaveSlotInfo& Each) { return Each.SlotName == SlotName; });
	Index->Thumbnails.RemoveAll([&SlotName](const FALKSaveThumbnail& Each) { return Each.SlotName == SlotName; });
	ThumbnailCache.Remove(SlotName);
	// With no autosave left, the next one is Autosave 1 again.
	if (!Index->Slots.ContainsByPredicate([](const FALKSaveSlotInfo& Each) { return Each.Type == EALKSaveSlotType::Auto; }))
	{
		Index->NextAutosave = 1;
	}
	SaveIndex();
	NotifySavesChanged();
	return true;
}

// ---------------------------------------------------------------- Loading

bool UALKSaveSubsystem::LoadSave(const FString& SlotName)
{
	if (!IsEnabled() || PendingSave || PendingLoad)
	{
		return false;
	}
	UALKSaveGame* Save = Cast<UALKSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
	if (!Save)
	{
		UE_LOG(LogALK, Warning, TEXT("Load Save: '%s' could not be read."), *SlotName);
		return false;
	}

	const UALKMenuConfig* Config = UALKDeveloperSettings::GetMenuConfig();
	FString Level = Save->Info.LevelName;
	if (Level.IsEmpty())
	{
		Level = Config->NewGameLevel.ToSoftObjectPath().GetLongPackageName();
	}
	APlayerController* PlayerController = GetFirstPlayerController();
	if (Level.IsEmpty() || !PlayerController)
	{
		UE_LOG(LogALK, Warning, TEXT("Load Save: '%s' has no level to open."), *SlotName);
		return false;
	}

	PendingLoad = Save;
	LoadedSave = Save;
	PlaytimeSeconds = Save->Info.PlaytimeSeconds;
	CurrentChapter = Save->Info.ChapterId;
	SaveSummary = Save->Info.Summary;
	LoadStartTime = FPlatformTime::Seconds();

	if (UALKMenuSubsystem* Menu = GetMenuSubsystem())
	{
		Menu->CloseAllMenus();
	}
	UGameplayStatics::OpenLevel(PlayerController, FName(*Level));
	return true;
}

bool UALKSaveSubsystem::LoadNewestSave()
{
	FALKSaveSlotInfo Newest;
	return GetNewestSave(Newest) && LoadSave(Newest.SlotName);
}

bool UALKSaveSubsystem::QuickLoad()
{
	const FString SlotName = MakeSlotName(EALKSaveSlotType::Quick, 1);
	FALKSaveSlotInfo Quick;
	if (!FindSave(SlotName, Quick))
	{
		ShowHUDNotice(LOCTEXT("NoQuicksaveTitle", "No Quicksave"), LOCTEXT("NoQuicksaveDetail", "Quick save first."), false);
		return false;
	}
	return LoadSave(SlotName);
}

void UALKSaveSubsystem::HandlePostLoadMap(UWorld* World)
{
	if (PendingLoad && World && World->IsGameWorld() && !ApplyLoadTicker.IsValid())
	{
		ApplyLoadTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::TickApplyLoad));
	}
}

bool UALKSaveSubsystem::TickApplyLoad(float DeltaTime)
{
	UALKSaveGame* Save = PendingLoad;
	if (!Save)
	{
		ApplyLoadTicker.Reset();
		return false;
	}
	APlayerController* PlayerController = GetFirstPlayerController();
	APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	const bool bTimedOut = FPlatformTime::Seconds() - LoadStartTime > ALKSavePrivate::LoadTimeout;
	if (!Pawn && !bTimedOut)
	{
		// The level's player is not in yet.
		return true;
	}
	if (Pawn && Save->bHasPlayerTransform && UALKDeveloperSettings::GetMenuConfig()->bSavePlayerTransform)
	{
		Pawn->SetActorLocationAndRotation(Save->PlayerTransform.GetLocation(), Save->PlayerTransform.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics);
		PlayerController->SetControlRotation(Save->ControlRotation);
	}
	PendingLoad = nullptr;
	ApplyLoadTicker.Reset();
	OnSaveLoaded.Broadcast(Save);
	return false;
}

// ---------------------------------------------------------------- When saving is allowed

void UALKSaveSubsystem::SetSavingAllowed(bool bAllowed, FText Reason)
{
	bSavingBlocked = !bAllowed;
	SavingBlockedReason = bAllowed ? FText::GetEmpty() : Reason;
	if (UALKMenuSubsystem* Menu = GetMenuSubsystem())
	{
		Menu->OnMenuEntriesChanged.Broadcast();
	}
}

bool UALKSaveSubsystem::IsSavingAllowed() const
{
	return IsEnabled() && !bSavingBlocked && IsInGame() && !PendingSave && !PendingLoad;
}

FText UALKSaveSubsystem::GetSavingBlockedReason() const
{
	if (!IsEnabled())
	{
		return LOCTEXT("SavingOff", "Saving is turned off.");
	}
	if (bSavingBlocked)
	{
		return SavingBlockedReason.IsEmpty() ? LOCTEXT("SavingBlocked", "You cannot save right now.") : SavingBlockedReason;
	}
	if (!IsInGame())
	{
		return LOCTEXT("SavingNoGame", "There is no game to save.");
	}
	if (PendingSave || PendingLoad)
	{
		return LOCTEXT("SavingBusy", "Please wait a moment.");
	}
	return FText::GetEmpty();
}

bool UALKSaveSubsystem::IsInGame() const
{
	const UWorld* World = GetGameWorld();
	if (!World || !World->IsGameWorld() || !GetFirstPlayerController())
	{
		return false;
	}
	const FString MainMenu = UALKDeveloperSettings::GetMenuConfig()->MainMenuLevel.ToSoftObjectPath().GetLongPackageName();
	return MainMenu.IsEmpty() || ALKSavePrivate::GetWorldPackageName(World) != MainMenu;
}

// ---------------------------------------------------------------- The game being played

void UALKSaveSubsystem::BeginNewGame(FName ChapterId)
{
	PlaytimeSeconds = 0.f;
	CurrentChapter = ChapterId;
	SaveSummary = FText::GetEmpty();
	LoadedSave = nullptr;
	PendingLoad = nullptr;
}

void UALKSaveSubsystem::SetCurrentChapter(FName ChapterId)
{
	CurrentChapter = ChapterId;
}

void UALKSaveSubsystem::SetSaveSummary(const FText& Summary)
{
	SaveSummary = Summary;
}

bool UALKSaveSubsystem::TickSession(float DeltaTime)
{
	// Playtime: in a gameplay level, not paused, not loading.
	const UWorld* World = GetGameWorld();
	if (!PendingLoad && World && !World->IsPaused() && IsInGame())
	{
		PlaytimeSeconds += DeltaTime;
	}

	// A screenshot that never came (minimised window...): save without it.
	if (bCaptureRequested && FPlatformTime::Seconds() - CaptureRequestTime > ALKSavePrivate::CaptureTimeout)
	{
		UGameViewportClient::OnScreenshotCaptured().Remove(ScreenshotHandle);
		ScreenshotHandle.Reset();
		bCaptureRequested = false;
		if (bWriteAfterCapture)
		{
			WritePendingSave();
		}
	}
	return true;
}

// ---------------------------------------------------------------- Index

void UALKSaveSubsystem::LoadIndex()
{
	const FString IndexName = ALKSavePrivate::GetIndexSlotName();
	Index = UGameplayStatics::DoesSaveGameExist(IndexName, 0) ? Cast<UALKSaveIndex>(UGameplayStatics::LoadGameFromSlot(IndexName, 0)) : nullptr;
	const bool bHadIndex = Index != nullptr;
	if (!Index)
	{
		Index = NewObject<UALKSaveIndex>(this);
	}

	// Saves deleted outside the game leave the list; saves without an entry (a lost index) join it.
	const int32 Before = Index->Slots.Num();
	Index->Slots.RemoveAll([](const FALKSaveSlotInfo& Each) { return !UGameplayStatics::DoesSaveGameExist(Each.SlotName, 0); });
	Index->Thumbnails.RemoveAll([this](const FALKSaveThumbnail& Each)
	{
		return !Index->Slots.ContainsByPredicate([&Each](const FALKSaveSlotInfo& Slot) { return Slot.SlotName == Each.SlotName; });
	});
	const int32 Kept = Index->Slots.Num();
	RebuildIndexFromFiles();
	if (Kept != Before || Index->Slots.Num() != Kept || (!bHadIndex && Index->Slots.Num() > 0))
	{
		SaveIndex();
	}
}

void UALKSaveSubsystem::RebuildIndexFromFiles()
{
	const UALKMenuConfig* Config = UALKDeveloperSettings::GetMenuConfig();
	TArray<FString> Names;
	for (int32 Number = 1; Number <= Config->ManualSlots; ++Number)
	{
		Names.Add(MakeSlotName(EALKSaveSlotType::Manual, Number));
	}
	for (int32 Number = 1; Number <= Config->AutosaveSlots; ++Number)
	{
		Names.Add(MakeSlotName(EALKSaveSlotType::Auto, Number));
	}
	Names.Add(MakeSlotName(EALKSaveSlotType::Quick, 1));
	for (const FString& Name : Names)
	{
		if (Index->Slots.ContainsByPredicate([&Name](const FALKSaveSlotInfo& Each) { return Each.SlotName == Name; }) || !UGameplayStatics::DoesSaveGameExist(Name, 0))
		{
			continue;
		}
		if (const UALKSaveGame* Save = Cast<UALKSaveGame>(UGameplayStatics::LoadGameFromSlot(Name, 0)))
		{
			FALKSaveSlotInfo Info = Save->Info;
			Info.SlotName = Name;
			Info.bHasThumbnail = false;
			Index->Slots.Add(Info);
		}
	}
}

void UALKSaveSubsystem::SaveIndex()
{
	if (Index)
	{
		UGameplayStatics::AsyncSaveGameToSlot(Index, ALKSavePrivate::GetIndexSlotName(), 0);
	}
}

void UALKSaveSubsystem::NotifySavesChanged()
{
	OnSavesChanged.Broadcast();
	if (UALKMenuSubsystem* Menu = GetMenuSubsystem())
	{
		Menu->OnMenuEntriesChanged.Broadcast();
	}
}

// ---------------------------------------------------------------- Helpers

UWorld* UALKSaveSubsystem::GetGameWorld() const
{
	return GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
}

APlayerController* UALKSaveSubsystem::GetFirstPlayerController() const
{
	return GetGameInstance() ? GetGameInstance()->GetFirstLocalPlayerController() : nullptr;
}

UALKMenuSubsystem* UALKSaveSubsystem::GetMenuSubsystem() const
{
	const ULocalPlayer* LocalPlayer = GetGameInstance() ? GetGameInstance()->GetFirstGamePlayer() : nullptr;
	return LocalPlayer ? LocalPlayer->GetSubsystem<UALKMenuSubsystem>() : nullptr;
}

void UALKSaveSubsystem::ShowHUDNotice(const FText& Title, const FText& Detail, bool bSaved) const
{
	const ULocalPlayer* LocalPlayer = GetGameInstance() ? GetGameInstance()->GetFirstGamePlayer() : nullptr;
	if (UALKHUDSubsystem* HUD = LocalPlayer ? LocalPlayer->GetSubsystem<UALKHUDSubsystem>() : nullptr)
	{
		HUD->ShowNotification(Title, Detail, bSaved ? EALKIcon::Save : EALKIcon::Alert, 3.f);
	}
}

void UALKSaveSubsystem::SetSavingIndicator(bool bShown) const
{
	const ULocalPlayer* LocalPlayer = GetGameInstance() ? GetGameInstance()->GetFirstGamePlayer() : nullptr;
	if (UALKHUDSubsystem* HUD = LocalPlayer ? LocalPlayer->GetSubsystem<UALKHUDSubsystem>() : nullptr)
	{
		if (bShown)
		{
			HUD->ShowSavingIndicator(1.5f, true);
		}
		else
		{
			HUD->HideSavingIndicator();
		}
	}
}

#undef LOCTEXT_NAMESPACE
