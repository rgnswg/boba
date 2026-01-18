#include "pathfinding.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>

typedef struct {
    int x, y;
    int parentIndex; // Indice en el array flat (-1 si no tiene)
    float gCost;
    float hCost;
    float fCost;
    bool closed;
    bool opened;
} Node;

int Path_Find(GameMap* map, Vector3 startPos, Vector3 endPos, Vector3* pathOut, int maxPoints) {
    Vector2 startGrid = WorldToGrid(map, startPos);
    Vector2 endGrid = WorldToGrid(map, endPos);
    
    int startX = (int)startGrid.x;
    int startY = (int)startGrid.y;
    int endX = (int)endGrid.x;
    int endY = (int)endGrid.y;

    // Validaciones básicas
    if (!Map_IsGridWalkable(map, endX, endY)) return 0;
    if (startX == endX && startY == endY) return 0;

    int totalNodes = map->width * map->height;
    
    // Alocamos nodos dinámicamente según el tamaño del mapa
    // NOTA: Para performance extrema en loops rápidos, esto debería ser un pool pre-alocado.
    Node* nodes = (Node*)malloc(totalNodes * sizeof(Node));
    
    // Inicializar nodos (podríamos usar memset para velocidad y solo setear lo necesario)
    for(int i=0; i<totalNodes; i++) {
        nodes[i].closed = false;
        nodes[i].opened = false;
        nodes[i].fCost = 999999.0f;
        nodes[i].parentIndex = -1;
        nodes[i].x = i % map->width;
        nodes[i].y = i / map->width;
    }

    int startIndex = startY * map->width + startX;
    int endIndex = endY * map->width + endX;

    nodes[startIndex].gCost = 0;
    nodes[startIndex].hCost = 0;
    nodes[startIndex].fCost = 0;
    nodes[startIndex].opened = true;

    bool pathFound = false;
    int finalNodeIndex = -1;

    // Loop principal A*
    while (true) {
        float lowestF = 999999.0f;
        int currentIndex = -1;

        // Búsqueda lineal del menor F (Optimizable con Binary Heap)
        // Para mapas de 128x128 todavía es aceptable, pero lento para mapas grandes.
        for(int i=0; i<totalNodes; i++) {
            if (nodes[i].opened && !nodes[i].closed) {
                if (nodes[i].fCost < lowestF) {
                    lowestF = nodes[i].fCost;
                    currentIndex = i;
                }
            }
        }

        if (currentIndex == -1) break; // No path

        if (currentIndex == endIndex) {
            pathFound = true;
            finalNodeIndex = currentIndex;
            break;
        }

        nodes[currentIndex].closed = true;
        
        int cX = nodes[currentIndex].x;
        int cY = nodes[currentIndex].y;

        // Vecinos
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                if (dx == 0 && dy == 0) continue;

                int nX = cX + dx;
                int nY = cY + dy;

                if (!Map_IsGridWalkable(map, nX, nY)) continue;

                // Evitar cortar esquinas (opcional, pero mejora realismo)
                // Si nX, cY es pared Y cX, nY es pared, no puedo pasar diagonal
                if (dx != 0 && dy != 0) {
                   if (!Map_IsGridWalkable(map, nX, cY) || !Map_IsGridWalkable(map, cX, nY)) continue;
                }

                int neighborIndex = nY * map->width + nX;
                
                if (nodes[neighborIndex].closed) continue;

                float dist = (dx == 0 || dy == 0) ? 1.0f : 1.414f;
                float tentativeG = nodes[currentIndex].gCost + dist;

                if (!nodes[neighborIndex].opened || tentativeG < nodes[neighborIndex].gCost) {
                    nodes[neighborIndex].gCost = tentativeG;
                    float hX = (float)(endX - nX);
                    float hY = (float)(endY - nY);
                    nodes[neighborIndex].hCost = sqrtf(hX*hX + hY*hY);
                    nodes[neighborIndex].fCost = nodes[neighborIndex].gCost + nodes[neighborIndex].hCost;
                    nodes[neighborIndex].parentIndex = currentIndex;
                    nodes[neighborIndex].opened = true;
                }
            }
        }
    }

    int count = 0;
    if (pathFound) {
        // Backtracking
        int currIdx = finalNodeIndex;
        Vector3 tempPath[1024]; // Buffer temporal interno

        while (nodes[currIdx].parentIndex != -1 && count < 1024) {
            tempPath[count] = GridToWorld(map, nodes[currIdx].x, nodes[currIdx].y);
            count++;
            currIdx = nodes[currIdx].parentIndex;
        }

        // Invertir y copiar a pathOut
        int outLimit = (count < maxPoints) ? count : maxPoints;
        for (int i = 0; i < outLimit; i++) {
            pathOut[i] = tempPath[count - 1 - i];
        }
        count = outLimit;
    }

    free(nodes);
    return count;
}
