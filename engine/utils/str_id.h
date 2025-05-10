#pragma once

#include "str_id_consteval.h"

// A uint32_t which can be feed with strings
struct StrId {
	uint32_t id = 0;
	StrId( ) = default;
	constexpr StrId( uint32_t new_id ) : id( new_id ) { };
	StrId(const char* text) : id(getID(text)) {}

	operator uint32_t() const { return id; }
	bool operator==(const StrId& other) const { return id == other.id; }
	bool operator!=(const StrId& other) const { return !(id == other.id); }
	bool operator==(uint32_t other) const { return id == other; }
	bool operator!=(uint32_t other) const { return !(id == other); }
	const char* c_str() const { return getStr( id ); }
};

