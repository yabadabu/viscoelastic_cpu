#pragma once

struct IResource {
	char name[64] = { 0x00 };
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


const IResource* getResource(const char* name);
void destroyAllResources();
void addResource(IResource* res);

template< typename T >
const T* Resource(const char* res_name) {
	return getResource(res_name)->template as<T>();
}
