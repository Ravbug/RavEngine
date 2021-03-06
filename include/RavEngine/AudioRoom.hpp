#pragma once
#include <api/resonance_audio_api.h>
#include "mathtypes.hpp"
#include "Ref.hpp"
#include "ComponentStore.hpp"
#include "SpinLock.hpp"
#include "Queryable.hpp"
#include "AudioRoomMaterial.hpp"
#include "AudioSource.hpp"
#include "DebugDraw.hpp"

namespace RavEngine{

class AudioRoomSyncSystem;
class AudioPlayer;

using RoomMat = vraudio::MaterialName;

/**
 Renders audio buffers based on its owning world's state
 */
class AudioRoom : public Component, public Queryable<AudioRoom>{
	friend class RavEngine::AudioRoomSyncSystem;
	friend class RavEngine::AudioPlayer;
public:
	static constexpr uint16_t NFRAMES = 4096;
protected:
	vraudio::ResonanceAudioApi* audioEngine = nullptr;
	vraudio::ResonanceAudioApi::SourceId src = vraudio::ResonanceAudioApi::kInvalidSourceId;

	vector3 roomDimensions = vector3(0,0,0);	//size of 0 = infinite
	
	// Material name of each surface of the shoebox room in this order:
	// [0] (-)ive x-axis wall (left)
	// [1] (+)ive x-axis wall (right)
	// [2] (-)ive y-axis wall (bottom)
	// [3] (+)ive y-axis wall (top)
	// [4] (-)ive z-axis wall (front)
	// [5] (+)ive z-axis wall (back)
	std::array<RoomMat, 6> wallMaterials{
		RoomMat::kTransparent,
		RoomMat::kTransparent,
		RoomMat::kTransparent,
		RoomMat::kTransparent,
		RoomMat::kTransparent,
		RoomMat::kTransparent
	};
	
	float reflection_scalar = 1, reverb_gain = 1, reverb_time = 1.0, reverb_brightness = 0;
	
	void SimulateSingle(float* ptr, size_t nbytes, AudioPlayerData*, const vector3& pos, const quaternion& rot);
	
public:
	
	AudioRoom(){
		audioEngine = vraudio::CreateResonanceAudioApi(2, NFRAMES, 44100);
		src = audioEngine->CreateSoundObjectSource(vraudio::RenderingMode::kBinauralLowQuality);
	}
	~AudioRoom(){
		delete audioEngine;
		src = vraudio::ResonanceAudioApi::kInvalidSourceId;
	}
	
	/**
	 Set the dimensions of the room. A dimension of 0 is interpreted as unbounded.
	 @param dim the x, y, and z dimensions of the room. Unlike Physics, this is diameter not radius.
	 */
	void SetRoomDimensions(const vector3& dim){ roomDimensions = dim; }
	
	/**
	 Update the position of the listener in the Audio Engine
	 @param worldpos the position of the listener in world space
	 @param worldrotation the rotation of the listener in world space
	 */
	void SetListenerTransform(const vector3& worldpos, const quaternion& worldrotation);
	
	/**
	 Simulate spacial audio for a set of audio sources
	 @param ptr destination for the calculated audio
	 @param nbytes length of the buffer in bytes
	 @param sources the AudioSource components to calculate for
	 */
	void Simulate(float* ptr, size_t nbytes, const ComponentStore<phmap::NullMutex>::entry_type& sources);
	
	/**
	 @return the dimensions of this room
	 */
	decltype(roomDimensions) GetRoomDimensions() const {return roomDimensions; }
	
	/**
	 @return a writable reference to the wall materials
	 */
	decltype(wallMaterials)& WallMaterials() { return wallMaterials; }
	
	/**
	 Render the debug shape for this room. Invoke in a debug rendering component
	 */
	void DrawDebug(DebugDraw&);
	
//	void SetRoomMaterial(const RoomMaterial& properties){
//		audioEngine->SetReverbProperties(static_cast<RoomMaterial>(properties));
//	}
};

}
