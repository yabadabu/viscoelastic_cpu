#include "platform.h"
#include <cctype>     // isalpha / isdigit
#include "json.h"

#define dbg_printf if( 0 ) printf

enum class JObjType {
	MAP
	, ARRAY
	, LITERAL
	, OBJ_INVALID
};

class JsonParser {

public:

	// -----------------------------------------
	JsonParser(size_t chunck_size = 1024);
	~JsonParser();
	const char* getErrorText() const { return error_text; }

	json parse(const char* buf, size_t nbytes);

private:

	enum TOpCode {
		MAP_OPEN
		, MAP_CLOSE
		, MAP_KEY_SEPARATOR
		, ARRAY_OPEN
		, ARRAY_CLOSE
		, COMMA_SEPARATOR
		, MAP_KEY
		, BOOL_TRUE_VALUE
		, BOOL_FALSE_VALUE
		, NULL_VALUE
		, NUMERIC_VALUE
		, STRING_VALUE
		, END_OF_STREAM
		, INVALID_OPCODE
		, NUM_OPCODES
	};

	TOpCode  scanOpCode();
	bool     checkKeyword(const char* word, size_t word_length);
	JObjType parseObj(JObj* obj);
	JObjType parseMap(JObj* obj);
	JObjType parseArray(JObj* obj);
	JObjType parseLiteral(TOpCode op, JObj* obj);

	const char* start;
	const char* end;
	const char* top;

	// -----------------------------------------
	// Used while parsing
	const char* last_str;

	// -----------------------------------------
	// Error 
	char error_text[256];
	void setErrorText(const char* new_error);

	// -----------------------------------------
	// Destination structured buffer
	typedef unsigned char u8;
	u8* initial_chunk = nullptr;
	u8* out_base = nullptr;
	u8* out_top = nullptr;
	size_t num_chunks_required = 0;
	size_t chunck_size = 0;
	static const int max_children_per_node = 256;
	size_t getUsedBytes() const { return (out_top - out_base); }
	void* allocData(size_t nbytes);
	JObj* allocObj();
	const char* allocStr(const char* buf, size_t nbytes);
	u8* allocChunk();
	u8** nextChunkAddr(u8* curr_chunk);
};

struct JObj {
    
  enum TLiteralType {
      LITERAL_BOOL_TRUE
    , LITERAL_BOOL_FALSE
    , LITERAL_NULL
    , LITERAL_NUMERIC
    , LITERAL_STRING
    , LITERAL_INVALID
  };

  JObj() : type(JObjType::OBJ_INVALID) { }
  JObj(JObjType in_type)
    : type(in_type)
  {
    if (in_type == JObjType::MAP) {
      object.len = 0;
      object.keys = nullptr;
      object.values = nullptr;
    }
    else {
      printf("Invalid JObj ctor\n");
    }
  }

  // Can be called even when this is null
  bool isArray() const { return type == JObjType::ARRAY; }
  bool isObject() const { return type == JObjType::MAP; }
  bool isLiteral() const { return type == JObjType::LITERAL; }

  struct TLiteral {
    const char* text;
    TLiteralType type;
  };

  struct TAttributes {
    const char** keys; // Array of keys 
    JObj** values;     // Array of values.
    size_t len;        // Number of key-value-pairs.
    const JObj* get(const char* akey) const;
  };

  struct TArray {
    JObj** values; // Array of values
    size_t len;    // Number of elements
    const JObj* at(size_t n) const;
  };

  JObjType      type;
  union {
    TLiteral    literal;
    TAttributes object;
    TArray      array;
  };

  friend class json;
};

JsonParser* allocJsonParser() {
    return new JsonParser;
}

void freeJsonParser(JsonParser* p) {
    delete p;
}

json parseJson(JsonParser* p, const char* buf, size_t nbytes) {
    return p->parse(buf, nbytes );
}

const char* getParseErrorText(JsonParser* p) {
  assert( p );
  return p->getErrorText();
}

bool json::isArray() const { return obj && obj->type == JObjType::ARRAY; }
bool json::isObject() const { return obj && obj->type == JObjType::MAP; }
bool json::isLiteral() const { return obj && obj->type == JObjType::LITERAL; }
bool json::is_bool() const { return isLiteral() && (obj->literal.type == JObj::LITERAL_BOOL_TRUE || obj->literal.type == JObj::LITERAL_BOOL_FALSE); }
bool json::is_string() const { return isLiteral() && obj->literal.type == JObj::LITERAL_STRING; }
bool json::is_null() const { return isLiteral() && obj->literal.type == JObj::LITERAL_NULL; }
bool json::is_number() const { return isLiteral() && obj->literal.type == JObj::LITERAL_NUMERIC; }
json::operator bool() const { return obj != nullptr && obj->type != JObjType::OBJ_INVALID; }
json::operator char const* (void)const {
    assert( is_string() );
    return obj->literal.text;
}
int json::count(const char* key) const {
  if (!isObject())
    return false;
  const JObj* v = obj->object.get(key);
  return v ? 1 : 0;
}

