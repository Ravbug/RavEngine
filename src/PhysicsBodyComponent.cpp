
#include "PhysicsBodyComponent.hpp"
#include "PhysicsSolver.hpp"
#include <foundation/PxTransform.h>
#include <foundation/PxVec3.h>
#include <PxRigidBody.h>
#include "Transform.hpp"
#include "Entity.hpp"
#include "IPhysicsActor.hpp"

using namespace physx;
using namespace RavEngine;

/// Dynamic Body ========================================

RigidBodyDynamicComponent::RigidBodyDynamicComponent() {
	rigidActor = PhysicsSolver::phys->createRigidDynamic(PxTransform(PxVec3(0, 0, 0)));	//will be set pre-tick to the entity's location
}

void RavEngine::PhysicsBodyComponent::AddHook(const WeakRef<RavEngine::Entity>& e)
{
	setPos(e.lock()->transform()->GetWorldPosition());
	setRot(e.lock()->transform()->GetWorldRotation());
}

void RavEngine::PhysicsBodyComponent::AddReceiver(Ref<IPhysicsActor> obj)
{
	receivers.insert(obj);
}

void RavEngine::PhysicsBodyComponent::RemoveReceiver(Ref<IPhysicsActor> obj)
{
	receivers.erase(obj);
}

RavEngine::PhysicsBodyComponent::~PhysicsBodyComponent()
{
	//note: do not need to delete the rigid actor here. The PhysicsSolver will delete it
	if (rigidActor != nullptr) {
		delete ((Ref<PhysicsBodyComponent>*)rigidActor->userData);	//free the dynamically-allocated shared_ptr
		rigidActor->userData = nullptr;
		rigidActor->release();
	}
}

vector3 PhysicsBodyComponent::getPos() const {
	auto pos = rigidActor->getGlobalPose();
	return vector3(pos.p.x, pos.p.y, pos.p.z);
}

void PhysicsBodyComponent::setPos(const vector3& pos) {
	rigidActor->setGlobalPose(PxTransform(PxVec3(pos.x, pos.y, pos.z),rigidActor->getGlobalPose().q));
}

quaternion PhysicsBodyComponent::getRot() const {
	auto rot = rigidActor->getGlobalPose();
	return quaternion(rot.q.w, rot.q.x,rot.q.y,rot.q.z);
}

void PhysicsBodyComponent::setRot(const quaternion& quat) {
	rigidActor->setGlobalPose(PxTransform(rigidActor->getGlobalPose().p,PxQuat(quat.x,quat.y,quat.z,quat.w)));
}

/**
Enable or disable gravity on this body
@param state the new state of gravity
*/
void RavEngine::PhysicsBodyComponent::SetGravityEnabled(bool state)
{
	rigidActor->setActorFlag(PxActorFlag::eDISABLE_GRAVITY,state);
}

bool RavEngine::PhysicsBodyComponent::GetGravityEnabled() const
{
	return rigidActor->getActorFlags() & PxActorFlag::eDISABLE_GRAVITY;
}

/**
Sets whether or not onWake and onSleep events get called.
@param state new state for this property
*/
void RavEngine::PhysicsBodyComponent::SetSleepNotificationsEnabled(bool state)
{
	rigidActor->setActorFlag(PxActorFlag::eSEND_SLEEP_NOTIFIES, state);
}

bool RavEngine::PhysicsBodyComponent::GetSleepNotificationsEnabled() const
{
	return rigidActor->getActorFlags() & PxActorFlag::eSEND_SLEEP_NOTIFIES;
}

/**
Enable or disable simulation for this body.
@param state the new simulation status.
@note If simulation is disbaled, all constraints are removed, all velocities and forces are cleared, and actor is set to sleep.
*/
void RavEngine::PhysicsBodyComponent::SetSimulationEnabled(bool state)
{
	rigidActor->setActorFlag(PxActorFlag::eDISABLE_SIMULATION,state);
}

bool RavEngine::PhysicsBodyComponent::GetSimulationEnabled() const
{
	return rigidActor->getActorFlags() & PxActorFlag::eDISABLE_SIMULATION;
}


RigidBodyDynamicComponent::~RigidBodyDynamicComponent() {
	//note: do not need to delete the rigid actor here. The PhysicsSolver will delete it.
}

vector3 RavEngine::RigidBodyDynamicComponent::GetLinearVelocity() const
{
	rigidActor->getScene()->lockRead();
	auto vel = static_cast<PxRigidBody*>(rigidActor)->getLinearVelocity();
	auto ret = vector3(vel.x,vel.y,vel.z);
	rigidActor->getScene()->unlockRead();
	return ret;
}

vector3 RavEngine::RigidBodyDynamicComponent::GetAngularVelocity() const
{
	rigidActor->getScene()->lockRead();
	auto vel = static_cast<PxRigidBody*>(rigidActor)->getAngularVelocity();
	auto ret = vector3(vel.x,vel.y,vel.z);
	rigidActor->getScene()->unlockRead();
	return ret;
}

