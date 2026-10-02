/* llaya.c - LLaya: Lua binding for LibLayaX, the unofficial C API library for Laya
 * typed-decision inference (laya.dll / liblaya.so / liblaya.dylib, built from laya.cpp).
 *
 *   local llaya = require "llaya"
 *   local agent = assert(llaya.new("/models/laya", { backend = "cpu" }))
 *   local json  = assert(agent:ask_yes_no("Please refund the duplicate charge.",
 *                                         "Does the customer ask for a refund?", "refund"))
 *   local t     = assert(agent:ask_yes_no_table(...))   -- the same answer as a Lua table
 *   print(t.results[1].answers.refund.noul)
 *
 * Builds against Lua 5.1, 5.2, 5.3, 5.4 and LuaJIT. The native library is not linked: it is
 * loaded at run time from the folder this module is in (or from LLAYA_LIBRARY, or from the
 * system's normal search path), so the module can be built without it.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* dladdr */
#endif

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include "lua.h"
#include "lauxlib.h"

#define LLAYA_VERSION "LLaya 0.1.0"
#define LLAYA_API_VERSION 1 /* the LibLayaX C API version this binding was written for */
#define AGENT_TYPE "llaya.agent"
#define MAX_DEPTH 100

#if defined(_WIN32)
#define LLAYA_EXPORT __declspec(dllexport)
#define LAYA_CALL __cdecl
#elif defined(__GNUC__)
#define LLAYA_EXPORT __attribute__((visibility("default")))
#define LAYA_CALL
#else
#define LLAYA_EXPORT
#define LAYA_CALL
#endif

#if LUA_VERSION_NUM < 502
#define lua_rawlen lua_objlen
#endif

/* ------------------------------------------------------------------------------------------
 * The LibLayaX C API (see laya_c.h), resolved at run time.
 * ---------------------------------------------------------------------------------------- */

typedef struct laya_agent laya_agent;

static struct {
    int loaded;
    const char* (LAYA_CALL* version)(void);
    int (LAYA_CALL* api_version)(void);
    laya_agent* (LAYA_CALL* create)(const char* model_dir_utf8, const char* options_json);
    char* (LAYA_CALL* predict)(laya_agent* agent, const char* request_json);
    char* (LAYA_CALL* prepare)(laya_agent* agent, const char* request_json);
    char* (LAYA_CALL* info)(laya_agent* agent);
    void (LAYA_CALL* free_string)(char* text);
    void (LAYA_CALL* destroy)(laya_agent* agent);
    const char* (LAYA_CALL* last_error)(void);
} api;

#if defined(_WIN32)
#define LIBRARY_NAME "laya.dll"
#elif defined(__APPLE__)
#define LIBRARY_NAME "liblaya.dylib"
#else
#define LIBRARY_NAME "liblaya.so"
#endif

#ifdef _WIN32
typedef HMODULE library_t;
static library_t open_library(char* error, size_t error_size) {
    HMODULE library = NULL, self = NULL;
    wchar_t path[MAX_PATH * 4];
    const wchar_t* override = _wgetenv(L"LLAYA_LIBRARY");
    if (override && *override) {
        library = LoadLibraryExW(override, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (!library) snprintf(error, error_size, "cannot load the library named by LLAYA_LIBRARY (error %lu)", GetLastError());
        return library;
    }
    /* 1. laya.dll in the folder this module is in */
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCWSTR)(void*)&open_library, &self)) {
        DWORD n = GetModuleFileNameW(self, path, (DWORD)(sizeof path / sizeof path[0]) - 16);
        if (n > 0 && n < sizeof path / sizeof path[0] - 16) {
            wchar_t* slash = wcsrchr(path, L'\\');
            if (slash) {
                wcscpy(slash + 1, L"laya.dll");
                library = LoadLibraryExW(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
            }
        }
    }
    /* 2. the normal search: the program's folder, the current folder, PATH */
    if (!library) library = LoadLibraryW(L"laya.dll");
    if (!library)
        snprintf(error, error_size, "cannot load " LIBRARY_NAME " (error %lu): put it next to the llaya module or "
                 "next to the program, or set LLAYA_LIBRARY to its full path", GetLastError());
    return library;
}
static void* find_symbol(library_t library, const char* name) { return (void*)GetProcAddress(library, name); }
#else
typedef void* library_t;
static library_t open_library(char* error, size_t error_size) {
    void* library = NULL;
    Dl_info self;
    char path[PATH_MAX + 32];
    const char* override = getenv("LLAYA_LIBRARY");
    if (override && *override) {
        library = dlopen(override, RTLD_NOW | RTLD_LOCAL);
        if (!library) snprintf(error, error_size, "cannot load the library named by LLAYA_LIBRARY: %s", dlerror());
        return library;
    }
    /* 1. the library in the folder this module is in */
    if (dladdr((void*)&api, &self) && self.dli_fname && strlen(self.dli_fname) < PATH_MAX) {
        const char* slash = strrchr(self.dli_fname, '/');
        if (slash) {
            size_t n = (size_t)(slash - self.dli_fname) + 1;
            memcpy(path, self.dli_fname, n);
            strcpy(path + n, LIBRARY_NAME);
        } else {
            strcpy(path, "./" LIBRARY_NAME);
        }
        library = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    }
    /* 2. the system's normal search (LD_LIBRARY_PATH / DYLD_LIBRARY_PATH, standard folders) */
    if (!library) library = dlopen(LIBRARY_NAME, RTLD_NOW | RTLD_LOCAL);
    if (!library)
        snprintf(error, error_size, "cannot load " LIBRARY_NAME ": %s. Put it next to the llaya module, or set "
                 "LLAYA_LIBRARY to its full path", dlerror());
    return library;
}
static void* find_symbol(library_t library, const char* name) { return dlsym(library, name); }
#endif

