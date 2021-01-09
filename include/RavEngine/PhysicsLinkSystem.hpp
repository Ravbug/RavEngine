//
//  PhysicsLinkSystem.hpp
//  RavEngine_Static
//
//  Copyright © 2020 Ravbug.
//

#pragma once
#include "System.hpp"
#include "PhysXDefines.h"
#include <PxPhysicsAPI.h>
#include "PhysicsBodyComponent.hpp"
#include "ScriptSystem.hpp"
#include "CTTI.hpp"

namespace RavEngine {
    /**
    This System copies the Entity's transform to the physics simulation transform.
    It must run after any transform modifications in other systems, ideally at the end of the pipeline.
    */
    class PhysicsLinkSystemWrite : public System {
    public:
		PhysicsLinkSystemWrite(physx::PxScene* scene ) : dynamicsWorld(scene){}
		
        physx::PxScene* dynamicsWorld = nullptr;
        virtual ~PhysicsLinkSystemWrite() {}
        void Tick(float fpsScale, Ref<Entity> e) override;

		const list_type& QueryTypes() const override {
			return queries;
        }
		
		ctti_t ID() const override{
			return CTTI<PhysicsLinkSystemWrite>;
		}
		
	protected:
		static const list_type queries;
    };

    /**
     This System copies the state of the physics simulation transform to the Entity's transform.
     It must run before any transform modifications in other systems, ideally at the beginning of the pipeline.
     */
    class PhysicsLinkSystemRead : public System {
    public:
		PhysicsLinkSystemRead(physx::PxScene* scene ) : dynamicsWorld(scene){}
		
        physx::PxScene* dynamicsWorld = nullptr;
        virtual ~PhysicsLinkSystemRead() {}
        void Tick(float fpsScale, Ref<Entity> e) override;

		const list_type& QueryTypes() const override {
            return queries;
        }

		//must run before write system
		const list_type& MustRunBefore() const override {
			return runbefore;
		}
		
		ctti_t ID() const override{
			return CTTI<PhysicsLinkSystemRead>;
		}
		
	protected:
		static const list_type queries, runbefore;
    };
}
