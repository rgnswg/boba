CC = gcc
CFLAGS = -Wall -std=c99 -Wno-missing-braces -I/opt/homebrew/include
LDFLAGS = -L/opt/homebrew/lib -lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
TARGET = boba

# Archivos fuente
SRCS = main.c map.c pathfinding.c entity.c projectile.c characters/mongo.c characters/dummy.c

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET) map.png