TARGET := SSNES-D3D9.dll

SOURCES := $(wildcard *.cpp)
OBJECTS := $(SOURCES:.cpp=.o)

ifeq ($(D3D_INCLUDE_DIR),)
   $(error "D3D_INCLUDE_DIR is not defined.")
endif
ifeq ($(D3D_LIB_DIR),)
   $(error "D3D_LIB_DIR is not defined.")
endif
ifeq ($(CG_INCLUDE_DIR),)
   $(error "CG_INCLUDE_DIR is not defined.")
endif
ifeq ($(CG_LIB_DIR),)
   $(error "CG_LIB_DIR is not defined.")
endif

CXX = g++

INCDIRS := -I. -I$(CG_INCLUDE_DIR) -I$(D3D_INCLUDE_DIR)
LIBDIRS := -L$(CG_LIB_DIR) -L$(D3D_LIB_DIR)

LIBS := -ld3d9 -lcg -lcgD3D9 -ld3dx9 -ldxguid -ldinput8

CXXFLAGS += -O3 -std=gnu++0x
LDFLAGS += -shared -Wl,--version-script=link.T -static-libgcc -static-libstdc++ -s

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) -o $@ $(OBJECTS) $(LIBDIRS) $(LIBS) $(LDFLAGS)

%.o: %.cpp
	$(CXX) -c -o $@ $< $(CXXFLAGS) $(INCDIRS)

clean:
	rm -f $(TARGET) $(OBJECTS)

.PHONY: clean

