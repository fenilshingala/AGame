#pragma once

struct ShaderModule;
struct AppModel;
struct AppMesh;
struct Texture;
enum class MeshType;

struct ResourceLoader
{
	std::unordered_map<uint32_t, AppModel*>			modelMap;
	std::unordered_map<uint32_t, AppMesh*>			meshMap;
	std::unordered_map<uint32_t, ShaderModule*>		shaderMap;
	std::unordered_map<uint32_t, Texture*>			textureMap;
};

void InitResourceLoader(ResourceLoader** a_ppResourceLoader);
void ExitResourceLoader(ResourceLoader** a_ppResourceLoader);
void LoadResourceLoader(ResourceLoader** a_ppResourceLoader);
void UnloadResourceLoader(ResourceLoader** a_ppResourceLoader);

void GetModel(ResourceLoader* a_pResourceLoader, const char* a_sPath, AppModel** a_ppModel);
void GetMesh(ResourceLoader* a_pResourceLoader, MeshType e_MeshType, AppMesh** a_ppAppMesh);
void GetShaderModule(ResourceLoader* a_pResourceLoader, const char* a_sShaderName, ShaderModule** a_ppShaderModule);
void GetTexture(ResourceLoader* a_pResourceLoader, const char* a_sTexturePath, Texture** a_ppTexture);