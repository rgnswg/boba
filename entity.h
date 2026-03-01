#ifndef ENTITY_H
#define ENTITY_H

#include "raylib.h"

// Forward declaration para evitar dependencias circulares si crecemos
typedef struct Entity Entity;

// Punteros a función para definir comportamiento específico
typedef void (*AbilityFunc)(Entity *self, Vector3 targetPos);
typedef void (*UpdateFunc)(Entity *self, float dt);
typedef void (*DrawFunc)(Entity *self);

typedef enum {
  TEAM_NEUTRAL = 0, // Atacable por todos
  TEAM_BLUE,
  TEAM_RED
} Team;

struct Entity {
  // Identificador de red (asignado por el servidor; -1 si no aplica)
  int netId;

  // Estado Físico
  Vector3 position;
  float radius;
  float speed;
  Team team; // Equipo
  bool active;

  // Stats
  float health;
  float maxHealth;
  float mana;

  // Stats de Ataque Básico
  float attackRange;
  float attackDamage;
  float attackCooldown;        // Tiempo entre ataques (ej: 1.0s)
  float attackTimer;           // Contador para el siguiente ataque
  struct Entity *targetEntity; // A quién estamos atacando (o persiguiendo)

  // Estado de Movimiento Especial (Dash/Leap)
  bool isDashing;
  float dashTimer;
  Vector3 dashVelocity;

  // Sistema de Habilidades (Q, W, E, R)
  AbilityFunc onAttack; // Callback para ejecutar el ataque básico (disparar
                        // proyectil o melé)
  AbilityFunc onQ;
  AbilityFunc onW;
  AbilityFunc onE;
  AbilityFunc onR;

  // Callbacks genéricos
  UpdateFunc onUpdate;
  DrawFunc onDraw;
  void (*onDeath)(Entity *self); // Nuevo: Callback cuando la vida llega a 0

  // Datos internos (Cooldowns, etc)
  float cdQ, cdW, cdE, cdR;
  float maxCdQ, maxCdW, maxCdE, maxCdR;

  // Referencias a sistemas externos
  void *context;
};

// Inicializa una entidad vacía por defecto
void Entity_Init(Entity *ent);

// Actualiza cooldowns
void Entity_Update(Entity *ent, float dt);

// Aplica daño a la entidad
void Entity_TakeDamage(Entity *ent, float amount);

#endif
