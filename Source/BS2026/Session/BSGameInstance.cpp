// Copyright Epic Games, Inc. All Rights Reserved.

#include "BSGameInstance.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Engine/World.h"
#include "BS2026.h"

UBSGameInstance::UBSGameInstance()
{
}

void UBSGameInstance::Init()
{
	Super::Init();
}

void UBSGameInstance::HostMatch(int32 MaxPlayers)
{
	IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
	if (!OnlineSub)
	{
		UE_LOG(LogBS2026, Warning, TEXT("HostMatch: No online subsystem found."));
		return;
	}

	IOnlineSessionPtr Sessions = OnlineSub->GetSessionInterface();
	if (!Sessions.IsValid())
	{
		return;
	}

	PendingMaxPlayers = MaxPlayers;

	// Destroy any existing session first; mark intent as rehost so
	// OnDestroySessionComplete recreates the session instead of traveling to lobby.
	if (Sessions->GetNamedSession(SessionName))
	{
		bPendingRehost = true;
		Sessions->OnDestroySessionCompleteDelegates.AddUObject(
			this, &UBSGameInstance::OnDestroySessionComplete);
		Sessions->DestroySession(SessionName);
		return;
	}

	FOnlineSessionSettings Settings;
	Settings.bIsLANMatch            = (OnlineSub->GetSubsystemName() == TEXT("NULL"));
	Settings.NumPublicConnections   = MaxPlayers;
	Settings.bAllowJoinInProgress   = true;
	Settings.bAllowJoinViaPresence  = true;
	Settings.bShouldAdvertise       = true;
	Settings.bUsesPresence          = true;
	Settings.bUseLobbiesIfAvailable = true;

	Sessions->OnCreateSessionCompleteDelegates.AddUObject(
		this, &UBSGameInstance::OnCreateSessionComplete);
	Sessions->CreateSession(0, SessionName, Settings);
}

void UBSGameInstance::FindAndJoinMatch()
{
	IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
	if (!OnlineSub)
	{
		OnMatchFound.Broadcast(false);
		return;
	}

	IOnlineSessionPtr Sessions = OnlineSub->GetSessionInterface();
	if (!Sessions.IsValid())
	{
		OnMatchFound.Broadcast(false);
		return;
	}

	SessionSearch = MakeShareable(new FOnlineSessionSearch());
	SessionSearch->MaxSearchResults = 20;
	SessionSearch->bIsLanQuery = (OnlineSub->GetSubsystemName() == TEXT("NULL"));
	SessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);

	Sessions->OnFindSessionsCompleteDelegates.AddUObject(
		this, &UBSGameInstance::OnFindSessionsComplete);
	Sessions->FindSessions(0, SessionSearch.ToSharedRef());
}

void UBSGameInstance::LeaveMatch()
{
	IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
	if (!OnlineSub)
	{
		return;
	}

	IOnlineSessionPtr Sessions = OnlineSub->GetSessionInterface();
	if (Sessions.IsValid() && Sessions->GetNamedSession(SessionName))
	{
		bPendingRehost = false;
		Sessions->OnDestroySessionCompleteDelegates.AddUObject(
			this, &UBSGameInstance::OnDestroySessionComplete);
		Sessions->DestroySession(SessionName);
	}
	else
	{
		// No session to destroy — just travel back to the lobby
		UWorld* World = GetWorld();
		if (!World)
		{
			return;
		}

		if (World->GetNetMode() == NM_Client)
		{
			if (APlayerController* PlayerController = World->GetFirstPlayerController())
			{
				PlayerController->ClientTravel(LobbyMapPath, TRAVEL_Absolute);
			}
		}
		else
		{
			World->ServerTravel(LobbyMapPath, true);
		}
	}
}

// ── Callbacks ─────────────────────────────────────────────────────────────────

void UBSGameInstance::OnCreateSessionComplete(FName InSessionName, bool bWasSuccessful)
{
	IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
	if (OnlineSub)
	{
		IOnlineSessionPtr Sessions = OnlineSub->GetSessionInterface();
		if (Sessions.IsValid())
		{
			Sessions->ClearOnCreateSessionCompleteDelegates(this);
		}
	}

	if (bWasSuccessful)
	{
		// Travel to the combat map and open as a listen server
		GetWorld()->ServerTravel(CombatMapPath + TEXT("?listen"), true);
	}
	else
	{
		UE_LOG(LogBS2026, Warning, TEXT("OnCreateSessionComplete: session creation failed."));
	}
}

