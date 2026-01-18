#include "entity.h"

void Entity_Init(Entity* ent) {
    ent->position = (Vector3){0,0,0};
    ent->radius = 0.5f;
    ent->speed = 10.0f;
    ent->active = true;
    ent->health = 100;
    ent->maxHealth = 100;
    ent->mana = 100;

    ent->onQ = 0; // NULL
    ent->onW = 0;
    ent->onE = 0;
    ent->onR = 0;
    ent->onUpdate = 0;
    ent->onDraw = 0;

    ent->cdQ = 0; ent->cdW = 0; ent->cdE = 0; ent->cdR = 0;
}

void Entity_Update(Entity* ent, float dt) {
    // Reducir Cooldowns
    if (ent->cdQ > 0) ent->cdQ -= dt;
    if (ent->cdW > 0) ent->cdW -= dt;
    if (ent->cdE > 0) ent->cdE -= dt;
    if (ent->cdR > 0) ent->cdR -= dt;

    // Ejecutar update específico del personaje si existe
    if (ent->onUpdate) ent->onUpdate(ent, dt);
}