/* Loads the library once per process. Returns 1, or 0 with a message in error. */
static int load_api(char* error, size_t error_size) {
    library_t library;
    if (api.loaded) return 1;
    library = open_library(error, error_size);
    if (!library) return 0;
#define RESOLVE(field, symbol)                                                               \
    do {                                                                                     \
        void* address = find_symbol(library, symbol);                                        \
        if (!address) {                                                                      \
            snprintf(error, error_size, LIBRARY_NAME " has no function %s: not a LibLayaX library?", symbol); \
            return 0;                                                                        \
        }                                                                                    \
        memcpy(&api.field, &address, sizeof address);                                        \
    } while (0)
    RESOLVE(version, "laya_version");
    RESOLVE(api_version, "laya_api_version");
    RESOLVE(create, "laya_create");
    RESOLVE(predict, "laya_predict");
    RESOLVE(prepare, "laya_prepare");
    RESOLVE(info, "laya_info");
    RESOLVE(free_string, "laya_free_string");
    RESOLVE(destroy, "laya_destroy");
    RESOLVE(last_error, "laya_last_error");
#undef RESOLVE
    if (api.api_version() != LLAYA_API_VERSION) {
        snprintf(error, error_size, LIBRARY_NAME " has C API version %d, this module needs %d",
                 api.api_version(), LLAYA_API_VERSION);
        return 0;
    }
    api.loaded = 1;
    return 1;
}

/* ------------------------------------------------------------------------------------------
 * A growable string, independent of the Lua stack (so nothing is lost if Lua raises an error
 * only after the buffer has been freed).
 * ---------------------------------------------------------------------------------------- */

typedef struct {
    char* data;
    size_t length, capacity;
    int failed; /* out of memory */
} buffer;

static void buffer_add(buffer* b, const char* text, size_t n) {
    if (b->failed) return;
    if (b->length + n + 1 > b->capacity) {
        size_t capacity = b->capacity ? b->capacity * 2 : 256;
        char* data;
        while (capacity < b->length + n + 1) capacity *= 2;
        data = (char*)realloc(b->data, capacity);
        if (!data) {
            b->failed = 1;
            return;
        }
        b->data = data;
        b->capacity = capacity;
    }
    memcpy(b->data + b->length, text, n);
    b->length += n;
    b->data[b->length] = '\0';
}
static void buffer_text(buffer* b, const char* text) { buffer_add(b, text, strlen(text)); }
static void buffer_char(buffer* b, char c) { buffer_add(b, &c, 1); }

/* ------------------------------------------------------------------------------------------
 * JSON encoder: Lua value -> JSON text.
 * ---------------------------------------------------------------------------------------- */

