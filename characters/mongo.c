#include "mongo.h"
#include "../projectile.h"
#include "raymath.h"
#include <stdio.h>

// --- Habilidades de Mongo ---

void Mongo_Q(Entity* self, Vector3 targetPos) {
    // Disparar proyectil hacia targetPos
    ProjectileManager* pm = (ProjectileManager*)self->context;
    
    // Dirección puramente horizontal
    Vector3 dir = Vector3Subtract(targetPos, self->position);
    dir.y = 0; // Forzar plano horizontal
    dir = Vector3Normalize(dir);

    // Salir desde el centro del personaje (Y=1.0 si position.y=0, pero position ya es el centro?)
    // Asumimos self->position es la base (Y=1.0 según main.c playerPos).
    // Espera, en main.c playerPos es {0, 1, 0}. Si el cilindro mide 2.0 de alto, el centro geométrico es Y=1.0?
    // Raylib DrawCylinder dibuja desde la posición base o centro? 
    // DrawCylinder: "Draw a cylinder/cone... position is the center of the base" NO.
    // Revisando Raylib cheatsheet: "position is center". 
    // Entonces si playerPos es (0,1,0), el centro es (0,1,0). Perfecto.

    Proj_Spawn(pm, 
               self->position, // Salir desde el centro mismo del jugador
               dir, 
               20.0f, 
               0.5f, 
               0.5f, // Radio mas grande (0.5f) para facilitar colision
               50.0f, 
               ORANGE);
}

void Mongo_Draw(Entity* self) {
    // Mongo es un cilindro rojo clásico
    DrawCylinder(self->position, self->radius, self->radius, 2.0f, 16, RED);
    DrawCylinderWires(self->position, self->radius, self->radius, 2.0f, 16, MAROON);
    
    // Ojos (Detalle para saber donde mira es dificil sin rotacion, asi que solo dibujamos cuerpo)
}

// --- Inicializador ---

void Mongo_Init(Entity* ent, void* projectileManager) {
    Entity_Init(ent);
    
    ent->context = projectileManager;
    ent->maxHealth = 500;
    ent->health = 500;
    ent->speed = 10.0f;
    ent->radius = 0.8f; // Un poco más gordito

    // Asignar funciones
    ent->onQ = Mongo_Q;
    ent->onDraw = Mongo_Draw;
    
    // Cooldowns
    ent->maxCdQ = 1.0f; // 1 segundo de CD
}
