#pragma once
#include "System.hpp"
#include "CTTI.hpp"
#include "QueryIterator.hpp"
#include "AudioRoom.hpp"

namespace RavEngine{

class AudioRoomSyncSystem : public AutoCTTI {
public:
	void Tick(float fpsScale, AccessReadWrite<AudioRoom> c);
	
	constexpr QueryIteratorAND<AudioRoom> QueryTypes() const {
		return QueryIteratorAND<AudioRoom>();
	}
};

}
