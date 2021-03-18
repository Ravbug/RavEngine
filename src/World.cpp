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
#include "App.hpp"
#include "PhysicsLinkSystem.hpp"
#include "GUI.hpp"
#include "InputManager.hpp"
#include "ChildEntityComponent.hpp"
#include "AudioRoomSyncSystem.hpp"
#include "CameraComponent.hpp"
#include "StaticMesh.hpp"
#include "BuiltinMaterials.hpp"
#include "NetworkIdentity.hpp"

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
	systemManager.RegisterSystem<AudioRoomSyncSystem>(make_shared<AudioRoomSyncSystem>());
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

		//start entity
		e->Start();

		//merge the entity into the world
		Merge(*e.get());

		e->parent = shared_from_this();	//set parent so that this entity synchronizes its components with this world

		//get all child entities
		auto children = e->GetAllComponentsOfType<ChildEntityComponent>();
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
	
	e->SetWorld(nullptr);
	e->Stop();

	//also remove its components
	Unmerge(*e.get());

	e->parent.reset();	//set parent to null so that this entity no longer synchronizes its components with this world

	Entities.erase(e);
	
	//get all child entities
	auto children = e->GetAllComponentsOfType<ChildEntityComponent>();
	for(const auto c : children){
		Destroy(std::static_pointer_cast<ChildEntityComponent>(c)->get());
	}
	return true;
}

void World::OnAddComponent(Ref<Component> comp){
	//is this a script? if so, call its start
	{
		auto scr = dynamic_pointer_cast<ScriptComponent>(comp);
		if (scr){
			scr->Start();
			return;
		}
	}
	//is this a physics body? if so, call physics simulator to create it
	{
		auto phys = dynamic_pointer_cast<PhysicsBodyComponent>(comp);
		auto parent = comp->getOwner().lock();
		if (phys && parent){
			Solver.Spawn(parent);
			return;
		}
	}
	//is this a NetworkIdentity? if so, call Add on the NetworkManager
	{
		auto nid = dynamic_pointer_cast<NetworkIdentity>(comp);
		if (nid && nid->triggerMessage) {
            //only the server may spawn objects
            App::networkManager.Spawn(shared_from_this(), nid);
            return;
		}
	}
}

void World::OnRemoveComponent(Ref<Component> comp){
	//is this a script? if so, call its stop
	{
		auto scr = dynamic_pointer_cast<ScriptComponent>(comp);
		if (scr){
			scr->Stop();
			return;
		}
	}
	//is this a physics body? if so, call physics simulator to stop it
	{
		auto phys = dynamic_pointer_cast<PhysicsBodyComponent>(comp);
		auto parent = comp->getOwner().lock();
		if (phys && parent){
			Solver.Destroy(parent);
			return;
		}
	}
	//is this a NetworkIdentity? if so, call destroy on the NetworkManager
	{
		auto nid = dynamic_pointer_cast<NetworkIdentity>(comp);
		if (nid && nid->triggerMessage) {
            //ownership is checked serverside to decide if this should be honored
			App::networkManager.Destroy(shared_from_this(), nid);
            return;
		}
	}
}

/**
 Tick all of the objects in the world, multithreaded
 @param fpsScale the scale factor to apply to all operations based on the frame rate
 */
