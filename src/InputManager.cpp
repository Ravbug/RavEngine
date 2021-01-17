
#include "InputManager.hpp"
#include <SDL_events.h>
#include <SDL.h>
#include <RenderEngine.hpp>
#include "Debug.hpp"

using namespace std;
using namespace RavEngine;


InputManager::InputManager() {
	//register all the controllers
	SDL_GameControllerEventState(SDL_ENABLE);
}

void InputManager::ProcessActionID(int id, ActionState state_in, CID controller){
	if (CodeToAction.contains(id)){
		//get the actions that need to be run
		for(const auto& actionName : CodeToAction[id]){
			//for each action, get the bindings to execute
			for(const auto& binding : ActionBindings[actionName]){
				binding(state_in, controller);	//execute the binding
			}
		}
	}
	//process the Any actions
	for(auto l : AnyEventBindings){
		Ref<IInputListener> listener = l.lock();
		if (listener){
			if (state_in){
				listener->AnyActionDown(id);
			}
			else{
				listener->AnyActionUp(id);
			}
		}
	}
	
}

void InputManager::ProcessAxisID(int ID, float value, CID controller){
	if (CodeToAxis.contains(ID)){
		//buffer the input
		for(const auto& axisName : CodeToAxis[ID]){
			auto scale = axisName.scale;
			AxisBindings[axisName.identifier].bufferedInputs.push_back({value * scale,controller});
		}
	}
}

void InputManager::TickAxes(){
	
	for(auto& binding : AxisBindings){
		//if there were no recorded inputs, add a 0, because axes must be called at least once per update
		if (binding.second.bufferedInputs.size() == 0){
			binding.second.bufferedInputs.push_back({0,CID::ANY});
		}
		
		//get each binding
		for(const auto& axis : binding.second.bindings){
			//pass each buffered value to each Action
			for(const auto& buffered_value : binding.second.bufferedInputs){
				axis(buffered_value.value, buffered_value.source_controller);
			}
		}
		//clear buffered axis inputs
		binding.second.bufferedInputs.clear();
	}
	
	CleanupBindings();
}

void InputManager::CleanupBindings(){
	
	//clean up invalid action bindings
	for(auto& binding : ActionBindings){
		binding.second.remove_if([](const ActionBinding& b) -> bool{
			return !b.IsValid();
		});
	}
	
	//clean up invalid axis bindings
	for(auto& binding : AxisBindings){
		binding.second.bindings.remove_if([](const AxisBinding& b) -> bool{
			return !b.IsValid();
		});
	}
	
	//clean up invalid Any Actions
	AnyEventBindings.remove_if([](const WeakPtrKey<IInputListener>& w) -> bool{
		return w.getWeak().expired();
	});
}

void InputManager::ProcessInput(const SDL_Event& event, uint32_t windowflags, float scale){
	switch (event.type) {
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			ProcessActionID(event.key.keysym.scancode, static_cast<ActionState>(event.key.state), Make_CID(0));
			ProcessAxisID(event.key.keysym.scancode, event.key.state, Make_CID(0));
			break;
		case SDL_MOUSEMOTION:
			if (windowflags & SDL_WINDOW_INPUT_FOCUS) {
				int width, height;
				SDL_GetWindowSize(RenderEngine::GetWindow(), &width, &height);
				
				float velscale = 1 / scale;
				
				ProcessAxisID(Special::MOUSEMOVE_X, (float)event.motion.x / width, CID::C0);
				ProcessAxisID(Special::MOUSEMOVE_Y, (float)event.motion.y / height, CID::C0);
				
				ProcessAxisID(Special::MOUSEMOVE_XVEL, event.motion.xrel * velscale, CID::C0);
				ProcessAxisID(Special::MOUSEMOVE_YVEL, event.motion.yrel * velscale, CID::C0);
			}
			break;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			if (windowflags & SDL_WINDOW_INPUT_FOCUS) {
				ProcessActionID(event.button.button, static_cast<ActionState>(event.button.state), CID::C0);
			}
			break;
		case SDL_CONTROLLERAXISMOTION:
		case SDL_CONTROLLER_AXIS_LEFTX:
		case SDL_CONTROLLER_AXIS_LEFTY:
			ProcessAxisID(event.caxis.axis + Special::CONTROLLER_AXIS_OFFSET, event.caxis.value, Make_CID(event.cdevice.which + 1));
			break;
		case SDL_CONTROLLERBUTTONDOWN:
		case SDL_CONTROLLERBUTTONUP:
			ProcessActionID(event.cbutton.button + Special::CONTROLLER_BUTTON_OFFSET, static_cast<ActionState>(event.cbutton.state), Make_CID(event.cdevice.which + 1));
			break;
		case SDL_CONTROLLERDEVICEADDED:
		case SDL_CONTROLLERDEVICEREMOVED:
			break;
	}
}

void RavEngine::InputManager::SetRelativeMouseMode(bool mode){
	SDL_SetRelativeMouseMode(mode ? SDL_TRUE : SDL_FALSE);
}

bool InputManager::GetRelativeMouseMode(){
	return SDL_GetRelativeMouseMode();
}

