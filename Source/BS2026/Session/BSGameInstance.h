// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "BSGameInstance.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBSMatchFound, bool, bSuccess);

/**
 *  Game Instance for vehicular combat sessions.
 *
 *  Wraps the Online Subsystem session interface to provide three operations:
 *
 *    HostMatch(MaxPlayers)   — Creates a session and travels the server to the
 *                              combat map.  Works with the Null subsystem (PIE
 *                              multi-client) and EOS without code changes.
 *
 *    FindAndJoinMatch()      — Searches for an open session and joins the first
 *                              result.  Broadcasts OnMatchFound on completion.
 *
 *    LeaveMatch()            — Destroys the current session and travels the
 *                              client back to the lobby map.
 *
 *  Local testing (no EOS credentials):
 *    DefaultEngine.ini sets DefaultPlatformService=Null, so every call goes
 *    through the Null subsystem which emulates LAN sessions within the editor.
 */
UCLASS()
class UBSGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:

	UBSGameInstance();

	// ── Configuration ─────────────────────────────────────────────────

	/** Map to load when hosting or joining a match */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Session")
	FString CombatMapPath = TEXT("/Game/BS2026/Maps/CombatMap");

	/** Map to travel to after leaving a match */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Session")
	FString LobbyMapPath = TEXT("/Game/BS2026/Maps/LobbyMap");

	/** Name used to advertise and find sessions */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Session")
	FName SessionName = FName("BSCombatSession");

	// ── Public API ────────────────────────────────────────────────────

	/**
	 *  Creates a session with MaxPlayers slots and server-travels to CombatMapPath.
	 *  Safe to call from Blueprint (e.g. from a "Host" button).
	 */
	UFUNCTION(BlueprintCallable, Category="Session")
	void HostMatch(int32 MaxPlayers = 4);

	/**
	 *  Searches for an open session.  Broadcasts OnMatchFound(true/false) when done.
	 */
	UFUNCTION(BlueprintCallable, Category="Session")
	void FindAndJoinMatch();

	/**
	 *  Destroys the current session and travels to LobbyMapPath.
	 */
	UFUNCTION(BlueprintCallable, Category="Session")
	void LeaveMatch();

	/** Fired when a session search completes */
	UPROPERTY(BlueprintAssignable, Category="Session")
	FOnBSMatchFound OnMatchFound;

protected:

	virtual void Init() override;

private:

	// ── Online subsystem callbacks ────────────────────────────────────

	void OnCreateSessionComplete(FName InSessionName, bool bWasSuccessful);
	void OnFindSessionsComplete(bool bWasSuccessful);
	void OnJoinSessionComplete(FName InSessionName, EOnJoinSessionCompleteResult::Type Result);
	void OnDestroySessionComplete(FName InSessionName, bool bWasSuccessful);

	/** Active session search object */
	TSharedPtr<FOnlineSessionSearch> SessionSearch;

	/** Cached max-player count for the session being created */
	int32 PendingMaxPlayers = 4;

	/** True when a destroy was triggered by HostMatch() (rehost), false when triggered by LeaveMatch() */
	bool bPendingRehost = false;
};
