#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_IMPLEMENTATION

#if defined(__ANDROID_API__)
#define TINYGLTF_ANDROID_LOAD_FROM_ASSETS
#endif

#include "../ModelLoader.h"
#include "../Renderer.h"
#include "../Log.h"

void LoadNode(Node* parent, const tinygltf::Node& node, uint32_t nodeIndex, const tinygltf::Model& model,
	std::vector<uint32_t>& indexBuffer, std::vector<Model::Vertex>& vertexBuffer, float globalscale, Model* a_pModel);
void LoadSkins(tinygltf::Model& gltfModel, Model* a_pModel);
void LoadTextures(Renderer* pRenderer, tinygltf::Model& gltfModel, Model* a_pModel);
void LoadTextureSamplers(Renderer* a_pRenderer, tinygltf::Model& gltfModel, Model* a_pModel);
void LoadMaterials(tinygltf::Model& gltfModel, Model* a_pModel);
void LoadAnimations(tinygltf::Model& gltfModel, Model* a_pModel);
void DrawNode(Node* node, CommandBuffer* commandBuffer);
void Draw(CommandBuffer* commandBuffer, Model* a_pModel);
void CalculateBoundingBox(Node* node, Node* parent, Model* a_pModel);
void GetSceneDimensions(Model* a_pModel);
void UpdateAnimation(uint32_t index, float time, Model* a_pModel);
Node* FindNode(Node* parent, uint32_t index);
Node* NodeFromIndex(uint32_t index, Model* a_pModel);

// Bounding box

BoundingBox::BoundingBox()
{
}

BoundingBox::BoundingBox(glm::vec3 a_vMin, glm::vec3 a_vMax)
	: min(a_vMin), max(a_vMax)
{
}

BoundingBox BoundingBox::getAABB(glm::mat4 a_mM)
{
	glm::vec3 min = glm::vec3(a_mM[3]);
	glm::vec3 max = min;
	glm::vec3 v0, v1;

	glm::vec3 right = glm::vec3(a_mM[0]);
	v0 = right * this->min.x;
	v1 = right * this->max.x;
	min += glm::min(v0, v1);
	max += glm::max(v0, v1);

	glm::vec3 up = glm::vec3(a_mM[1]);
	v0 = up * this->min.y;
	v1 = up * this->max.y;
	min += glm::min(v0, v1);
	max += glm::max(v0, v1);

	glm::vec3 back = glm::vec3(a_mM[2]);
	v0 = back * this->min.z;
	v1 = back * this->max.z;
	min += glm::min(v0, v1);
	max += glm::max(v0, v1);

	return BoundingBox(min, max);
}

// Primitive
Primitive::Primitive(uint32_t a_uFirstIndex, uint32_t a_uIndexCount, uint32_t a_uVertexCount, Material& a_Material)
	: firstIndex(a_uFirstIndex), indexCount(a_uIndexCount), vertexCount(a_uVertexCount), material(a_Material)
{
	hasIndices = indexCount > 0;
};

void Primitive::setBoundingBox(glm::vec3 a_vMin, glm::vec3 a_vMax)
{
	bb.min = a_vMin;
	bb.max = a_vMax;
	bb.valid = true;
}

// Mesh
Mesh::Mesh(Renderer* a_pRenderer, glm::mat4 a_mMatrix)
{
	this->pRenderer = a_pRenderer;
	this->uniformBlock.matrix = a_mMatrix;
	
	uniformBuffer = new Buffer();
	uniformBuffer->desc = {
		(uint64_t)(sizeof(uniformBlock)),
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		(void*)(&uniformBlock)
	};
	CreateBuffer(this->pRenderer, &uniformBuffer);
};

Mesh::~Mesh()
{
	DestroyBuffer(pRenderer, &uniformBuffer);
	for (Primitive* p : primitives)
		delete p;
}

void Mesh::setBoundingBox(glm::vec3 a_vMin, glm::vec3 a_vMax)
{
	bb.min = a_vMin;
	bb.max = a_vMax;
	bb.valid = true;
}

// Node
glm::mat4 Node::localMatrix()
{
	return glm::translate(glm::mat4(1.0f), translation) * glm::mat4(rotation) * glm::scale(glm::mat4(1.0f), scale) * matrix;
}

glm::mat4 Node::getMatrix()
{
	glm::mat4 m = localMatrix();
	Node* p = parent;
	while (p) {
		m = p->localMatrix() * m;
		p = p->parent;
	}
	return m;
}

void Node::update()
{
	if (mesh)
	{
		glm::mat4 m = getMatrix();
		if (skin)
		{
			mesh->uniformBlock.matrix = m;
			// Update join matrices
			glm::mat4 inverseTransform = glm::inverse(m);
			size_t numJoints = std::min((uint32_t)skin->joints.size(), MAX_NUM_JOINTS);
			for (size_t i = 0; i < numJoints; i++)
			{
				Node* jointNode = skin->joints[i];
				glm::mat4 jointMat = jointNode->getMatrix() * skin->inverseBindMatrices[i];
				jointMat = inverseTransform * jointMat;
				mesh->uniformBlock.jointMatrix[i] = jointMat;
			}
			mesh->uniformBlock.jointcount = (float)numJoints;
			UpdateBuffer(mesh->pRenderer, mesh->uniformBuffer, &mesh->uniformBlock, sizeof(mesh->uniformBlock));
		}
		else
			UpdateBuffer(mesh->pRenderer, mesh->uniformBuffer, &m, sizeof(glm::mat4));
	}

	for (Node* child : children)
		child->update();
}

