#include "Component.h"

ComponentRegistrar* ComponentRegistrar::instance = NULL;

ComponentRegistrar* ComponentRegistrar::getInstance()
{
	if (!instance)
	{
		instance = new ComponentRegistrar;
		return instance;
	}
	return instance;
}

void ComponentRegistrar::destroyInstance()
{
	if (ComponentRegistrar::instance)
	{
		delete instance;
	}
}

void ComponentRegistrar::mapInsertion(uint32_t str, ComponentGeneratorFctPtr ptr)
{
	componentGeneratorMap.insert(std::pair<uint32_t, ComponentGeneratorFctPtr>(str, ptr));
}

Component* ComponentRegistrar::GetComponent(const char* component)
{
	auto itr = ComponentRegistrar::getInstance()->componentGeneratorMap.find((uint32_t)std::hash<std::string>{}(component));
	if (itr != ComponentRegistrar::getInstance()->componentGeneratorMap.end())
	{
		return itr->second();
	}

	return nullptr;
}

void Component::instertIntoComponentGeneratorMap(uint32_t component_name, Component* (*func)())
{
	ComponentRegistrar::getInstance()->mapInsertion(component_name, func);
}