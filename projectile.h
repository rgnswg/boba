#ifndef PROJECTILE_H
#define PROJECTILE_H

#include "raylib.h"
#include <stdbool.h>

typedef struct {
    Vector3 position;
    Vector3 direction;
    float speed;
    float lifeTime;     // Cuanto tiempo vive antes de desaparecer
    float radius;
    bool active;
    Color color;
} Projectile;

#define MAX_PROJECTILES 100

// Gestor global de proyectiles (simple pool)
typedef struct {
    Projectile pool[MAX_PROJECTILES];
} ProjectileManager;

void Proj_Init(ProjectileManager* pm);
void Proj_Spawn(ProjectileManager* pm, Vector3 pos, Vector3 dir, float speed, float life, float radius, Color color);
void Proj_Update(ProjectileManager* pm, float dt);
void Proj_Draw(ProjectileManager* pm);

#endif