static void encode_string(buffer* b, const char* text, size_t n) {
    static const char hex[] = "0123456789abcdef";
    size_t i, start = 0;
    buffer_char(b, '"');
    for (i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)text[i];
        const char* escape = NULL;
        char unicode[7];
        switch (c) {
            case '"': escape = "\\\""; break;
            case '\\': escape = "\\\\"; break;
            case '\b': escape = "\\b"; break;
            case '\f': escape = "\\f"; break;
            case '\n': escape = "\\n"; break;
            case '\r': escape = "\\r"; break;
            case '\t': escape = "\\t"; break;
            default:
                if (c < 0x20) {
                    unicode[0] = '\\'; unicode[1] = 'u'; unicode[2] = '0'; unicode[3] = '0';
                    unicode[4] = hex[c >> 4]; unicode[5] = hex[c & 15]; unicode[6] = '\0';
                    escape = unicode;
                }
        }
        if (escape) {
            buffer_add(b, text + start, i - start);
            buffer_text(b, escape);
            start = i + 1;
        }
    }
    buffer_add(b, text + start, n - start);
    buffer_char(b, '"');
}

static int encode_number(lua_State* L, int index, buffer* b, const char** error) {
    char text[64];
    char* comma;
    double value;
#if LUA_VERSION_NUM >= 503
    if (lua_isinteger(L, index)) {
        snprintf(text, sizeof text, "%lld", (long long)lua_tointeger(L, index));
        buffer_text(b, text);
        return 1;
    }
#endif
    value = (double)lua_tonumber(L, index);
    if (value != value || value - value != 0) { /* NaN or infinity */
        *error = "cannot encode NaN or infinity as JSON";
        return 0;
    }
    if (value == floor(value) && fabs(value) < 1e15) {
        snprintf(text, sizeof text, "%.0f", value);
    } else {
        snprintf(text, sizeof text, "%.15g", value); /* shortest form that reads back exactly */
        if (strtod(text, NULL) != value) snprintf(text, sizeof text, "%.17g", value);
    }
    comma = strchr(text, ','); /* a locale with a decimal comma */
    if (comma) *comma = '.';
    buffer_text(b, text);
    return 1;
}

static int encode_value(lua_State* L, int index, buffer* b, int depth, const char** error);

/* A table is written as a JSON array when its keys are exactly 1..n (n >= 1), otherwise as an
 * object whose keys must be strings or numbers. An empty table is written as {}. */
static int encode_table(lua_State* L, int index, buffer* b, int depth, const char** error) {
    size_t n = lua_rawlen(L, index), count = 0, i;
    int first = 1;
    if (depth > MAX_DEPTH) {
        *error = "table is nested too deeply (or refers to itself)";
        return 0;
    }
    if (!lua_checkstack(L, 4)) {
        *error = "out of stack space";
        return 0;
    }
    if (n > 0) {
        lua_pushnil(L);
        while (lua_next(L, index)) {
            ++count;
            lua_pop(L, 1);
        }
    }
    if (n > 0 && count == n) {
        buffer_char(b, '[');
        for (i = 1; i <= n; ++i) {
            if (i > 1) buffer_char(b, ',');
            lua_rawgeti(L, index, (int)i);
            if (!encode_value(L, lua_gettop(L), b, depth + 1, error)) {
                lua_pop(L, 1);
                return 0;
            }
            lua_pop(L, 1);
        }
        buffer_char(b, ']');
        return 1;
    }
    buffer_char(b, '{');
    lua_pushnil(L);
    while (lua_next(L, index)) {
        int key = lua_gettop(L) - 1;
        if (!first) buffer_char(b, ',');
        first = 0;
        if (lua_type(L, key) == LUA_TSTRING) {
            size_t length;
            const char* text = lua_tolstring(L, key, &length);
            encode_string(b, text, length);
        } else if (lua_type(L, key) == LUA_TNUMBER) {
            buffer_char(b, '"'); /* number keys become their text; the key itself is not converted */
            if (!encode_number(L, key, b, error)) {
                lua_pop(L, 2);
                return 0;
            }
            buffer_char(b, '"');
        } else {
            *error = "table keys must be strings or numbers to be written as JSON";
            lua_pop(L, 2);
            return 0;
        }
        buffer_char(b, ':');
        if (!encode_value(L, key + 1, b, depth + 1, error)) {
            lua_pop(L, 2);
            return 0;
        }
        lua_pop(L, 1);
    }
    buffer_char(b, '}');
    return 1;
}

