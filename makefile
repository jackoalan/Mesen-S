#Welcome to what must be the most terrible makefile ever (but hey, it works)
#Both clang & gcc work fine - clang seems to output faster code
#The only external dependency is SDL2 - everything else is pretty standard.
#Run "make" to build, "make run" to run

#----------------------
#Platform Configuration
#----------------------
#To specify whether you want to build for x86 or x64:
#"MESENPLATFORM=x86 make" or "MESENPLATFORM=x64 make"
#Default is x64

#-----------------------
# Link Time Optimization
#-----------------------
#LTO is supported for clang and gcc (but only seems to help for clang?)
#LTO gives a 25-30% performance boost, so use it whenever you can
#Usage: LTO=true make

MESENFLAGS=
libretro : MESENFLAGS=-D LIBRETRO

ifeq ($(USE_GCC),true)
	CPPC=g++
	CC=gcc
	PROFILE_GEN_FLAG=-fprofile-generate
	PROFILE_USE_FLAG=-fprofile-use
else
	CPPC=clang++
	CC=clang
	PROFILE_GEN_FLAG = -fprofile-instr-generate=$(CURDIR)/PGOHelper/pgo.profraw
	PROFILE_USE_FLAG = -fprofile-instr-use=$(CURDIR)/PGOHelper/pgo.profdata
endif

GCCOPTIONS=-fPIC -Wall --std=c++17 -O0 -g $(MESENFLAGS)
CCOPTIONS=-fPIC -Wall -O0 -g $(MESENFLAGS)
LINKOPTIONS=

ifeq ($(MESENPLATFORM),x86)
	MESENPLATFORM=x86

	GCCOPTIONS += -m32
	CCOPTIONS += -m32
else
	MESENPLATFORM=x64
	GCCOPTIONS += -m64
	CCOPTIONS += -m64
endif

ifeq ($(LTO),true)
	CCOPTIONS += -flto
	GCCOPTIONS += -flto
endif

ifeq ($(PGO),profile)
	CCOPTIONS += ${PROFILE_GEN_FLAG}
	GCCOPTIONS += ${PROFILE_GEN_FLAG}
endif

ifeq ($(PGO),optimize)
	CCOPTIONS += ${PROFILE_USE_FLAG}
	GCCOPTIONS += ${PROFILE_USE_FLAG}
endif

ifeq ($(STATICLINK),true)
	LINKOPTIONS += -static-libgcc -static-libstdc++ 
endif

OBJFOLDER=obj.$(MESENPLATFORM)
SHAREDLIB=libMesenSCore.$(MESENPLATFORM).dll
LIBRETROLIB=mesen-s_libretro.$(MESENPLATFORM).so
RELEASEFOLDER=bin/$(MESENPLATFORM)/Debug

