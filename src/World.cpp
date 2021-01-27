//
//  World.cpp
//  RavEngine_Static
//
//  Copyright © 2020 Ravbug.
//

#include "World.hpp"
#include <iostream>
#include <algorithm>
#include "System.hpp"
#include "ScriptComponent.hpp"
#include <future>
#include "App.hpp"
#include "PhysicsLinkSystem.hpp"
#include "GUI.hpp"
#include "InputManager.hpp"
#include "ChildEntityComponent.hpp"
#include "AudioSyncSystem.hpp"

using namespace std;
using namespace RavEngine;

void RavEngine::World::Tick(float scale) {
	
    pretick(scale);
	
	//Tick the game code
	TickECS(scale);

    posttick(scale);
}


RavEngine::World::World(){
	//reserve space to reduce rehashing
	Entities.reserve(4000);
	systemManager.RegisterSystem<ScriptSystem>(make_shared<ScriptSystem>());
	systemManager.RegisterSystem<AudioSyncSystem>(make_shared<AudioSyncSystem>(synthesizer));
}

/**
 Spawn an entity immediately
 @param e the entity to spawn
 @return true if the spawn succeeded, false if it failed
 */
bool RavEngine::World::Spawn(Ref<Entity> e){
	//cannot spawn an entity that is already in a world
	if (e->GetWorld().expired()){
		Entities.insert(e);
		e->Sync();	//ensure all components have their owner backpointers up-to-date
		e->SetWorld(shared_from_this());

		//start all scripts
		e->Start();
		auto coms = e->GetAllComponentsOfTypeFastPath<ScriptComponent>();
		for (const auto c : coms) {
			std::static_pointer_cast<ScriptComponent>(c)->Start();
		}

		//make the physics system aware of this entity
		Solver.Spawn(e);
		synthesizer.Spawn(e);

		//merge the entity into the world
		Merge(*e.get());

		e->parent = shared_from_this();	//set parent so that this entity synchronizes its components with this world

		//get all child entities
		auto children = e->GetAllComponentsOfTypeFastPath<ChildEntityComponent>();
		for(const auto c : children){
			Spawn(std::static_pointer_cast<ChildEntityComponent>(c)->get());	//spawn the child entities
		}
		return true;
	}
	return false;
}

/**
 Destroy an entity immediately
 @param e the entity to destroy
 @return true if destruction succeeded, false otherwise
 */
bool RavEngine::World::Destroy(Ref<Entity> e){
	//if entity is somehow not spawned, do nothing
	if (e->GetWorld().expired()){
		return false;
	}
	
	//stop all scripts
	auto coms = e->GetAllComponentsOfTypeFastPath<ScriptComponent>();
	for (const auto c : coms) {
		std::static_pointer_cast<ScriptComponent>(c)->Stop();
	}
	e->Stop();

	e->SetWorld(nullptr);

	//also remove its components
	Unmerge(*e.get());

	e->parent.reset();	//set parent to null so that this entity no longer synchronizes its components with this world

	//remove the objects from the Physics system
	Solver.Destroy(e);
	synthesizer.Destroy(e);
	Entities.erase(e);
	
	//get all child entities
	auto children = e->GetAllComponentsOfTypeFastPath<ChildEntityComponent>();
	for(const auto c : children){
		Destroy(std::static_pointer_cast<ChildEntityComponent>(c)->get());
	}
	return true;
}

/**
 Tick all of the objects in the world, multithreaded
 @param fpsScale the scale factor to apply to all operations based on the frame rate
 */
void RavEngine::World::TickECS(float fpsScale) {
	struct systaskpair{
		tf::Task task1;
		Ref<System> system;
	};
	
	phmap::flat_hash_map<ctti_t, systaskpair> graphs;

	size_t count = 0;
	for (auto& s : systemManager.GetInternalStorage()) {
		auto system = s.second;

		auto& queries = system->QueryTypes();
		count += queries.size();
	}
	phmap::flat_hash_map<ctti_t, ComponentStore::entry_type> copies(count);
	
    //tick the systems
	for (auto& s : systemManager.GetInternalStorage()) {
		auto system = s.second;
		
		auto& queries = system->QueryTypes();
		
		auto func = [=](Ref<Component> e) {
			Ref<Entity> en = e->getOwner().lock();
			if (en) {
				system->Tick(fpsScale, en);
			}
		};
		
		for (const auto& query : queries) {
			//add the Task to the hashmap
			
			//Avoid repeating the same query copies multiple times via hashmap
			if (!copies.contains(query)) {
				copies[query] = GetAllComponentsOfTypeIndexFastPath(query);
			}

			auto& l1 = copies[query];

			graphs[system->ID()] = {
				masterTasks.for_each(l1.begin(), l1.end(), func),
				system };
		}
	}
	
	
	if (physicsActive){
		//add the PhysX tick, must run after write but before read
		auto RunPhysics = masterTasks.emplace([fpsScale, this]{
			Solver.Tick(fpsScale);
		});
		RunPhysics.precede(graphs[CTTI<PhysicsLinkSystemRead>].task1);
		RunPhysics.succeed(graphs[CTTI<PhysicsLinkSystemWrite>].task1);
	}
	
	//add the AudioSystem tick
	masterTasks.emplace([fpsScale, this]{
		synthesizer.Tick(fpsScale);
	});
	
	//figure out dependencies
	for(auto& graph : graphs){
		tf::Task& task1 = graph.second.task1;
		
		//call precede
		{
			auto& runbefore = graph.second.system->MustRunBefore();
			for(const auto id : runbefore){
				if (graphs.contains(id)){
					task1.precede(graphs[id].task1);
				}
			}
		}
		//call succeed
		{
			auto& runafter = graph.second.system->MustRunAfter();
			for(const auto id : runafter){
				if (graphs.contains(id)){
					task1.succeed(graphs[id].task1);
				}
			}
		}
	}
	
	//execute and wait
	App::executor.run(masterTasks).wait();
	masterTasks.clear();
}

bool RavEngine::World::InitPhysics() {
	if (physicsActive){
		return false;
	}
	
	systemManager.RegisterSystem<PhysicsLinkSystemRead>(make_shared<PhysicsLinkSystemRead>(Solver.scene));
	systemManager.RegisterSystem<PhysicsLinkSystemWrite>(make_shared<PhysicsLinkSystemWrite>(Solver.scene));
	
	physicsActive = true;

	return true;
}
