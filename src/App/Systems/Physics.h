#pragma once

class ColliderComponent;
struct Collider;

class Physics
{
public:
	Physics();
	~Physics();

	void Update(float dt);
	std::vector<std::pair<Collider*, Collider*>>& GetCollisions();

	void AddColliderComponent(ColliderComponent* a_pColliderComponent);
	void RemoveColliderComponent(ColliderComponent* a_pColliderComponent);
};