static int encode_value(lua_State* L, int index, buffer* b, int depth, const char** error) {
    switch (lua_type(L, index)) {
        case LUA_TNIL:
            buffer_text(b, "null");
            return 1;
        case LUA_TBOOLEAN:
            buffer_text(b, lua_toboolean(L, index) ? "true" : "false");
            return 1;
        case LUA_TNUMBER:
            return encode_number(L, index, b, error);
        case LUA_TSTRING: {
            size_t length;
            const char* text = lua_tolstring(L, index, &length);
            encode_string(b, text, length);
            return 1;
        }
        case LUA_TTABLE:
            return encode_table(L, index, b, depth, error);
        case LUA_TLIGHTUSERDATA:
            if (lua_touserdata(L, index) == NULL) { /* llaya.null */
                buffer_text(b, "null");
                return 1;
            }
            /* fall through */
        default:
            *error = "only nil, booleans, numbers, strings and tables can be written as JSON";
            return 0;
    }
}

/* ------------------------------------------------------------------------------------------
 * JSON decoder: JSON text -> Lua value. Arrays become tables indexed from 1, null becomes
 * llaya.null (a light userdata), so that no key or position is lost.
 * ---------------------------------------------------------------------------------------- */

typedef struct {
    const char* at;
    const char* end;
    const char* error;
} parser;

static void skip_space(parser* p) {
    while (p->at < p->end && (*p->at == ' ' || *p->at == '\t' || *p->at == '\n' || *p->at == '\r')) ++p->at;
}

static int parse_hex4(parser* p, unsigned* out) {
    unsigned value = 0;
    int i;
    if (p->end - p->at < 4) return 0;
    for (i = 0; i < 4; ++i) {
        char c = p->at[i];
        value <<= 4;
        if (c >= '0' && c <= '9') value |= (unsigned)(c - '0');
        else if (c >= 'a' && c <= 'f') value |= (unsigned)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') value |= (unsigned)(c - 'A' + 10);
        else return 0;
    }
    p->at += 4;
    *out = value;
    return 1;
}

static void add_utf8(buffer* b, unsigned code) {
    char out[4];
    if (code < 0x80) {
        out[0] = (char)code;
        buffer_add(b, out, 1);
    } else if (code < 0x800) {
        out[0] = (char)(0xC0 | (code >> 6));
        out[1] = (char)(0x80 | (code & 0x3F));
        buffer_add(b, out, 2);
    } else if (code < 0x10000) {
        out[0] = (char)(0xE0 | (code >> 12));
        out[1] = (char)(0x80 | ((code >> 6) & 0x3F));
        out[2] = (char)(0x80 | (code & 0x3F));
        buffer_add(b, out, 3);
    } else {
        out[0] = (char)(0xF0 | (code >> 18));
        out[1] = (char)(0x80 | ((code >> 12) & 0x3F));
        out[2] = (char)(0x80 | ((code >> 6) & 0x3F));
        out[3] = (char)(0x80 | (code & 0x3F));
        buffer_add(b, out, 4);
    }
}

/* Parses a string (the opening quote already consumed) into text; the caller frees it. */
static int parse_string(parser* p, buffer* text) {
    while (p->at < p->end) {
        const char* start = p->at;
        while (p->at < p->end && *p->at != '"' && *p->at != '\\') ++p->at;
        buffer_add(text, start, (size_t)(p->at - start));
        if (p->at >= p->end) break;
        if (*p->at == '"') {
            ++p->at;
            if (text->failed) {
                p->error = "out of memory";
                return 0;
            }
            return 1;
        }
        ++p->at; /* backslash */
        if (p->at >= p->end) break;
        switch (*p->at++) {
            case '"': buffer_char(text, '"'); break;
            case '\\': buffer_char(text, '\\'); break;
            case '/': buffer_char(text, '/'); break;
            case 'b': buffer_char(text, '\b'); break;
            case 'f': buffer_char(text, '\f'); break;
            case 'n': buffer_char(text, '\n'); break;
            case 'r': buffer_char(text, '\r'); break;
            case 't': buffer_char(text, '\t'); break;
            case 'u': {
                unsigned code, low;
                if (!parse_hex4(p, &code)) {
                    p->error = "bad \\u escape in JSON string";
                    return 0;
                }
                if (code >= 0xD800 && code <= 0xDBFF && p->end - p->at >= 6 && p->at[0] == '\\' && p->at[1] == 'u') {
                    const char* rewind = p->at;
                    p->at += 2;
                    if (parse_hex4(p, &low) && low >= 0xDC00 && low <= 0xDFFF)
                        code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                    else
                        p->at = rewind;
                }
                add_utf8(text, code);
                break;
            }
            default:
                p->error = "bad escape in JSON string";
                return 0;
        }
    }
    p->error = "unterminated JSON string";
    return 0;
}

