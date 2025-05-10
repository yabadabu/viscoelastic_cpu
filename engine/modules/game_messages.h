#pragma once

struct ResolvedData {
	const char* str = nullptr;
	int         age = 0;
	TStr16      buf;
	bool        boolean_value = false;
	float       float_value = 0.0f;
	StrId       id;
	const void* custom_data = nullptr;

	void setInt(int v) {
		buf.format("%d", v);
		str = buf.c_str();
	}
	void setFloat(float v, const char* fmt = "%1.6f") {
		float_value = v;
		buf.format(fmt, v);
		str = buf.c_str();
	}
	void setBool(bool new_value) {
		boolean_value = new_value;
	}
};

using ResolverFn = jaba::Callback<void(ResolvedData&)>;

struct GameMsgRegisterResolver {
	StrId      id;
	ResolverFn resolver;
};

struct GameMsgUnregisterResolver {
	StrId		id;
};

struct GameMsgExplosionAt {
	VEC3    pos;
};

// -------------------------------------------------------------------
struct GameMsgInteractWithPOI {
	uint32_t id;
};

namespace OSM {
	struct TileCoordsHiPrec;
}
struct GameMsgSetCurrentTileCoords {
  const OSM::TileCoordsHiPrec* coords = nullptr;
};
struct GameMsgSetTileGridOrigin {
  const OSM::TileCoordsHiPrec* coords = nullptr;
};

// -------------------------------------------------------------------
