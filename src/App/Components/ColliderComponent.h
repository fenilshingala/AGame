#pragma once

enum ColliderType
{
	AABB = 0,
	SPHERE,
	COUNT
};

struct Collider
{
	ColliderType mColliderType = ColliderType::AABB;
	float		 mCenter[3];
	float		 mR[3]; // radius or halfwidth extents
};

DECLARE_COMPONENT(ColliderComponent)
public:
	ColliderComponent();
	virtual void Init() override;
	virtual void Exit() override;
	virtual void Load() override;
	virtual void Unload() override;

	void UpdateScaledCollider();
	uint32_t GetModelMatrixIndexInBuffer() { return modelMatrixIndexInBuffer; }
	
	Collider mScaledCollider;

private:
	Collider mCollider;
	uint32_t modelMatrixIndexInBuffer;
END



REGISTER_COMPONENT_CLASS(ColliderComponent)
END