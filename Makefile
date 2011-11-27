TARGET := SSNES-D3D9.dll

CXX_SOURCES := $(wildcard *.cpp)
C_SOURCES := $(wildcard *.c)
OBJECTS := $(C_SOURCES:.c=.o) $(CXX_SOURCES:.cpp=.o)
HEADERS := $(wildcard *.h) $(wildcard *.hpp)

ifeq ($(D3D_INCLUDE_DIR),)
   $(error "D3D_INCLUDE_DIR is not defined. You will most likely need to have D3D SDK installed.")
endif
ifeq ($(D3D_LIB_DIR),)
   $(error "D3D_LIB_DIR is not defined. Point it to MinGW lib dir if it provides D3D import libs.")
endif
ifeq ($(CG_INCLUDE_DIR),)
   $(error "CG_INCLUDE_DIR is not defined.")
endif
ifeq ($(CG_LIB_DIR),)
   $(error "CG_LIB_DIR is not defined. Point this to bin/ or bin.x64/. Import libs might break.")
endif

CC = gcc
CXX = g++

INCDIRS := -I. -I"$(CG_INCLUDE_DIR)" -I"$(D3D_INCLUDE_DIR)"
LIBDIRS := -L"$(CG_LIB_DIR)" -L"$(D3D_LIB_DIR)"

LIBS := -ld3d9 -lcg -lcgD3D9 -ld3dx9 -ldxguid -ldinput8

CFLAGS += -O3 -std=gnu99 -Wall -pedantic
CXXFLAGS += -O3 -std=gnu++0x -fcheck-new
LDFLAGS += -shared -Wl,--version-script=link.T -Wl,--no-undefined -static-libgcc -static-libstdc++ -s

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) -o $@ $(OBJECTS) $(LIBDIRS) $(LIBS) $(LDFLAGS)

%.o: %.cpp $(HEADERS)
	$(CXX) -c -o $@ $< $(CXXFLAGS) $(INCDIRS)

%.o: %.c $(HEADERS)
	$(CC) -c -o $@ $< $(CFLAGS) $(INCDIRS)

clean:
	rm -f $(TARGET) $(OBJECTS)

.PHONY: clean

