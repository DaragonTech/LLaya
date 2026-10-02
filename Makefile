# Native build of the llaya module for the Lua installed on this machine.
#
#   make LUA_INCDIR=/usr/include/lua5.1                 Linux, macOS
#   make LUA_INCDIR=C:/lua/include LUA_LIBDIR=C:/lua LUA_LIB=lua51      Windows (MinGW)
#
# LUA_INCDIR is the folder with lua.h. On Windows the module links against the Lua DLL of the
# program that will load it: LUA_LIBDIR is where that DLL (or its import library) is and
# LUA_LIB its name without extension. scripts/build-all.sh cross-builds all platforms at once.
LUA_INCDIR ?= /usr/include/lua5.1
LUA_LIBDIR ?= .
LUA_LIB    ?= lua51
CC         ?= cc
CFLAGS     ?= -O2 -Wall -Wextra

ifeq ($(OS),Windows_NT)
  MODULE  := llaya.dll
  LDFLAGS += -shared -L$(LUA_LIBDIR) -l$(LUA_LIB)
  CFLAGS  += -DLUA_BUILD_AS_DLL
else ifeq ($(shell uname -s),Darwin)
  MODULE  := llaya.so
  LDFLAGS += -bundle -undefined dynamic_lookup
else
  MODULE  := llaya.so
  CFLAGS  += -fPIC
  LDFLAGS += -shared -ldl
endif

$(MODULE): src/llaya.c
	$(CC) $(CFLAGS) -fvisibility=hidden -I$(LUA_INCDIR) -o $@ $< $(LDFLAGS)

# Needs the LibLayaX library (laya.dll / liblaya.so / liblaya.dylib) in this folder.
test: $(MODULE)
	$(LUA) test/test.lua $(MODEL)
LUA ?= lua

clean:
	rm -f llaya.so llaya.dll

.PHONY: test clean
