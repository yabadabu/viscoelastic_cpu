#include "platform.h"
#include "resource.h"
#include <unordered_map>

class ResourcesManager {

	std::unordered_map< std::string, IResource* > resources;

public:

	const IResource* get(const char* name);
	void addResource(IResource* res);
	void destroyAll();

	template< typename Fn>
	void onEach(Fn&& fn) {
		for (auto it : resources)
			fn(it.second);
	}

};

const IResource* ResourcesManager::get(const char* name) {
	std::string name_id(name);
	auto it = resources.find(name_id);
	if (it != resources.end())
		return it->second;
	fatal("Failed to find resource %s", name);
	return nullptr;
}

void ResourcesManager::addResource(IResource* res) {
	assert(res);
	assert(strlen(res->getName()) > 0);
	std::string name_id(res->getName());
	resources[name_id] = res;
}

void ResourcesManager::destroyAll() {
	for (auto it : resources) {
		it.second->destroy();
		delete it.second;
	}
	resources.clear();
}

// ------------------------------------------------------------------------------------------------------
static ResourcesManager resources_manager;

const IResource* getResource(const char* name) {
	return resources_manager.get(name);
}

void addResource(IResource* res) {
	resources_manager.addResource( res );
}

void destroyAllResources() {
	resources_manager.destroyAll();
}