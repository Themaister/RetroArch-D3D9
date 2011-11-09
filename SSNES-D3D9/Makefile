TARGET := SSNES-D3D9.dll

SOURCES := $(wildcard *.cpp)
OBJECTS := $(SOURCES:.cpp=.o)

CXX = g++

INCDIRS := -I. -IInclude

ifneq ($(BUILD_64BIT),)
   LIBDIRS := -LLib/x64
else
   LIBDIRS := -LLib/x86
endif

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

