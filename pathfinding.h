#ifndef PATHFINDING_H
#define PATHFINDING_H

#include "raylib.h"
#include "map.h"

// Encuentra un camino desde startPos hasta endPos usando A*.
// Retorna la cantidad de puntos y llena pathOut.
// maxPoints es el tamaño del buffer pathOut.
int Path_Find(GameMap* map, Vector3 startPos, Vector3 endPos, Vector3* pathOut, int maxPoints);

#endif
