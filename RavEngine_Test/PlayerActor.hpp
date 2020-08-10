#include "Entity.hpp"
#include "CameraComponent.hpp"
#include "GameplayStatics.hpp"
#include <functional>

class PlayerActor : public Entity {
protected:
	decimalType dt = 0;
	decimalType movementSpeed = 0.3;
	decimalType sensitivity = 0.1;

	//transform cache
	Ref<TransformComponent> trans;


	decimalType scaleMovement(decimalType f) {
		return f * dt * movementSpeed;
	}

	decimalType scaleRotation(decimalType f) {
		return glm::radians(sensitivity * dt * f);
	}
public:
	Ref<Entity> cameraEntity;

	void MoveForward(decimalType amt) {
		trans->LocalTranslateDelta(scaleMovement(amt) * trans->Forward());
	}
	void MoveRight(decimalType amt) {
		trans->LocalTranslateDelta(scaleMovement(amt) * trans->Right());
	}

	void MoveUp(decimalType amt) {
		trans->LocalTranslateDelta(scaleMovement(amt) * trans->Up());
	}

	void LookUp(decimalType amt) {
		//framePitch = amt;
		cameraEntity->transform()->LocalRotateDelta(vector3(scaleRotation(amt), 0, 0));
	}
	void LookRight(decimalType amt) {
		//frameYaw = amt;
		trans->LocalRotateDelta(quaternion(vector3(0, scaleRotation(amt), 0)));
	}

	void Tick(float time) override{
		dt = time;
	}

	PlayerActor() : Entity() {
		//create a child entity for the camera
		cameraEntity = new Entity();
		auto cam = cameraEntity->AddComponent<CameraComponent>(new CameraComponent());

		//set the active camera
		cam->setActive(true);

		trans = transform();
		trans->AddChild(cameraEntity->transform());
	}

	virtual void Start() override {
		Ref<World>(GetWorld())->Spawn(cameraEntity);
	}

	virtual ~PlayerActor(){}
};