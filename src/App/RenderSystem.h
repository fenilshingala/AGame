#pragma once

struct Renderer;
struct Node;
struct CommandBuffer;
struct Material;
enum class AplhaMode;
class IApp;
class Camera;

class RenderSystem
{
public:
	RenderSystem() : pRenderer(nullptr) {}
	~RenderSystem() {}

	void Init(IApp* a_pApp);
	void Exit();
	void Load();
	void Unload();
	void Update();
	void DrawScene();

private:
	Renderer* pRenderer;
	Camera* pCamera;
};