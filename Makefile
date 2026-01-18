CC = gcc
CFLAGS = -Wall -std=c99 -Wno-missing-braces -I/opt/homebrew/include
LDFLAGS = -L/opt/homebrew/lib -lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
TARGET = boba

# Archivos fuente (incluyendo los nuevos)
SRCS = main.c map.c pathfinding.c entity.c projectile.c characters/mongo.c

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET) map.png
