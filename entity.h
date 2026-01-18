#ifndef ENTITY_H
#define ENTITY_H

#include "raylib.h"

// Forward declaration para evitar dependencias circulares si crecemos
typedef struct Entity Entity;

// Punteros a función para definir comportamiento específico
typedef void (*AbilityFunc)(Entity* self, Vector3 targetPos);
typedef void (*UpdateFunc)(Entity* self, float dt);
typedef void (*DrawFunc)(Entity* self);

struct Entity {
    // Estado Físico
    Vector3 position;
    float radius;
    float speed;
    bool active;

    // Stats
    float health;
    float maxHealth;
    float mana;
    
    // Sistema de Habilidades (Q, W, E, R)
    AbilityFunc onQ;
    AbilityFunc onW;
    AbilityFunc onE;
    AbilityFunc onR;
    
    // Callbacks genéricos
    UpdateFunc onUpdate; 
    DrawFunc onDraw;
    void (*onDeath)(Entity* self); // Nuevo: Callback cuando la vida llega a 0

    // Datos internos (Cooldowns, etc)
    float cdQ, cdW, cdE, cdR;
    float maxCdQ, maxCdW, maxCdE, maxCdR;

    // Referencias a sistemas externos
    void* context; 
};

// Inicializa una entidad vacía por defecto
void Entity_Init(Entity* ent);

// Actualiza cooldowns
void Entity_Update(Entity* ent, float dt);

// Aplica daño a la entidad
void Entity_TakeDamage(Entity* ent, float amount);

#endif
