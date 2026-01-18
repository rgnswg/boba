#include "projectile.h"
#include "raymath.h"

void Proj_Init(ProjectileManager* pm) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        pm->pool[i].active = false;
    }
}

void Proj_Spawn(ProjectileManager* pm, Vector3 pos, Vector3 dir, float speed, float life, float radius, Color color) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!pm->pool[i].active) {
            pm->pool[i].active = true;
            pm->pool[i].position = pos;
            pm->pool[i].direction = Vector3Normalize(dir);
            pm->pool[i].speed = speed;
            pm->pool[i].lifeTime = life;
            pm->pool[i].radius = radius;
            pm->pool[i].color = color;
            return;
        }
    }
}

void Proj_Update(ProjectileManager* pm, float dt) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (pm->pool[i].active) {
            pm->pool[i].lifeTime -= dt;
            if (pm->pool[i].lifeTime <= 0) {
                pm->pool[i].active = false;
                continue;
            }
            
            // Movimiento
            Vector3 velocity = Vector3Scale(pm->pool[i].direction, pm->pool[i].speed * dt);
            pm->pool[i].position = Vector3Add(pm->pool[i].position, velocity);
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
