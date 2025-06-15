# Define project-specific variables
# Use $(CURDIR) to refer to the current directory where the Makefile is located.
# However, for clarity and matching the original script's intent of relative paths,
# we'll define them relative to the Makefile's location.
BCM2835_LIB_SOURCE_DIR := ./lib/bcm2835-1.75
LIBINIH_LIB_SOURCE_DIR := ./lib/inih-r60
BIN_DIR := bin
LIB_DEST_DIR := lib
INCLUDE_DEST_DIR := include

# Define compilers and their flags
CC := gcc
CXX := g++
CFLAGS := -O3 -Wall
# CXXFLAGS for C++ sources, including C++11 standard and suppressing unused variable warning
CXXFLAGS := -O3 -Wall -std=c++11 -Wno-unused-variable

# Libraries to link with the final executables
# -fPIC is often used for position-independent code, though it might not be strictly needed for executables.
# -lm for math library, -lrt for real-time extensions, -lpthread for pthreads.
LDLIBS := -fPIC -lm -lrt -lpthread

# List of directories to create if they don't exist
# This ensures that our output directories are ready before files are copied/generated into them.
OUTPUT_DIRS := $(BIN_DIR) $(LIB_DEST_DIR) $(INCLUDE_DEST_DIR)

# .PHONY targets are rules that do not produce files by the same name.
# This prevents Make from confusing them with actual files.
.PHONY: all clean bcm2835_lib inih_lib

# Default target: builds everything.
# It depends on ensuring output directories exist and then building both executables.
all: $(OUTPUT_DIRS) $(BIN_DIR)/ws2812rpi_spi $(BIN_DIR)/ws2812rpi_pipe

# Rule to create output directories.
# The $@ variable expands to the name of the target (e.g., bin, lib, include).
$(OUTPUT_DIRS):
	@echo "Creating directory: $@"
	@mkdir -p $@

# --- Rule for building and installing bcm2835 library ---
# This target ensures the libbcm2835.a and bcm2835.h files are available in
# our project's lib/ and include/ directories respectively.
bcm2835_lib:
	@echo "--- Building bcm2835 library (version 1.75) ---"
	# Navigate to the library source directory, configure, and compile.
	# $(MAKE) -C ... allows running make in a subdirectory.
	# $(shell nproc) gets the number of available CPU cores for parallel compilation.
	cd $(BCM2835_LIB_SOURCE_DIR) && ./configure
	$(MAKE) -C $(BCM2835_LIB_SOURCE_DIR) -j $(shell nproc)
	# Copy the compiled static library and header to our project's designated directories.
	cp $(BCM2835_LIB_SOURCE_DIR)/src/libbcm2835.a $(LIB_DEST_DIR)/
	cp $(BCM2835_LIB_SOURCE_DIR)/src/bcm2835.h $(INCLUDE_DEST_DIR)/

# --- Rule for building and installing inih library ---
# This target ensures libinih.a, libINIReader.a, ini.h, and INIReader.h are available.
inih_lib:
	@echo "--- Building inih library (inih-r60) ---"
	# Compile ini.c and create libinih.a
	cd $(LIBINIH_LIB_SOURCE_DIR) && \
	$(CC) $(CFLAGS) -c ini.c && \
	ar -rcs libinih.a ini.o
	# Compile INIReader.cpp and create libINIReader.a
	cd $(LIBINIH_LIB_SOURCE_DIR) && \
	$(CXX) $(CXXFLAGS) -c ./cpp/INIReader.cpp && \
	ar -rcs libINIReader.a INIReader.o
	# Copy the compiled static libraries and headers to our project's designated directories.
	cp $(LIBINIH_LIB_SOURCE_DIR)/libinih.a $(LIB_DEST_DIR)/
	cp $(LIBINIH_LIB_SOURCE_DIR)/libINIReader.a $(LIB_DEST_DIR)/
	cp $(LIBINIH_LIB_SOURCE_DIR)/cpp/INIReader.h $(INCLUDE_DEST_DIR)/
	cp $(LIBINIH_LIB_SOURCE_DIR)/ini.h $(INCLUDE_DEST_DIR)/

# --- Target for the ws2812rpi_spi executable ---
# This executable depends on its source file, the bcm2835 library, and the inih library.
# The dependencies on the .a and .h files ensure that the 'bcm2835_lib' and 'inih_lib'
# targets are run if the required files are missing or older.
$(BIN_DIR)/ws2812rpi_spi: src/ws2812rpi_spi.cpp bcm2835_lib inih_lib \
                          $(LIB_DEST_DIR)/libbcm2835.a $(LIB_DEST_DIR)/libINIReader.a $(LIB_DEST_DIR)/libinih.a \
                          $(INCLUDE_DEST_DIR)/bcm2835.h $(INCLUDE_DEST_DIR)/INIReader.h $(INCLUDE_DEST_DIR)/ini.h
	@echo "--- Linking ws2812rpi_spi ---"
	# Compile and link the main source file.
	# -I$(INCLUDE_DEST_DIR) tells the compiler where to find headers.
	# $< expands to the first prerequisite (src/ws2812rpi_spi.cpp).
	# -o $@ expands to the target name (bin/ws2812rpi_spi).
	# Explicitly link all necessary static libraries.
	$(CXX) $(CXXFLAGS) -I$(INCLUDE_DEST_DIR) $< -o $@ \
	  $(LIB_DEST_DIR)/libbcm2835.a $(LIB_DEST_DIR)/libINIReader.a $(LIB_DEST_DIR)/libinih.a $(LDLIBS)

# --- Target for the ws2812rpi_pipe executable ---
# This executable depends on its source file and the inih library.
$(BIN_DIR)/ws2812rpi_pipe: src/ws2812rpi_pipe.cpp inih_lib \
                           $(LIB_DEST_DIR)/libINIReader.a $(LIB_DEST_DIR)/libinih.a \
                           $(INCLUDE_DEST_DIR)/INIReader.h $(INCLUDE_DEST_DIR)/ini.h
	@echo "--- Linking ws2812rpi_pipe ---"
	# Compile and link the main source file.
	$(CXX) $(CXXFLAGS) -I$(INCLUDE_DEST_DIR) $< -o $@ \
	  $(LIB_DEST_DIR)/libINIReader.a $(LIB_DEST_DIR)/libinih.a $(LDLIBS)

# --- Clean target ---
# Removes all generated files and directories to ensure a clean build environment.
# 'rm -rf' removes directories and their contents recursively.
# '|| true' ensures the script continues even if a file/directory doesn't exist.
clean:
	@echo "--- Cleaning up project files ---"
	rm -rf $(BIN_DIR) \
	  $(LIB_DEST_DIR)/libbcm2835.a $(LIB_DEST_DIR)/libinih.a $(LIB_DEST_DIR)/libINIReader.a \
	  $(INCLUDE_DEST_DIR)/bcm2835.h $(INCLUDE_DEST_DIR)/ini.h $(INCLUDE_DEST_DIR)/INIReader.h
	# Clean the original library build directories as well.
	@echo "--- Cleaning bcm2835 source directory ---"
	$(MAKE) -C $(BCM2835_LIB_SOURCE_DIR) clean || true
	@echo "--- Cleaning inih source directory ---"
	rm -f $(LIBINIH_LIB_SOURCE_DIR)/*.o $(LIBINIH_LIB_SOURCE_DIR)/*.a || true

