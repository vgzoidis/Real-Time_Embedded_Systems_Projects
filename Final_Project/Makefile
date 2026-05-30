# For cross-compiling to Raspberry Pi, you can run: make pi
CROSS_COMPILE ?=
CC = $(if $(CROSS_COMPILE),$(CROSS_COMPILE)gcc,gcc)

# If cross-compiling, you need the Pi's libraries (sysroot)
SYSROOT ?=

CFLAGS = -Wall -Wextra -Wpedantic -O2 -I./include
LDFLAGS = -lpthread -lwebsockets

# Apply sysroot and Pi Zero W specific architecture flags if cross compiling
ifneq ($(CROSS_COMPILE),)
	CFLAGS += -mcpu=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
endif

ifneq ($(SYSROOT),)
    CFLAGS += --sysroot=$(SYSROOT) -I$(SYSROOT)/usr/include/arm-linux-gnueabihf
    LDFLAGS += --sysroot=$(SYSROOT) -B$(SYSROOT)/usr/lib/arm-linux-gnueabihf -L$(SYSROOT)/usr/lib/arm-linux-gnueabihf -L$(SYSROOT)/lib/arm-linux-gnueabihf -Wl,-rpath-link=$(SYSROOT)/usr/lib/arm-linux-gnueabihf,-rpath-link=$(SYSROOT)/lib/arm-linux-gnueabihf
endif

# Directories
SRC_DIR = src
OBJ_DIR = obj
INC_DIR = include

# Source files
SRCS = $(SRC_DIR)/main.c \
       $(SRC_DIR)/buffer.c \
       $(SRC_DIR)/consumer.c \
       $(SRC_DIR)/producer.c \
       $(SRC_DIR)/monitor.c \
       $(SRC_DIR)/cJSON.c

# Object files
OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

# Target executable
TARGET = telemetry_logger

all: $(TARGET)

pi:
	$(MAKE) CROSS_COMPILE=arm-linux-gnueabihf- SYSROOT=$$HOME/pi-sysroot all

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(TARGET)
.PHONY: all clean
