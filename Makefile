rwildcard = $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2) $(filter $(subst *,%,$2),$d))

TARGET  	:= something
BINDIR  	:= bin
BUILDDIR	:= build
SRCDIR  	:= src

CPP_SOURCES	:= $(call rwildcard,$(SRCDIR)/,*.cpp)
C_SOURCES	:= $(call rwildcard,$(SRCDIR)/,*.c)

INCLUDES	:= include src/ui/imgui /usr/include/freetype2

CPP_OBJECTS	:= $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$(CPP_SOURCES))
C_OBJECTS	:= $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(C_SOURCES))

OBJECTS	:= $(CPP_OBJECTS) $(C_OBJECTS)

CXX	:= g++
CC	:= gcc

SDL_CFLAGS := $(shell pkg-config --cflags sdl2)

CFLAGS		:= -Wall -Wextra \
               $(addprefix -I,$(INCLUDES)) \
			   $(SDL_CFLAGS)
CXXFLAGS	:= -std=c++20 $(CFLAGS)
DEPFLAGS	:= -MMD -MP

LDFLAGS	:= -static-libstdc++ -static-libgcc
LIBS 	:= $(shell pkg-config --libs sdl2) \
		   -lopenal -lGL -ldl -lpthread -lfreetype

all: $(BINDIR)/$(TARGET)

$(BINDIR)/$(TARGET): $(OBJECTS)
	@mkdir -p $(BINDIR)
	$(CXX) $(OBJECTS) -o $@ $(LDFLAGS) $(LIBS)

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILDDIR)
	rm -rf $(BINDIR)/logs
	rm -f $(BINDIR)/$(TARGET)

start: all
	cd $(BINDIR) && ./$(TARGET)

-include $(OBJECTS:.o=.d)

.PHONY: all clean start
