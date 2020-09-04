#include "Entity.hpp"
#include "CameraComponent.hpp"
#include "GameplayStatics.hpp"
#include <functional>
#include "IInputAction.hpp"

class PlayerActor : public RavEngine::Entity, public RavEngine::IInputListener {
protected:
	decimalType dt = 0;
	decimalType movementSpeed = 0.3;
	decimalType sensitivity = 0.1;

	//transform cache
	Ref<RavEngine::Transform> trans;


	decimalType scaleMovement(decimalType f) {
		return f * dt * movementSpeed;
	}

	decimalType scaleRotation(decimalType f) {
		return glm::radians(sensitivity * dt * f);
	}
public:
	Ref<Entity> cameraEntity;

	void MoveForward(float amt) {
		trans->LocalTranslateDelta(scaleMovement(amt) * trans->Forward());
	}
	void MoveRight(float amt) {
		trans->LocalTranslateDelta(scaleMovement(amt) * trans->Right());
	}

	void MoveUp(float amt) {
		trans->LocalTranslateDelta(scaleMovement(amt) * trans->Up());
	}

	void LookUp(float amt) {
		//framePitch = amt;
		cameraEntity->transform()->LocalRotateDelta(vector3(scaleRotation(amt), 0, 0));
	}
	void LookRight(float amt) {
		//frameYaw = amt;
		trans->LocalRotateDelta(quaternion(vector3(0, scaleRotation(amt), 0)));
	}

	void Tick(float time) override{
		dt = time;
	}

	PlayerActor() : Entity() {
		//create a child entity for the camera
		cameraEntity = new Entity();
		auto cam = cameraEntity->AddComponent<RavEngine::CameraComponent>(new RavEngine::CameraComponent());

		//set the active camera
		cam->setActive(true);

		trans = transform();
		trans->AddChild(cameraEntity->transform());
	}

	virtual void Start() override {
		Ref<RavEngine::World>(GetWorld())->Spawn(cameraEntity);
	}

	virtual ~PlayerActor(){}
};