static int parse_number(lua_State* L, parser* p) {
    char text[64];
    size_t n = 0;
    int is_integer = 1;
    char* stop;
    double value;
    const char* start = p->at;
    while (p->at < p->end && n < sizeof text - 1 &&
           (isdigit((unsigned char)*p->at) || *p->at == '-' || *p->at == '+' || *p->at == '.' || *p->at == 'e' || *p->at == 'E')) {
        if (*p->at == '.' || *p->at == 'e' || *p->at == 'E') is_integer = 0;
        text[n++] = *p->at++;
    }
    text[n] = '\0';
    if (n == 0 || (!isdigit((unsigned char)text[0]) && text[0] != '-')) {
        p->at = start;
        p->error = "unexpected character in JSON";
        return 0;
    }
#if LUA_VERSION_NUM >= 503
    if (is_integer) {
        long long integer;
        errno = 0;
        integer = strtoll(text, &stop, 10);
        if (errno == 0 && *stop == '\0') {
            lua_pushinteger(L, (lua_Integer)integer);
            return 1;
        }
    }
#else
    (void)is_integer;
#endif
    value = strtod(text, &stop);
    if (*stop == '.') { /* the C locale of the host uses another decimal separator */
        const struct lconv* locale = localeconv();
        if (locale && locale->decimal_point && locale->decimal_point[0]) {
            *stop = locale->decimal_point[0];
            value = strtod(text, &stop);
        }
    }
    if (*stop != '\0') {
        p->error = "bad number in JSON";
        return 0;
    }
    lua_pushnumber(L, (lua_Number)value);
    return 1;
}

static int parse_literal(parser* p, const char* word) {
    size_t n = strlen(word);
    if ((size_t)(p->end - p->at) >= n && memcmp(p->at, word, n) == 0) {
        p->at += n;
        return 1;
    }
    p->error = "unexpected character in JSON";
    return 0;
}

/* Pushes one value. On failure nothing is left on the stack and p->error is set. */
static int parse_value(lua_State* L, parser* p, int depth) {
    if (depth > MAX_DEPTH) {
        p->error = "JSON is nested too deeply";
        return 0;
    }
    if (!lua_checkstack(L, 4)) {
        p->error = "out of stack space";
        return 0;
    }
    skip_space(p);
    if (p->at >= p->end) {
        p->error = "unexpected end of JSON";
        return 0;
    }
    switch (*p->at) {
        case '{': {
            ++p->at;
            lua_newtable(L);
            skip_space(p);
            if (p->at < p->end && *p->at == '}') {
                ++p->at;
                return 1;
            }
            for (;;) {
                buffer key = {0};
                skip_space(p);
                if (p->at >= p->end || *p->at != '"') {
                    p->error = "expected a string key in JSON object";
                    lua_pop(L, 1);
                    return 0;
                }
                ++p->at;
                if (!parse_string(p, &key)) {
                    free(key.data);
                    lua_pop(L, 1);
                    return 0;
                }
                lua_pushlstring(L, key.data ? key.data : "", key.length);
                free(key.data);
                skip_space(p);
                if (p->at >= p->end || *p->at != ':') {
                    p->error = "expected ':' in JSON object";
                    lua_pop(L, 2);
                    return 0;
                }
                ++p->at;
                if (!parse_value(L, p, depth + 1)) {
                    lua_pop(L, 2);
                    return 0;
                }
                lua_rawset(L, -3);
                skip_space(p);
                if (p->at < p->end && *p->at == ',') {
                    ++p->at;
                    continue;
                }
                if (p->at < p->end && *p->at == '}') {
                    ++p->at;
                    return 1;
                }
                p->error = "expected ',' or '}' in JSON object";
                lua_pop(L, 1);
                return 0;
            }
        }
        case '[': {
            int n = 0;
            ++p->at;
            lua_newtable(L);
            skip_space(p);
            if (p->at < p->end && *p->at == ']') {
                ++p->at;
                return 1;
            }
            for (;;) {
                if (!parse_value(L, p, depth + 1)) {
                    lua_pop(L, 1);
                    return 0;
                }
                lua_rawseti(L, -2, ++n);
                skip_space(p);
                if (p->at < p->end && *p->at == ',') {
                    ++p->at;
                    continue;
                }
                if (p->at < p->end && *p->at == ']') {
                    ++p->at;
                    return 1;
                }
                p->error = "expected ',' or ']' in JSON array";
                lua_pop(L, 1);
                return 0;
            }
        }
        case '"': {
            buffer text = {0};
            ++p->at;
            if (!parse_string(p, &text)) {
                free(text.data);
                return 0;
            }
            lua_pushlstring(L, text.data ? text.data : "", text.length);
            free(text.data);
            return 1;
        }
        case 't':
            if (!parse_literal(p, "true")) return 0;
            lua_pushboolean(L, 1);
            return 1;
        case 'f':
            if (!parse_literal(p, "false")) return 0;
            lua_pushboolean(L, 0);
            return 1;
        case 'n':
            if (!parse_literal(p, "null")) return 0;
            lua_pushlightuserdata(L, NULL);
            return 1;
        default:
            return parse_number(L, p);
    }
}

