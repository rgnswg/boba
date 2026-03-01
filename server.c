/*
 * server.c — Servidor Headless de Boba MOBA (ENet/UDP)
 *
 * Servidor autoritativo con ENet:
 * - Canal 0 (unreliable): recibe inputs, envía snapshots
 * - Canal 1 (reliable): envía ConnectAck, game events
 */

#include "characters/dummy.h"
#include "characters/mongo.h"
#include "entity.h"
#include "map.h"
#include "movement.h"
#include "net.h"
#include "projectile.h"
#include "raylib.h"
#include "raymath.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SERVER_TICK_HZ 30 // 30Hz tick rate (estándar MOBA)
#define SERVER_TICK_NS (1000000000 / SERVER_TICK_HZ)

// ----- Estado del juego -----

#define MAX_SERVER_ENTITIES 32

static MovingEntity g_entities[MAX_SERVER_ENTITIES];
static int g_entityCount = 0;
static ProjectileManager g_projMgr;
static GameMap g_map;

static int register_entity(Entity *ent) {
  if (g_entityCount >= MAX_SERVER_ENTITIES)
    return -1;
  int idx = g_entityCount++;
  g_entities[idx].entity = *ent;
  MovingEntity_Init(&g_entities[idx]);
  g_entities[idx].netId = idx;
  return idx;
}

static void build_snapshot(StateSnapshot *snap) {
  snap->entityCount = g_entityCount;
  for (int i = 0; i < g_entityCount; i++) {
    Entity *e = &g_entities[i].entity;
    NetEntity *ne = &snap->entities[i];
    ne->netId = g_entities[i].netId;
    ne->x = e->position.x;
    ne->y = e->position.y;
    ne->z = e->position.z;
    ne->health = e->health;
    ne->maxHealth = e->maxHealth;
    ne->radius = e->radius;
    ne->team = (int32_t)e->team;
    ne->active = e->active ? 1 : 0;
  }
  int projCount = 0;
  for (int i = 0; i < MAX_PROJECTILES; i++) {
    Projectile *p = &g_projMgr.pool[i];
    if (!p->active)
      continue;
    NetProjectile *np = &snap->projectiles[projCount];
    np->x = p->position.x;
    np->y = p->position.y;
    np->z = p->position.z;
    // Velocidad = dirección * speed (para extrapolación client-side)
    np->vx = p->direction.x * p->speed;
    np->vy = p->direction.y * p->speed;
    np->vz = p->direction.z * p->speed;
    np->radius = p->radius;
    np->colorR = p->color.r;
    np->colorG = p->color.g;
    np->colorB = p->color.b;
    np->active = 1;
    if (++projCount >= NET_MAX_PROJECTILES)
      break;
  }
  snap->projectileCount = projCount;
}

// ----- Aplicar input -----

static void apply_input(int playerNetId, const InputPacket *inp) {
  if (playerNetId < 0 || playerNetId >= g_entityCount)
    return;
  MovingEntity *me = &g_entities[playerNetId];
  Entity *player = &me->entity;

  if (inp->rightClick) {
    if (inp->attackTarget >= 0 && inp->attackTarget < g_entityCount) {
      player->targetEntity = &g_entities[inp->attackTarget].entity;
      me->pathLength = 0;
      me->pathIndex = 0;
    } else {
      Vector3 dest = {inp->targetX, 0.0f, inp->targetZ};
      MovingEntity_MoveTo(me, &g_map, dest);
    }
  }
  if (inp->keyQ && player->onQ && player->cdQ <= 0) {
    Vector3 aimPos = {inp->aimX, 0.0f, inp->aimZ};
    if (inp->aimTargetId >= 0 && inp->aimTargetId < g_entityCount)
      aimPos = g_entities[inp->aimTargetId].entity.position;
    player->onQ(player, aimPos);
    player->cdQ = player->maxCdQ;
  }
  if (inp->keyW && player->onW && player->cdW <= 0) {
    if (inp->aimTargetId >= 0 && inp->aimTargetId < g_entityCount) {
      Entity *ally = &g_entities[inp->aimTargetId].entity;
      if (ally->team == player->team) {
        player->onW(player, ally->position);
        player->cdW = player->maxCdW;
        me->pathLength = 0;
        me->pathIndex = 0;
      }
    }
  }
}

// ----- Movimiento + persecución -----

static void update_movement(int playerNetId, float dt) {
  if (playerNetId < 0 || playerNetId >= g_entityCount)
    return;
  MovingEntity *me = &g_entities[playerNetId];
  Entity *player = &me->entity;
  if (player->isDashing)
    return;

  if (player->targetEntity) {
    if (!player->targetEntity->active) {
      player->targetEntity = NULL;
    } else {
      Vector3 diff =
          Vector3Subtract(player->targetEntity->position, player->position);
      diff.y = 0;
      float dist = Vector3Length(diff);
      if (dist <= player->attackRange) {
        me->pathLength = 0;
        if (player->attackTimer <= 0 && player->onAttack) {
          player->onAttack(player, player->targetEntity->position);
          player->attackTimer = player->attackCooldown;
        }
      } else {
        me->pathLength =
            Path_Find(&g_map, player->position, player->targetEntity->position,
                      me->path, MAX_PATH_NODES);
        me->pathIndex = 0;
      }
    }
  }
  MovingEntity_Update(me, dt);
}

