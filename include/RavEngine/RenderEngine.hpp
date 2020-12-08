//
//  RenderEngine.hpp
//  RavEngine_Static
//
//  Copyright © 2020 Ravbug.
//

#pragma once
#include "SharedObject.hpp"
#include "Entity.hpp"
#include "WeakRef.hpp"
#include "Uniform.hpp"
#include <bgfx/bgfx.h>

struct SDL_Window;

namespace RavEngine {
    class SDLSurface;
    class DeferredBlitShader;

    class RenderEngine : public SharedObject {
    public:
        virtual ~RenderEngine();
        RenderEngine();
        void Draw(Ref<World>);

        static const std::string currentBackend();

		static SDL_Window* const GetWindow(){
			return window;
		}

        void resize();
		
		static struct vs {
			int width = 960; int height = 540;
			bool vsync = true;
		} VideoSettings;
		
    protected:
		static SDL_Window* window;
        static void Init();
		static uint32_t GetResetFlags();
		
		static constexpr uint8_t gbufferSize = 4;
		
		bgfx::TextureHandle attachments[gbufferSize];
			//RGBA (A not used in opaque)
			//normal vectors
			//xyz of pixel
			//depth texture
			//lighting texture
		bgfx::UniformHandle gBufferSamplers[gbufferSize];

		bgfx::FrameBufferHandle gBuffer;	//full gbuffer
        
        bgfx::VertexBufferHandle screenSpaceQuadVert = BGFX_INVALID_HANDLE;
        bgfx::IndexBufferHandle screenSpaceQuadInd = BGFX_INVALID_HANDLE;
        
        Ref<DeferredBlitShader> blitShader;
		
		bgfx::FrameBufferHandle createFrameBuffer(bool, bool);
    };
}
