#pragma once

// Engine Renderer
struct Renderer;
struct CommandBuffer;
struct ShaderModule;
struct DescriptorSet;
struct ResourceDescriptor;
struct RenderTarget;
struct Pipeline;

// Engine ModelLoader
struct Node;
struct Buffer;
struct Material;
struct Model;
enum class AplhaMode;

class IApp;
class Camera;

struct PushConstBlockMaterial {
	glm::vec4 baseColorFactor;
	glm::vec4 emissiveFactor;
	glm::vec4 diffuseFactor;
	glm::vec4 specularFactor;
	float workflow;
	int colorTextureSet;
	int PhysicalDescriptorTextureSet;
	int normalTextureSet;
	int occlusionTextureSet;
	int emissiveTextureSet;
	float metallicFactor;
	float roughnessFactor;
	float alphaMask;
	float alphaMaskCutoff;
};

enum PBRWorkflows { PBR_WORKFLOW_METALLIC_ROUGHNESS = 0, PBR_WORKFLOW_SPECULAR_GLOSINESS = 1 };

struct ModelMatrixDynamicBuffer;

class Renderable
{
public:
	virtual void Draw(CommandBuffer* a_pCommandBuffer) = 0;
};

enum class MeshType
{
	SKYBOX = 0,
	DEBUG_BOX,
	//TRIANGLE,
	//RECTANGLE,

	COUNT
};

struct AppMesh
{
	uint32_t vertexCount;
	Buffer* pVertexBuffer;
	bool hasIndices;
	uint32_t indexCount;
	Buffer* pIndexBuffer;

	AppMesh() :
		vertexCount(-1), pVertexBuffer(nullptr), hasIndices(false), indexCount(-1), pIndexBuffer(nullptr)
	{}
};

struct AppModel
{
	Model*			pModel;
	DescriptorSet*	pNodeDescriptorSet;
	DescriptorSet*	pMaterialDescriptorSet;

	AppModel() :
		pModel(nullptr), pNodeDescriptorSet(nullptr), pMaterialDescriptorSet(nullptr)
	{}
};

class AppRenderer
{
public:
	AppRenderer() :
		pRenderer(nullptr), pCamera(nullptr), pDepthBuffer(nullptr), cmdBfrs(nullptr), renderSystemInitialized(false),
		ppSceneUniformBuffers(nullptr), pSceneDescriptorSet(nullptr), pPBRResDesc(nullptr), pPBRPipeline(nullptr),
		resourceDescriptorNameMap(), modelMatrixDynamicBufferMap(), renderQueue()
	{}
	~AppRenderer() {}

	void Init(IApp* a_pApp);
	void Exit();
	void Load();
	void Unload();
	void Update(float f_dt);
	void DrawScene();

	Renderer* GetRenderer() { return pRenderer; }
	void PushToRenderQueue(Renderable* a_pRenderable);
	
	void GetResourceDescriptorByName(const char* a_sName, ResourceDescriptor** a_ppResourceDescriptor);

	void AllocateModelMatrices(const char* a_sName, uint32_t a_uCount, ResourceDescriptor* a_pResourceDescriptor);
	void DestroyModelMatrices(const char* a_sName);
	void GetModelMatrixFreeIndex(const char* a_sName, uint32_t* a_pIndex);
	void RevokeModelMatrixIndex(const char* a_sName, const uint32_t a_pIndex);
	void GetModelMatrixCpuBufferForIndex(const char* a_sName, const uint32_t a_pIndex, glm::mat4** a_ppCpuBuffer);
	void UpdateModelMatrixGpuBufferForIndex(const char* a_sName, const uint32_t a_pIndex);
	void BindModelMatrixDescriptorSet(CommandBuffer* a_pCommandBuffer, const char* a_sName, const uint32_t a_pIndex);

	// Pipelines
	Pipeline* pPBRPipeline;
	Pipeline* pSkyboxPipeline;
	Pipeline* pDebugDrawPipeline;
	//
	DescriptorSet* pSceneDescriptorSet;

private:
	Renderer*			pRenderer;
	Camera*				pCamera;
	RenderTarget*		pDepthBuffer;
	CommandBuffer**		cmdBfrs;
	bool				renderSystemInitialized;

	// projection and view matrices
	Buffer**			ppSceneUniformBuffers;
	//

	// Resource Descriptors
	ResourceDescriptor*	pPBRResDesc;
	ResourceDescriptor*	pSkyboxResDesc;
	ResourceDescriptor*	pDebugDrawResDesc;
	//

	std::unordered_map<uint32_t, ResourceDescriptor*>			resourceDescriptorNameMap;
	std::unordered_map<uint32_t, ModelMatrixDynamicBuffer*>		modelMatrixDynamicBufferMap;
	std::queue<Renderable*>	renderQueue;
};