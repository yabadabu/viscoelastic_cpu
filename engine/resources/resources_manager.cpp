#include "platform.h"
#include "resource.h"
#include <unordered_map>

class ResourcesManager {

	using KeyType = std::string;

	std::unordered_map< KeyType, IResource* > resources;
	std::unordered_map< KeyType, ResourceFactoryFn >  factories;

public:

	const IResource* get(const char* name);
	void addResource(IResource* res);
	void addFactory(const char* res_typename, ResourceFactoryFn fn);
	void destroyAll();

	template< typename Fn>
	void onEach(Fn&& fn) {
		for (auto it : resources)
			fn(it.second);
	}

};

const IResource* ResourcesManager::get(const char* name) {
	KeyType name_id(name);
	auto it = resources.find(name_id);
	if (it != resources.end())
		return it->second;

	const char* ext = strrchr(name, '.');		// .mesh
	if (!ext) {
		fatal("Failed to identify extension in resource: '%s'\n", name);
		return nullptr;
	}

	KeyType res_typeid(ext + 1);
	auto fn = factories.find(res_typeid);
	if (fn == factories.end()) {
		fatal("Failed to find a factory for resources of type %s, resource name %s\n", ext, name);
		return nullptr;
	}
	assert(fn != factories.end());

	IResource* new_res = (*fn->second)(name);
	if (!new_res) {
		fatal("Failed to create resource '%s' using factory of type %s\n", name, ext);
		return nullptr;
	}

	new_res->setName(name);
	dbg("Adding resource %s\n", name);
	addResource(new_res);
	return new_res;
}

void ResourcesManager::addResource(IResource* res) {
	assert(res);
	assert(strlen(res->getName()) > 0);
	std::string name_id(res->getName());
	resources[name_id] = res;
}

void ResourcesManager::addFactory(const char* res_typename, ResourceFactoryFn fn) {
	assert(res_typename);
	assert(fn);
	std::string res_typeid(res_typename);
	factories[res_typeid] = fn;
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

void addResourcesFactory(const char* res_typename, ResourceFactoryFn fn) {
	resources_manager.addFactory( res_typename, fn );
}

void addResource(IResource* res) {
	resources_manager.addResource( res );
}

void destroyAllResources() {
	resources_manager.destroyAll();
}