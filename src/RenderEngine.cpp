//
//  RenderEngine.cpp
//  RavEngine_Static
//
//  Copyright © 2020 Ravbug. All rights reserved.
//

#include "RenderEngine.hpp"
#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>
#include "GameplayStatics.hpp"
#include "CameraComponent.hpp"
#include "World.hpp"

#include <filament/Engine.h>
#include <filament/SwapChain.h>
#include <filament/Camera.h>
#include <filament/Renderer.h>
#include <filament/Viewport.h>
#include <filament/View.h>
#include <filament/Scene.h>
#include <utils/Entity.h>
#include <filament/Material.h>
#include <filament/VertexBuffer.h>
#include <filament/IndexBuffer.h>
#include <filament/RenderableManager.h>
#include <utils/EntityManager.h>
#include <filament/TransformManager.h>
#include <filament/Skybox.h>
#include <filamentapp/FilamentApp.h>
#include <filamentapp/Config.h>

#include <SDL_syswm.h>
#include <SDL.h>

#ifdef __APPLE_
#include <Cocoa/Cocoa.h>
#endif
#include <filamat/MaterialBuilder.h>

using namespace std;
using namespace RavEngine;
//using namespace utils;
using namespace filament;

SDL_Window* RenderEngine::window = nullptr;
filament::SwapChain* RenderEngine::filamentSwapChain = nullptr;
filament::Engine* RenderEngine::filamentEngine = nullptr;
filament::Renderer* RenderEngine::filamentRenderer = nullptr;

struct Vertex {
	filament::math::float2 position;
	uint32_t color;
};

static const Vertex TRIANGLE_VERTICES[3] = {
	{{1, 0}, 0xffff0000u},
	{{cos(M_PI * 2 / 3), sin(M_PI * 2 / 3)}, 0xff00ff00u},
	{{cos(M_PI * 4 / 3), sin(M_PI * 4 / 3)}, 0xff0000ffu},
};

static constexpr uint16_t TRIANGLE_INDICES[3] = { 0, 1, 2 };

void test() {
	struct App {
		VertexBuffer* vb;
		IndexBuffer* ib;
		Material* mat;
		filament::Camera* cam;
		utils::Entity camera;
		Skybox* skybox;
		utils::Entity renderable;
	};
	Config config;
	config.title = "hellotriangle";

	//material
	string mat;
	{
		auto path = "../deps/filament/filament/generated/material/defaultMaterial.filamat";
#ifdef _WIN32
		path += 3;
#endif
		ifstream fin(path, ios::binary);
		assert(fin.good());	//ensure file exists
		ostringstream buffer;
		buffer << fin.rdbuf();
		mat = buffer.str();
	}

	App app;
	auto setup = [&app, &mat](Engine* engine, View* view, Scene* scene) {
		app.skybox = Skybox::Builder().color({ 0.1, 0.125, 0.25, 1.0 }).build(*engine);
		scene->setSkybox(app.skybox);
		view->setPostProcessingEnabled(false);
		static_assert(sizeof(Vertex) == 12, "Strange vertex size.");
		app.vb = VertexBuffer::Builder()
			.vertexCount(3)
			.bufferCount(1)
			.attribute(VertexAttribute::POSITION, 0, VertexBuffer::AttributeType::FLOAT2, 0, 12)
			.attribute(VertexAttribute::COLOR, 0, VertexBuffer::AttributeType::UBYTE4, 8, 12)
			.normalized(VertexAttribute::COLOR)
			.build(*engine);
		app.vb->setBufferAt(*engine, 0,
			VertexBuffer::BufferDescriptor(TRIANGLE_VERTICES, 36, nullptr));
		app.ib = IndexBuffer::Builder()
			.indexCount(3)
			.bufferType(IndexBuffer::IndexType::USHORT)
			.build(*engine);
		app.ib->setBuffer(*engine,
			IndexBuffer::BufferDescriptor(TRIANGLE_INDICES, 6, nullptr));
		app.mat = Material::Builder()
			.package(mat.c_str(), mat.size())
			.build(*engine);
		app.renderable = utils::EntityManager::get().create();
		RenderableManager::Builder(1)
			.boundingBox({ { -1, -1, -1 }, { 1, 1, 1 } })
			.material(0, app.mat->getDefaultInstance())
			.geometry(0, RenderableManager::PrimitiveType::TRIANGLES, app.vb, app.ib, 0, 3)
			.culling(false)
			.receiveShadows(false)
			.castShadows(false)
			.build(*engine, app.renderable);
		scene->addEntity(app.renderable);
		app.camera = utils::EntityManager::get().create();
		app.cam = engine->createCamera(app.camera);
		view->setCamera(app.cam);
	};

	auto cleanup = [&app](Engine* engine, View*, Scene*) {
		engine->destroy(app.skybox);
		engine->destroy(app.renderable);
		engine->destroy(app.mat);
		engine->destroy(app.vb);
		engine->destroy(app.ib);
		engine->destroyCameraComponent(app.camera);
		utils::EntityManager::get().destroy(app.camera);
	};

	FilamentApp::get().animate([&app](Engine* engine, View* view, double now) {
		constexpr float ZOOM = 1.5f;
		const uint32_t w = view->getViewport().width;
		const uint32_t h = view->getViewport().height;
		const float aspect = (float)w / h;
		app.cam->setProjection(filament::Camera::Projection::ORTHO,
			-aspect * ZOOM, aspect * ZOOM,
			-ZOOM, ZOOM, 0, 1);
		auto& tcm = engine->getTransformManager();
		tcm.setTransform(tcm.getInstance(app.renderable),
			filament::math::mat4f::rotation(now, filament::math::float3{ 0, 0, 1 }));
		});

	FilamentApp::get().run(config, setup, cleanup);
}

