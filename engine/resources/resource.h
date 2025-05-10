#pragma once

struct ENGINE_API IResource {
	char name[64];
	virtual ~IResource() { }
	virtual void destroy() { }
	const char* getName() const {
		return name;
	}
	virtual void setName( const char* new_name ) {
		strcpy(name, new_name);
	}

	template< typename T >
	const T* as( ) const {
		return static_cast<const T*>( this );
	}
};

using ResourceFactoryFn = IResource * (*)(const char*);

ENGINE_API const IResource* getResource(const char* name);
ENGINE_API void destroyAllResources();
ENGINE_API void addResourcesFactory(const char* res_typename, ResourceFactoryFn fn);
ENGINE_API void addResource(IResource* res);

//void onEachResource(jaba::Callback<void(IResource*)>&& fn);

template< typename T >
const T* Resource(const char* res_name) {
	return getResource(res_name)->template as<T>();
}
