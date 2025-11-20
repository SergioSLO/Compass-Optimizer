# --------- Compiler and flags ---------
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra

# --------- Directories ---------
SRC_DIR   := src
BUILD_DIR := build
BIN_DIR   := bin
DATA_DIR  := data

# --------- Cross-platform mkdir ---------
ifeq ($(OS),Windows_NT)
    MKDIR_P = mkdir
    RM_RF_BUILD = rmdir /S /Q $(BUILD_DIR)
    RM_RF_BIN   = rmdir /S /Q $(BIN_DIR)
else
    MKDIR_P = mkdir -p
    RM_RF_BUILD = rm -rf $(BUILD_DIR)
    RM_RF_BIN   = rm -rf $(BIN_DIR)
endif

# --------- Sources / Objects / Target ---------
SRCS  := $(wildcard $(SRC_DIR)/*.cpp)
OBJS  := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRCS))
TARGET := $(BIN_DIR)/compass_lite

# --------- Default target ---------
all: $(TARGET)

# --------- Build rules ---------
$(BUILD_DIR):
	$(MKDIR_P) $(BUILD_DIR)

$(BIN_DIR):
	$(MKDIR_P) $(BIN_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

# --------- Run target ---------
run: $(TARGET)
ifndef QUERY
	$(error Usage: make run QUERY=query/your_query.sql)
endif
	$(TARGET) $(wildcard $(DATA_DIR)/*.csv) $(QUERY)

# --------- Cleaning ---------
clean:
	-$(RM_RF_BUILD)
	-$(RM_RF_BIN)

.PHONY: all run clean
