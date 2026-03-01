#ifndef MAP_H
#define MAP_H

#include "raylib.h"
#include <stdbool.h>

typedef struct {
  int width;
  int height;
  bool *walkable;
  Texture2D texture;
  Vector2 offset;

  // Optimización: Un solo modelo estático que contiene TODO el nivel
  Model mapModel;
} GameMap;

// Inicializa el mapa cargando "map.png". Si no existe, lo crea.
void Map_Init(GameMap *map);

// Versión headless (solo datos lógicos, sin GPU). Apta para el servidor.
void Map_InitHeadless(GameMap *map);

// Libera memoria
void Map_Unload(GameMap *map);

// Verifica si una coordenada del MUNDO es caminable
bool Map_IsWalkable(GameMap *map, float worldX, float worldZ);

// Verifica si una coordenada del GRID (pixel) es caminable
bool Map_IsGridWalkable(GameMap *map, int x, int y);

// Dibuja el mapa (por ahora cubos, basado en los pixeles)
void Map_Draw(GameMap *map);

// Convierte Mundo -> Grid
Vector2 WorldToGrid(GameMap *map, Vector3 worldPos);

// Convierte Grid -> Mundo
Vector3 GridToWorld(GameMap *map, int x, int y);

#endif
