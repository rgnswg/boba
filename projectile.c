#include "projectile.h"
#include "raymath.h"

void Proj_Init(ProjectileManager* pm) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        pm->pool[i].active = false;
    }
}

void Proj_Spawn(ProjectileManager* pm, Vector3 pos, Vector3 dir, float speed, float life, float radius, float damage, Team team, Color color) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!pm->pool[i].active) {
            pm->pool[i].active = true;
            pm->pool[i].position = pos;
            pm->pool[i].direction = Vector3Normalize(dir);
            pm->pool[i].speed = speed;
            pm->pool[i].lifeTime = life;
            pm->pool[i].radius = radius;
            pm->pool[i].damage = damage;
            pm->pool[i].team = team;
            pm->pool[i].color = color;
            return;
        }
    }
}

void Proj_Update(ProjectileManager* pm, float dt, Entity* targets[], int targetCount) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!pm->pool[i].active) continue;

        Projectile* p = &pm->pool[i];

        // 1. Ciclo de Vida
        p->lifeTime -= dt;
        if (p->lifeTime <= 0) {
            p->active = false;
            continue;
        }
        
        // 2. Movimiento
        Vector3 velocity = Vector3Scale(p->direction, p->speed * dt);
        Vector3 nextPos = Vector3Add(p->position, velocity);
        
        // 3. Colisión
        bool hit = false;
        for (int t = 0; t < targetCount; t++) {
            Entity* ent = targets[t];
            if (!ent->active) continue;
            
            // FUEGO AMIGO OFF:
            // Si son del mismo equipo, ignorar colisión.
            // Excepción: TEAM_NEUTRAL recibe daño de todos (y sus proyectiles dañan a todos)
            if (p->team != TEAM_NEUTRAL && ent->team == p->team) continue;

            if (CheckCollisionSpheres(nextPos, p->radius, ent->position, ent->radius)) {
                // IMPACTO!
                Entity_TakeDamage(ent, p->damage);
                hit = true;
                break; 
            }
        }

        if (hit) {
            p->active = false; 
        } else {
            p->position = nextPos;
        }
    }
}

void Proj_Draw(ProjectileManager* pm) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (pm->pool[i].active) {
            DrawSphere(pm->pool[i].position, pm->pool[i].radius, pm->pool[i].color);
        }
    }
}