COREOBJ=$(patsubst Core/%.cpp,Core/$(OBJFOLDER)/%.o,$(wildcard Core/*.cpp))
UTILOBJ=$(patsubst Utilities/%.cpp,Utilities/$(OBJFOLDER)/%.o,$(wildcard Utilities/*.cpp)) $(patsubst Utilities/HQX/%.cpp,Utilities/$(OBJFOLDER)/%.o,$(wildcard Utilities/HQX/*.cpp)) $(patsubst Utilities/xBRZ/%.cpp,Utilities/$(OBJFOLDER)/%.o,$(wildcard Utilities/xBRZ/*.cpp)) $(patsubst Utilities/KreedSaiEagle/%.cpp,Utilities/$(OBJFOLDER)/%.o,$(wildcard Utilities/KreedSaiEagle/*.cpp)) $(patsubst Utilities/Scale2x/%.cpp,Utilities/$(OBJFOLDER)/%.o,$(wildcard Utilities/Scale2x/*.cpp))
LINUXOBJ=$(patsubst Linux/%.cpp,Linux/$(OBJFOLDER)/%.o,$(wildcard Linux/*.cpp)) 
SEVENZIPOBJ=$(patsubst SevenZip/%.c,SevenZip/$(OBJFOLDER)/%.o,$(wildcard SevenZip/*.c))
LUAOBJ=$(patsubst Lua/%.c,Lua/$(OBJFOLDER)/%.o,$(wildcard Lua/*.c))
LIBELFOBJ=$(patsubst ElfUtils/libelf/%.c,ElfUtils/libelf/$(OBJFOLDER)/%.o,$(wildcard ElfUtils/libelf/*.c))
LIBDWOBJ=$(patsubst ElfUtils/libdw/%.c,ElfUtils/libdw/$(OBJFOLDER)/%.o,$(wildcard ElfUtils/libdw/*.c))
LIBEUOBJ=$(patsubst ElfUtils/lib/%.c,ElfUtils/lib/$(OBJFOLDER)/%.o,$(wildcard ElfUtils/lib/*.c))
ZLIBOBJ=$(patsubst ZLib/%.c,ZLib/$(OBJFOLDER)/%.o,$(wildcard ZLib/*.c))
DLLOBJ=$(patsubst InteropDLL/%.cpp,InteropDLL/$(OBJFOLDER)/%.o,$(wildcard InteropDLL/*.cpp))

ifeq ($(SYSTEM_LIBEVDEV), true)
	LIBEVDEVLIB=$(shell pkg-config --libs libevdev)
	LIBEVDEVINC=$(shell pkg-config --cflags libevdev)
else
	LIBEVDEVOBJ=$(patsubst Linux/libevdev/%.c,Linux/$(OBJFOLDER)/%.o,$(wildcard Linux/libevdev/*.c))
	LIBEVDEVINC=-I../
endif
SDL2LIB=$(shell sdl2-config --libs)
SDL2INC=$(shell sdl2-config --cflags)
FSLIB=-lstdc++fs

all: ui

ui: InteropDLL/$(OBJFOLDER)/$(SHAREDLIB)
	mkdir -p $(RELEASEFOLDER)/Dependencies
	rm -fr $(RELEASEFOLDER)/Dependencies/*
	cd UpdateHelper && xbuild /property:Configuration="Debug" /property:Platform="AnyCPU"
	cp "bin/Any CPU/Debug/MesenUpdater.exe" $(RELEASEFOLDER)/Dependencies/
	cp -r UI/Dependencies/* $(RELEASEFOLDER)/Dependencies/
	cp InteropDLL/$(OBJFOLDER)/$(SHAREDLIB) $(RELEASEFOLDER)/Dependencies/$(SHAREDLIB)	
	cd $(RELEASEFOLDER)/Dependencies && zip -r ../Dependencies.zip *	
	cd UI && xbuild /property:Configuration="Debug" /property:Platform="$(MESENPLATFORM)" /property:PreBuildEvent="" /property:DefineConstants="HIDETESTMENU,DISABLEAUTOUPDATE"

libretro: Libretro/$(OBJFOLDER)/$(LIBRETROLIB)
	mkdir -p bin
	cp ./Libretro/$(OBJFOLDER)/$(LIBRETROLIB) ./bin/

core: InteropDLL/$(OBJFOLDER)/$(SHAREDLIB)

runtests:
	cd TestHelper/$(OBJFOLDER) && ./testhelper

testhelper: InteropDLL/$(OBJFOLDER)/$(SHAREDLIB)
	mkdir -p TestHelper/$(OBJFOLDER)
	$(CPPC) $(GCCOPTIONS) -Wl,-z,defs -o testhelper TestHelper/*.cpp InteropDLL/ConsoleWrapper.cpp $(SEVENZIPOBJ) $(LUAOBJ) $(LIBELFOBJ) $(LIBDWOBJ) $(LIBEUOBJ) $(ZLIBOBJ) $(LINUXOBJ) $(LIBEVDEVOBJ) $(UTILOBJ) $(COREOBJ) -pthread $(FSLIB) $(SDL2LIB) $(LIBEVDEVLIB)
	mv testhelper TestHelper/$(OBJFOLDER)

pgohelper: InteropDLL/$(OBJFOLDER)/$(SHAREDLIB)
	mkdir -p PGOHelper/$(OBJFOLDER) && cd PGOHelper/$(OBJFOLDER) && $(CPPC) $(GCCOPTIONS) -Wl,-z,defs -o pgohelper ../PGOHelper.cpp ../../bin/pgohelperlib.so -pthread $(FSLIB) $(SDL2LIB) $(LIBEVDEVLIB)
	
SevenZip/$(OBJFOLDER)/%.o: SevenZip/%.c
	mkdir -p SevenZip/$(OBJFOLDER) && cd SevenZip/$(OBJFOLDER) && $(CC) $(CCOPTIONS) -c $(patsubst SevenZip/%, ../%, $<)
Lua/$(OBJFOLDER)/%.o: Lua/%.c
	mkdir -p Lua/$(OBJFOLDER) && cd Lua/$(OBJFOLDER) && $(CC) $(CCOPTIONS) -c $(patsubst Lua/%, ../%, $<)
ElfUtils/lib/$(OBJFOLDER)/%.o: ElfUtils/lib/%.c
	mkdir -p ElfUtils/lib/$(OBJFOLDER) && cd ElfUtils/lib/$(OBJFOLDER) && $(CC) $(CCOPTIONS) -DHAVE_CONFIG_H=1 -I.. -I../.. -c $(patsubst ElfUtils/lib/%, ../%, $<)
ElfUtils/libelf/$(OBJFOLDER)/%.o: ElfUtils/libelf/%.c
	mkdir -p ElfUtils/libelf/$(OBJFOLDER) && cd ElfUtils/libelf/$(OBJFOLDER) && $(CC) $(CCOPTIONS) -DHAVE_CONFIG_H=1 -I.. -I../.. -I../../lib -c $(patsubst ElfUtils/libelf/%, ../%, $<)
ElfUtils/libdw/$(OBJFOLDER)/%.o: ElfUtils/libdw/%.c
	mkdir -p ElfUtils/libdw/$(OBJFOLDER) && cd ElfUtils/libdw/$(OBJFOLDER) && $(CC) $(CCOPTIONS) -DHAVE_CONFIG_H=1 -I.. -I../.. -I../../lib -I../../libelf -c $(patsubst ElfUtils/libdw/%, ../%, $<)
ZLib/$(OBJFOLDER)/%.o: ZLib/%.c
	mkdir -p ZLib/$(OBJFOLDER) && cd ZLib/$(OBJFOLDER) && $(CC) $(CCOPTIONS) -c $(patsubst ZLib/%, ../%, $<)
Utilities/$(OBJFOLDER)/%.o: Utilities/%.cpp
	mkdir -p Utilities/$(OBJFOLDER) && cd Utilities/$(OBJFOLDER) && $(CPPC) $(GCCOPTIONS) -I../../ElfUtils -I../../ElfUtils/libdw -I../../ElfUtils/libelf -c $(patsubst Utilities/%, ../%, $<)
Utilities/$(OBJFOLDER)/%.o: Utilities/HQX/%.cpp
	mkdir -p Utilities/$(OBJFOLDER) && cd Utilities/$(OBJFOLDER) && $(CPPC) $(GCCOPTIONS) -c $(patsubst Utilities/%, ../%, $<)
Utilities/$(OBJFOLDER)/%.o: Utilities/xBRZ/%.cpp
	mkdir -p Utilities/$(OBJFOLDER) && cd Utilities/$(OBJFOLDER) && $(CPPC) $(GCCOPTIONS) -c $(patsubst Utilities/%, ../%, $<)
Utilities/$(OBJFOLDER)/%.o: Utilities/KreedSaiEagle/%.cpp
	mkdir -p Utilities/$(OBJFOLDER) && cd Utilities/$(OBJFOLDER) && $(CPPC) $(GCCOPTIONS) -c $(patsubst Utilities/%, ../%, $<)
Utilities/$(OBJFOLDER)/%.o: Utilities/Scale2x/%.cpp
	mkdir -p Utilities/$(OBJFOLDER) && cd Utilities/$(OBJFOLDER) && $(CPPC) $(GCCOPTIONS) -c $(patsubst Utilities/%, ../%, $<)
Core/$(OBJFOLDER)/%.o: Core/%.cpp
	mkdir -p Core/$(OBJFOLDER) && cd Core/$(OBJFOLDER) && $(CPPC) $(GCCOPTIONS)  -c $(patsubst Core/%, ../%, $<)
Linux/$(OBJFOLDER)/%.o: Linux/%.cpp
	mkdir -p Linux/$(OBJFOLDER) && cd Linux/$(OBJFOLDER) && $(CPPC) $(GCCOPTIONS) -c $(patsubst Linux/%, ../%, $<) $(SDL2INC) $(LIBEVDEVINC)
Linux/$(OBJFOLDER)/%.o: Linux/libevdev/%.c
	mkdir -p Linux/$(OBJFOLDER) && cd Linux/$(OBJFOLDER) && $(CC) $(CCOPTIONS) -c $(patsubst Linux/%, ../%, $<)
InteropDLL/$(OBJFOLDER)/%.o: InteropDLL/%.cpp
	mkdir -p InteropDLL/$(OBJFOLDER) && cd InteropDLL/$(OBJFOLDER) && $(CPPC) $(GCCOPTIONS)  -c $(patsubst InteropDLL/%, ../%, $<) $(SDL2INC)
	
InteropDLL/$(OBJFOLDER)/$(SHAREDLIB): $(SEVENZIPOBJ) $(LUAOBJ) $(LIBELFOBJ) $(LIBDWOBJ) $(LIBEUOBJ) $(ZLIBOBJ) $(UTILOBJ) $(COREOBJ) $(LIBEVDEVOBJ) $(LINUXOBJ) $(DLLOBJ)
	mkdir -p bin
	mkdir -p InteropDLL/$(OBJFOLDER)
	$(CPPC) $(GCCOPTIONS) $(LINKOPTIONS) -Wl,-z,defs -shared -o $(SHAREDLIB) $(DLLOBJ) $(SEVENZIPOBJ) $(LUAOBJ) $(LIBELFOBJ) $(LIBDWOBJ) $(LIBEUOBJ) $(ZLIBOBJ) $(LINUXOBJ) $(LIBEVDEVOBJ) $(UTILOBJ) $(COREOBJ) $(SDL2INC) -pthread $(FSLIB) $(SDL2LIB) $(LIBEVDEVLIB)
	cp $(SHAREDLIB) bin/pgohelperlib.so
	mv $(SHAREDLIB) InteropDLL/$(OBJFOLDER)
	
Libretro/$(OBJFOLDER)/$(LIBRETROLIB): $(SEVENZIPOBJ) $(UTILOBJ) $(COREOBJ) Libretro/libretro.cpp
	mkdir -p bin
	mkdir -p Libretro/$(OBJFOLDER)
	$(CPPC) $(GCCOPTIONS) $(LINKOPTIONS) -Wl,-z,defs -shared -o $(LIBRETROLIB) Libretro/*.cpp $(SEVENZIPOBJ) $(UTILOBJ) $(COREOBJ) -pthread
	cp $(LIBRETROLIB) bin/pgohelperlib.so
	mv $(LIBRETROLIB) Libretro/$(OBJFOLDER) 

pgo:
	./buildPGO.sh
	
official:
	./build.sh
	
debug:
	MONO_LOG_LEVEL=debug mono $(RELEASEFOLDER)/Mesen-S.exe

run:
	mono $(RELEASEFOLDER)/Mesen-S.exe

clean:
	rm -rf Lua/$(OBJFOLDER)
	rm -rf SevenZip/$(OBJFOLDER)
	rm -rf ElfUtils/libelf/$(OBJFOLDER)
	rm -rf ElfUtils/libdw/$(OBJFOLDER)
	rm -rf ElfUtils/lib/$(OBJFOLDER)
	rm -rf ZLib/$(OBJFOLDER)
	rm -rf Utilities/$(OBJFOLDER)
	rm -rf Core/$(OBJFOLDER)
	rm -rf Linux/$(OBJFOLDER)
	rm -rf InteropDLL/$(OBJFOLDER)
	rm -rf Libretro/$(OBJFOLDER)
	rm -rf TestHelper/$(OBJFOLDER)
	rm -rf PGOHelper/$(OBJFOLDER)
	rm -rf $(RELEASEFOLDER)

