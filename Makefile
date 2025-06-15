# Define project-specific variables
BCM2835_LIB_SOURCE_DIR := ./lib/bcm2835-1.75
LIBINIH_LIB_SOURCE_DIR := ./lib/inih-r60
BIN_DIR := bin
LIB_DEST_DIR := lib
INCLUDE_DEST_DIR := include

# Define compilers and their flags
CC := gcc
CXX := g++
CFLAGS := -O3 -Wall
CXXFLAGS := -O3 -Wall -std=c++11 -Wno-unused-variable

# Libraries to link with the final executables
LDLIBS := -fPIC -lm -lrt -lpthread

# List of directories to create if they don't exist
OUTPUT_DIRS := $(BIN_DIR) $(LIB_DEST_DIR) $(INCLUDE_DEST_DIR)

# .PHONY targets are rules that do not produce files by the same name.
# This prevents Make from confusing them with actual files.
.PHONY: all clean bcm2835_lib inih_lib

# Default target: builds everything.
all: $(OUTPUT_DIRS) $(BIN_DIR)/ws2812rpi_spi $(BIN_DIR)/ws2812rpi_pipe

# Rule to create output directories.
$(OUTPUT_DIRS):
	@echo "Creating directory: $@"
	@mkdir -p $@

# --- Rule for building and installing bcm2835 library ---
# This target ensures the libbcm2835.a and bcm2835.h files are available.
bcm2835_lib:
	@echo "--- Building bcm2835 library (version 1.75) ---"
	cd $(BCM2835_LIB_SOURCE_DIR) && ./configure
	$(MAKE) -C $(BCM2835_LIB_SOURCE_DIR) -j $(shell nproc)
	cp $(BCM2835_LIB_SOURCE_DIR)/src/libbcm2835.a $(LIB_DEST_DIR)/
	cp $(BCM2835_LIB_SOURCE_DIR)/src/bcm2835.h $(INCLUDE_DEST_DIR)/

# --- Rule for building and installing inih library ---
# This target ensures libinih.a, libINIReader.a, ini.h, and INIReader.h are available.
inih_lib:
	@echo "--- Building inih library (inih-r60) ---"
	cd $(LIBINIH_LIB_SOURCE_DIR) && \
	$(CC) $(CFLAGS) -c ini.c && \
	ar -rcs libinih.a ini.o
	cd $(LIBINIH_LIB_SOURCE_DIR) && \
	$(CXX) $(CXXFLAGS) -c ./cpp/INIReader.cpp && \
	ar -rcs libINIReader.a INIReader.o
	cp $(LIBINIH_LIB_SOURCE_DIR)/libinih.a $(LIB_DEST_DIR)/
	cp $(LIBINIH_LIB_SOURCE_DIR)/libINIReader.a $(LIB_DEST_DIR)/
	cp $(LIBINIH_LIB_SOURCE_DIR)/cpp/INIReader.h $(INCLUDE_DEST_DIR)/
	cp $(LIBINIH_LIB_SOURCE_DIR)/ini.h $(INCLUDE_DEST_DIR)/

# --- Target for the ws2812rpi_spi executable ---
# Now, this target depends on the phony targets 'bcm2835_lib' and 'inih_lib'.
# This ensures those build processes complete before linking.
$(BIN_DIR)/ws2812rpi_spi: src/ws2812rpi_spi.cpp bcm2835_lib inih_lib
	@echo "--- Linking ws2812rpi_spi ---"
	$(CXX) $(CXXFLAGS) -I$(INCLUDE_DEST_DIR) $< -o $@ \
	  $(LIB_DEST_DIR)/libbcm2835.a $(LIB_DEST_DIR)/libINIReader.a $(LIB_DEST_DIR)/libinih.a $(LDLIBS)

# --- Target for the ws2812rpi_pipe executable ---
# This target also depends on the phony 'inih_lib' target.
$(BIN_DIR)/ws2812rpi_pipe: src/ws2812rpi_pipe.cpp inih_lib
	@echo "--- Linking ws2812rpi_pipe ---"
	$(CXX) $(CXXFLAGS) -I$(INCLUDE_DEST_DIR) $< -o $@ \
	  $(LIB_DEST_DIR)/libINIReader.a $(LIB_DEST_DIR)/libinih.a $(LDLIBS)

# --- Clean target ---
clean:
	@echo "--- Cleaning up project files ---"
	rm -rf $(BIN_DIR) \
	  $(LIB_DEST_DIR)/libbcm2835.a $(LIB_DEST_DIR)/libinih.a $(LIB_DEST_DIR)/libINIReader.a \
	  $(INCLUDE_DEST_DIR)/bcm2835.h $(INCLUDE_DEST_DIR)/ini.h $(INCLUDE_DEST_DIR)/INIReader.h
	@echo "--- Cleaning bcm2835 source directory ---"
	$(MAKE) -C $(BCM2835_LIB_SOURCE_DIR) clean || true
	@echo "--- Cleaning inih source directory ---"
	rm -f $(LIBINIH_LIB_SOURCE_DIR)/*.o $(LIBINIH_LIB_SOURCE_DIR)/*.a || true

