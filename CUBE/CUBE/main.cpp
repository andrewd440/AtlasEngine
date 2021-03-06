// AtlasEngine.cpp : Defines the entry point for the console application.
//

#include "CubeRoot.h"
#include "Input\MouseAxis.h"
#include "Rendering\Camera.h"
#include "Math\PerspectiveMatrix.h"
#include "Rendering\RenderSystem.h"
#include "Rendering\ImageEffects\FogPostProcess.h"
#include "Rendering\ImageEffects\SSAOPostProcess.h"
#include "Rendering\ImageEffects\EdgeDetection.h"
#include "ChunkSystems\ChunkManager.h"
#include "Components\BlockPlacer.h"
#include "Components\BoxShooter.h"
#include "Components\FlyingCamera.h"
#include "Components\MeshRenderer.h"
#include "Components\TimeBomb.h"
#include "Components\TimeBombShooter.h"
#include "Rendering\Light.h"
#include "Atlas\ComponentTypes.h"
#include "Components\SoundListener.h"
#include "Components\SoundEmitter.h"

#include "FileIO\RegionFile.h"

#include "LibNoise\noise.h"
#include "ChunkSystems\WorldGenerator.h"
#include "Math\Vector2.h"
#include "Utils\Event.h"
#include "ChunkSystems\BlockTypes.h"
#include "Components\MeshRenderer.h"

using namespace Atlas;

int main()
{
	const Vector2ui Resolution{ 1920, 1080 };
	FCubeRoot Root{ L"CUBE", Resolution, sf::Style::Default };
	Root.GetChunkManager().SetViewDistance(14);

	SMouseAxis::SetDefaultMousePosition(Resolution / 2);
	SMouseAxis::SetMouseVisible(false);
	SMouseAxis::SetMouseLock(true);

	const Vector4f BlockColors[5] =
	{
		Vector4f{ 0.11f, 0.35f, 0.15f },		// Grass
		Vector4f{ 0.47f, 0.28f, 0.0f },		// Dirt
		Vector4f{ 1.0f, 0.98f, 0.98f },		// Snow
		Vector4f{ 0.59f, 0.086f, 0.043f },	// DarkBrick
		Vector4f{0.69f, 0.086f, 0.43f}		// LightBrick
	};

	enum Type : uint8_t
	{
		Air,
		Grass,
		Dirt,
		Snow,
		DarkBrick,
		LightBrick
	};

	FOR(i, 5)
	{
		FBlockTypes::AddBlock(i+1, BlockColors[i]);
	}

	FCamera Camera;
	const Vector3f CameraPosition = Vector3f{ 560.0f, 320.0f, 560.0f };
	Camera.Transform.SetLocalPosition(CameraPosition);
	Camera.SetProjection(FPerspectiveMatrix{ (float)Resolution.x / (float)Resolution.y, 35.0f, 0.1f, 600.0f });

	auto& Renderer = Root.GetRenderSystem();
	std::unique_ptr<FEdgeDetection> EdgeDetection{ new FEdgeDetection{} };
	Renderer.AddPostProcess(std::move(EdgeDetection));

	std::unique_ptr<FSSAOPostProcess> SSAO{ new FSSAOPostProcess{} };
	SSAO->SetGlobalAmbient(Vector3f{ .3f, .3f, .3f });
	SSAO->SetKernalSize(16);
	SSAO->SetNoiseSize(4);
	SSAO->SetPower(1.25f);
	SSAO->SetRadius(1.25f);
	Renderer.AddPostProcess(std::move(SSAO));

	std::unique_ptr<FFogPostProcess> FogPostProcess{ new FFogPostProcess{} };
	FogPostProcess->SetBounds(0, 1);
	FogPostProcess->SetColor(Vector3f{ .6f, .6f, .6f });
	FogPostProcess->SetDensity(0.000009f);
	Renderer.AddPostProcess(std::move(FogPostProcess));

	//Renderer.EnablePostProcess(0);
	Renderer.EnablePostProcess(1);
	Renderer.EnablePostProcess(2);

	auto& ChunkManager = Root.GetChunkManager();
	ChunkManager.LoadWorld(L"NewWorld");


	auto& GameObjectManager = Root.GetGameObjectManager();

	auto& PlayerController = GameObjectManager.CreateGameObject();
	PlayerController.Transform.SetParent(&Camera.Transform);
	PlayerController.AddBehavior<CFlyingCamera>();
	PlayerController.AddBehavior<CBlockPlacer>();
	//PlayerController.AddBehavior<CTimeBombShooter>();
	PlayerController.AddBehavior<CBoxShooter>();
	PlayerController.AddComponent<EComponent::SoundListener>();

	////////////////////////////////////////////////////////////////////////
	//////// Directional Light /////////////////////////////////////////////
	auto& DirectionalLight = GameObjectManager.CreateGameObject();
	FDirectionalLight& DLight = DirectionalLight.AddComponent<Atlas::EComponent::DirectionalLight>();
	DLight.Color = Vector3f(.6f, .6f, .6f);
	DirectionalLight.Transform.SetRotation(FQuaternion{ -130, -20, 0 });


	SMeshHolder::Load("Box");
	auto& BoxMesh = SMeshHolder::Get("Box");
	BoxMesh.Mesh.LoadModel("Box.obj");

	SMeshHolder::Load("Sword");
	auto& SwordMesh = SMeshHolder::Get("Sword");
	SwordMesh.Mesh.LoadModel("Sword.obj");

	//auto& PointLight = GameObjectManager.CreateGameObject();
	//PointLight.Transform.SetPosition(Vector3f{ 260.0f, 245.0f, 260.0f });
	//FPointLight& PLight = PointLight.AddComponent<Atlas::EComponent::PointLight>();
	//PLight.Color = Vector3f(1.0f, .1f, .1f);
	//PLight.Constant = 0.1f;
	//PLight.Linear = 0.3f;
	//PLight.Quadratic = 0.6f;
	//PLight.Intensity = 1000.0f;
	//PLight.MaxDistance = 100;

	auto& Sword = GameObjectManager.CreateGameObject();
	Sword.AddComponent<Atlas::EComponent::MeshRenderer>().LinkToMesh("Sword");
	Sword.Transform.SetLocalPosition(Vector3f{ 300.0f, 270.0f, 300.0f });
	Sword.Transform.Rotate(FQuaternion{ 90, 90, -45 });

	Root.Start();

}