Node::~Node()
{
	if (mesh)
		delete mesh;
	for (Node* child : children)
		delete child;
}

// Model

void CreateModelFromFile(Renderer* a_pRenderer, std::string a_sFilename, Model* a_pModel, float a_fScale)
{
	tinygltf::Model gltfModel;
	tinygltf::TinyGLTF gltfContext;
	std::string error;
	std::string warning;

	a_pModel->pRenderer = a_pRenderer;

	bool binary = false;
	size_t extpos = a_sFilename.rfind('.', a_sFilename.length());
	if (extpos != std::string::npos) {
		binary = (a_sFilename.substr(extpos + 1, a_sFilename.length() - extpos) == "glb");
	}

	bool fileLoaded = binary ? gltfContext.LoadBinaryFromFile(&gltfModel, &error, &warning, a_sFilename.c_str()) : gltfContext.LoadASCIIFromFile(&gltfModel, &error, &warning, a_sFilename.c_str());

	std::vector<uint32_t> indexBuffer;
	std::vector<Model::Vertex> vertexBuffer;

	if (fileLoaded) {
		LoadTextureSamplers(a_pRenderer, gltfModel, a_pModel);
		LoadTextures(a_pModel->pRenderer, gltfModel, a_pModel);
		LoadMaterials(gltfModel, a_pModel);
		// TODO: scene handling with no default scene
		const tinygltf::Scene& scene = gltfModel.scenes[gltfModel.defaultScene > -1 ? gltfModel.defaultScene : 0];
		for (size_t i = 0; i < scene.nodes.size(); i++) {
			const tinygltf::Node node = gltfModel.nodes[scene.nodes[i]];
			LoadNode(nullptr, node, scene.nodes[i], gltfModel, indexBuffer, vertexBuffer, a_fScale, a_pModel);
		}
		if (gltfModel.animations.size() > 0) {
			LoadAnimations(gltfModel, a_pModel);
		}
		LoadSkins(gltfModel, a_pModel);

		for (Node* node : a_pModel->linearNodes) {
			// Assign skins
			if (node->skinIndex > -1) {
				node->skin = a_pModel->skins[node->skinIndex];
			}
			// Initial pose
			if (node->mesh) {
				node->update();
			}
		}
	}
	else {
		LOG(LogSeverity::ERR, "Could not load gltf file: %s", error.c_str());
		return;
	}

	a_pModel->extensions = gltfModel.extensionsUsed;

	a_pModel->vertices = new Buffer();
	a_pModel->indices = new Buffer();

	size_t vertexBufferSize = vertexBuffer.size() * sizeof(Model::Vertex);
	size_t indexBufferSize = indexBuffer.size() * sizeof(uint32_t);

	assert(vertexBufferSize > 0);

	// Vertex data
	a_pModel->vertices->desc.bufferUsageFlags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	a_pModel->vertices->desc.memoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	a_pModel->vertices->desc.bufferSize = vertexBufferSize;
	a_pModel->vertices->desc.pData = vertexBuffer.data();
	CreateBuffer(a_pModel->pRenderer, &a_pModel->vertices);

	// Index data
	if (indexBufferSize > 0) {
		a_pModel->indices->desc.bufferUsageFlags = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		a_pModel->indices->desc.memoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		a_pModel->indices->desc.bufferSize = indexBufferSize;
		a_pModel->indices->desc.pData = indexBuffer.data();
		CreateBuffer(a_pModel->pRenderer, &a_pModel->indices);
	}

	GetSceneDimensions(a_pModel);
}

void DestroyModel(Model* a_pModel)
{
	if (a_pModel->vertices->buffer != VK_NULL_HANDLE) {
		DestroyBuffer(a_pModel->pRenderer, &a_pModel->vertices);
	}
	if (a_pModel->indices->buffer != VK_NULL_HANDLE) {
		DestroyBuffer(a_pModel->pRenderer, &a_pModel->indices);
		a_pModel->indices->buffer = VK_NULL_HANDLE;
	}
	for (Sampler*& pSampler : a_pModel->samplers)
	{
		DestroySampler(a_pModel->pRenderer, &pSampler);
		delete pSampler;
		pSampler = nullptr;
	}
	for (TextureSampler*& pTextureSampler : a_pModel->textures)
	{
		DestroyTexture(a_pModel->pRenderer, &pTextureSampler->texture);
		delete pTextureSampler;
		pTextureSampler = nullptr;
	}
	
	a_pModel->textures.resize(0);
	a_pModel->samplers.resize(0);
	for (Node* node : a_pModel->nodes) {
		delete node;
		node = nullptr;
	}
	a_pModel->materials.resize(0);
	a_pModel->animations.resize(0);
	a_pModel->nodes.resize(0);
	a_pModel->linearNodes.resize(0);
	a_pModel->extensions.resize(0);
	for (Skin* skin : a_pModel->skins) {
		delete skin;
		skin = nullptr;
	}
	a_pModel->skins.resize(0);
};