/**
Set the linear velocity of the physics body
@param newvel the new velocity for each axis
@param autowake whether to automatically wake this physics body to apply the change. If set to false, the body must be woken manually.
*/
void RavEngine::RigidBodyDynamicComponent::SetLinearVelocity(const vector3& newvel, bool autowake)
{
	Write([&]{
		static_cast<PxRigidBody*>(rigidActor)->setLinearVelocity(PxVec3(newvel.x, newvel.y, newvel.z),autowake);
	});
}

/**
Set the angular velocity of the physics body
@param newvel the new velocity for each euler axis
@param autowake whether to automatically wake this physics body to apply the change. If set to false, the body must be woken manually.
*/
void RavEngine::RigidBodyDynamicComponent::SetAngularVelocity(const vector3& newvel, bool autowake)
{
	Write([&]{
		static_cast<PxRigidBody*>(rigidActor)->setAngularVelocity(PxVec3(newvel.x, newvel.y, newvel.z), autowake);
	});
}

void RavEngine::RigidBodyDynamicComponent::SetAxisLock(uint16_t LockFlags){
	static_cast<PxRigidDynamic*>(rigidActor)->setRigidDynamicLockFlags(static_cast<physx::PxRigidDynamicLockFlag::Enum>(LockFlags));
}

uint16_t RavEngine::RigidBodyDynamicComponent::GetAxisLock() const{
	return static_cast<PxRigidDynamic*>(rigidActor)->getRigidDynamicLockFlags();
}

void RavEngine::RigidBodyDynamicComponent::Wake()
{
	static_cast<PxRigidDynamic*>(rigidActor)->wakeUp();
}

void RavEngine::RigidBodyDynamicComponent::Sleep()
{
	static_cast<PxRigidDynamic*>(rigidActor)->putToSleep();
}

bool RavEngine::RigidBodyDynamicComponent::IsSleeping()
{
	return static_cast<PxRigidDynamic*>(rigidActor)->isSleeping();
}

void PhysicsBodyComponent::OnColliderEnter(Ref<PhysicsBodyComponent> other)
{
	for (auto& reciever : receivers) {
		Ref<IPhysicsActor> strong = reciever.getWeak().lock();
		if (strong){
			strong->OnColliderEnter(other);
		}
	}
}

void PhysicsBodyComponent::OnColliderPersist(Ref<PhysicsBodyComponent> other)
{
	for (auto& reciever : receivers) {
		Ref<IPhysicsActor> strong = reciever.getWeak().lock();
		if (strong){
			strong->OnColliderPersist(other);
		}
	}
}

void PhysicsBodyComponent::OnColliderExit(Ref<PhysicsBodyComponent> other)
{
	for (auto& reciever : receivers) {
		Ref<IPhysicsActor> strong = reciever.getWeak().lock();
		if (strong){
			strong->OnColliderExit(other);
		}
	}
}


void PhysicsBodyComponent::OnTriggerEnter(Ref<PhysicsBodyComponent> other){
	for (auto& reciever : receivers) {
		Ref<IPhysicsActor> strong = reciever.getWeak().lock();
		if (strong){
			strong->OnTriggerEnter(other);
		}
	}
}

void PhysicsBodyComponent::OnTriggerExit(Ref<PhysicsBodyComponent> other){
	for (auto& reciever : receivers) {
		Ref<IPhysicsActor> strong = reciever.getWeak().lock();
		if (strong){
			strong->OnTriggerExit(other);
		}
	}
}

void RigidBodyDynamicComponent::SetMass(decimalType mass){
	static_cast<PxRigidDynamic*>(rigidActor)->setMass(mass);
}

decimalType RigidBodyDynamicComponent::GetMass() const{
	return static_cast<PxRigidDynamic*>(rigidActor)->getMass();
}

decimalType RigidBodyDynamicComponent::GetMassInverse() const{
	return static_cast<PxRigidDynamic*>(rigidActor)->getInvMass();
}

void RigidBodyDynamicComponent::AddForce(const vector3 &force){
	Write([&]{
		static_cast<PxRigidDynamic*>(rigidActor)->addForce(PxVec3(force.x,force.y,force.z));
	});
}

void RigidBodyDynamicComponent::AddTorque(const vector3 &torque){
	Write([&]{
		static_cast<PxRigidDynamic*>(rigidActor)->addTorque(PxVec3(torque.x,torque.y,torque.z));
	});
}

void RigidBodyDynamicComponent::ClearAllForces(){
	Write([&]{
		static_cast<PxRigidDynamic*>(rigidActor)->clearForce();
	});
}

void RigidBodyDynamicComponent::ClearAllTorques(){
	Write([&]{
		static_cast<PxRigidDynamic*>(rigidActor)->clearTorque();
	});
}

/// Static Body ========================================
RigidBodyStaticComponent::RigidBodyStaticComponent() {
	rigidActor = PhysicsSolver::phys->createRigidStatic(PxTransform(PxVec3(0, 0, 0)));	//will be set pre-tick to the entity's location
}

RigidBodyStaticComponent::~RigidBodyStaticComponent() {
	//note: do not need to delete the rigid actor here. The PhysicsSolver will delete it
}
