//
//  Entity.hpp
//  RavEngine_Static
//
//  Copyright © 2020 Ravbug. All rights reserved.
//

#pragma once
#include "SharedObject.hpp"
#include "Component.h"
#include "Transform.h"
#include "WeakRef.hpp"
#include "PhysicsBodyComponent.h" 
#include "ComponentStore.hpp"

class PhysicsBodyComponent;

typedef std::list<std::type_index> SystemsList;

class SystemRef;
class World;
/**
 This class defines an Entity for the Entity Component System.
 */
class Entity : public SharedObject{
protected:
	//store the components on this Entity
	ComponentStore components;
	
	//store what Systems will run on this Entity, in the order they will run
	SystemsList SystemsOrder;
    
    WeakRef<World> worldptr;  //non-owning
	
public:

	//required virtual destructor for SharedObject
    virtual ~Entity();

	/**
	Get a const reference to the components in this entity
	@return the components in this Entity
	*/
	ComponentStore& Components() {
		return components;
	}
   
    /**
     Get a pointer to the world that this entity is in. May be nullptr.
     @return the pointer to the world, or nullptr if there is no world
     */
    WeakRef<World> GetWorld() const{
        return worldptr;
    }
    
    /**
     Called by the world on Spawn and Destroy.
     @param world the pointer to the current world
     */
    void SetWorld(const WeakRef<World>& world){
        worldptr = world;
    }
    
    /**
     Check if this Entity has been spawned
     @return true if the entity is in the world, false otherwise
     @note Does not investigate dangling pointer
     */
    bool IsInWorld() const{
        return worldptr.isNull();
    }
	
	/**
	 Add a system to this entity.
	 */
	template<class ref>
	void AddSystem(){
        SystemsOrder.push_back(std::type_index(typeid(ref)));
    }
	
	/**
	 Remove a system from this entity.
	 */
	template<class ref>
	void RemoveSystem(){
        SystemsOrder.remove(std::type_index(typeid(ref)));
    }

	
	/**
	 Get the list of systems this entity needs to run, in the order that they must run in
	 @return constant reference to this Entity's systems order
	 */
	const SystemsList& GetSystemsOrder() const{
		return SystemsOrder;
	}

	/**
	Add a component to this entity. Use this call to ensure the component post-add hook gets invoked, and ownership is set correctly.
	@param componentRef the component to add
	*/
	template<class T>
	Ref<T> AddComponent(Ref<T> componentRef) {
		componentRef->SetOwner(this);
		componentRef->AddHook(this);
		return components.AddComponent<T>(componentRef);
	}
	
	/**
	 Create an Entity. This constructor will add the components and their default values to the Entity.
	 The constructor is also responsible for setting the SystemsOrder
	 */
	Entity();

	/**
	Called by a PhysicsBodyComponent when it has collided with another. Override in subclasses.
	@param other the other component 
	*/
	virtual void OnColliderEnter(const WeakRef<PhysicsBodyComponent>& other) {}

	/**
	Called by a PhysicsBodyComponent when it has exited collision with another. Override in subclasses.
	@param other the other component
	*/
	virtual void OnColliderExit(const WeakRef<PhysicsBodyComponent>& other) {}

	/**
	Called by a PhysicsBodyComponent when it has collided with another and the collision has persisted. Override in subclasses.
	@param other the other component
	*/
	virtual void OnColliderPersist(const WeakRef<PhysicsBodyComponent>& other) {}

	/**
	Called by the world when the entity is spawned.
	*/
	virtual void Start() {}

	/**
	Called by the world when the entity has been despawned, but before despawn work has begun. It may not be deallocated immediately after depending on reference counts. However it is best to stop or destroy anything possible.
	*/
	virtual void Stop(){}
	
    /**
     @return a reference to the transform component, which all entities possess
     */
	Ref<Transform> transform();
	
	/**
	 Tick logic on this Entity
	 @param timeScale the scale calculated by the World
	 @note this may be called on any thread, so ensure resource access is thread-safe
	 */
	virtual void Tick(float timeScale){};

	/**
	Render this entity
	*/
	virtual void Draw();
	
};