void LoadNode(Node* a_Parent, const tinygltf::Node& a_Node, uint32_t a_uNodeIndex, const tinygltf::Model& a_Model,
					 std::vector<uint32_t>& a_IndexBuffer, std::vector<Model::Vertex>& a_VertexBuffer, float a_fGlobalscale, Model* a_pModel)
{
	Node* newNode = new Node{};
	newNode->index = a_uNodeIndex;
	newNode->parent = a_Parent;
	newNode->name = a_Node.name;
	newNode->skinIndex = a_Node.skin;
	newNode->matrix = glm::mat4(1.0f);

	// Generate local node matrix
	glm::vec3 translation = glm::vec3(0.0f);
	if (a_Node.translation.size() == 3) {
		translation = glm::make_vec3(a_Node.translation.data());
		newNode->translation = translation;
	}
	if (a_Node.rotation.size() == 4) {
		glm::quat q = glm::make_quat(a_Node.rotation.data());
		newNode->rotation = glm::mat4(q);
	}
	glm::vec3 scale = glm::vec3(1.0f);
	if (a_Node.scale.size() == 3) {
		scale = glm::make_vec3(a_Node.scale.data());
		newNode->scale = scale;
	}
	if (a_Node.matrix.size() == 16) {
		newNode->matrix = glm::make_mat4x4(a_Node.matrix.data());
	};

	// Node with children
	if (a_Node.children.size() > 0) {
		for (size_t i = 0; i < a_Node.children.size(); i++) {
			LoadNode(newNode, a_Model.nodes[a_Node.children[i]], a_Node.children[i], a_Model, a_IndexBuffer, a_VertexBuffer, a_fGlobalscale, a_pModel);
		}
	}

	// Node contains mesh data
	if (a_Node.mesh > -1) {
		const tinygltf::Mesh mesh = a_Model.meshes[a_Node.mesh];
		Mesh* newMesh = new Mesh(a_pModel->pRenderer, newNode->matrix);
		for (size_t j = 0; j < mesh.primitives.size(); j++) {
			const tinygltf::Primitive& primitive = mesh.primitives[j];
			uint32_t indexStart = static_cast<uint32_t>(a_IndexBuffer.size());
			uint32_t vertexStart = static_cast<uint32_t>(a_VertexBuffer.size());
			uint32_t indexCount = 0;
			uint32_t vertexCount = 0;
			glm::vec3 posMin{};
			glm::vec3 posMax{};
			bool hasSkin = false;
			bool hasIndices = primitive.indices > -1;
			// Vertices
			{
				const float* bufferPos = nullptr;
				const float* bufferNormals = nullptr;
				const float* bufferTexCoordSet0 = nullptr;
				const float* bufferTexCoordSet1 = nullptr;
				const void* bufferJoints = nullptr;
				const float* bufferWeights = nullptr;

				int posByteStride;
				int normByteStride;
				int uv0ByteStride;
				int uv1ByteStride;
				int jointByteStride;
				int weightByteStride;

				int jointComponentType;

				// Position attribute is required
				assert(primitive.attributes.find("POSITION") != primitive.attributes.end());

				const tinygltf::Accessor& posAccessor = a_Model.accessors[primitive.attributes.find("POSITION")->second];
				const tinygltf::BufferView& posView = a_Model.bufferViews[posAccessor.bufferView];
				bufferPos = reinterpret_cast<const float*>(&(a_Model.buffers[posView.buffer].data[posAccessor.byteOffset + posView.byteOffset]));
				posMin = glm::vec3(posAccessor.minValues[0], posAccessor.minValues[1], posAccessor.minValues[2]);
				posMax = glm::vec3(posAccessor.maxValues[0], posAccessor.maxValues[1], posAccessor.maxValues[2]);
				vertexCount = static_cast<uint32_t>(posAccessor.count);
				posByteStride = posAccessor.ByteStride(posView) ? (posAccessor.ByteStride(posView) / sizeof(float)) : tinygltf::GetTypeSizeInBytes(TINYGLTF_TYPE_VEC3);

				if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
					const tinygltf::Accessor& normAccessor = a_Model.accessors[primitive.attributes.find("NORMAL")->second];
					const tinygltf::BufferView& normView = a_Model.bufferViews[normAccessor.bufferView];
					bufferNormals = reinterpret_cast<const float*>(&(a_Model.buffers[normView.buffer].data[normAccessor.byteOffset + normView.byteOffset]));
					normByteStride = normAccessor.ByteStride(normView) ? (normAccessor.ByteStride(normView) / sizeof(float)) : tinygltf::GetTypeSizeInBytes(TINYGLTF_TYPE_VEC3);
				}

				if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
					const tinygltf::Accessor& uvAccessor = a_Model.accessors[primitive.attributes.find("TEXCOORD_0")->second];
					const tinygltf::BufferView& uvView = a_Model.bufferViews[uvAccessor.bufferView];
					bufferTexCoordSet0 = reinterpret_cast<const float*>(&(a_Model.buffers[uvView.buffer].data[uvAccessor.byteOffset + uvView.byteOffset]));
					uv0ByteStride = uvAccessor.ByteStride(uvView) ? (uvAccessor.ByteStride(uvView) / sizeof(float)) : tinygltf::GetTypeSizeInBytes(TINYGLTF_TYPE_VEC2);
				}
				if (primitive.attributes.find("TEXCOORD_1") != primitive.attributes.end()) {
					const tinygltf::Accessor& uvAccessor = a_Model.accessors[primitive.attributes.find("TEXCOORD_1")->second];
					const tinygltf::BufferView& uvView = a_Model.bufferViews[uvAccessor.bufferView];
					bufferTexCoordSet1 = reinterpret_cast<const float*>(&(a_Model.buffers[uvView.buffer].data[uvAccessor.byteOffset + uvView.byteOffset]));
					uv1ByteStride = uvAccessor.ByteStride(uvView) ? (uvAccessor.ByteStride(uvView) / sizeof(float)) : tinygltf::GetTypeSizeInBytes(TINYGLTF_TYPE_VEC2);
				}

				// Skinning
				// Joints
				if (primitive.attributes.find("JOINTS_0") != primitive.attributes.end()) {
					const tinygltf::Accessor& jointAccessor = a_Model.accessors[primitive.attributes.find("JOINTS_0")->second];
					const tinygltf::BufferView& jointView = a_Model.bufferViews[jointAccessor.bufferView];
					bufferJoints = &(a_Model.buffers[jointView.buffer].data[jointAccessor.byteOffset + jointView.byteOffset]);
					jointComponentType = jointAccessor.componentType;
					jointByteStride = jointAccessor.ByteStride(jointView) ? (jointAccessor.ByteStride(jointView) / tinygltf::GetComponentSizeInBytes(jointComponentType)) : tinygltf::GetTypeSizeInBytes(TINYGLTF_TYPE_VEC4);
				}

				if (primitive.attributes.find("WEIGHTS_0") != primitive.attributes.end()) {
					const tinygltf::Accessor& weightAccessor = a_Model.accessors[primitive.attributes.find("WEIGHTS_0")->second];
					const tinygltf::BufferView& weightView = a_Model.bufferViews[weightAccessor.bufferView];
					bufferWeights = reinterpret_cast<const float*>(&(a_Model.buffers[weightView.buffer].data[weightAccessor.byteOffset + weightView.byteOffset]));
					weightByteStride = weightAccessor.ByteStride(weightView) ? (weightAccessor.ByteStride(weightView) / sizeof(float)) : tinygltf::GetTypeSizeInBytes(TINYGLTF_TYPE_VEC4);
				}

				hasSkin = (bufferJoints && bufferWeights);

				for (size_t v = 0; v < posAccessor.count; v++) {
					Model::Vertex vert{};
					vert.pos = glm::vec4(glm::make_vec3(&bufferPos[v * posByteStride]), 1.0f);
					vert.normal = glm::normalize(glm::vec3(bufferNormals ? glm::make_vec3(&bufferNormals[v * normByteStride]) : glm::vec3(0.0f)));
					vert.uv0 = bufferTexCoordSet0 ? glm::make_vec2(&bufferTexCoordSet0[v * uv0ByteStride]) : glm::vec3(0.0f);
					vert.uv1 = bufferTexCoordSet1 ? glm::make_vec2(&bufferTexCoordSet1[v * uv1ByteStride]) : glm::vec3(0.0f);

					if (hasSkin)
					{
						switch (jointComponentType) {
						case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
							const uint16_t* buf = static_cast<const uint16_t*>(bufferJoints);
							vert.joint0 = glm::vec4(glm::make_vec4(&buf[v * jointByteStride]));
							break;
						}
						case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
							const uint8_t* buf = static_cast<const uint8_t*>(bufferJoints);
							vert.joint0 = glm::vec4(glm::make_vec4(&buf[v * jointByteStride]));
							break;
						}
						default:
							// Not supported by spec
							LOG(LogSeverity::ERR, "Joint component type %d not supported!", jointComponentType);
							break;
						}
					}
					else {
						vert.joint0 = glm::vec4(0.0f);
					}
					vert.weight0 = hasSkin ? glm::make_vec4(&bufferWeights[v * weightByteStride]) : glm::vec4(0.0f);
					// Fix for all zero weights
					if (glm::length(vert.weight0) == 0.0f) {
						vert.weight0 = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
					}
					a_VertexBuffer.push_back(vert);
				}
			}
			// Indices
			if (hasIndices)
			{
				const tinygltf::Accessor& accessor = a_Model.accessors[primitive.indices > -1 ? primitive.indices : 0];
				const tinygltf::BufferView& bufferView = a_Model.bufferViews[accessor.bufferView];
				const tinygltf::Buffer& buffer = a_Model.buffers[bufferView.buffer];

				indexCount = static_cast<uint32_t>(accessor.count);
				const void* dataPtr = &(buffer.data[accessor.byteOffset + bufferView.byteOffset]);

				switch (accessor.componentType) {
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
					const uint32_t* buf = static_cast<const uint32_t*>(dataPtr);
					for (size_t index = 0; index < accessor.count; index++) {
						a_IndexBuffer.push_back(buf[index] + vertexStart);
					}
					break;
				}
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
					const uint16_t* buf = static_cast<const uint16_t*>(dataPtr);
					for (size_t index = 0; index < accessor.count; index++) {
						a_IndexBuffer.push_back(buf[index] + vertexStart);
					}
					break;
				}
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
					const uint8_t* buf = static_cast<const uint8_t*>(dataPtr);
					for (size_t index = 0; index < accessor.count; index++) {
						a_IndexBuffer.push_back(buf[index] + vertexStart);
					}
					break;
				}
				default:
					LOG(LogSeverity::ERR, "Index component type %d not supported!", accessor.componentType);
					return;
				}
			}
			Primitive* newPrimitive = new Primitive(indexStart, indexCount, vertexCount, primitive.material > -1 ? a_pModel->materials[primitive.material] : a_pModel->materials.back());
			newPrimitive->setBoundingBox(posMin, posMax);
			newMesh->primitives.push_back(newPrimitive);
		}
		// Mesh BB from BBs of primitives
		for (Primitive* p : newMesh->primitives) {
			if (p->bb.valid && !newMesh->bb.valid) {
				newMesh->bb = p->bb;
				newMesh->bb.valid = true;
			}
			newMesh->bb.min = glm::min(newMesh->bb.min, p->bb.min);
			newMesh->bb.max = glm::max(newMesh->bb.max, p->bb.max);
		}
		newNode->mesh = newMesh;
	}
	if (a_Parent) {
		a_Parent->children.push_back(newNode);
	}
	else {
		a_pModel->nodes.push_back(newNode);
	}
	a_pModel->linearNodes.push_back(newNode);
}

