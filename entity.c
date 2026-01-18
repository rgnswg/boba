#include "entity.h"

void Entity_Init(Entity* ent) {
    ent->position = (Vector3){0,0,0};
    ent->radius = 0.5f;
    ent->speed = 10.0f;
    ent->active = true;
    ent->health = 100;
    ent->maxHealth = 100;
    ent->mana = 100;

    ent->onQ = 0; 
    ent->onW = 0;
    ent->onE = 0;
    ent->onR = 0;
    ent->onUpdate = 0;
    ent->onDraw = 0;
    ent->onDeath = 0; // NULL por defecto

    ent->cdQ = 0; ent->cdW = 0; ent->cdE = 0; ent->cdR = 0;
}

void Entity_Update(Entity* ent, float dt) {
    if (!ent->active) return;

    // Reducir Cooldowns
    if (ent->cdQ > 0) ent->cdQ -= dt;
    if (ent->cdW > 0) ent->cdW -= dt;
    if (ent->cdE > 0) ent->cdE -= dt;
    if (ent->cdR > 0) ent->cdR -= dt;

    if (ent->onUpdate) ent->onUpdate(ent, dt);
}

void Entity_TakeDamage(Entity* ent, float amount) {
    if (!ent->active) return;

    ent->health -= amount;
    
    // Lógica de Muerte
    if (ent->health <= 0) {
        ent->health = 0;
        
        // Si la entidad tiene comportamiento especial al morir (ej: Dummy resucita)
        if (ent->onDeath) {
            ent->onDeath(ent);
        } else {
            // Comportamiento default: Desaparecer
            ent->active = false;
        }
    }
}