void UBSGameInstance::OnFindSessionsComplete(bool bWasSuccessful)
{
	IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
	if (OnlineSub)
	{
		IOnlineSessionPtr Sessions = OnlineSub->GetSessionInterface();
		if (Sessions.IsValid())
		{
			Sessions->ClearOnFindSessionsCompleteDelegates(this);
		}
	}

	if (!bWasSuccessful || !SessionSearch.IsValid() || SessionSearch->SearchResults.Num() == 0)
	{
		OnMatchFound.Broadcast(false);
		return;
	}

	IOnlineSessionPtr Sessions =
		IOnlineSubsystem::Get()->GetSessionInterface();
	if (!Sessions.IsValid())
	{
		OnMatchFound.Broadcast(false);
		return;
	}

	Sessions->OnJoinSessionCompleteDelegates.AddUObject(
		this, &UBSGameInstance::OnJoinSessionComplete);
	Sessions->JoinSession(0, SessionName, SessionSearch->SearchResults[0]);
}

void UBSGameInstance::OnJoinSessionComplete(FName InSessionName,
                                            EOnJoinSessionCompleteResult::Type Result)
{
	IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
	if (!OnlineSub)
	{
		OnMatchFound.Broadcast(false);
		return;
	}

	IOnlineSessionPtr Sessions = OnlineSub->GetSessionInterface();
	if (!Sessions.IsValid())
	{
		OnMatchFound.Broadcast(false);
		return;
	}

	Sessions->ClearOnJoinSessionCompleteDelegates(this);

	if (Result == EOnJoinSessionCompleteResult::Success)
	{
		// Resolve the platform-specific connection URL and travel to it
		FString TravelURL;
		if (APlayerController* PC = GetFirstLocalPlayerController())
		{
			if (Sessions->GetResolvedConnectString(InSessionName, TravelURL))
			{
				PC->ClientTravel(TravelURL, TRAVEL_Absolute);
				OnMatchFound.Broadcast(true);
				return;
			}
		}
	}

	OnMatchFound.Broadcast(false);
}

void UBSGameInstance::OnDestroySessionComplete(FName InSessionName, bool bWasSuccessful)
{
	IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
	if (OnlineSub)
	{
		IOnlineSessionPtr Sessions = OnlineSub->GetSessionInterface();
		if (Sessions.IsValid())
		{
			Sessions->ClearOnDestroySessionCompleteDelegates(this);
		}
	}

	if (!bWasSuccessful)
	{
		UE_LOG(LogBS2026, Warning, TEXT("OnDestroySessionComplete: failed to destroy session '%s'."),
		       *InSessionName.ToString());
		return;
	}

	if (bPendingRehost)
	{
		// HostMatch() initiated this destroy — recreate the session and travel to the
		// combat map once creation succeeds (same path as a fresh HostMatch call).
		bPendingRehost = false;

		IOnlineSubsystem* RehostSub = IOnlineSubsystem::Get();
		if (!RehostSub)
		{
			UE_LOG(LogBS2026, Warning, TEXT("OnDestroySessionComplete: no online subsystem for rehost."));
			return;
		}

		IOnlineSessionPtr RehostSessions = RehostSub->GetSessionInterface();
		if (!RehostSessions.IsValid())
		{
			return;
		}

		FOnlineSessionSettings Settings;
		Settings.bIsLANMatch            = (RehostSub->GetSubsystemName() == TEXT("NULL"));
		Settings.NumPublicConnections   = PendingMaxPlayers;
		Settings.bAllowJoinInProgress   = true;
		Settings.bAllowJoinViaPresence  = true;
		Settings.bShouldAdvertise       = true;
		Settings.bUsesPresence          = true;
		Settings.bUseLobbiesIfAvailable = true;

		RehostSessions->OnCreateSessionCompleteDelegates.AddUObject(
			this, &UBSGameInstance::OnCreateSessionComplete);
		RehostSessions->CreateSession(0, SessionName, Settings);
		return;
	}

	// LeaveMatch() path — travel back to the lobby using the appropriate travel
	// function for this net role (clients cannot call ServerTravel).
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (World->GetNetMode() == NM_Client)
	{
		if (APlayerController* PlayerController = World->GetFirstPlayerController())
		{
			PlayerController->ClientTravel(LobbyMapPath, TRAVEL_Absolute);
		}
	}
	else
	{
		World->ServerTravel(LobbyMapPath, true);
	}
}