void LoadSkins(tinygltf::Model& a_GltfModel, Model* a_pModel)
{
	for (tinygltf::Skin& source : a_GltfModel.skins) {
		Skin* newSkin = new Skin{};
		newSkin->name = source.name;

		// Find skeleton root node
		if (source.skeleton > -1) {
			newSkin->skeletonRoot = NodeFromIndex(source.skeleton, a_pModel);
		}

		// Find joint nodes
		for (int jointIndex : source.joints) {
			Node* node = NodeFromIndex(jointIndex, a_pModel);
			if (node) {
				newSkin->joints.push_back(NodeFromIndex(jointIndex, a_pModel));
			}
		}

		// Get inverse bind matrices from buffer
		if (source.inverseBindMatrices > -1) {
			const tinygltf::Accessor& accessor = a_GltfModel.accessors[source.inverseBindMatrices];
			const tinygltf::BufferView& bufferView = a_GltfModel.bufferViews[accessor.bufferView];
			const tinygltf::Buffer& buffer = a_GltfModel.buffers[bufferView.buffer];
			newSkin->inverseBindMatrices.resize(accessor.count);
			memcpy(newSkin->inverseBindMatrices.data(), &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(glm::mat4));
		}

		a_pModel->skins.push_back(newSkin);
	}
}