/* Pushes the decoded value, or nil and a message. Returns the number of results. */
static int decode_json(lua_State* L, const char* text, size_t length) {
    parser p;
    p.at = text;
    p.end = text + length;
    p.error = NULL;
    if (parse_value(L, &p, 0)) {
        skip_space(&p);
        if (p.at == p.end) return 1;
        lua_pop(L, 1);
        p.error = "unexpected text after the JSON value";
    }
    lua_pushnil(L);
    lua_pushfstring(L, "%s at position %d", p.error ? p.error : "invalid JSON", (int)(p.at - text) + 1);
    return 2;
}

/* ------------------------------------------------------------------------------------------
 * Helpers shared by the Lua functions.
 * ---------------------------------------------------------------------------------------- */

static int fail(lua_State* L, const char* message) {
    lua_pushnil(L);
    lua_pushstring(L, message);
    return 2;
}

/* The JSON text for argument `index`: a string is passed through, a table is encoded.
 * Returns malloc'ed text (caller frees) or NULL with *error set. */
static char* json_argument(lua_State* L, int index, const char** error) {
    buffer b = {0};
    if (lua_type(L, index) == LUA_TSTRING) {
        size_t length;
        const char* text = lua_tolstring(L, index, &length);
        if (strlen(text) != length) {
            *error = "the JSON text contains a NUL character";
            return NULL;
        }
        buffer_add(&b, text, length);
    } else if (lua_type(L, index) == LUA_TTABLE) {
        if (!encode_value(L, index, &b, 0, error)) {
            free(b.data);
            return NULL;
        }
    } else {
        *error = "expected a JSON string or a table";
        return NULL;
    }
    if (b.failed || !b.data) {
        free(b.data);
        *error = "out of memory";
        return NULL;
    }
    return b.data;
}

typedef struct {
    laya_agent* handle;
} agent_box;

static laya_agent* check_agent(lua_State* L) {
    agent_box* box = (agent_box*)luaL_checkudata(L, 1, AGENT_TYPE);
    return box->handle;
}

/* Hands a string returned by the library to Lua: the JSON text, or the decoded table.
 * An {"error":...} response becomes nil plus the message. */
static int push_response(lua_State* L, char* response, int as_table) {
    int results;
    if (!response) return fail(L, "laya: out of memory");
    if (strncmp(response, "{\"error\":", 9) == 0) {
        const char* message = api.last_error();
        lua_pushnil(L);
        lua_pushstring(L, message && *message ? message : response);
        api.free_string(response);
        return 2;
    }
    if (as_table) {
        results = decode_json(L, response, strlen(response));
    } else {
        lua_pushstring(L, response);
        results = 1;
    }
    api.free_string(response);
    return results;
}

/* ------------------------------------------------------------------------------------------
 * Agent methods. Each exists twice: name() returns JSON text, name_table() a Lua table.
 * ---------------------------------------------------------------------------------------- */

enum { CALL_PREDICT, CALL_PREPARE };

