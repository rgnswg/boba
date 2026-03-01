#include "map.h"
#include "raymath.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

void CreateDefaultMapImage() {
  int size = 128;
  Image img = GenImageColor(size, size, BLACK);
  int cx = size / 2;
  int cy = size / 2;
  ImageDrawCircle(&img, cx, cy, 50, WHITE);
  ImageDrawCircle(&img, cx, cy, 9, BLACK);
  ImageDrawCircle(&img, cx, cy, 7, WHITE);
  ImageDrawCircle(&img, cx, cy, 16, BLACK);
  ImageDrawCircle(&img, cx, cy, 14, WHITE);
  ImageDrawRectangle(&img, cx - 10, cy - 2, 20, 4, WHITE);
  ImageDrawRectangle(&img, cx - 2, cy - 17, 4, 34, WHITE);
  ExportImage(img, "map.png");
  UnloadImage(img);
  printf("Mapa por defecto creado: map.png\n");
}

// Genera la geometría de un cubo y la añade a los arrays de la malla gigante
// pos: posición del centro del cubo en el mundo
void AddCubeToMesh(Vector3 pos, Vector3 size, int *vIndex, float *vertices,
                   float *texcoords, float *normals) {
  float x = pos.x;
  float y = pos.y;
  float z = pos.z;
  float w = size.x / 2.0f;
  float h = size.y / 2.0f;
  float d = size.z / 2.0f;

  // Definimos las 6 caras (Front, Back, Top, Bottom, Left, Right)
  // 6 caras * 2 triangulos * 3 vertices = 36 vertices por cubo (sin indices
  // para simplificar generación)

  // Normales
  Vector3 nFront = {0, 0, 1};
  Vector3 nBack = {0, 0, -1};
  Vector3 nTop = {0, 1, 0};
  Vector3 nBottom = {0, -1, 0};
  Vector3 nRight = {1, 0, 0};
  Vector3 nLeft = {-1, 0, 0};

// Helper macro para añadir vertice
#define PUSH_VERT(px, py, pz, nx, ny, nz, tx, ty)                              \
  vertices[*vIndex * 3 + 0] = px;                                              \
  vertices[*vIndex * 3 + 1] = py;                                              \
  vertices[*vIndex * 3 + 2] = pz;                                              \
  normals[*vIndex * 3 + 0] = nx;                                               \
  normals[*vIndex * 3 + 1] = ny;                                               \
  normals[*vIndex * 3 + 2] = nz;                                               \
  texcoords[*vIndex * 2 + 0] = tx;                                             \
  texcoords[*vIndex * 2 + 1] = ty;                                             \
  (*vIndex)++;

  // Front Face
  PUSH_VERT(x - w, y - h, z + d, 0, 0, 1, 0, 1);
  PUSH_VERT(x + w, y - h, z + d, 0, 0, 1, 1, 1);
  PUSH_VERT(x - w, y + h, z + d, 0, 0, 1, 0, 0);
  PUSH_VERT(x + w, y + h, z + d, 0, 0, 1, 1, 0);
  PUSH_VERT(x - w, y + h, z + d, 0, 0, 1, 0, 0);
  PUSH_VERT(x + w, y - h, z + d, 0, 0, 1, 1, 1);

  // Back Face
  PUSH_VERT(x - w, y + h, z - d, 0, 0, -1, 1, 0);
  PUSH_VERT(x + w, y - h, z - d, 0, 0, -1, 0, 1);
  PUSH_VERT(x - w, y - h, z - d, 0, 0, -1, 1, 1);
  PUSH_VERT(x + w, y - h, z - d, 0, 0, -1, 0, 1);
  PUSH_VERT(x - w, y + h, z - d, 0, 0, -1, 1, 0);
  PUSH_VERT(x + w, y + h, z - d, 0, 0, -1, 0, 0);

  // Top Face
  PUSH_VERT(x - w, y + h, z - d, 0, 1, 0, 0, 1);
  PUSH_VERT(x - w, y + h, z + d, 0, 1, 0, 0, 0);
  PUSH_VERT(x + w, y + h, z + d, 0, 1, 0, 1, 0);
  PUSH_VERT(x + w, y + h, z + d, 0, 1, 0, 1, 0);
  PUSH_VERT(x + w, y + h, z - d, 0, 1, 0, 1, 1);
  PUSH_VERT(x - w, y + h, z - d, 0, 1, 0, 0, 1);

  // Bottom Face
  PUSH_VERT(x - w, y - h, z - d, 0, -1, 0, 1, 1);
  PUSH_VERT(x + w, y - h, z - d, 0, -1, 0, 0, 1);
  PUSH_VERT(x - w, y - h, z + d, 0, -1, 0, 1, 0);
  PUSH_VERT(x + w, y - h, z + d, 0, -1, 0, 0, 0);
  PUSH_VERT(x - w, y - h, z + d, 0, -1, 0, 1, 0);
  PUSH_VERT(x + w, y - h, z - d, 0, -1, 0, 0, 1);

  // Right Face
  PUSH_VERT(x + w, y - h, z - d, 1, 0, 0, 1, 1);
  PUSH_VERT(x + w, y + h, z - d, 1, 0, 0, 1, 0);
  PUSH_VERT(x + w, y + h, z + d, 1, 0, 0, 0, 0);
  PUSH_VERT(x + w, y + h, z + d, 1, 0, 0, 0, 0);
  PUSH_VERT(x + w, y - h, z + d, 1, 0, 0, 0, 1);
  PUSH_VERT(x + w, y - h, z - d, 1, 0, 0, 1, 1);

  // Left Face
  PUSH_VERT(x - w, y - h, z - d, -1, 0, 0, 0, 1);
  PUSH_VERT(x - w, y - h, z + d, -1, 0, 0, 1, 1);
  PUSH_VERT(x - w, y + h, z + d, -1, 0, 0, 1, 0);
  PUSH_VERT(x - w, y + h, z + d, -1, 0, 0, 1, 0);
  PUSH_VERT(x - w, y + h, z - d, -1, 0, 0, 0, 0);
  PUSH_VERT(x - w, y - h, z - d, -1, 0, 0, 0, 1);
}

