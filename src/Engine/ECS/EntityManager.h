#pragma once

#include "Component.h"
#include <list>

class Entity;
class EntityManager;

typedef unsigned int uint32;
typedef uint32		 EntityID;
typedef std::unordered_map<uint32, Component*> ComponentMap;
typedef std::unordered_map<EntityID, Entity*>  EntityMap;

class Entity
{
	friend class EntityManager;
public:
	Entity() : ID(0), mComponents() {}
	~Entity();

	template <typename T>
	T* GetComponent()
	{
		T* componentOut = nullptr;
		
		ComponentMap::iterator it = mComponents.find(T::getTypeStatic());

#ifdef _DEBUG
		if (it != mComponents.end())
#endif
			componentOut = (T*)it->second;

		return componentOut;
	}

	void AddComponent(Component* component);
private:

	EntityID ID;
	ComponentMap mComponents;
	static std::unordered_map<uint32_t, std::list<Component*>> mComponentPoolMap;
};

class EntityManager
{
public:
	EntityManager();
	~EntityManager();

	template <typename T>
	void addComponent(EntityID id)
	{
		Entity* pEntity = getEntityByID(id);
		if (pEntity)
		{
			T* pComponent = new T;
			pEntity->AddComponent(pComponent);
		}
	}

	EntityID createEntity()
	{
		Entity* pEntity = new Entity;
		pEntity->ID = mEntityIDCounter++;
		mEntities.emplace(pEntity->ID, pEntity);
		return pEntity->ID;
	}

	void destroyEntity(EntityID id)
	{
		Entity* pEntity = getEntityByID(id);
		if (pEntity)
		{
			delete pEntity;
		}
	}

	Entity* getEntityByID(EntityID id)
	{
		EntityMap::iterator itr = mEntities.find(id);
		if (itr != mEntities.end())
		{
			return itr->second;
		}
		return nullptr;
	}

	template <typename T>
	std::list<Component*> GetComponents();

private:
	EntityMap mEntities;
	EntityID  mEntityIDCounter;
};

template <typename T>
std::list<Component*> EntityManager::GetComponents()
{
	std::unordered_map<uint32_t, std::list<Component*>>::iterator itr = Entity::mComponentPoolMap.find(T::getTypeStatic());
	if (itr != Entity::mComponentPoolMap.end())
	{
		return itr->second;
	}
	return std::list<Component*>();
}