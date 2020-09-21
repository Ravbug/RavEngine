
#include "RavEngine_App.hpp"

#include <GameplayStatics.hpp>
#include "RenderEngine.hpp"
#include <SDL_events.h>
#include <bgfx/bgfx.h>
#include "AppEnd.h"
#include <algorithm>

using namespace std;
using namespace RavEngine;
using namespace std::chrono;

const float RavEngine::App::evalNormal = 60;

int App::run(int argc, char** argv) {

	//invoke startup hook
	OnStartup(argc, argv);

	int width = 0; int height = 0;
	auto& inputManager = GameplayStatics::inputManager;
    
	bool exit = false;
	SDL_Event event;
	while (!exit) {
		//setup framerate scaling for next frame
		auto now = clocktype::now();
		//will cause engine to run in slow motion if the frame rate is <= 1fps
		deltaTimeMicroseconds = std::min(duration_cast<timeDiff>((now - lastFrameTime)), maxTimeStep);
		float deltaSeconds = deltaTimeMicroseconds.count() / 1000.0 / 1000;
		float scale = deltaSeconds * evalNormal;

		auto windowflags = SDL_GetWindowFlags(RenderEngine::GetWindow());
		while (SDL_PollEvent(&event)) {

			switch (event.type) {
			case SDL_QUIT:
				exit = true;
				break;

			case SDL_WINDOWEVENT: {
				const SDL_WindowEvent& wev = event.window;
				switch (wev.event) {
				case SDL_WINDOWEVENT_RESIZED:
				case SDL_WINDOWEVENT_SIZE_CHANGED:
					SDL_GetWindowSize(RenderEngine::GetWindow(), &width, &height);
					GameplayStatics::currentWorld->resize();
					break;

				case SDL_WINDOWEVENT_CLOSE:
					exit = true;
					break;
				}
			} break;

			case SDL_KEYDOWN:
			case SDL_KEYUP:
				inputManager->SDL_key(event.key.state, event.key.keysym.scancode);
				break;
			case SDL_MOUSEMOTION:
				if (windowflags & SDL_WINDOW_INPUT_FOCUS) {
					inputManager->SDL_mousemove((float)event.motion.x / width, (float)event.motion.y / height, event.motion.xrel, event.motion.yrel, scale);
				}
				break;
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
				if (windowflags & SDL_WINDOW_INPUT_FOCUS) {
					inputManager->SDL_mousekey(event.button.state, event.button.button);
				}
				break;
			case SDL_CONTROLLERAXISMOTION:
			case SDL_CONTROLLER_AXIS_LEFTX:
			case SDL_CONTROLLER_AXIS_LEFTY:
				inputManager->SDL_ControllerAxis(event.caxis.axis + Special::CONTROLLER_AXIS_OFFSET, (event.caxis.value) / ((float)SHRT_MAX));
				break;
			case SDL_CONTROLLERBUTTONDOWN:
			case SDL_CONTROLLERBUTTONUP:
				inputManager->SDL_mousekey(event.cbutton.state + Special::CONTROLLER_BUTTON_OFFSET, event.cbutton.button);
				break;
			case SDL_CONTROLLERDEVICEADDED:
			case SDL_CONTROLLERDEVICEREMOVED:
				break;
			}
		}
		inputManager->Tick();
		GameplayStatics::currentWorld->Tick(scale);

#ifdef LIMIT_TICK
		this_thread::sleep_for(tickrate);
#endif
		lastFrameTime = now;
	}

	bgfx::shutdown();
	
	atexit([](){
		RAVENGINE_ATEXIT = true;
		cout << "atexit" << endl;
	});
	
    return OnShutdown();
}
