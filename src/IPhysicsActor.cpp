#include "IPhysicsActor.hpp"
#include "Ref.hpp"
#include "PhysicsBodyComponent.hpp"

using namespace std;
using namespace RavEngine;

void RavEngine::IPhysicsActor::OnRegisterBody(const WeakRef<PhysicsBodyComponent>& p)
{
	senders.insert(p);
}

void RavEngine::IPhysicsActor::OnUnregisterBody(const WeakRef<PhysicsBodyComponent>& p)
{
	senders.erase(p);
}

RavEngine::IPhysicsActor::~IPhysicsActor()
{
	for (auto& a : senders) {
        shared_ptr<PhysicsBodyComponent> ptr = a.lock();
		if (ptr){
			ptr->RemoveReceiver(shared_from_this());
		}
	}
}