void LoadTextures(Renderer* a_pRenderer, tinygltf::Model& a_GltfModel, Model* a_pModel)
{
	a_pModel->pRenderer = a_pRenderer;

	for (tinygltf::Texture& tex : a_GltfModel.textures) {
		tinygltf::Image image = a_GltfModel.images[tex.source];
		Sampler* pTextureSampler = nullptr;
		if (tex.sampler == -1) {
			// No sampler specified, use a default one
			pTextureSampler = new Sampler();
			pTextureSampler->desc.magFilter = VK_FILTER_LINEAR;
			pTextureSampler->desc.minFilter = VK_FILTER_LINEAR;
			pTextureSampler->desc.mipMapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
			pTextureSampler->desc.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			pTextureSampler->desc.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			pTextureSampler->desc.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		}
		else {
			pTextureSampler = a_pModel->samplers[tex.sampler];
		}

		unsigned char* buffer = nullptr;
		VkDeviceSize bufferSize = 0;
		bool deleteBuffer = false;
		if (image.component == 3) {
			// Most devices don't support RGB only on Vulkan so convert if necessary
			// TODO: Check actual format support and transform only if required
			bufferSize = image.width * image.height * 4;
			buffer = new unsigned char[bufferSize];
			unsigned char* rgba = buffer;
			unsigned char* rgb = &image.image[0];
			for (int32_t i = 0; i < image.width * image.height; ++i) {
				for (int32_t j = 0; j < 3; ++j) {
					rgba[j] = rgb[j];
				}
				rgba += 4;
				rgb += 3;
			}
			deleteBuffer = true;
		}
		else {
			buffer = &image.image[0];
			bufferSize = image.image.size();
		}

		Texture* pTexture = new Texture();
		pTexture->desc.rawData = buffer;
		pTexture->desc.rawDataSize = bufferSize;
		pTexture->desc.width = image.width;
		pTexture->desc.height = image.height;
		pTexture->desc.format = VK_FORMAT_R8G8B8A8_UNORM;
		pTexture->desc.tiling = VK_IMAGE_TILING_OPTIMAL;
		pTexture->desc.sampleCount = VK_SAMPLE_COUNT_1_BIT;
		pTexture->desc.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		pTexture->desc.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		pTexture->desc.aspectBits = VK_IMAGE_ASPECT_COLOR_BIT;
		pTexture->desc.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		pTexture->desc.mipMaps = true;
		CreateTexture(a_pRenderer, &pTexture);
		
		TextureSampler* pModelTexture = new TextureSampler();
		pModelTexture->texture = pTexture;
		pModelTexture->sampler = pTextureSampler;
		a_pModel->textures.push_back(pModelTexture);
	}
}

static VkSamplerAddressMode getVkWrapMode(int32_t a_iWrapMode)
{
	switch (a_iWrapMode) {
	case 10497:
		return VK_SAMPLER_ADDRESS_MODE_REPEAT;
	case 33071:
		return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	case 33648:
		return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
	}
	return VK_SAMPLER_ADDRESS_MODE_REPEAT;
}

static VkFilter getVkFilterMode(int32_t a_iFilterMode)
{
	switch (a_iFilterMode) {
	case 9728:
		return VK_FILTER_NEAREST;
	case 9729:
		return VK_FILTER_LINEAR;
	case 9984:
		return VK_FILTER_NEAREST;
	case 9985:
		return VK_FILTER_NEAREST;
	case 9986:
		return VK_FILTER_LINEAR;
	case 9987:
		return VK_FILTER_LINEAR;
	}
	return VK_FILTER_NEAREST;
}

