CC = gcc
CFLAGS = -Wall -Wextra -O2 -I./include
LDFLAGS = -lpthread -lwebsockets

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

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
        @mkdir -p $(OBJ_DIR)

.PHONY: all clean