json json::empty_obj() {
  static JObj null_obj(JObjType::MAP);
  static json j_empty_obj(&null_obj);
  return j_empty_obj;
}

bool JsonParser::checkKeyword(const char *word, size_t word_length) {

  if (top + word_length > end) {
    setErrorText("Unexpected end of stream.");
    return false;
  }

  // all those -1 are because top has already been advanced
  if (strncmp(top - 1, word, word_length) == 0) {
    last_str = allocStr(top - 1, word_length);
    top += word_length - 1;
    return true;
  }

  return false;
}

void JsonParser::setErrorText(const char *new_error) {
  int nline = 0;
  const char* p = start;
  while( p < top ) {
    if( *p == '\n' ) 
      ++nline;
    ++p;
  }
  int n = 32;
  if (top + n > end) {
	  n = (int)(end - top);
      if (n < 5 ) n = 5;
  }
  snprintf(error_text, sizeof( error_text )-1, "At offset %ld Line %d: %s. Near text: %.*s", (long)(top-1 - start), nline, new_error, n, top - 5);
}

JsonParser::TOpCode JsonParser::scanOpCode() {
  while (top != end) {
    char c = *top++;

    switch (c) {
    case '{': return MAP_OPEN;
    case ':': return MAP_KEY_SEPARATOR;
    case '}': return MAP_CLOSE;
    case '[': return ARRAY_OPEN;
    case ',': return COMMA_SEPARATOR;
    case ']': return ARRAY_CLOSE;
    case '"': {
      const char *text = top;
      while (top < end && *top != '"') {
        if (*top == '\\')
          ++top;
        ++top;
      }
      if (top >= end) {
        setErrorText("Unexpected end of stream parsing string.");
        return END_OF_STREAM;
      }
      last_str = allocStr(text, top - text);
      ++top;
      return STRING_VALUE;
    }

    case 'n':
      if (checkKeyword("null", 4))
        return NULL_VALUE;
      break;
    case 't':
      if (checkKeyword("true", 4))
        return BOOL_TRUE_VALUE;
      break;
    case 'f':
      if (checkKeyword("false", 5))
        return BOOL_FALSE_VALUE;
      break;
    }
    if (isalpha(c)) {
      last_str = top - 1;
      return INVALID_OPCODE;
    }
    if (isdigit(c) || c == '-' || c == '+' ) {

      // Need to take care of floats, e....
      const char *text = top - 1;
      while (top != end && (isdigit(*top) || *top == '.'))
        ++top;
      if (top == end) {
        setErrorText("Unexpected end of stream parsing number.");
        return END_OF_STREAM;
      }
      last_str = allocStr(text, top - text);
      return NUMERIC_VALUE;
    }
  }
  return END_OF_STREAM;
}

// ------------------------------------------------------------------
// { key1:obj, key2:obj, .. }
JObjType JsonParser::parseMap(JObj *obj) {

  obj->type = JObjType::MAP;
  obj->object.len = 0;
  obj->object.values = NULL;
  obj->object.keys = NULL;

  // Reserve some space
  size_t      nobjs = 0;
  const char *keys[max_children_per_node];
  JObj *      objs[max_children_per_node];

  dbg_printf("map {\n");

  TOpCode op = INVALID_OPCODE;
  while (top != end) {
    op = scanOpCode();

    // Just in case the map is empty
    if (op == MAP_CLOSE) {
      if (nobjs != 0) {
			setErrorText("Expecting another key/value after the ',' before closing the object ^with '}'");
            return JObjType::OBJ_INVALID;
      }
      dbg_printf("empty map }\n");
      return JObjType::MAP;
    }

    // The identifier must be a string literal
    if (op != STRING_VALUE) {
      setErrorText("Expecting identifier of key map.");
      return JObjType::OBJ_INVALID;
    }

    keys[nobjs] = last_str;
    dbg_printf("Key   : (%d) %s\n", op, last_str);

    // Now the separator : between the key and the value
    op = scanOpCode();
    if (op != MAP_KEY_SEPARATOR) {
      setErrorText("Expecting a ':' while parsing map");
      return JObjType::OBJ_INVALID;
    }

    // Then the object value
    dbg_printf("Value : ");
    JObj *child = allocObj();

    JObjType otype = parseObj(child);
    if (otype == JObjType::OBJ_INVALID)
      return JObjType::OBJ_INVALID;

    // If all is OK, save pointer and incr # objs
    assert(nobjs < max_children_per_node);
    objs[nobjs++] = child;

    // Check if another key will come or we have reach the end of the map
    op = scanOpCode();

    // Close the map
    if (op == MAP_CLOSE) {
      dbg_printf("map }\n");

      // Copy pointers 
      size_t bytes_for_pointers = nobjs * sizeof(JObj *);
      obj->object.len = nobjs;
      obj->object.values = (JObj **)allocData(bytes_for_pointers);
      obj->object.keys = (const char **)allocData(bytes_for_pointers);
      memcpy(obj->object.values, objs, bytes_for_pointers);
      memcpy(obj->object.keys, keys, bytes_for_pointers);
      return JObjType::MAP;
    }

    // Then we expect a ',', any other option is an error
    if (op != COMMA_SEPARATOR) {
      setErrorText("Expecting a ',' while parsing map");
      return JObjType::OBJ_INVALID;
    }
  }

  return JObjType::OBJ_INVALID;
}

