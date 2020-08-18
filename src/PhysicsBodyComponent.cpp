
#include "PhysicsBodyComponent.hpp"
#include "PhysicsSolver.hpp"
#include <foundation/PxTransform.h>
#include <foundation/PxVec3.h>
#include "Transform.hpp"
#include "Entity.hpp"

using namespace physx;
using namespace RavEngine;

/// Dynamic Body ========================================

RigidBodyDynamicComponent::RigidBodyDynamicComponent() {
	rigidActor = PhysicsSolver::phys->createRigidDynamic(PxTransform(PxVec3(0, 0, 0)));	//will be set pre-tick to the entity's location
	RegisterAllAlternateTypes();
}

vector3 PhysicsBodyComponent::getPos() {
	auto pos = rigidActor->getGlobalPose();
	return vector3(pos.p.x, pos.p.y, pos.p.z);
}

void PhysicsBodyComponent::setPos(const vector3& pos) {
	rigidActor->setGlobalPose(PxTransform(PxVec3(pos.x, pos.y, pos.z),rigidActor->getGlobalPose().q));
}

quaternion PhysicsBodyComponent::getRot() {
	auto rot = rigidActor->getGlobalPose();
	return quaternion(vector3(rot.q.x,rot.q.y,rot.q.z));
}

void PhysicsBodyComponent::setRot(const quaternion& quat) {
	rigidActor->setGlobalPose(PxTransform(rigidActor->getGlobalPose().p,PxQuat(quat.x,quat.y,quat.z,quat.w)));
}

RigidBodyDynamicComponent::~RigidBodyDynamicComponent() {
	if (rigidActor != nullptr) {
		rigidActor->release();
	}
}

void PhysicsBodyComponent::OnColliderEnter(PhysicsBodyComponent* other)
{
	Ref<RavEngine::Entity>(owner)->OnColliderEnter(other);
}

void PhysicsBodyComponent::OnColliderPersist(PhysicsBodyComponent* other)
{
	Ref<RavEngine::Entity>(owner)->OnColliderPersist(other);
}

void PhysicsBodyComponent::OnColliderExit(PhysicsBodyComponent* other)
{
	Ref<RavEngine::Entity>(owner)->OnColliderExit(other);
}

/// Static Body ========================================
RigidBodyStaticComponent::RigidBodyStaticComponent() {
	rigidActor = PhysicsSolver::phys->createRigidStatic(PxTransform(PxVec3(0, 0, 0)));	//will be set pre-tick to the entity's location
	RegisterAllAlternateTypes();
}

RigidBodyStaticComponent::~RigidBodyStaticComponent() {
	if (rigidActor != nullptr) {
		rigidActor->release();
	}
}