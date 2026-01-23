#include "entity.h"
#include "raymath.h"

void Entity_Init(Entity* ent) {
    ent->position = (Vector3){0,0,0};
    ent->radius = 0.5f;
    ent->speed = 10.0f;
    ent->active = true;
    ent->team = TEAM_NEUTRAL; // Por defecto
    ent->health = 100;
    ent->maxHealth = 100;
    ent->mana = 100;

    ent->attackRange = 5.0f;
    ent->attackDamage = 10.0f;
    ent->attackCooldown = 1.0f;
    ent->attackTimer = 0.0f;
    ent->targetEntity = 0; // NULL
    ent->onAttack = 0;

    ent->isDashing = false;
    ent->dashTimer = 0.0f;
    ent->dashVelocity = (Vector3){0,0,0};

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

    // Timer de ataque
    if (ent->attackTimer > 0) ent->attackTimer -= dt;

    // Lógica de Dash (Movimiento forzado que ignora todo lo demás)
    if (ent->isDashing) {
        ent->position = Vector3Add(ent->position, Vector3Scale(ent->dashVelocity, dt));
        ent->dashTimer -= dt;
        if (ent->dashTimer <= 0) {
            ent->isDashing = false;
            ent->dashTimer = 0;
        }
        return; // Si estamos dasheando, no hacemos nada más (cooldowns siguen bajando?)
        // Decisión de diseño: Cooldowns bajan durante dash? Digamos que sí.
    }

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