// Versión headless: solo carga los datos lógicos de caminos (sin GPU).
// Usar en el servidor headless en lugar de Map_Init.
void Map_InitHeadless(GameMap *map) {
  if (!FileExists("map.png")) {
    CreateDefaultMapImage();
  }

  Image img = LoadImage("map.png");
  map->width = img.width;
  map->height = img.height;
  map->offset = (Vector2){map->width / 2.0f, map->height / 2.0f};
  map->walkable = (bool *)malloc(map->width * map->height * sizeof(bool));

  for (int i = 0; i < map->width * map->height; i++) {
    Color c = GetImageColor(img, i % map->width, i / map->width);
    map->walkable[i] = (c.r > 100);
  }
  UnloadImage(img);

  // Dejar el modelo y la textura sin inicializar (no se usan en el servidor)
  map->mapModel = (Model){0};
}

void Map_Init(GameMap *map) {
  if (!FileExists("map.png")) {
    CreateDefaultMapImage();
  }

  Image img = LoadImage("map.png");
  map->width = img.width;
  map->height = img.height;
  map->offset = (Vector2){map->width / 2.0f, map->height / 2.0f};
  map->walkable = (bool *)malloc(map->width * map->height * sizeof(bool));

  // 1. Analizar Mapa y Contar Paredes
  int wallCount = 0;
  for (int i = 0; i < map->width * map->height; i++) {
    Color c = GetImageColor(img, i % map->width, i / map->width);
    bool isFloor = (c.r > 100);
    map->walkable[i] = isFloor;
    if (!isFloor)
      wallCount++;
  }
  UnloadImage(img);

  printf("Generando Malla Estática... (%d bloques)\n", wallCount);

  // 2. Construir UNA sola malla gigante
  Mesh mesh = {0};
  mesh.vertexCount =
      wallCount * 36; // 36 vertices por cubo (6 caras * 2 tris * 3 verts)
  mesh.triangleCount = wallCount * 12;

  mesh.vertices = (float *)malloc(mesh.vertexCount * 3 * sizeof(float));
  mesh.texcoords = (float *)malloc(mesh.vertexCount * 2 * sizeof(float));
  mesh.normals = (float *)malloc(mesh.vertexCount * 3 * sizeof(float));

  int vIndex = 0;
  Vector3 cubeSize = {1.0f, 2.0f, 1.0f};

  for (int y = 0; y < map->height; y++) {
    for (int x = 0; x < map->width; x++) {
      if (!map->walkable[y * map->width + x]) {
        Vector3 pos = GridToWorld(map, x, y);
        AddCubeToMesh(pos, cubeSize, &vIndex, mesh.vertices, mesh.texcoords,
                      mesh.normals);
      }
    }
  }

  UploadMesh(&mesh, false); // Subir a GPU

  map->mapModel = LoadModelFromMesh(mesh);

  // 3. Crear y asignar textura de borde
  Image blockImg = GenImageColor(64, 64, DARKBROWN);
  ImageDrawRectangleLines(&blockImg, (Rectangle){0, 0, 64, 64}, 4,
                          BLACK); // Borde mas grueso
  ImageDrawRectangleLines(&blockImg, (Rectangle){2, 2, 60, 60}, 2, DARKGRAY);
  Texture2D blockTex = LoadTextureFromImage(blockImg);
  UnloadImage(blockImg);

  map->mapModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = blockTex;
}

void Map_Unload(GameMap *map) {
  if (map->walkable)
    free(map->walkable);

  // Solo liberar GPU si el modelo fue inicializado (no headless)
  if (map->mapModel.materialCount > 0) {
    UnloadTexture(
        map->mapModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture);
    UnloadModel(map->mapModel);
  }
}

bool Map_IsGridWalkable(GameMap *map, int x, int y) {
  if (x < 0 || x >= map->width || y < 0 || y >= map->height)
    return false;
  return map->walkable[y * map->width + x];
}

bool Map_IsWalkable(GameMap *map, float worldX, float worldZ) {
  Vector2 gridPos = WorldToGrid(map, (Vector3){worldX, 0, worldZ});
  return Map_IsGridWalkable(map, (int)gridPos.x, (int)gridPos.y);
}

Vector2 WorldToGrid(GameMap *map, Vector3 worldPos) {
  return (Vector2){worldPos.x + map->offset.x, worldPos.z + map->offset.y};
}

Vector3 GridToWorld(GameMap *map, int x, int y) {
  return (Vector3){x - map->offset.x + 0.5f, 1.0f, y - map->offset.y + 0.5f};
}

void Map_Draw(GameMap *map) {
  DrawPlane((Vector3){0, 0, 0}, (Vector2){100.0f, 100.0f}, DARKGREEN);

  // Dibujar el modelo único. Renderizado instantáneo.
  DrawModel(map->mapModel, (Vector3){0, 0, 0}, 1.0f, WHITE);
}