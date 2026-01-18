#include "mongo.h"
#include "../projectile.h"
#include "raymath.h"
#include <stdio.h>

// --- Habilidades de Mongo ---

void Mongo_Q(Entity* self, Vector3 targetPos) {
    // Disparar proyectil hacia targetPos
    ProjectileManager* pm = (ProjectileManager*)self->context;
    
    Vector3 dir = Vector3Subtract(targetPos, self->position);
    dir.y = 0; // Mantener plano horizontal

    // Spawnear proyectil: Pos, Dir, Speed=15, Vida=1.0s (aprox 15m), Radio=0.3, Color=Orange
    // Vida = Distancia / Velocidad. Queremos 5 cubos (metros) de distancia?
    // 5 metros / 15 m/s = 0.33 segundos. 
    // Vamos a ponerle 1.0s para que viaje un poco más lejos visualmente.
    
    Proj_Spawn(pm, 
               Vector3Add(self->position, (Vector3){0, 1.0f, 0}), // Salir un poco arriba
               dir, 
               20.0f, 
               0.5f, // 0.5s * 20m/s = 10 metros de rango
               0.3f, 
               ORANGE);
    
    // printf("Mongo lanza Q!\n");
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