// ------------------------------------------------------------------
// [ obj, obj, obj .. ]
JObjType JsonParser::parseArray(JObj *obj) {
  obj->type = JObjType::ARRAY;
  obj->array.len = 0;
  obj->array.values = NULL;

  size_t nobjs = 0;
  JObj *objs[max_children_per_node];

  // Check if the array is empty, else, restore the scan point
  const char *prev_top = top;
  TOpCode nop = scanOpCode();
  if (nop == ARRAY_CLOSE) {
    dbg_printf("empty array []\n");
    return JObjType::ARRAY;
  }
  top = prev_top;

  dbg_printf("array [\n");
  TOpCode op = INVALID_OPCODE;
  while (top != end) {

    // Allocate an object and parse it.
    JObj *child = allocObj();
    JObjType otype = parseObj(child);

    if (otype == JObjType::OBJ_INVALID)
      return JObjType::OBJ_INVALID;

    assert(nobjs < max_children_per_node);
    objs[nobjs++] = child;

    // Check if another array member will come or we have reach the end of the array
    op = scanOpCode();
    if (op == ARRAY_CLOSE) {
      dbg_printf("]\n");
      
      // Save pointers
      size_t bytes_for_pointers = nobjs * sizeof(JObj *);
      obj->array.len = nobjs;
      obj->array.values = (JObj **)allocData(bytes_for_pointers );
      memcpy(obj->array.values, objs, bytes_for_pointers);

      return JObjType::ARRAY;
    }

    // Then we expect a comma between each object in the array
    if (op != COMMA_SEPARATOR) {
      setErrorText("Expected ',' as array member separator while parsing array");
      return JObjType::OBJ_INVALID;
    }

  }

  setErrorText("Unexpected end of stream parsing array.");
  return JObjType::OBJ_INVALID;
}

// ------------------------------------------------------------------
// true|false|null|int|"string"
JObjType JsonParser::parseLiteral(TOpCode op, JObj *obj) {

  obj->type = JObjType::LITERAL;
  obj->literal.text = last_str;

  switch (op) {
  case BOOL_FALSE_VALUE: obj->literal.type = JObj::LITERAL_BOOL_FALSE; break;
  case BOOL_TRUE_VALUE:  obj->literal.type = JObj::LITERAL_BOOL_TRUE;  break;
  case NULL_VALUE:       obj->literal.type = JObj::LITERAL_NULL;       break;
  case STRING_VALUE:     obj->literal.type = JObj::LITERAL_STRING;     break;
  case NUMERIC_VALUE:    obj->literal.type = JObj::LITERAL_NUMERIC;    break;
  default:
    printf("Invalid literal type %d : %s", op, last_str);
    obj->literal.type = JObj::LITERAL_INVALID; break;
  }

  dbg_printf("(%d) %s\n", op, last_str);
  return JObjType::LITERAL;
}

// ---------------------------------------------------------
JObjType JsonParser::parseObj( JObj *obj ) {
  while (top != end) {
    TOpCode op = scanOpCode();
    if (op == END_OF_STREAM)
      break;
    else if (op == INVALID_OPCODE) {
      setErrorText("Invalid characters found");
      return JObjType::OBJ_INVALID;
    }
    else if (op == MAP_OPEN) 
      return parseMap( obj );
    else if (op == ARRAY_OPEN) 
      return parseArray( obj );
    else
      return parseLiteral( op, obj );
  }
  setErrorText("Unexpected end of stream parsing object.");
  return JObjType::OBJ_INVALID;
}

