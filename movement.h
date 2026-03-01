/*
 * movement.h — Lógica de movimiento compartida entre servidor y cliente
 *
 * Solo contiene: estructura MovingEntity + funciones de pathfinding/movimiento.
 * La aplicación de input (que depende del array de entidades) se hace en
 * server.c y client.c por separado, porque la memoria está organizada de
 * forma distinta en cada uno.
 */

#ifndef MOVEMENT_H
#define MOVEMENT_H

#include "entity.h"
#include "map.h"
#include "pathfinding.h"
#include "raymath.h"

#include <stddef.h>

#define MAX_PATH_NODES 256

// Entidad que puede moverse con pathfinding
typedef struct {
  Entity entity;
  int netId;

  Vector3 path[MAX_PATH_NODES];
  int pathLength;
  int pathIndex;
} MovingEntity;

static inline void MovingEntity_Init(MovingEntity *me) {
  me->pathLength = 0;
  me->pathIndex = 0;
  me->netId = -1;
}

// Calcula un path al destino. Retorna true si encontró camino.
static inline bool MovingEntity_MoveTo(MovingEntity *me, GameMap *map,
                                       Vector3 dest) {
  me->entity.targetEntity = NULL;
  me->pathLength =
      Path_Find(map, me->entity.position, dest, me->path, MAX_PATH_NODES);
  me->pathIndex = 0;
  return me->pathLength > 0;
}

// Avanza la posición un step de dt según su path.
// No maneja persecución ni ataques — eso lo hace el caller.
static inline void MovingEntity_Update(MovingEntity *me, float dt) {
  Entity *player = &me->entity;

  if (player->isDashing)
    return;

  // Movimiento por nodos del path
  if (me->pathLength > 0 && me->pathIndex < me->pathLength) {
    Vector3 targetNode = me->path[me->pathIndex];
    Vector3 diff = Vector3Subtract(targetNode, player->position);
    diff.y = 0;

    if (Vector3Length(diff) < 0.2f) {
      me->pathIndex++;
    } else {
      Vector3 dir = Vector3Normalize(diff);
      player->position =
          Vector3Add(player->position, Vector3Scale(dir, player->speed * dt));
    }
  }
}

#endif // MOVEMENT_H
