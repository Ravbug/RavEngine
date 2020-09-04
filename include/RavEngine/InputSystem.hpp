#pragma once
#include "SharedObject.hpp"
#include <SDL_scancode.h>
#include <SDL_mouse.h>
#include <SDL_gamecontroller.h>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <iostream>
#include <functional>
#include <typeindex>
#include "IInputAction.hpp"

enum class ActionState{
  Released, Pressed
};

struct Special {
    enum {
        MOUSEMOVE_X = -500,
        MOUSEMOVE_Y = -501,
        MOUSEMOVE_XVEL = -502,
        MOUSEMOVE_YVEL = -503,
        CONTROLLER_AXIS_OFFSET = -8000,
        CONTROLLER_BUTTON_OFFSET = -10000
    };
};

//use this when binding controller buttons
struct ControllerButton {
    enum {
        SDL_CONTROLLER_BUTTON_INVALID = -1,
        SDL_CONTROLLER_BUTTON_A = Special::CONTROLLER_BUTTON_OFFSET,
        SDL_CONTROLLER_BUTTON_B,
        SDL_CONTROLLER_BUTTON_X,
        SDL_CONTROLLER_BUTTON_Y,
        SDL_CONTROLLER_BUTTON_BACK,
        SDL_CONTROLLER_BUTTON_GUIDE,
        SDL_CONTROLLER_BUTTON_START,
        SDL_CONTROLLER_BUTTON_LEFTSTICK,
        SDL_CONTROLLER_BUTTON_RIGHTSTICK,
        SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
        SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
        SDL_CONTROLLER_BUTTON_DPAD_UP,
        SDL_CONTROLLER_BUTTON_DPAD_DOWN,
        SDL_CONTROLLER_BUTTON_DPAD_LEFT,
        SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
        SDL_CONTROLLER_BUTTON_MAX
    };
};

//use this when binding controller axis
struct ControllerAxis {
    enum {
        SDL_CONTROLLER_AXIS_INVALID = -1,
        SDL_CONTROLLER_AXIS_LEFTX = Special::CONTROLLER_AXIS_OFFSET,
        SDL_CONTROLLER_AXIS_LEFTY,
        SDL_CONTROLLER_AXIS_RIGHTX,
        SDL_CONTROLLER_AXIS_RIGHTY,
        SDL_CONTROLLER_AXIS_TRIGGERLEFT,
        SDL_CONTROLLER_AXIS_TRIGGERRIGHT,
        SDL_CONTROLLER_AXIS_MAX
    };
};


struct Event{
    int ID;
    ActionState value;
    friend std::ostream& operator<<(std::ostream& os, const Event& dt){
        os << "EVT id = " << dt.ID << " value = " << (dt.value == ActionState::Released ? "Released" : "Pressed");
        return os;
    }
};

typedef std::function<void(float)> axisCallback;
typedef std::function<void()> actionCallback;

namespace RavEngine {
    class InputSystem : public SharedObject
    {
    protected:
        std::list<Event> actionValues;
        std::unordered_set<int> awareActionValues;
        std::unordered_map<int, std::list<std::string>> codeToAction;
        std::unordered_map<std::string, std::list<std::pair<std::pair<actionCallback, IInputListener*>, ActionState>>> actionMappings;

        //axis storage
        std::unordered_map<int, float> axisValues;                      //ids to current values
        std::unordered_map<int, float> axisScalars;                     //ids to scalars
        std::unordered_map<int, std::list<std::string>> codeToAxis;                //ids to strings
        std::unordered_map<std::string, std::list<std::pair<axisCallback, IInputListener*>>> axisMappings;     //strings to methods

        /**
         Helper used for registering axis inputs inside the engine
         */
        void reg_axis(int code, float value) {
            if (axisScalars.find(code) != axisScalars.end()) {
                axisValues[code] = value;
            }
        }

        std::unordered_set<SDL_GameController*> connectedControllers;