// ---------------------------------------------------------
json JsonParser::parse(const char *buf, size_t nbytes) {
  
  // Reset parsing pointer
  start = buf;
  end = buf + nbytes;
  top = buf;
  last_str = NULL;

  // Reset error msgs
  error_text[0] = 0x00;

  JObj *root = allocObj();
  JObjType otype = parseObj(root);
  dbg_printf("%ld bytes parsed. %ld chunks required (%ld bytes) type:%d\n", (long)nbytes, (long)num_chunks_required, (long)(num_chunks_required * chunck_size), (int)otype);
  if (otype == JObjType::OBJ_INVALID)
    root = nullptr;
  json j;
  j.obj = root;
  return j;
}

// ------------------------------------------------------------------------------
JsonParser::JsonParser( size_t in_chunck_size) {
  chunck_size = in_chunck_size;
  initial_chunk = allocChunk( );
}

JsonParser::~JsonParser() {
  u8* chunk = initial_chunk;
  while (chunk) {
    u8* next_chunk = *nextChunkAddr( chunk );
    dbg_printf( "freeChunk at %p\n", chunk );
    delete[] chunk;
    chunk = next_chunk;
  }
}

// ------------------------------------------------------------------------------
const char *JsonParser::allocStr(const char *buf, size_t nbytes) {
  char *dst = (char *)allocData(nbytes+1);    // +1 for the terminator
  // evaluate escaped sequences/unicodes/etc..
  memcpy(dst, buf, nbytes);
  dst[nbytes] = 0x0;
  return dst;
}

JObj *JsonParser::allocObj( ) {
  return (JObj *)allocData( sizeof( JObj ) );
}

void *JsonParser::allocData(size_t nbytes) {
  nbytes = (nbytes + 3) & (~3);   // Keep data aligned to 4 bytes
  assert( nbytes <= chunck_size );

  if( getUsedBytes() + nbytes > chunck_size )
    allocChunk();

  void *buf = out_top;
  out_top += nbytes;
  assert(getUsedBytes() <= chunck_size);
  assert( buf >= out_base && buf < out_base + chunck_size);
  return buf;
}

JsonParser::u8* JsonParser::allocChunk() {
  u8* chunk = new u8[chunck_size];
  dbg_printf( "AllocChunk[%ld] %p of %ld bytes\n", (long)num_chunks_required, chunk, (long)chunck_size );
  ++num_chunks_required;

  // Clear the address of the next chunk that may be linked to us
  u8** next = nextChunkAddr( chunk );
  *next = 0;

  // Link prev chunk to the new chunk
  if (out_base) 
      *(u8**)out_base = chunk;

  out_base = chunk;
  out_top = out_base + sizeof( void* );
  return chunk;
}

JsonParser::u8** JsonParser::nextChunkAddr(u8* curr_chunk) {
    return (u8**) curr_chunk;
}

// ------------------------------------------------------------------------------
const JObj *JObj::TAttributes::get(const char *akey) const {
  for (size_t i = 0; i < len; ++i) {
    if (strcmp(keys[i], akey) == 0)
      return values[i];
  }
  return NULL;
}

const JObj *JObj::TArray::at(size_t n) const {
  assert(n < len);
  return values[n];
}

// -------------------------------------------------------------------------------------
template<>
ENGINE_API void load(json j, float& v) {
	assert(j.isLiteral());
  v =(float)atof(j.obj->literal.text);
}

template<>
ENGINE_API void load(json j, double& v) {
  assert(j.isLiteral());
  v = atof(j.obj->literal.text);
}

template<>
ENGINE_API void load(json j, int& v) {
	assert(j.isLiteral());
	v = (int)atol(j.obj->literal.text);
}

template<>
ENGINE_API void load(json j, bool& v) {
	assert(j.isLiteral());
	const JObj* jv = j.obj;
	if (jv->literal.type == JObj::LITERAL_BOOL_FALSE)
		v = false;
	if (jv->literal.type == JObj::LITERAL_BOOL_TRUE)
		v = true;
}

template<>
void load(json j, char const* &v) {
  assert( j.isLiteral() );
	if( j.obj->isLiteral() && j.obj->literal.type == JObj::LITERAL_STRING)
	    v = j.obj->literal.text;
}

json json::operator[]( size_t idx ) const {
  assert(isArray() || isObject());
  json j;
  j.obj = isArray() ? obj->array.at(idx) : obj->object.values[idx];
  return j;
}

json json::operator[]( const char* key ) const {
  assert(isObject());
  json j;
  j.obj = obj->object.get(key);
  return j;
}

size_t json::size() const {
  assert( isArray() || isObject() );
  return isArray() ? obj->array.len : obj->object.len;
}

const char* json::key(size_t idx) const {
  assert( isObject() );
  assert( idx < obj->object.len );
  return obj->object.keys[ idx ];
}