// ----- Timer -----

static long long get_ns(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

static void sleep_ns(long long ns) {
  if (ns <= 0)
    return;
  struct timespec ts;
  ts.tv_sec = ns / 1000000000LL;
  ts.tv_nsec = ns % 1000000000LL;
  nanosleep(&ts, NULL);
}

// ----- Main -----

int main(void) {
  printf("[SERVER] Iniciando Boba Server (ENet/UDP)...\n");

  if (enet_initialize() != 0) {
    fprintf(stderr, "[SERVER] Error inicializando ENet.\n");
    return 1;
  }

  // Inicializar sistemas de juego
  Map_InitHeadless(&g_map);
  Proj_Init(&g_projMgr);

  Entity playerEnt;
  Mongo_Init(&playerEnt, &g_projMgr);
  playerEnt.position = (Vector3){0.0f, 1.0f, 0.0f};
  playerEnt.team = TEAM_BLUE;
  int playerNetId = register_entity(&playerEnt);

  Entity dummyEnt;
  Dummy_Init(&dummyEnt);
  dummyEnt.position = (Vector3){10.0f, 1.0f, 5.0f};
  register_entity(&dummyEnt);

  Entity allyDummyEnt;
  Dummy_Init(&allyDummyEnt);
  allyDummyEnt.position = (Vector3){-5.0f, 1.0f, 0.0f};
  allyDummyEnt.team = TEAM_BLUE;
  register_entity(&allyDummyEnt);

  Entity *projTargets[MAX_SERVER_ENTITIES];
  int projTargetCount = 0;
  for (int i = 0; i < g_entityCount; i++) {
    if (i != playerNetId)
      projTargets[projTargetCount++] = &g_entities[i].entity;
  }

  // Crear host ENet (servidor)
  ENetAddress address;
  address.host = ENET_HOST_ANY;
  address.port = SERVER_PORT;

  ENetHost *server = enet_host_create(&address, 1, NUM_CHANNELS, 0, 0);
  if (!server) {
    fprintf(stderr, "[SERVER] Error creando host ENet.\n");
    return 1;
  }

  printf("[SERVER] Escuchando en puerto %d (UDP/ENet) — Esperando cliente...\n",
         SERVER_PORT);

  // Esperar conexión del cliente
  ENetPeer *clientPeer = NULL;
  while (!clientPeer) {
    ENetEvent event;
    if (enet_host_service(server, &event, 100) > 0) {
      if (event.type == ENET_EVENT_TYPE_CONNECT) {
        clientPeer = event.peer;
        printf("[SERVER] Cliente conectado!\n");

        // Enviar ACK (reliable)
        ConnectAck ack = {.playerNetId = playerNetId};
        send_reliable(clientPeer, PKT_CONNECT_ACK, &ack, sizeof(ack));
        enet_host_flush(server);
      }
    }
  }

  // Game Loop
  bool running = true;
  long long prevTime = get_ns();

  while (running) {
    long long frameStart = get_ns();
    float dt = (float)(frameStart - prevTime) / 1e9f;
    if (dt > 0.1f)
      dt = 0.1f;
    prevTime = frameStart;

    // 1. Procesar eventos ENet (drenar todos los inputs pendientes)
    {
      InputPacket lastAction = {0};
      lastAction.attackTarget = -1;
      lastAction.aimTargetId = -1;
      bool gotAction = false;

      ENetEvent event;
      while (enet_host_service(server, &event, 0) > 0) {
        switch (event.type) {
        case ENET_EVENT_TYPE_RECEIVE: {
          PacketHeader hdr;
          InputPacket inp;
          int pktType =
              parse_packet(event.packet, &hdr, &inp, sizeof(InputPacket));
          if (pktType == PKT_INPUT && hdr.length == (int)sizeof(InputPacket)) {
            if (inp.rightClick || inp.keyQ || inp.keyW) {
              lastAction = inp;
              gotAction = true;
            }
          }
          enet_packet_destroy(event.packet);
          break;
        }
        case ENET_EVENT_TYPE_DISCONNECT:
          printf("[SERVER] Cliente desconectado.\n");
          running = false;
          break;
        default:
          break;
        }
      }

      if (gotAction) {
        apply_input(playerNetId, &lastAction);
      }
    }

    // 2. Actualizar movimiento
    update_movement(playerNetId, dt);

    // 3. Actualizar entidades
    for (int i = 0; i < g_entityCount; i++) {
      Entity_Update(&g_entities[i].entity, dt);
    }

    // 4. Actualizar proyectiles
    Proj_Update(&g_projMgr, dt, projTargets, projTargetCount);

    // 5. Enviar snapshot (unreliable — si se pierde, el siguiente reemplaza)
    if (clientPeer) {
      StateSnapshot snap;
      build_snapshot(&snap);
      send_unreliable(clientPeer, PKT_STATE_SNAPSHOT, &snap, sizeof(snap));
    }

    // 6. Sleep
    long long frameEnd = get_ns();
    sleep_ns(SERVER_TICK_NS - (frameEnd - frameStart));
  }

  enet_host_destroy(server);
  enet_deinitialize();
  Map_Unload(&g_map);
  printf("[SERVER] Apagado limpio.\n");
  return 0;
}
