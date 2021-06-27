#pragma once

#include <string>
#include <vector>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "../../include/glm/glm.hpp"
#include "../../include/glm/gtc/matrix_transform.hpp"
#include "../../include/glm/gtc/type_ptr.hpp"
#include "../../include/glm/gtx/string_cast.hpp"

#include "../../include/tinygltf/tiny_gltf.h"

struct Node;
struct Texture;
struct Sampler;
struct Buffer;
struct DescriptorSet;
struct CommandBuffer;
struct Renderer;

#define MAX_NUM_JOINTS 128u

struct BoundingBox
{
	glm::vec3 min;
	glm::vec3 max;
	bool valid = false;
	BoundingBox();
	BoundingBox(glm::vec3 min, glm::vec3 max);
	BoundingBox getAABB(glm::mat4 m);
};

struct ModelTexture
{
	Texture* texture;
	Sampler* sampler;
};

struct Material
{
	enum AlphaMode { ALPHAMODE_OPAQUE, ALPHAMODE_MASK, ALPHAMODE_BLEND };
	AlphaMode alphaMode = ALPHAMODE_OPAQUE;
	float alphaCutoff = 1.0f;
	float metallicFactor = 1.0f;
	float roughnessFactor = 1.0f;
	glm::vec4 baseColorFactor = glm::vec4(1.0f);
	glm::vec4 emissiveFactor = glm::vec4(1.0f);
	ModelTexture* baseColorTexture;
	ModelTexture* metallicRoughnessTexture;
	ModelTexture* normalTexture;
	ModelTexture* occlusionTexture;
	ModelTexture* emissiveTexture;
	struct TexCoordSets
	{
		uint8_t baseColor = 0;
		uint8_t metallicRoughness = 0;
		uint8_t specularGlossiness = 0;
		uint8_t normal = 0;
		uint8_t occlusion = 0;
		uint8_t emissive = 0;
	} texCoordSets;
	struct Extension
	{
		ModelTexture* specularGlossinessTexture;
		ModelTexture* diffuseTexture;
		glm::vec4 diffuseFactor = glm::vec4(1.0f);
		glm::vec3 specularFactor = glm::vec3(0.0f);
	} extension;
	struct PbrWorkflows
	{
		bool metallicRoughness = true;
		bool specularGlossiness = false;
	} pbrWorkflows;
	DescriptorSet* descriptorSet = nullptr;
	uint32_t indexInDescriptorSet = 0;
};

struct Primitive {
	uint32_t firstIndex;
	uint32_t indexCount;
	uint32_t vertexCount;
	Material& material;
	bool hasIndices;
	BoundingBox bb;
	Primitive(uint32_t firstIndex, uint32_t indexCount, uint32_t vertexCount, Material& material);
	void setBoundingBox(glm::vec3 min, glm::vec3 max);
};

struct Mesh {
	Renderer* pRenderer;
	std::vector<Primitive*> primitives;
	BoundingBox bb;
	BoundingBox aabb;
	Buffer* uniformBuffer;
	DescriptorSet* descriptorSet = nullptr;
	uint32_t indexInDescriptorSet = 0;
	struct UniformBlock {
		glm::mat4 matrix;
		glm::mat4 jointMatrix[MAX_NUM_JOINTS]{};
		float jointcount{ 0 };
	} uniformBlock;
	Mesh(Renderer* pRenderer, glm::mat4 matrix);
	~Mesh();
	void setBoundingBox(glm::vec3 min, glm::vec3 max);
};

struct Skin {
	std::string name;
	Node* skeletonRoot = nullptr;
	std::vector<glm::mat4> inverseBindMatrices;
	std::vector<Node*> joints;
};

struct Node {
	Node* parent;
	uint32_t index;
	std::vector<Node*> children;
	glm::mat4 matrix;
	std::string name;
	Mesh* mesh;
	Skin* skin;
	int32_t skinIndex = -1;
	glm::vec3 translation{};
	glm::vec3 scale{ 1.0f };
	glm::quat rotation{};
	BoundingBox bvh;
	BoundingBox aabb;
	glm::mat4 localMatrix();
	glm::mat4 getMatrix();
	void update();
	~Node();
};

struct AnimationChannel
{
	enum PathType { TRANSLATION, ROTATION, SCALE };
	PathType path;
	Node* node;
	uint32_t samplerIndex;
};

struct AnimationSampler
{
	enum InterpolationType { LINEAR, STEP, CUBICSPLINE };
	InterpolationType interpolation;
	std::vector<float> inputs;
	std::vector<glm::vec4> outputsVec4;
};

struct Animation
{
	std::string name;
	std::vector<AnimationSampler> samplers;
	std::vector<AnimationChannel> channels;
	float start = std::numeric_limits<float>::max();
	float end = std::numeric_limits<float>::min();
};

struct Model
{
	Renderer* pRenderer;

	struct Vertex
	{
		glm::vec3 pos;
		glm::vec3 normal;
		glm::vec2 uv0;
		glm::vec2 uv1;
		glm::vec4 joint0;
		glm::vec4 weight0;
	};

	Buffer* vertices;
	Buffer* indices;

	glm::mat4 aabb;

	std::vector<Node*> nodes;
	std::vector<Node*> linearNodes;

	std::vector<Skin*> skins;

	std::vector<ModelTexture*> textures;
	std::vector<Sampler*> textureSamplers;
	std::vector<Material> materials;
	std::vector<Animation> animations;
	std::vector<std::string> extensions;

	struct Dimensions
	{
		glm::vec3 min = glm::vec3(FLT_MAX);
		glm::vec3 max = glm::vec3(-FLT_MAX);
	} dimensions;

	void destroy();
	void loadNode(Node* parent, const tinygltf::Node& node, uint32_t nodeIndex, const tinygltf::Model& model, std::vector<uint32_t>& indexBuffer, std::vector<Vertex>& vertexBuffer, float globalscale);
	void loadSkins(tinygltf::Model& gltfModel);
	void loadTextures(Renderer* pRenderer, tinygltf::Model& gltfModel);
	void loadTextureSamplers(tinygltf::Model& gltfModel);
	void loadMaterials(tinygltf::Model& gltfModel);
	void loadAnimations(tinygltf::Model& gltfModel);
	void loadFromFile(Renderer* pRenderer, std::string filename, float scale = 1.0f);
	void drawNode(Node* node, CommandBuffer* commandBuffer);
	void draw(CommandBuffer* commandBuffer);
	void calculateBoundingBox(Node* node, Node* parent);
	void getSceneDimensions();
	void updateAnimation(uint32_t index, float time);
	Node* findNode(Node* parent, uint32_t index);
	Node* nodeFromIndex(uint32_t index);
};