static int agent_call(lua_State* L, int which, int as_table) {
    laya_agent* agent = check_agent(L);
    const char* error = NULL;
    char* request;
    char* response;
    if (!agent) return fail(L, "the agent is closed");
    luaL_checkany(L, 2);
    request = json_argument(L, 2, &error);
    if (!request) return fail(L, error);
    response = which == CALL_PREDICT ? api.predict(agent, request) : api.prepare(agent, request);
    free(request);
    return push_response(L, response, as_table);
}
static int agent_predict(lua_State* L) { return agent_call(L, CALL_PREDICT, 0); }
static int agent_predict_table(lua_State* L) { return agent_call(L, CALL_PREDICT, 1); }
static int agent_prepare(lua_State* L) { return agent_call(L, CALL_PREPARE, 0); }
static int agent_prepare_table(lua_State* L) { return agent_call(L, CALL_PREPARE, 1); }

static int agent_info_any(lua_State* L, int as_table) {
    laya_agent* agent = check_agent(L);
    if (!agent) return fail(L, "the agent is closed");
    return push_response(L, api.info(agent), as_table);
}
static int agent_info(lua_State* L) { return agent_info_any(L, 0); }
static int agent_info_table(lua_State* L) { return agent_info_any(L, 1); }

/* agent:ask_*(state, instructions [, criteria] [, id]): one state, one question.
 * state is a string, or a table that is sent as a JSON object. */
static int agent_ask(lua_State* L, const char* type, int has_criteria, int as_table) {
    laya_agent* agent = check_agent(L);
    const char* error = NULL;
    buffer b = {0};
    size_t length;
    const char* text;
    int id_index = has_criteria ? 5 : 4;
    char* response;
    /* check every argument before allocating anything: these raise a Lua error */
    if (lua_type(L, 2) != LUA_TTABLE) luaL_checkstring(L, 2);
    luaL_checkstring(L, 3);
    if (has_criteria) luaL_checktype(L, 4, LUA_TTABLE);
    if (!lua_isnoneornil(L, id_index)) luaL_checkstring(L, id_index);
    if (!agent) return fail(L, "the agent is closed");

    buffer_text(&b, "{\"state\":");
    if (lua_type(L, 2) == LUA_TTABLE) {
        if (!encode_value(L, 2, &b, 0, &error)) {
            free(b.data);
            return fail(L, error);
        }
    } else {
        text = lua_tolstring(L, 2, &length);
        encode_string(&b, text, length);
    }
    buffer_text(&b, ",\"questions\":{");
    if (lua_isnoneornil(L, id_index)) {
        buffer_text(&b, "\"q\"");
    } else {
        text = lua_tolstring(L, id_index, &length);
        encode_string(&b, text, length);
    }
    buffer_text(&b, ":{\"type\":\"");
    buffer_text(&b, type);
    buffer_text(&b, "\",\"instructions\":");
    text = lua_tolstring(L, 3, &length);
    encode_string(&b, text, length);
    if (has_criteria) {
        buffer_text(&b, ",\"criteria\":");
        if (!encode_value(L, 4, &b, 0, &error)) {
            free(b.data);
            return fail(L, error);
        }
    }
    buffer_text(&b, "}}}");
    if (b.failed || !b.data) {
        free(b.data);
        return fail(L, "out of memory");
    }
    if (strlen(b.data) != b.length) {
        free(b.data);
        return fail(L, "a string contains a NUL character");
    }
    response = api.predict(agent, b.data);
    free(b.data);
    return push_response(L, response, as_table);
}
static int agent_ask_yes_no(lua_State* L) { return agent_ask(L, "noul", 0, 0); }
static int agent_ask_yes_no_table(lua_State* L) { return agent_ask(L, "noul", 0, 1); }
static int agent_ask_choice(lua_State* L) { return agent_ask(L, "choice", 1, 0); }
static int agent_ask_choice_table(lua_State* L) { return agent_ask(L, "choice", 1, 1); }
static int agent_ask_score(lua_State* L) { return agent_ask(L, "score", 1, 0); }
static int agent_ask_score_table(lua_State* L) { return agent_ask(L, "score", 1, 1); }

/* agent:close(): unloads the model now instead of waiting for the garbage collector. */
static int agent_close(lua_State* L) {
    agent_box* box = (agent_box*)luaL_checkudata(L, 1, AGENT_TYPE);
    if (box->handle) {
        api.destroy(box->handle);
        box->handle = NULL;
    }
    return 0;
}

static int agent_tostring(lua_State* L) {
    agent_box* box = (agent_box*)luaL_checkudata(L, 1, AGENT_TYPE);
    if (box->handle) lua_pushfstring(L, "llaya.agent: %p", (void*)box->handle);
    else lua_pushliteral(L, "llaya.agent (closed)");
    return 1;
}

