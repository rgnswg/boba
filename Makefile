CC      = gcc
CFLAGS  = -Wall -std=gnu99 -Wno-missing-braces
LDFLAGS = -lraylib -lenet -lGL -lm -lpthread -ldl -lrt -lX11

# Fuentes de lógica pura (usados por server y client)
LOGIC_SRCS = map.c pathfinding.c entity.c projectile.c \
             characters/mongo.c characters/dummy.c

# ── Targets ────────────────────────────────────────────────────────────────

.PHONY: all server client boba clean

all: server client

# Servidor headless (sin ventana, pero linka raylib para Vector3/math)
server: server.c $(LOGIC_SRCS)
	$(CC) $(CFLAGS) $^ -o server $(LDFLAGS)

# Cliente gráfico
client: client.c $(LOGIC_SRCS)
	$(CC) $(CFLAGS) $^ -o client $(LDFLAGS)

# Binario standalone original (referencia)
boba: main.c $(LOGIC_SRCS)
	$(CC) $(CFLAGS) $^ -o boba $(LDFLAGS)

clean:
	rm -f server client boba