/**
Construct a render engine instance
@param w the owning world for this engine instance
*/
RenderEngine::RenderEngine(const WeakRef<World>& w) : world(w) {
	if (filamentEngine == nullptr) {
		Init();
	}
	filamentView = filamentEngine->createView();
	filamentScene = filamentEngine->createScene();
	
	filamentView->setViewport({0,0,800,480});

	filament::Camera* camera = filamentEngine->createCamera(utils::EntityManager::get().create());
	constexpr float ZOOM = 1.5f;
	const uint32_t width = filamentView->getViewport().width;
	const uint32_t height = filamentView->getViewport().height;
	const float aspect = (float)width / height;
	camera->setProjection(filament::Camera::Projection::ORTHO,
		-aspect * ZOOM, aspect * ZOOM,
		-ZOOM, ZOOM, 0, 1);

	filamentView->setCamera(camera);
	filamentView->setScene(filamentScene);

	utils::Entity renderable = utils::EntityManager::get().create();
	// build a quad

	//material
	string mat;
	{
		auto path = "../deps/filament/filament/generated/material/defaultMaterial.filamat";
#ifdef _WIN32
		path += 3;
#endif
		ifstream fin(path, ios::binary);
		assert(fin.good());	//ensure file exists
		ostringstream buffer;
		buffer << fin.rdbuf();
		mat = buffer.str();
	}
	
	Material* material = Material::Builder()
		.package((void*)mat.c_str(), mat.size())
		.build(*filamentEngine);
	MaterialInstance* materialInstance = material->createInstance();
	
	auto vertexBuffer = VertexBuffer::Builder()
		.vertexCount(3)
		.bufferCount(1)
		.attribute(VertexAttribute::POSITION, 0, VertexBuffer::AttributeType::FLOAT2, 0, 12)
		.attribute(VertexAttribute::COLOR, 0, VertexBuffer::AttributeType::UBYTE4, 8, 12)
		.normalized(VertexAttribute::COLOR)
		.build(*filamentEngine);
	vertexBuffer->setBufferAt(*filamentEngine, 0, VertexBuffer::BufferDescriptor(TRIANGLE_VERTICES, 36, nullptr));

	auto indexBuffer = IndexBuffer::Builder()
		.indexCount(3)
		.bufferType(IndexBuffer::IndexType::USHORT)
		.build(*filamentEngine);

	indexBuffer->setBuffer(*filamentEngine,IndexBuffer::BufferDescriptor(TRIANGLE_INDICES, 6, nullptr));

	RenderableManager::Builder(1)
		.boundingBox({ { -1, -1, -1 }, { 1, 1, 1 } })
		.material(0,material->getDefaultInstance())
		.geometry(0, RenderableManager::PrimitiveType::TRIANGLES, vertexBuffer, indexBuffer, 0, 3)
		.culling(false)
		.receiveShadows(false)
		.castShadows(false)
		.build(*filamentEngine, renderable);
	filamentScene->addEntity(renderable);

	auto skybox = Skybox::Builder().color({ 0.1, 0.125, 0.25, 1.0 }).build(*filamentEngine);
	filamentScene->setSkybox(skybox);

	auto& tcm = filamentEngine->getTransformManager();
	tcm.setTransform(tcm.getInstance(renderable),filament::math::mat4f::rotation(0, filament::math::float3{ 0, 0, 1 }));
}

