#include "MeshAsset.hpp"
#include "Common3D.hpp"
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/material.h>
#include <assimp/mesh.h>
#include "App.hpp"
#include <filesystem>
#include "Debug.hpp"

using namespace RavEngine;

// Vertex data structure
using namespace std;


MeshAsset::MeshAsset(const string& name, const decimalType scale, bool keepCopyInSystemMemory){
	string dir = "objects/" + name;
	
	if (!App::Resources->Exists(dir.c_str())) {
		Debug::Fatal("Cannot open resource: {}", dir);
	}

	auto str = App::Resources->FileContentsAt(dir.c_str());
	
	//pull from cmrc
	auto file_ext = filesystem::path(dir).extension();
	//uses a meta-flag to auto-triangulate the input file
	const aiScene* scene = aiImportFileFromMemory(str.data(), str.size(),
												  aiProcess_CalcTangentSpace |
												  aiProcess_GenSmoothNormals              |
												  aiProcess_JoinIdenticalVertices         |
												  aiProcess_ImproveCacheLocality          |
												  aiProcess_LimitBoneWeights              |
												  aiProcess_RemoveRedundantMaterials      |
												  aiProcess_SplitLargeMeshes              |
												  aiProcess_Triangulate                   |
												  aiProcess_GenUVCoords                   |
												  aiProcess_SortByPType                   |
												  //aiProcess_FindDegenerates               |
												  aiProcess_FindInstances                  |
												  aiProcess_ValidateDataStructure          |
												  aiProcess_OptimizeMeshes				|
												  aiProcess_FindInvalidData     ,
												  file_ext.string().c_str());
	
	
	if (!scene){
		Debug::Fatal("Cannot load: {}", aiGetErrorString());
	}
	
	//generate the vertex and index lists
    
	matrix4 scalemat = glm::scale(matrix4(1), vector3(scale,scale,scale));
	
	vector<MeshPart> meshes;
	meshes.reserve(scene->mNumMeshes);
	for(int i = 0; i < scene->mNumMeshes; i++){
		aiMesh* mesh = scene->mMeshes[i];
		MeshPart mp;
		mp.indices.reserve(mesh->mNumFaces * 3);
		mp.vertices.reserve(mesh->mNumVertices);
		for(int vi = 0; vi < mesh->mNumVertices; vi++){
			auto vert = mesh->mVertices[vi];
			vector4 scaled(vert.x,vert.y,vert.z,1);
			
			scaled = scalemat * scaled;
			
			auto normal = mesh->mNormals[vi];
			
			//does mesh have uvs?
			float uvs[2] = {0};
			if(mesh->mTextureCoords[0]){
				uvs[0] = mesh->mTextureCoords[0][vi].x;
				uvs[1] = mesh->mTextureCoords[0][vi].y;
			}
			
			mp.vertices.push_back({
				static_cast<float>(scaled.x),static_cast<float>(scaled.y),static_cast<float>(scaled.z),	//coordinates
				normal.x,normal.y,normal.z,																//normals
				uvs[0],uvs[1]																			//UVs
			});
		}
		
		for(int ii = 0; ii < mesh->mNumFaces; ii++){
			//alert if encounters a degenerate triangle
			if(mesh->mFaces[ii].mNumIndices != 3){
				throw runtime_error("Cannot load model: Degenerate triangle (Num indices = " + to_string(mesh->mFaces[ii].mNumIndices) + ")");
			}
		
			mp.indices.push_back(mesh->mFaces[ii].mIndices[0]);
			mp.indices.push_back(mesh->mFaces[ii].mIndices[1]);
			mp.indices.push_back(mesh->mFaces[ii].mIndices[2]);

		}
		
		meshes.push_back(mp);
	}
	
	//free afterward
	aiReleaseImport(scene);
	
	InitializeFromMeshPartFragments(meshes, keepCopyInSystemMemory);
}

void MeshAsset::InitializeFromMeshPartFragments(const std::vector<MeshPart>& meshes, bool keepCopyInSystemMemory){
	//combine all meshes
	for(int i = 0; i < meshes.size(); i++){
		totalVerts += meshes[i].vertices.size();
		totalIndices += meshes[i].indices.size();
	}
	
	MeshPart allMeshes;
	allMeshes.vertices.reserve(totalVerts);
	allMeshes.indices.reserve(totalIndices);
	
	size_t baseline_index = 0;
	for(const auto& mesh : meshes){
		for(const auto& vert : mesh.vertices){
			allMeshes.vertices.push_back(vert);
		}
		for(const auto& index : mesh.indices){
			allMeshes.indices.push_back(index + baseline_index);	//must recompute index here
		}
		baseline_index += mesh.vertices.size();
	}
	InitializeFromRawMesh(allMeshes, keepCopyInSystemMemory);
}

void MeshAsset::InitializeFromRawMesh(const MeshPart& allMeshes, bool keepCopyInSystemMemory){
	if (keepCopyInSystemMemory){
		systemRAMcopy = allMeshes;
	}
	
	//copy out of intermediate
	auto& v = allMeshes.vertices;
	auto& i = allMeshes.indices;
	
	bgfx::VertexLayout pcvDecl;
	
	//vertex format
	pcvDecl.begin()
	.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
	.add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
	.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float,true,true)
	.end();
	
	//create buffers
	auto vbm = bgfx::copy(&v[0], v.size() * sizeof(vertex_t));
	vertexBuffer = bgfx::createVertexBuffer(vbm, pcvDecl);
	
	auto ibm = bgfx::copy(&i[0], i.size() * sizeof(decltype(allMeshes.indices)::value_type));
	indexBuffer = bgfx::createIndexBuffer(ibm,BGFX_BUFFER_INDEX32);
	
	if(! bgfx::isValid(vertexBuffer) || ! bgfx::isValid(indexBuffer)){
		Debug::Fatal("Buffers could not be created.");
	}
}