void LoadTextureSamplers(Renderer* a_pRenderer, tinygltf::Model& a_GltfModel, Model* a_pModel)
{
	for (tinygltf::Sampler smpl : a_GltfModel.samplers)
	{
		Sampler* pSampler = new Sampler();
		pSampler->desc.minFilter = getVkFilterMode(smpl.minFilter);
		pSampler->desc.magFilter = getVkFilterMode(smpl.magFilter);
		pSampler->desc.addressModeU = getVkWrapMode(smpl.wrapS);
		pSampler->desc.addressModeV = getVkWrapMode(smpl.wrapT);
		pSampler->desc.addressModeW = pSampler->desc.addressModeV;
		pSampler->desc.mipMapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		CreateSampler(a_pRenderer, &pSampler);

		a_pModel->samplers.push_back(pSampler);
	}
}

void LoadMaterials(tinygltf::Model& a_GltfModel, Model* a_pModel)
{
	for (tinygltf::Material& mat : a_GltfModel.materials) {
		Material material{};
		if (mat.values.find("baseColorTexture") != mat.values.end()) {
			material.baseColorTexture = a_pModel->textures[mat.values["baseColorTexture"].TextureIndex()];
			material.texCoordSets.baseColor = mat.values["baseColorTexture"].TextureTexCoord();
		}
		if (mat.values.find("metallicRoughnessTexture") != mat.values.end()) {
			material.metallicRoughnessTexture = a_pModel->textures[mat.values["metallicRoughnessTexture"].TextureIndex()];
			material.texCoordSets.metallicRoughness = mat.values["metallicRoughnessTexture"].TextureTexCoord();
		}
		if (mat.values.find("roughnessFactor") != mat.values.end()) {
			material.roughnessFactor = static_cast<float>(mat.values["roughnessFactor"].Factor());
		}
		if (mat.values.find("metallicFactor") != mat.values.end()) {
			material.metallicFactor = static_cast<float>(mat.values["metallicFactor"].Factor());
		}
		if (mat.values.find("baseColorFactor") != mat.values.end()) {
			material.baseColorFactor = glm::make_vec4(mat.values["baseColorFactor"].ColorFactor().data());
		}
		if (mat.additionalValues.find("normalTexture") != mat.additionalValues.end()) {
			material.normalTexture = a_pModel->textures[mat.additionalValues["normalTexture"].TextureIndex()];
			material.texCoordSets.normal = mat.additionalValues["normalTexture"].TextureTexCoord();
		}
		if (mat.additionalValues.find("emissiveTexture") != mat.additionalValues.end()) {
			material.emissiveTexture = a_pModel->textures[mat.additionalValues["emissiveTexture"].TextureIndex()];
			material.texCoordSets.emissive = mat.additionalValues["emissiveTexture"].TextureTexCoord();
		}
		if (mat.additionalValues.find("occlusionTexture") != mat.additionalValues.end()) {
			material.occlusionTexture = a_pModel->textures[mat.additionalValues["occlusionTexture"].TextureIndex()];
			material.texCoordSets.occlusion = mat.additionalValues["occlusionTexture"].TextureTexCoord();
		}
		if (mat.additionalValues.find("alphaMode") != mat.additionalValues.end()) {
			tinygltf::Parameter param = mat.additionalValues["alphaMode"];
			if (param.string_value == "BLEND") {
				material.alphaMode = Material::AlphaMode::ALPHAMODE_BLEND;
			}
			if (param.string_value == "MASK") {
				material.alphaCutoff = 0.5f;
				material.alphaMode = Material::AlphaMode::ALPHAMODE_MASK;
			}
		}
		if (mat.additionalValues.find("alphaCutoff") != mat.additionalValues.end()) {
			material.alphaCutoff = static_cast<float>(mat.additionalValues["alphaCutoff"].Factor());
		}
		if (mat.additionalValues.find("emissiveFactor") != mat.additionalValues.end()) {
			material.emissiveFactor = glm::vec4(glm::make_vec3(mat.additionalValues["emissiveFactor"].ColorFactor().data()), 1.0);
			material.emissiveFactor = glm::vec4(0.0f);
		}

		// Extensions
		// @TODO: Find out if there is a nicer way of reading these properties with recent tinygltf headers
		if (mat.extensions.find("KHR_materials_pbrSpecularGlossiness") != mat.extensions.end()) {
			std::map<std::string, tinygltf::Value>::const_iterator ext = mat.extensions.find("KHR_materials_pbrSpecularGlossiness");
			if (ext->second.Has("specularGlossinessTexture")) {
				tinygltf::Value index = ext->second.Get("specularGlossinessTexture").Get("index");
				material.extension.specularGlossinessTexture = a_pModel->textures[index.Get<int>()];
				tinygltf::Value texCoordSet = ext->second.Get("specularGlossinessTexture").Get("texCoord");
				material.texCoordSets.specularGlossiness = texCoordSet.Get<int>();
				material.pbrWorkflows.specularGlossiness = true;
			}
			if (ext->second.Has("diffuseTexture")) {
				tinygltf::Value index = ext->second.Get("diffuseTexture").Get("index");
				material.extension.diffuseTexture = a_pModel->textures[index.Get<int>()];
			}
			if (ext->second.Has("diffuseFactor")) {
				tinygltf::Value factor = ext->second.Get("diffuseFactor");
				for (uint32_t i = 0; i < factor.ArrayLen(); i++) {
					tinygltf::Value val = factor.Get(i);
					material.extension.diffuseFactor[i] = val.IsNumber() ? (float)val.Get<double>() : (float)val.Get<int>();
				}
			}
			if (ext->second.Has("specularFactor")) {
				tinygltf::Value factor = ext->second.Get("specularFactor");
				for (uint32_t i = 0; i < factor.ArrayLen(); i++) {
					tinygltf::Value val = factor.Get(i);
					material.extension.specularFactor[i] = val.IsNumber() ? (float)val.Get<double>() : (float)val.Get<int>();
				}
			}
		}

		a_pModel->materials.push_back(material);
	}
	// Push a default material at the end of the list for meshes with no material assigned
	a_pModel->materials.push_back(Material());
}

