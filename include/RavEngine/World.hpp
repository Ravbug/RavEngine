//
//  World.hpp
//  RavEngine_Static
//
//  Copyright © 2020 Ravbug.
//

#pragma once
#include "SharedObject.hpp"
#include "System.hpp"
#include "PhysicsSolver.hpp"
#include "PhysicsLinkSystem.hpp"
#include "RenderEngine.hpp"
#include "Locked_Hashmap.hpp"
#include "SystemManager.hpp"
#include "SpinLock.hpp"
#include <type_traits>
#include "ScriptSystem.hpp"
#include <taskflow/taskflow.hpp>
#include <concurrentqueue.h>
#include "SpinLock.hpp"

namespace RavEngine {
	class Entity;
	class InputManager;
	typedef phmap::parallel_flat_hash_set<Ref<Entity>> EntityStore;

	class World : public ComponentStore<SpinLock> {
	protected:
		//Entity list
		EntityStore Entities;
		moodycamel::ConcurrentQueue<Ref<Entity>> PendingSpawn;
		moodycamel::ConcurrentQueue<Ref<Entity>> PendingDestruction;

		//physics system
		Ref<PhysicsSolver> Solver = new PhysicsSolver();

		/**
		Called before ticking components and entities synchronously
		 @param fpsScale the scale factor calculated
		 */
		virtual void pretick(float fpsScale) {}
		virtual void midtick(float fpsScale) {}
		void TickECS(float);
		/**
		 Called after physics and rendering synchronously
		 @param fpsScale the scale factor calculated
		 */
		virtual void posttick(float fpsScale) {}
        
        /**
         * Tick a System on all available threads. Blocks until all have finished.
         * @param system the System to tick
         * @param scale the frame rate scale to pass on to the System's tick method
         */
        void TickSystem(Ref<System> system, float scale);
		
		tf::Taskflow masterTasks;
		
		bool physicsActive = false;
	public:
		SystemManager systemManager;
		
		/**
		* Initializes the physics-related Systems.
		* @return true if the systems were loaded, false if they were not loaded because they are already loaded
		*/
		bool InitPhysics();
		
		/**
		* Evaluate the world given a scale factor. One tick = 1/App::EvalNormal
		* @param the tick fraction to evaluate
		* @note the GameplayStatics CurrentWorld is ticked automatically in the App
		*/
		void Tick(float);

		World();

		//spawn function (takes an Entity)
		bool Spawn(Ref<Entity>);

		bool Destroy(Ref<Entity>);

		inline const EntityStore& getEntities() const {
			return Entities;
		}

		/**
		 Called by GameplayStatics when the final world is being deallocated
		 */
		inline void DeallocatePhysics() {
			Solver->DeallocatePhysx();
		}

		virtual ~World() {
			std::cout << "world destructor @ " << this << std::endl;
		}
	};
}
