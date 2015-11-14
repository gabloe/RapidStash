#pragma once

#include <stdint.h>
#include <stddef.h>
#include <assert.h>


#ifdef JSON_EXPORTS
#define JSON_API __declspec(dllexport) 
#else
#define JSON_API __declspec(dllimport) 
#endif


enum JsonTag {
    JSON_NUMBER = 0,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT,
    JSON_TRUE,
    JSON_FALSE,
    JSON_NULL = 0xF
};

struct JsonNode;

#define JSON_VALUE_PAYLOAD_MASK 0x00007FFFFFFFFFFFULL
#define JSON_VALUE_NAN_MASK 0x7FF8000000000000ULL
#define JSON_VALUE_TAG_MASK 0xF
#define JSON_VALUE_TAG_SHIFT 47


JSON_API union JsonValue {
    uint64_t ival;
    double fval;

    JsonValue(double x)
        : fval(x) {
    }
    JsonValue(JsonTag tag = JSON_NULL, void *payload = nullptr) {
        assert((uintptr_t)payload <= JSON_VALUE_PAYLOAD_MASK);
        ival = JSON_VALUE_NAN_MASK | ((uint64_t)tag << JSON_VALUE_TAG_SHIFT) | (uintptr_t)payload;
    }
    bool isDouble() const {
        return (int64_t)ival <= (int64_t)JSON_VALUE_NAN_MASK;
    }
    JsonTag getTag() const {
        return isDouble() ? JSON_NUMBER : JsonTag((ival >> JSON_VALUE_TAG_SHIFT) & JSON_VALUE_TAG_MASK);
    }
    uint64_t getPayload() const {
        assert(!isDouble());
        return ival & JSON_VALUE_PAYLOAD_MASK;
    }
    double toNumber() const {
        assert(getTag() == JSON_NUMBER);
        return fval;
    }
    char *toString() const {
        assert(getTag() == JSON_STRING);
        return (char *)getPayload();
    }
    JsonNode *toNode() const {
        assert(getTag() == JSON_ARRAY || getTag() == JSON_OBJECT);
        return (JsonNode *)getPayload();
    }
};

struct JsonNode {
    JsonValue value;
    JsonNode *next;
    char *key;
};

JSON_API struct JsonIterator {
    JsonNode *p;

    void operator++() {
        p = p->next;
    }
    bool operator!=(const JsonIterator &x) const {
        return p != x.p;
    }
    JsonNode *operator*() const {
        return p;
    }
    JsonNode *operator->() const {
        return p;
    }
};

inline JsonIterator begin(JsonValue o) {
    return JsonIterator{o.toNode()};
}
inline JsonIterator end(JsonValue) {
    return JsonIterator{nullptr};
}

#define JSON_ERRNO_MAP(XX)                           \
    XX(OK, "ok")                                     \
    XX(BAD_NUMBER, "bad number")                     \
    XX(BAD_STRING, "bad string")                     \
    XX(BAD_IDENTIFIER, "bad identifier")             \
    XX(STACK_OVERFLOW, "stack overflow")             \
    XX(STACK_UNDERFLOW, "stack underflow")           \
    XX(MISMATCH_BRACKET, "mismatch bracket")         \
    XX(UNEXPECTED_CHARACTER, "unexpected character") \
    XX(UNQUOTED_KEY, "unquoted key")                 \
    XX(BREAKING_BAD, "breaking bad")                 \
    XX(ALLOCATION_FAILURE, "allocation failure")

JSON_API enum JsonErrno {
#define XX(no, str) JSON_##no,
    JSON_ERRNO_MAP(XX)
#undef XX
};

JSON_API const char *jsonStrError(int err);

class JSON_API JsonAllocator {

    struct Zone {
        Zone *next;
        size_t used;
    } *head = nullptr;

public:
    JsonAllocator(void);
	JsonAllocator(JsonAllocator &&x);
	JsonAllocator &operator=(JsonAllocator &&x);
	~JsonAllocator();
    void *allocate(size_t size);
    void deallocate();
};

JSON_API int jsonParse(char *str, char **endptr, JsonValue *value, JsonAllocator &allocator);