void LoadAnimations(tinygltf::Model& a_GltfModel, Model* a_pModel)
{
	for (tinygltf::Animation& anim : a_GltfModel.animations) {
		Animation animation{};
		animation.name = anim.name;
		if (anim.name.empty()) {
			animation.name = std::to_string(a_pModel->animations.size());
		}

		// Samplers
		for (tinygltf::AnimationSampler samp : anim.samplers) {
			AnimationSampler sampler{};

			if (samp.interpolation == "LINEAR") {
				sampler.interpolation = AnimationSampler::InterpolationType::LINEAR;
			}
			if (samp.interpolation == "STEP") {
				sampler.interpolation = AnimationSampler::InterpolationType::STEP;
			}
			if (samp.interpolation == "CUBICSPLINE") {
				sampler.interpolation = AnimationSampler::InterpolationType::CUBICSPLINE;
			}

			// Read sampler input time values
			{
				const tinygltf::Accessor& accessor = a_GltfModel.accessors[samp.input];
				const tinygltf::BufferView& bufferView = a_GltfModel.bufferViews[accessor.bufferView];
				const tinygltf::Buffer& buffer = a_GltfModel.buffers[bufferView.buffer];

				assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);

				const void* dataPtr = &buffer.data[accessor.byteOffset + bufferView.byteOffset];
				const float* buf = static_cast<const float*>(dataPtr);
				for (size_t index = 0; index < accessor.count; index++) {
					sampler.inputs.push_back(buf[index]);
				}

				for (float input : sampler.inputs) {
					if (input < animation.start) {
						animation.start = input;
					};
					if (input > animation.end) {
						animation.end = input;
					}
				}
			}

			// Read sampler output T/R/S values 
			{
				const tinygltf::Accessor& accessor = a_GltfModel.accessors[samp.output];
				const tinygltf::BufferView& bufferView = a_GltfModel.bufferViews[accessor.bufferView];
				const tinygltf::Buffer& buffer = a_GltfModel.buffers[bufferView.buffer];

				assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);

				const void* dataPtr = &buffer.data[accessor.byteOffset + bufferView.byteOffset];

				switch (accessor.type) {
				case TINYGLTF_TYPE_VEC3: {
					const glm::vec3* buf = static_cast<const glm::vec3*>(dataPtr);
					for (size_t index = 0; index < accessor.count; index++) {
						sampler.outputsVec4.push_back(glm::vec4(buf[index], 0.0f));
					}
					break;
				}
				case TINYGLTF_TYPE_VEC4: {
					const glm::vec4* buf = static_cast<const glm::vec4*>(dataPtr);
					for (size_t index = 0; index < accessor.count; index++) {
						sampler.outputsVec4.push_back(buf[index]);
					}
					break;
				}
				default: {
					LOG(LogSeverity::INFO, "unknown type");
					break;
				}
				}
			}

			animation.samplers.push_back(sampler);
		}

		// Channels
		for (tinygltf::AnimationChannel source : anim.channels) {
			AnimationChannel channel{};

			if (source.target_path == "rotation") {
				channel.path = AnimationChannel::PathType::ROTATION;
			}
			if (source.target_path == "translation") {
				channel.path = AnimationChannel::PathType::TRANSLATION;
			}
			if (source.target_path == "scale") {
				channel.path = AnimationChannel::PathType::SCALE;
			}
			if (source.target_path == "weights") {
				LOG(LogSeverity::INFO, "weights not yet supported, skipping channel");
				continue;
			}
			channel.samplerIndex = source.sampler;
			channel.node = NodeFromIndex(source.target_node, a_pModel);
			if (!channel.node) {
				continue;
			}

			animation.channels.push_back(channel);
		}

		a_pModel->animations.push_back(animation);
	}
}

void DrawNode(Node* a_Node, CommandBuffer* a_pCommandBuffer)
{
	if (a_Node->mesh) {
		for (Primitive* primitive : a_Node->mesh->primitives) {
			vkCmdDrawIndexed(a_pCommandBuffer->commandBuffer, primitive->indexCount, 1, primitive->firstIndex, 0, 0);
		}
	}
	for (Node* child : a_Node->children) {
		DrawNode(child, a_pCommandBuffer);
	}
}

void Draw(CommandBuffer* a_pCommandBuffer, Model* a_pModel)
{
	const VkDeviceSize offsets[1] = { 0 };
	vkCmdBindVertexBuffers(a_pCommandBuffer->commandBuffer, 0, 1, &a_pModel->vertices->buffer, offsets);
	vkCmdBindIndexBuffer(a_pCommandBuffer->commandBuffer, a_pModel->indices->buffer, 0, VK_INDEX_TYPE_UINT32);
	for (Node* node : a_pModel->nodes) {
		DrawNode(node, a_pCommandBuffer);
	}
}

