#pragma once

class EntityManager;
class Entity;

struct Serializer
{
	std::unordered_map<std::string, void*> cachedLevelsData;

	Serializer() :
		cachedLevelsData()
	{}
};

void InitSerializer(Serializer** a_ppSerializer);
void ExitSerializer(Serializer** a_ppSerializer);
void LoadLevel(Serializer* a_pSerializer, const char* a_sPath, EntityManager* a_pEntityManager, uint32_t* a_uEntityCount, Entity** a_ppEntities);