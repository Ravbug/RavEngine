
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
Ref<VirtualFilesystem> App::Resources;

SpinLock App::queue_lock;
queue<function<void(void)>> App::main_tasks;

int App::run(int argc, char** argv) {

	//invoke startup hook
	OnStartup(argc, argv);
	
	lastFrameTime = clocktype::now();

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
							Renderer->resize();
							break;
							
						case SDL_WINDOWEVENT_CLOSE:
							exit = true;
							break;
					}
				} break;
			}
			//process others
			inputManager->AggregateInput(event,windowflags,scale);
			
		}
		inputManager->Tick();
		GameplayStatics::currentWorld->Tick(scale);
		
		//process main thread tasks
		while (!main_tasks.empty()){
			auto& front = main_tasks.front();
			front();
			main_tasks.pop();
		}
		
		Renderer->Draw(GameplayStatics::currentWorld);

#ifdef LIMIT_TICK
		this_thread::sleep_for(tickrate);
#endif
		lastFrameTime = now;
	}

	//GameplayStatics::currentWorld = nullptr;
	
	//bgfx::shutdown();
	
	RAVENGINE_ATEXIT = true;
	
	atexit([](){
		cout << "atexit" << endl;
	});
	
    return OnShutdown();
}