void CalculateBoundingBox(Node* a_Node, Node* a_Parent, Model* a_pModel) {
	BoundingBox parentBvh = a_Parent ? a_Parent->bvh : BoundingBox(a_pModel->dimensions.min, a_pModel->dimensions.max);

	if (a_Node->mesh) {
		if (a_Node->mesh->bb.valid) {
			a_Node->aabb = a_Node->mesh->bb.getAABB(a_Node->getMatrix());
			if (a_Node->children.size() == 0) {
				a_Node->bvh.min = a_Node->aabb.min;
				a_Node->bvh.max = a_Node->aabb.max;
				a_Node->bvh.valid = true;
			}
		}
	}

	parentBvh.min = glm::min(parentBvh.min, a_Node->bvh.min);
	parentBvh.max = glm::min(parentBvh.max, a_Node->bvh.max);

	for (Node* child : a_Node->children) {
		CalculateBoundingBox(child, a_Node, a_pModel);
	}
}

void GetSceneDimensions(Model* a_pModel)
{
	// Calculate binary volume hierarchy for all nodes in the scene
	for (Node* node : a_pModel->linearNodes) {
		CalculateBoundingBox(node, nullptr, a_pModel);
	}

	a_pModel->dimensions.min = glm::vec3(FLT_MAX);
	a_pModel->dimensions.max = glm::vec3(-FLT_MAX);

	for (Node* node : a_pModel->linearNodes) {
		if (node->bvh.valid) {
			a_pModel->dimensions.min = glm::min(a_pModel->dimensions.min, node->bvh.min);
			a_pModel->dimensions.max = glm::max(a_pModel->dimensions.max, node->bvh.max);
		}
	}

	// Calculate scene aabb
	a_pModel->aabb = glm::scale(glm::mat4(1.0f), glm::vec3(a_pModel->dimensions.max[0] - a_pModel->dimensions.min[0],
		a_pModel->dimensions.max[1] - a_pModel->dimensions.min[1],
		a_pModel->dimensions.max[2] - a_pModel->dimensions.min[2]));
	a_pModel->aabb[3][0] = a_pModel->dimensions.min[0];
	a_pModel->aabb[3][1] = a_pModel->dimensions.min[1];
	a_pModel->aabb[3][2] = a_pModel->dimensions.min[2];
}

void UpdateAnimation(uint32_t a_uIndex, float a_fTime, Model* a_pModel)
{
	if (a_pModel->animations.empty()) {
		LOG(LogSeverity::INFO, ".glTF does not contain animation.");
		return;
	}
	if (a_uIndex > static_cast<uint32_t>(a_pModel->animations.size()) - 1) {
		LOG(LogSeverity::INFO, "No animation with index %d", a_uIndex);
		return;
	}
	Animation& animation = a_pModel->animations[a_uIndex];

	bool updated = false;
	for (AnimationChannel channel : animation.channels) {
		AnimationSampler& sampler = animation.samplers[channel.samplerIndex];
		if (sampler.inputs.size() > sampler.outputsVec4.size()) {
			continue;
		}

		for (size_t i = 0; i < sampler.inputs.size() - 1; i++) {
			if ((a_fTime >= sampler.inputs[i]) && (a_fTime <= sampler.inputs[i + 1])) {
				float u = std::max(0.0f, a_fTime - sampler.inputs[i]) / (sampler.inputs[i + 1] - sampler.inputs[i]);
				if (u <= 1.0f) {
					switch (channel.path) {
					case AnimationChannel::PathType::TRANSLATION: {
						glm::vec4 trans = glm::mix(sampler.outputsVec4[i], sampler.outputsVec4[i + 1], u);
						channel.node->translation = glm::vec3(trans);
						break;
					}
					case AnimationChannel::PathType::SCALE: {
						glm::vec4 trans = glm::mix(sampler.outputsVec4[i], sampler.outputsVec4[i + 1], u);
						channel.node->scale = glm::vec3(trans);
						break;
					}
					case AnimationChannel::PathType::ROTATION: {
						glm::quat q1;
						q1.x = sampler.outputsVec4[i].x;
						q1.y = sampler.outputsVec4[i].y;
						q1.z = sampler.outputsVec4[i].z;
						q1.w = sampler.outputsVec4[i].w;
						glm::quat q2;
						q2.x = sampler.outputsVec4[i + 1].x;
						q2.y = sampler.outputsVec4[i + 1].y;
						q2.z = sampler.outputsVec4[i + 1].z;
						q2.w = sampler.outputsVec4[i + 1].w;
						channel.node->rotation = glm::normalize(glm::slerp(q1, q2, u));
						break;
					}
					}
					updated = true;
				}
			}
		}
	}
	if (updated) {
		for (Node* node : a_pModel->nodes) {
			node->update();
		}
	}
}

Node* FindNode(Node* a_Parent, uint32_t a_uIndex) {
	Node* nodeFound = nullptr;
	if (a_Parent->index == a_uIndex) {
		return a_Parent;
	}
	for (Node* child : a_Parent->children) {
		nodeFound = FindNode(child, a_uIndex);
		if (nodeFound) {
			break;
		}
	}
	return nodeFound;
}

Node* NodeFromIndex(uint32_t a_uIndex, Model* a_pModel) {
	Node* nodeFound = nullptr;
	for (Node* node : a_pModel->nodes) {
		nodeFound = FindNode(node, a_uIndex);
		if (nodeFound) {
			break;
		}
	}
	return nodeFound;
}