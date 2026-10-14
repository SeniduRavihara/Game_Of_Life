# ==============================================================================
# Master Makefile for Conway's Game of Life
# Architecture: Dual-Engine (CUDA GPU & Multi-Threaded OpenMP CPU) with GUI
# ==============================================================================

CXX    = g++
MPICXX ?= mpicxx
NVCC   = nvcc

# Directories
SRC_DIR   = src
INC_DIR   = include
BUILD_DIR = build
BIN_DIR   = bin

# Flags
CXXFLAGS  = -std=c++14 -O3 -Wall -fopenmp -I$(INC_DIR)
NVCCFLAGS = -std=c++14 -O3 -I$(INC_DIR)
MPIFLAGS  = -std=c++14 -O3 -Wall -fopenmp
LDFLAGS   = -lSDL2

# Targets
TARGET_NAME     = game_of_life
TARGET_BIN      = $(BIN_DIR)/$(TARGET_NAME)
MPI_TARGET_NAME = game_of_life_mpi
MPI_TARGET_BIN  = $(BIN_DIR)/$(MPI_TARGET_NAME)

# Check if nvcc is available on system PATH
HAS_NVCC := $(shell which nvcc 2>/dev/null)
HAS_MPI  := $(shell which $(MPICXX) 2>/dev/null)

# Objects
BASE_OBJS = $(BUILD_DIR)/font8x8.o \
            $(BUILD_DIR)/gui.o \
            $(BUILD_DIR)/benchmark.o \
            $(BUILD_DIR)/engine_cpu.o \
            $(BUILD_DIR)/engine_factory.o \
            $(BUILD_DIR)/main.o

CUDA_OBJ  = $(BUILD_DIR)/engine_cuda.o

.PHONY: all auto cpu cuda mpi clean run install uninstall help

# Default: auto-detect environment
all: auto

auto:
ifdef HAS_NVCC
	@echo "========================================================"
	@echo " [DETECTED] NVIDIA CUDA Toolkit found (nvcc)"
	@echo " Building Hardware-Accelerated version (CUDA + CPU + GUI)"
	@echo "========================================================"
	@$(MAKE) --no-print-directory cuda
else
	@echo "========================================================"
	@echo " [DETECTED] No NVIDIA CUDA Toolkit found"
	@echo " Building Multi-Threaded CPU version (OpenMP + GUI)"
	@echo "========================================================"
	@$(MAKE) --no-print-directory cpu
endif

# CPU build (runs on any PC without NVIDIA GPU)
cpu: $(BASE_OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $(TARGET_BIN) $(BASE_OBJS) $(LDFLAGS)
	@ln -sf $(TARGET_BIN) $(TARGET_NAME)
	@echo "--------------------------------------------------------"
	@echo " Build successful! Executable: ./$(TARGET_NAME) (or ./$(TARGET_BIN))"
	@echo "--------------------------------------------------------"

# CUDA build (requires NVIDIA GPU and nvcc)
cuda: CXXFLAGS += -DUSE_CUDA
cuda: NVCCFLAGS += -DUSE_CUDA
cuda: $(BASE_OBJS) $(CUDA_OBJ) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $(TARGET_BIN) $(BASE_OBJS) $(CUDA_OBJ) $(LDFLAGS) -lcudart
	@ln -sf $(TARGET_BIN) $(TARGET_NAME)
	@echo "--------------------------------------------------------"
	@echo " CUDA Build successful! Executable: ./$(TARGET_NAME) (or ./$(TARGET_BIN))"
	@echo "--------------------------------------------------------"

# Compile C++ source files into build/
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile CUDA source files into build/
$(BUILD_DIR)/engine_cuda.o: $(SRC_DIR)/engine_cuda.cu | $(BUILD_DIR)
	$(NVCC) $(NVCCFLAGS) -c $< -o $@

# Ensure directories exist
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

# Distributed-Memory MPI build (requires libopenmpi-dev / mpicxx)
mpi: | $(BIN_DIR)
ifdef HAS_MPI
	@echo "========================================================"
	@echo " [DETECTED] MPI Compiler found ($(MPICXX))"
	@echo " Building Distributed-Memory 2D Cartesian Engine"
	@echo "========================================================"
	$(MPICXX) $(MPIFLAGS) $(SRC_DIR)/main_mpi.cpp -o $(MPI_TARGET_BIN)
	@ln -sf $(MPI_TARGET_BIN) $(MPI_TARGET_NAME)
	@echo "--------------------------------------------------------"
	@echo " MPI Build successful! Executable: ./$(MPI_TARGET_NAME)"
	@echo " Run with: mpirun -np 4 ./$(MPI_TARGET_NAME) 1024 1024 100"
	@echo "--------------------------------------------------------"
else
	@echo "========================================================"
	@echo " [ERROR] MPI compiler '$(MPICXX)' not found!"
	@echo " To install OpenMPI on Ubuntu / Debian, run:"
	@echo "   sudo apt update && sudo apt install -y libopenmpi-dev openmpi-bin"
	@echo " Then retry: make mpi"
	@echo "========================================================"
	@exit 1
endif

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR) $(TARGET_NAME) $(MPI_TARGET_NAME)
	@echo "Cleaned build artifacts."

# Convenience run target
run: all
	./$(TARGET_NAME)

# Install as Desktop Application (with Icon & Application Menu entry)
install: all
	@./install.sh

# Uninstall Desktop Application
uninstall:
	@./uninstall.sh

help:
	@echo "Usage:"
	@echo "  make          : Auto-detects hardware and compiles the best target"
	@echo "  make cpu      : Forces Multi-Threaded CPU build (OpenMP)"
	@echo "  make cuda     : Forces CUDA GPU hardware-accelerated build"
	@echo "  make mpi      : Compiles Distributed-Memory 2D Cartesian MPI engine"
	@echo "  make run      : Compiles and immediately executes"
	@echo "  make install  : Installs the application & icon to your Desktop and App Menu"
	@echo "  make uninstall: Uninstalls the application from Desktop and App Menu"
	@echo "  make clean    : Removes build/ and bin/ directories and binaries"
