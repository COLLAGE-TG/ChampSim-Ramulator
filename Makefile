CXX = /usr/bin/g++
CXXFLAGS_OPT = -fdiagnostics-color=always -O3 -g -Wall -fopenmp -std=c++17
CXXFLAGS_DEFAULT = -fdiagnostics-color=always -g -Wall -fopenmp -std=c++17
INCLUDE_DIRS = -I${workspaceFolder}/vcpkg_installed/x64-linux/include/ -I${workspaceFolder}/include/
LIB_DIRS = -L${workspaceFolder}/vcpkg_installed/x64-linux/lib -L${workspaceFolder}/vcpkg_installed/x64-linux/lib/manual-link
LIBS = -lzma -lz -lbz2 -lfmt

# Source files
SRCS := $(wildcard \
  ${workspaceFolder}/source/ChampSim/*.cc \
  ${workspaceFolder}/source/ChampSim/branch/bimodal/*.cc \
  ${workspaceFolder}/source/ChampSim/branch/gshare/*.cc \
  ${workspaceFolder}/source/ChampSim/branch/hashed_perceptron/*.cc \
  ${workspaceFolder}/source/ChampSim/branch/perceptron/*.cc \
  ${workspaceFolder}/source/ChampSim/prefetcher/no/*.cc \
  ${workspaceFolder}/source/ChampSim/prefetcher/next_line/*.cc \
  ${workspaceFolder}/source/ChampSim/prefetcher/ip_stride/*.cc \
  ${workspaceFolder}/source/ChampSim/prefetcher/no_instr/*.cc \
  ${workspaceFolder}/source/ChampSim/prefetcher/next_line_instr/*.cc \
  ${workspaceFolder}/source/ChampSim/prefetcher/spp_dev/*.cc \
  ${workspaceFolder}/source/ChampSim/prefetcher/va_ampm_lite/*.cc \
  ${workspaceFolder}/source/ChampSim/replacement/lru/*.cc \
  ${workspaceFolder}/source/ChampSim/replacement/ship/*.cc \
  ${workspaceFolder}/source/ChampSim/replacement/srrip/*.cc \
  ${workspaceFolder}/source/ChampSim/replacement/drrip/*.cc \
  ${workspaceFolder}/source/ChampSim/btb/basic_btb/*.cc \
  ${workspaceFolder}/source/Ramulator/*.cpp \
  ${workspaceFolder}/source/*.cc \
)

# Object files
OBJS_OPT = $(SRCS:%.cc=%.o.opt)
OBJS_DEFAULT = $(SRCS:%.cc=%.o.default)

# Output binaries
TARGET_OPT = ${workspaceFolder}/bin/champsim_plus_ramulator_O3
TARGET_DEFAULT = ${workspaceFolder}/bin/champsim_plus_ramulator

# Main build targets
all: opt default

opt: $(TARGET_OPT)

default: $(TARGET_DEFAULT)

# Compile each source file with optimization flags
$(TARGET_OPT): $(OBJS_OPT)
	$(CXX) $(CXXFLAGS_OPT) $(INCLUDE_DIRS) $(LIB_DIRS) $^ -o $@ $(LIBS)

# Compile each source file with default flags
$(TARGET_DEFAULT): $(OBJS_DEFAULT)
	$(CXX) $(CXXFLAGS_DEFAULT) $(INCLUDE_DIRS) $(LIB_DIRS) $^ -o $@ $(LIBS)

# Compile source files with optimization flags to object files
%.o.opt: %.cc
	$(CXX) $(CXXFLAGS_OPT) $(INCLUDE_DIRS) -c $< -o $@

# Compile source files with default flags to object files
%.o.default: %.cc
	$(CXX) $(CXXFLAGS_DEFAULT) $(INCLUDE_DIRS) -c $< -o $@

.PHONY: clean

# Clean up generated files
clean:
	rm -f $(OBJS_OPT) $(OBJS_DEFAULT) $(TARGET_OPT) $(TARGET_DEFAULT)