    public:
        template<typename T, typename U>
        class AxisCallback {
        public:
            AxisCallback(T* thisptr, void(U::* f)(float)){
                func = std::bind(f, thisptr,std::placeholders::_1);
            }

            void operator()(float val) {
                func(val);
            }
        private:
            std::function<void(float)> func;
        };

       /* template<typename T>
        void Register(AxisCallback<IInputListener, T>* a) {
            templatedList.push_back(a);
        }
        template<typename T>
        std::list < AxisCallback<IInputListener, T>> templatedList;*/

        InputSystem();

        void InitGameControllers();

        //based on the state of inputs, invoke bound actions
        virtual void tick();

        //methods to get input values
        void SDL_key(bool state, int charcode);
        void SDL_mousemove(float x, float y, int xvel, int yvel);
        void SDL_mousekey(bool state, int charcode);
        void SDL_ControllerAxis(int axisID, float value);

        void AddActionMap(const std::string& name, int Id);
        void AddAxisMap(const std::string& name, int Id, float scale = 1);
        void RemoveActionMap(const std::string& name, int Id);
        void RemoveAxisMap(const std::string& name, int Id);

		/**
		 * Bind an action map to a member function
		 * @param name the name of the action map to bind to
		 * @param thisptr the object to bind to. Use `this` if within the class you want to bind to
		 * @param f the method to invoke when the action is triggered. Must take no parameters. Use &Classname::Methodname.
		 * @param type the required state of the action to invoke the method.
		 */
        template<typename U>
		void BindAction(const std::string& name, IInputListener* thisptr, void(U::* f)(), ActionState type){
			actionCallback callback = std::bind(f, thisptr, std::placeholders::_1);
			
			//need to store the original thisptr so that it can be identified later
			actionMappings[name].push_back(std::make_pair(std::make_pair(callback, thisptr), type));
		}

        /**
         Bind a function to an Axis mapping
		 @param name the Axis mapping to bind to
		 @param thisptr the object to bind to. Use `this` if within the class you want to bind to
		 @param f the method to invoke when the action is triggered. Must take one float parameter. Use &Classname::Methodname.
         */
        template<typename U>
        void BindAxis(const std::string& name, IInputListener* thisptr, void(U::* f)(float)) {
            axisCallback callback = std::bind(f, thisptr, std::placeholders::_1);

            //need to store the original thisptr so that it can be identified later
            axisMappings[name].push_back(std::make_pair(callback, thisptr));
        }

		/**
		 Bind a function to an Action mapping
		 @param name the Action mapping to unbind
		 @param thisptr the object to bind to. Use `this` if within the class you want to bind to
		 @param f the method to invoke when the action is triggered. Must take no parameters. Use &Classname::Methodname.
		 @param state the state to use to match the callback
		 */
		template<typename U>
		void UnbindAction(const std::string& name, IInputListener* thisptr, void(U::* f)(float),ActionState type){
			actionCallback callback = std::bind(f, thisptr, std::placeholders::_1);
			
			actionMappings[name].remove(std::make_pair(std::make_pair(callback, thisptr), type));
		}
		
		
		/**
		 Unbind an Axis mapping
		 @param thisptr the object to bind to. Use `this` if within the class you want to bind to
		 @param f the method to invoke when the action is triggered. Must take one float parameter. Use &Classname::Methodname.
		 */
		template<typename U>
		void UnbindAxis(const std::string& name, IInputListener* thisptr, void(U::* f)(float)){
			axisCallback callback = std::bind(f, thisptr, std::placeholders::_1);
			
			axisMappings[name].remove(std::make_pair(callback, thisptr));
		}

		/**
		 * Unbind all Action and Axis mappings for a given listener. Listeners automatically invoke this on destruction.
		 * @param act the listener to unbind
		 */
        void UnbindAllFor(IInputListener* act);

        virtual ~InputSystem();

    };
}