void RavEngine::World::TickECS(float fpsScale) {
	struct systaskpair{
		tf::Task task;
		const SystemEntry* system;
	};
	
	phmap::flat_hash_map<ctti_t, systaskpair> graphs;

	size_t count = 0;
	for (auto& s : systemManager.GetAlwaysTickSystems()) {
		count += s.second.QueryTypes().size();
	}
	phmap::flat_hash_map<ctti_t, ComponentStore::entry_type> copies(count);
	
	auto add_system_to_tick = [&](const SystemEntry& system, ctti_t ID){
		auto& queries = system.QueryTypes();
		
		auto func = [=](Ref<Component> e) {
			Ref<Entity> en = e->getOwner().lock();
			if (en) {
				system.Tick(fpsScale, en);
			}
		};
		
		for (const auto& query : queries) {
			//add the Task to the hashmap
			
			//Avoid repeating the same query copies multiple times via hashmap
			if (!copies.contains(query)) {
				copies[query] = GetAllComponentsOfTypeIndexFastPath(query);
			}
			
			auto& l1 = copies[query];
			
			graphs[ID] = {
				masterTasks.for_each(l1.begin(), l1.end(), func),
				&system
			};
		}
	};
	
    //tick the always-systems
	for (auto& s : systemManager.GetAlwaysTickSystems()) {
		add_system_to_tick(s.second, s.first);
	}
	
	//tick timed systems
	auto now = SystemManager::clock_t::now();
	for (auto& s : systemManager.GetTimedTickSystems()){
		if (now - s.second.last_timestamp > s.second.interval){
			add_system_to_tick(s.second.system,s.first);
			s.second.last_timestamp = now;
		}
	}
	
	ComponentStore::entry_type geometry;
	
	if (isRendering)
	{
		//render engine data collector
		//camera matrices
		auto camproc = masterTasks.emplace([this](){
			auto allcams = GetAllComponentsOfType<CameraComponent>();
			for (const auto& c : allcams) {
				auto cam = std::static_pointer_cast<CameraComponent>(c);
				if (cam->isActive()) {
					
					auto size = App::Renderer->GetBufferSize();
					cam->SetTargetSize(size.width, size.height);
					current->viewmatrix = cam->GenerateViewMatrix();
					current->projmatrix = cam->GenerateProjectionMatrix();
					
					break;
				}
			}
		});
		
		//opaque geometry
		geometry = GetAllComponentsOfType<StaticMesh>();
			
		//sort into the hashmap
		auto sort = masterTasks.for_each(geometry.begin(), geometry.end(), [this](Ref<Component> e){
			if (e){
				auto m = static_pointer_cast<StaticMesh>(e);
				auto ptr = e->getOwner().lock();
				if (ptr){
					auto pair = make_pair(m->getMesh(), m->GetMaterial());
					auto mat = ptr->transform()->CalculateWorldMatrix();
					auto& item = current->opaques[pair];
					item.mtx.lock();
					item.items.insert(mat);
					item.mtx.unlock();
				}
			}
		});
		
		auto copydirs = masterTasks.emplace([this](){
			auto dirs = GetAllComponentsOfType<DirectionalLight>();
			for(const auto& e : dirs){
				if (e){
					auto owner = e->getOwner().lock();
					if (owner){
						auto d = static_pointer_cast<DirectionalLight>(e);
						auto rot = owner->transform()->Up();
						FrameData::PackedDL::tinyvec3 r{
							static_cast<float>(rot.x),
							static_cast<float>(rot.y),
							static_cast<float>(rot.z)
						};
						current->directionals.emplace(*d.get(),r);
					}
				}
			}
		});
		auto copyambs = masterTasks.emplace([this](){
			auto ambs = GetAllComponentsOfType<AmbientLight>();
			for(const auto& e : ambs){
				if (e){
					auto d = static_pointer_cast<AmbientLight>(e);
					current->ambients.emplace(*d.get());
				}
			}
		});
		auto copyspots = masterTasks.emplace([this](){
			auto spots = GetAllComponentsOfType<SpotLight>();
			for(const auto& e : spots){
				if (e){
					auto d = static_pointer_cast<SpotLight>(e);
					auto ptr = e->getOwner().lock();
					if (ptr){
						auto transform = ptr->transform()->CalculateWorldMatrix();
						current->spots.emplace(*d.get(),d->CalculateMatrix(transform));
					}
				}
			}
		});
		auto copypoints = masterTasks.emplace([this](){
			auto points = GetAllComponentsOfType<PointLight>();
			for(const auto& e : points){
				if (e){
					auto d = static_pointer_cast<PointLight>(e);
					auto ptr = e->getOwner().lock();
					if (ptr){
						auto transform = ptr->transform()->CalculateWorldMatrix();
						current->points.emplace(*d.get(),d->CalculateMatrix(transform));
					}
				}
			}
		});
		

		auto swap = masterTasks.emplace([this]{
			SwapFrameData();
		});
		auto setup = masterTasks.emplace([this]{
			current->Clear();
		});
		setup.precede(camproc,copydirs,copyambs,copyspots,copypoints);
		sort.precede(swap);
		camproc.precede(sort);
		graphs[CTTI<ScriptSystem>].task.precede(camproc,copydirs,copyambs,copyspots,copypoints);
		swap.succeed(camproc,copydirs,copyambs,copyspots,copypoints);
	}

	if (physicsActive){
		//add the PhysX tick, must run after write but before read
		auto RunPhysics = masterTasks.emplace([fpsScale, this]{
			Solver.Tick(fpsScale);
		});
		RunPhysics.precede(graphs[CTTI<PhysicsLinkSystemRead>].task);
		RunPhysics.succeed(graphs[CTTI<PhysicsLinkSystemWrite>].task);
	}
	
	//figure out dependencies
	for(auto& graph : graphs){
		tf::Task& task = graph.second.task;
		
		//call precede
		{
			auto& runbefore = graph.second.system->MustRunBefore();
			for(const auto id : runbefore){
				if (graphs.find(id) != graphs.end()){
					task.precede(graphs[id].task);
				}
			}
		}
		//call succeed
		{
			auto& runafter = graph.second.system->MustRunAfter();
			for(const auto id : runafter){
				if (graphs.find(id) != graphs.end()){
					task.succeed(graphs[id].task);
				}
			}
		}
	}
	
	//execute and wait
	App::executor.run(masterTasks).wait();
	newFrame = true;	//TODO: only if rendering is enabled on this world
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
