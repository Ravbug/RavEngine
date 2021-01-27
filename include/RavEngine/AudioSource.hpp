#include <api/resonance_audio_api.h>
#include "Component.hpp"
#include "Queryable.hpp"
#include <vector>

namespace RavEngine{

class AudioAsset{
	friend class AudioEngine;
	friend class AudioSyncSystem;
	friend class AudioSourceComponent;
private:
	const float* audiodata[1];
	double lengthSeconds = 0;
	size_t frameSize = 0;
public:
	AudioAsset(const std::string& name);
	~AudioAsset();
	
	inline size_t GetFrameSize() const {return frameSize;}
	inline double getLength() const {return lengthSeconds;}
};

/**
 This is a marker component to indicate where the "microphone" is in the world. Do not have more than one in a world.
 */
class AudioListener : public Component, public Queryable<AudioListener>{};

/**
 Represents a single audio source. To represent multiple sources, simply attach multiple of this component type to your Entity.
 */
class AudioSourceComponent : public Component, public Queryable<AudioSourceComponent>{
protected:
	friend class AudioEngine;
	friend class AudioSyncSystem;
	Ref<AudioAsset> asset;
	float volume = 1;
	double playhead_pos = 0;
	bool loops = false;
	bool isPlaying = false;
	float playbackSpeed = 1;
	
	vraudio::ResonanceAudioApi::SourceId resonance_handle = vraudio::ResonanceAudioApi::kInvalidSourceId;
public:
	AudioSourceComponent(Ref<AudioAsset> a ) : asset(a){}
	
	inline void Play(){
		isPlaying = true;
	}
	
	inline void Pause(){
		isPlaying = false;
	}
	
	inline void Restart(){
		playhead_pos = 0;
	}
	
	void Tick(float scale);
	
	inline const float* GetPointerOffset() const{
		//return playhead_pos * 44100;	//TODO: get audio asset's sample  rate
		return asset->audiodata[0] + (uint32_t)(playhead_pos * 22050);
	}
};

}
