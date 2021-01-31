#include "Component.hpp"
#include "Queryable.hpp"
#include <vector>
#include <string>

namespace RavEngine{

class AudioAsset{
	friend class AudioEngine;
	friend class AudioSyncSystem;
	friend class AudioSourceComponent;
private:
	const float* audiodata;
	double lengthSeconds = 0;
	size_t numsamples = 0;
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
	size_t playhead_pos = 0;
	bool loops = false;
	bool isPlaying = false;
	
public:
	AudioSourceComponent(Ref<AudioAsset> a ) : asset(a){}
	
	/**
	 Starts playing the audio source if it is not playing. Call Pause() to suspend it.
	 */
	inline void Play(){
		isPlaying = true;
	}
	
	/**
	 Stop the source if it is playing. Call Play() to resume.
	 */
	inline void Pause(){
		isPlaying = false;
	}
	
	/**
	 Reset the audio playhead to the beginning of this source. This does not trigger it to begin playing.
	 */
	inline void Restart(){
		playhead_pos = 0;
	}
	
	inline float GetVolume() const { return volume; }
	
	/**
	 Change the volume for this source
	 @param vol new volume for this source.
	 */
	inline void SetVolume(float vol){volume = vol;}
	
	/**
	 Enable or disable looping for this audio source. A looping source will continuously play until manually stopped, whereas
	 non-looping sources will automatically deactivate when finished
	 @param loop new loop setting
	 */
	inline void SetLoop(bool loop) {loops = loop;}
	
	/**
	 @return true if the source is currently playing, false otherwise
	 */
	inline bool IsPlaying() const { return isPlaying; }

	/**
	 Generate an audio data buffer based on the current source
	 @param buffer destination for the data
	 @param count the size of the buffer, in bytes
	 */
	inline void GetSampleRegionAndAdvance(float* buffer, size_t count){
		for(size_t i = 0; i < count/sizeof(buffer[0]); i++){
			//is playhead past end of source?
			if (playhead_pos >= asset->numsamples){
				if (loops){
					playhead_pos = 0;
				}
				else{
					buffer[i] = 0;
					isPlaying = false;
					continue;
				}
			}
			buffer[i] = asset->audiodata[playhead_pos] * volume;
			playhead_pos++;
		}
	}
};

}
