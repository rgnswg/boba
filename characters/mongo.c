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
               self->team, // Equipo del dueño
               ORANGE);
}

void Mongo_W(Entity* self, Vector3 targetPos) {
    // Dash hacia aliado
    // Velocidad: 40.0f
    // Rango Max: 12.0f
    
    float dashSpeed = 40.0f;
    float maxRange = 12.0f;

    Vector3 diff = Vector3Subtract(targetPos, self->position);
    diff.y = 0; // Dash horizontal
    float dist = Vector3Length(diff);

    if (dist > maxRange) {
        // Fuera de rango (podriamos acercarnos hasta el rango maximo, pero por ahora cancelamos)
        // O mejor: Dasheamos hasta el maximo rango en esa direccion
        dist = maxRange;
        diff = Vector3Normalize(diff);
        diff = Vector3Scale(diff, maxRange);
    }

    if (dist < 0.1f) return; // Ya estamos ahi

    // Iniciar Dash
    self->isDashing = true;
    self->dashVelocity = Vector3Scale(Vector3Normalize(diff), dashSpeed);
    self->dashTimer = dist / dashSpeed;
}

void Mongo_OnAttack(Entity* self, Vector3 targetPos) {
    // Ataque Básico: Proyectil Amarillo pequeño
    ProjectileManager* pm = (ProjectileManager*)self->context;
    
    Vector3 dir = Vector3Subtract(targetPos, self->position);
    dir.y = 0; 
    
    Proj_Spawn(pm, 
               self->position, 
               dir, 
               25.0f, // Un poco mas rapido que la Q
               0.6f,  // Vida corta
               0.3f,  // Mas pequeño
               self->attackDamage, 
               self->team, 
               YELLOW);
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

    // Stats de Ataque
    ent->attackRange = 8.0f;
    ent->attackDamage = 20.0f;
    ent->attackCooldown = 0.8f;
    ent->onAttack = Mongo_OnAttack;

    // Asignar funciones
    ent->onQ = Mongo_Q;
    ent->onW = Mongo_W;
    ent->onDraw = Mongo_Draw;
    
    // Cooldowns
    ent->maxCdQ = 1.0f; // 1 segundo de CD
    ent->maxCdW = 5.0f; // 5 segundos de CD para el Dash
}