/* ------------------------------------------------------------------------------------------
 * Module functions.
 * ---------------------------------------------------------------------------------------- */

/* llaya.new(model_dir [, options]) -> agent | nil, message
 * options: nil, a JSON string, or a table such as { backend = "cpu", threads = 4 }. */
static int module_new(lua_State* L) {
    const char* directory = luaL_checkstring(L, 1);
    const char* error = NULL;
    char* options = NULL;
    laya_agent* handle;
    agent_box* box;
    lua_settop(L, 2); /* the options stay at index 2 even when they were omitted */
    /* the userdata first: if this raises (out of memory), nothing else is allocated yet */
    box = (agent_box*)lua_newuserdata(L, sizeof *box);
    box->handle = NULL;
    luaL_getmetatable(L, AGENT_TYPE);
    lua_setmetatable(L, -2);
    if (!lua_isnoneornil(L, 2)) {
        options = json_argument(L, 2, &error);
        if (!options) return fail(L, error);
    }
    handle = api.create(directory, options ? options : "");
    free(options);
    if (!handle) {
        const char* message = api.last_error();
        lua_pushnil(L);
        lua_pushfstring(L, "cannot load Laya model: %s", message && *message ? message : "unknown error");
        return 2;
    }
    box->handle = handle;
    return 1;
}

static int module_version(lua_State* L) {
    lua_pushstring(L, api.version());
    return 1;
}

static int module_api_version(lua_State* L) {
    lua_pushinteger(L, api.api_version());
    return 1;
}

/* llaya.encode(value) -> JSON text | nil, message */
static int module_encode(lua_State* L) {
    buffer b = {0};
    const char* error = NULL;
    luaL_checkany(L, 1);
    if (!encode_value(L, 1, &b, 0, &error)) {
        free(b.data);
        return fail(L, error);
    }
    if (b.failed || !b.data) {
        free(b.data);
        return fail(L, "out of memory");
    }
    lua_pushlstring(L, b.data, b.length);
    free(b.data);
    return 1;
}

/* llaya.decode(json) -> value | nil, message */
static int module_decode(lua_State* L) {
    size_t length;
    const char* text = luaL_checklstring(L, 1, &length);
    return decode_json(L, text, length);
}

static const luaL_Reg agent_methods[] = {
    {"predict", agent_predict},
    {"predict_table", agent_predict_table},
    {"prepare", agent_prepare},
    {"prepare_table", agent_prepare_table},
    {"info", agent_info},
    {"info_table", agent_info_table},
    {"ask_yes_no", agent_ask_yes_no},
    {"ask_yes_no_table", agent_ask_yes_no_table},
    {"ask_choice", agent_ask_choice},
    {"ask_choice_table", agent_ask_choice_table},
    {"ask_score", agent_ask_score},
    {"ask_score_table", agent_ask_score_table},
    {"close", agent_close},
    {NULL, NULL}};

static const luaL_Reg module_functions[] = {
    {"new", module_new},
    {"version", module_version},
    {"api_version", module_api_version},
    {"encode", module_encode},
    {"decode", module_decode},
    {NULL, NULL}};

static void register_functions(lua_State* L, const luaL_Reg* functions) {
#if LUA_VERSION_NUM < 502
    luaL_register(L, NULL, functions);
#else
    luaL_setfuncs(L, functions, 0);
#endif
}

LLAYA_EXPORT int luaopen_llaya(lua_State* L) {
    char error[512];
    if (!load_api(error, sizeof error)) return luaL_error(L, "llaya: %s", error);

    luaL_newmetatable(L, AGENT_TYPE);
    lua_newtable(L); /* methods */
    register_functions(L, agent_methods);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, agent_close);
    lua_setfield(L, -2, "__gc");
    lua_pushcfunction(L, agent_close);
    lua_setfield(L, -2, "__close"); /* Lua 5.4: local agent <close> = llaya.new(...) */
    lua_pushcfunction(L, agent_tostring);
    lua_setfield(L, -2, "__tostring");
    lua_pop(L, 1);

    lua_newtable(L);
    register_functions(L, module_functions);
    lua_pushlightuserdata(L, NULL);
    lua_setfield(L, -2, "null");
    lua_pushliteral(L, LLAYA_VERSION);
    lua_setfield(L, -2, "_VERSION");
    return 1;
}
