#ifndef PROJECTILE_H
#define PROJECTILE_H

#include "raylib.h"
#include "entity.h" // Necesitamos conocer a las entidades para golpearlas
#include <stdbool.h>

typedef struct {
    Vector3 position;
    Vector3 direction;
    float speed;
    float lifeTime;    
    float radius;
    float damage;       
    Team team;          // Equipo del proyectil (no daña aliados)
    bool active;
    Color color;
} Projectile;

#define MAX_PROJECTILES 100

// Gestor global de proyectiles (simple pool)
typedef struct {
    Projectile pool[MAX_PROJECTILES];
} ProjectileManager;

void Proj_Init(ProjectileManager* pm);
void Proj_Spawn(ProjectileManager* pm, Vector3 pos, Vector3 dir, float speed, float life, float radius, float damage, Team team, Color color);
// Ahora Update recibe la lista de posibles víctimas
void Proj_Update(ProjectileManager* pm, float dt, Entity* targets[], int targetCount);
void Proj_Draw(ProjectileManager* pm);

#endif
