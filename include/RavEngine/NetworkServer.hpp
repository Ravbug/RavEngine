#pragma once
#include <cstdint>
#include "NetworkBase.hpp"
#include <steam/isteamnetworkingsockets.h>
#include "DataStructures.hpp"
#include "Ref.hpp"
#include <uuids.h>
#include <phmap.h>
#include <functional>

namespace RavEngine {
	class Entity;
	class SyncVar_base;

class NetworkServer : public NetworkBase{
public:
	NetworkServer();
	void Start(uint16_t port);
	void Stop();
	~NetworkServer();	//calls stop
	static void SteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t*);

	void SpawnEntity(Ref<Entity> e);
	void DestroyEntity(Ref<Entity> e);

	void SendMessageToAllClients(const std::string_view& msg, Reliability mode) const;

	void SendMessageToClient(const std::string_view& msg, HSteamNetConnection connection, Reliability mode) const;

	void SendMessageToAllClientsExcept(const std::string_view& msg, HSteamNetConnection connection, Reliability mode) const;

	void OnRPC(const std::string_view& cmd, HSteamNetConnection);

	/**
	Change the ownership of a networked object
	@param newOwner the connection handle for the new owner. Use Invalid to make the server owner
	@param object the networkidentity to udpate the ownership of
	*/
	void ChangeOwnership(HSteamNetConnection newOwner, Ref<NetworkIdentity> object);
	
	void ChangeSyncVarOwnership(HSteamNetConnection newOwner, SyncVar_base& var);
	
	//attach event listeners here
	std::function<void(HSteamNetConnection)> OnClientConnecting, OnClientConnected, OnClientDisconnected;
	
protected:
	ISteamNetworkingSockets *net_interface = nullptr;
	HSteamListenSocket listenSocket = k_HSteamListenSocket_Invalid;
	HSteamNetPollGroup pollGroup = k_HSteamNetPollGroup_Invalid;
	void OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t*);
	
	phmap::flat_hash_set<HSteamNetConnection> clients;

	// since the server owns all objects by default, we only need to update this
	// when the ownership changes to a non-server client. If an object is not in this
	// structure, we assume the server owns it.
	phmap::flat_hash_map<HSteamNetConnection, phmap::flat_hash_set<WeakPtrKey<NetworkIdentity>>> OwnershipTracker;

	// invoked automatically when a client disconnects. We destroy the entities it owns.
	void HandleDisconnect(HSteamNetConnection);
	
	static NetworkServer* currentServer;
	
	void ServerTick();

	//invoked when clients request to have their worlds synchronized
	void SynchronizeWorldToClient(HSteamNetConnection connection, const std::string_view& in_message);
};

}
