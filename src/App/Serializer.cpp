#include <unordered_map>
#include <string>

#include "Serializer.h"
#include "../Engine/ECS/EntityManager.h"
#include "../Engine/FileSystem.h"
#include "../../include/tinygltf/json.hpp"

void InitSerializer(Serializer** a_ppSerializer)
{}

void ExitSerializer(Serializer** a_ppSerializer)
{
	for (std::pair<std::string, void*> levelData : (*a_ppSerializer)->cachedLevelsData)
	{
		delete (nlohmann::json*)levelData.second;
	}
	(*a_ppSerializer)->cachedLevelsData.clear();
}

void LoadLevel(Serializer* a_pSerializer, const char* a_sPath, EntityManager* a_pEntityManager, uint32_t* a_uEntityCount, Entity** a_ppEntities)
{
	std::unordered_map<std::string, void*>::const_iterator cache_itr = a_pSerializer->cachedLevelsData.find(a_sPath);
	if (cache_itr != a_pSerializer->cachedLevelsData.end())
		return;

	char* str = 0;
	uint32_t length = 0;

	FileHandle file = FileOpen(a_sPath, "r");
	if (!file)
		return;
	
	length = FileSize(file);
	str = (char*)malloc(length * sizeof(char));
	int bytesRead = FileRead(file, &str, length);
	FileClose(file);

	nlohmann::json* pJson = new nlohmann::json;
	*pJson = nlohmann::json::parse(str, str + bytesRead);
	free(str);

	if (!pJson->is_object())
		return;

	uint32_t entityCount = 0;
	nlohmann::json entities = pJson->get<nlohmann::json>();
	nlohmann::json::const_iterator entities_itr = entities.begin();
	for (entities_itr; entities_itr != entities.end(); ++entities_itr)
	{
		EntityID id = a_pEntityManager->createEntity();
		Entity* pEntity = a_pEntityManager->getEntityByID(id);

		nlohmann::json components = entities_itr->get<nlohmann::json>();

		nlohmann::json::const_iterator components_itr = components.begin();
		for (components_itr; components_itr != components.end(); ++components_itr)
		{
			nlohmann::json elements = components_itr->get<nlohmann::json>();
			std::string component_name = components_itr.key();
			
			Component* pNewComponent = ComponentRegistrar::getInstance()->GetComponent(component_name.c_str());
			if (!pNewComponent)
				continue;

			ComponentRepresentation* rep = pNewComponent->createRepresentation();
			std::unordered_map<std::string, uint32> vars = rep->getVarNames();

			for (std::pair<std::string, uint32> var : vars)
			{
				nlohmann::json::const_iterator element = elements.find(var.first);
				if (element != elements.end())
				{
					if (element->is_number_unsigned())
					{
						uint32_t val = element->get<uint32_t>();
						rep->setVar(var.first.c_str(), &val);
					}
					else if (element->is_number_integer())
					{
						int val = element->get<int>();
						rep->setVar(var.first.c_str(), &val);
					}
					else if (element->is_number_float())
					{
						float val = element->get<float>();
						rep->setVar(var.first.c_str(), &val);
					}
					else if (element->is_string())
					{
						std::string valStr = element->get<std::string>();
						const char* val = valStr.c_str();
						rep->setStr(var.first.c_str(), &val);
					}
					else if (element->is_boolean())
					{
						bool val = element->get<bool>();
						rep->setVar(var.first.c_str(), &val);
					}
				}
			}

			pNewComponent->SetOwnerID(id);
			pEntity->AddComponent(pNewComponent);
		}

		a_ppEntities[entityCount++] = pEntity;
	}

	a_pSerializer->cachedLevelsData[a_sPath] = pJson;
}