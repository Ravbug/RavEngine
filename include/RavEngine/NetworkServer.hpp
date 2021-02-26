#pragma once
#include <cstdint>
#include <steam/isteamnetworkingsockets.h>

namespace RavEngine {

class NetworkServer{
public:
	NetworkServer();
	void Start(uint16_t port);
	void Stop();
	~NetworkServer();	//calls stop
	static void SteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t*);
protected:
	ISteamNetworkingSockets *interface;
	HSteamListenSocket listenSocket;
	HSteamNetPollGroup pollGroup;
	void OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t*) ;
};

}
