#include "EntityManager.h"

#include <stdint.h>

std::unordered_map<uint32_t, std::list<Component*>> Entity::mComponentPoolMap;

EntityManager::EntityManager()
{
	mEntityIDCounter = 1;
}

EntityManager::~EntityManager()
{
	for (std::pair<EntityID, Entity*> entity : mEntities)
	{
		delete entity.second;
	}
	mEntities.clear();
}

Entity::~Entity()
{
	for (std::pair<uint32, Component*> component : mComponents)
	{
		Component* pComponent = component.second;

		uint32_t type = pComponent->getType();
		std::unordered_map<uint32_t, std::list<Component*>>::iterator itr = mComponentPoolMap.find(type);
		if (itr != mComponentPoolMap.end())
		{
			itr->second.remove(pComponent);
		}

		delete pComponent;
	}
	mComponents.clear();
}

void Entity::AddComponent(Component* component)
{
	uint32_t type = component->getType();
	mComponents.insert({ type, component });

	std::unordered_map<uint32_t, std::list<Component*>>::iterator itr = mComponentPoolMap.find(type);
	if (itr == mComponentPoolMap.end())
	{
		std::list<Component*> list;
		list.push_front(component);
		mComponentPoolMap.insert({ type, list });
	}
	else
	{
		itr->second.push_front(component);
	}
}