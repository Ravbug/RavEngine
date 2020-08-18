#pragma once
#include "Component.hpp"
#include "PhysXDefines.h"
#include <PxMaterial.h>
#include <PxPhysics.h>
#include <PxSimulationEventCallback.h>
#include <iostream>
#include "mathtypes.hpp"
#include "PhysicsMaterial.hpp"

namespace physx {
	class PxShape;
}

namespace RavEngine {
	class Entity;
	class PhysicsCollider : public Component
	{
	protected:
		physx::PxShape* collider = nullptr;
	public:
		enum class CollisionType { Trigger, Collider };

		bool eventsEnabled;
		CollisionType Type;

		virtual ~PhysicsCollider();
	};


	class BoxCollider : public PhysicsCollider {
	protected:
		vector3 extent;
		Ref<PhysicsMaterial> material;
	public:

		virtual ~BoxCollider() {}
		BoxCollider() : PhysicsCollider() { RegisterAllAlternateTypes(); };

		/**
		 * Create a box collider with an extent and a physics material
		 * @param ext the dimensions of the collider
		 * @param mat the physics material to assign
		 */
		BoxCollider(const vector3& ext, Ref<PhysicsMaterial> mat, CollisionType contact = CollisionType::Collider, bool trigger = false) : BoxCollider() {
			extent = ext;
			material = mat;
			Type = contact;
			eventsEnabled = trigger;
		}

		void AddHook(const WeakRef<RavEngine::Entity>& e) override;

		virtual void RegisterAllAlternateTypes() override {
			RegisterAlternateQueryType<PhysicsCollider>();
		}
	};
}