//////////////////////////////////////
// World Generation //////////////////
//////////////////////////////////////
//
//int main()
//{
//	IFileSystem* FileSys = new FFileSystem;
//
//	module::RidgedMulti mountainTerrain;
//	mountainTerrain.SetFrequency(0.5);
//	mountainTerrain.SetOctaveCount(1);
//	mountainTerrain.SetLacunarity(1.0);
//
//	module::Terrace Terrance;
//	Terrance.AddControlPoint(2.3);
//	Terrance.AddControlPoint(3.0);
//	Terrance.SetSourceModule(0, mountainTerrain);
//
//	module::Billow baseFlatTerrain;
//	baseFlatTerrain.SetFrequency(0.5);
//	baseFlatTerrain.SetPersistence(0.5);
//
//	module::ScaleBias flatTerrain;
//	flatTerrain.SetSourceModule(0, baseFlatTerrain);
//	flatTerrain.SetScale(0.04);
//	flatTerrain.SetBias(-0.75);
//
//	module::Perlin terrainType;
//	terrainType.SetFrequency(1.0);
//	terrainType.SetPersistence(0.125);
//
//	module::Select terrainSelector;
//	terrainSelector.SetSourceModule(0, flatTerrain);
//	terrainSelector.SetSourceModule(1, mountainTerrain);
//	terrainSelector.SetControlModule(terrainType);
//	terrainSelector.SetBounds(0.0, 600.0);
//	terrainSelector.SetEdgeFalloff(0.275);
//
//	module::Turbulence finalTerrain;
//	finalTerrain.SetSourceModule(0, terrainSelector);
//	finalTerrain.SetFrequency(1.0);
//	finalTerrain.SetPower(0.125);
//
//	FWorldGenerator Generator;
//	Generator.SetBounds(Vector2f{ 1.0f, 1.0f }, Vector2f{ 10.0f, 10.0f });
//	Generator.SetWorldSizeInChunks(32);
//	Generator.SetMaxHeight(300);
//	Generator.SetMinHeight(160);
//
//	enum Type : uint8_t
//	{
//		Air,
//		Grass,
//		Dirt,
//		Snow,
//		Brick,
//	};
//
//	Generator.AddTerrainLevel(0, Brick);
//	Generator.AddTerrainLevel(180, Grass);
//	//Generator.AddTerrainLevel(220, Dirt);
//	//Generator.AddTerrainLevel(250, Snow);
//
//	Generator.Build(finalTerrain, L"NewWorld");
//
//	delete FileSys;
//
//	return 0;
//}
//