RavEngine::RenderEngine::~RenderEngine()
{
	filamentEngine->destroy(filamentView);
	filamentEngine->destroy(filamentScene);
}

/**
 Make the rendering system aware of an object
 @param e the entity to load
 */
void RenderEngine::Spawn(Ref<Entity> e){
	
}

/**
 Remove an entity from the system. This does NOT destroy the entity from the world.
 @param e the entity to remove
 */
void RenderEngine::Destroy(Ref<Entity> e){
}

int counter = 0;
/**
 Render one frame using the current state of every object in the world
 */
void RenderEngine::Draw(){
	//get the active camera
	/*auto components = Ref<World>(world.get())->Components();
	auto allcams = components.GetAllComponentsOfType<CameraComponent>();*/

	//set the view transform - all entities drawn will use this matrix
	/*for (auto& cam : allcams) {
		auto owning = Ref<CameraComponent>(cam);
		if (owning->isActive()) {
			activeCamera = owning->getCamera();
			break;
		}
	}*/

    //draw each entity
	auto worldOwning = Ref<World>(world);
	auto entitylist = worldOwning->getEntities();
	for (auto& entity : entitylist) {
		//entity->transform()->ApplyToSceneNode();
		//entity->Draw();
    }

	if (filamentRenderer->beginFrame(filamentSwapChain)) {
		// for each View
		filamentRenderer->render(filamentView);
		filamentRenderer->endFrame();
	}
}

/**
@return the name of the current rendering API
*/
const string RenderEngine::currentBackend(){
	return "Unknown";
}

/**
Initialize static singletons. Invoked automatically if needed.
*/
void RenderEngine::Init()
{
	//if already initialized, don't do anything
	if (filamentEngine != nullptr) {
		return;
	}

	//test();

	//create SDL window

	SDL_Init(SDL_INIT_EVENTS);
	uint32_t windowFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI;

	window = SDL_CreateWindow("RavEngine - Filament", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 480, windowFlags);

	//get the native window 
    void* nativeWindow = getNativeWindow(window);
    
	auto backend = filament::Engine::Backend::OPENGL;

	
//#ifdef __APPLE__
//    //need to make a metal layer on Mac
//    nativeWindow = setUpMetalLayer(nativeWindow);
//	auto backend = filament::Engine::Backend::METAL;
//#else
//	auto backend = filament::Engine::Backend::OPENGL;
//#endif

	filamentEngine = filament::Engine::create(backend);	
	filamentSwapChain = filamentEngine->createSwapChain((void*)nativeWindow);
	filamentRenderer = filamentEngine->createRenderer();
}
