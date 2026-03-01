/*
 * server.c — Servidor Headless de Boba MOBA (ENet/UDP)
 *
 * Servidor autoritativo con ENet — Hasta 10 jugadores simultáneos.
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

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SERVER_TICK_HZ 60 // 60Hz tick rate (bajo latency para MOBA)
#define SERVER_TICK_NS (1000000000 / SERVER_TICK_HZ)
#define RESPAWN_TIME 5.0f // segundos de muerte antes de respawnear

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

// ----- Slots de Jugadores -----

#define MAX_PLAYERS 10

typedef struct {
  ENetPeer *peer; // NULL = slot libre
  int netId;      // índice en g_entities[]
} PlayerSlot;

static PlayerSlot g_players[MAX_PLAYERS];
static int g_playerCount = 0;

// Buscar slot por peer
static int find_slot_by_peer(ENetPeer *peer) {
  for (int i = 0; i < MAX_PLAYERS; i++) {
    if (g_players[i].peer == peer)
      return i;
  }
  return -1;
}

// Buscar un slot libre
static int find_free_slot(void) {
  for (int i = 0; i < MAX_PLAYERS; i++) {
    if (g_players[i].peer == NULL)
      return i;
  }
  return -1;
}

// ----- Snapshot -----

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
    ne->isDead = e->isDead ? 1 : 0;
    ne->respawnTimer = e->respawnTimer;
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

  // No aceptar inputs de jugadores muertos
  if (player->isDead)
    return;

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
  if (player->isDashing || player->isDead)
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
  printf("[SERVER] Iniciando Boba Server (ENet/UDP) — Hasta %d jugadores\n",
         MAX_PLAYERS);

  if (enet_initialize() != 0) {
    fprintf(stderr, "[SERVER] Error inicializando ENet.\n");
    return 1;
  }

  // Inicializar sistemas de juego
  Map_InitHeadless(&g_map);
  Proj_Init(&g_projMgr);

  // Inicializar slots de jugadores
  for (int i = 0; i < MAX_PLAYERS; i++) {
    g_players[i].peer = NULL;
    g_players[i].netId = -1;
  }

  // Entidades NPC de práctica (dummies)
  Entity dummyEnt;
  Dummy_Init(&dummyEnt);
  dummyEnt.position = (Vector3){10.0f, 1.0f, 5.0f};
  register_entity(&dummyEnt);

  Entity allyDummyEnt;
  Dummy_Init(&allyDummyEnt);
  allyDummyEnt.position = (Vector3){-5.0f, 1.0f, 0.0f};
  allyDummyEnt.team = TEAM_BLUE;
  register_entity(&allyDummyEnt);

  // Crear host ENet (servidor — acepta hasta MAX_PLAYERS conexiones)
  ENetAddress address;
  address.host = ENET_HOST_ANY;
  address.port = SERVER_PORT;

  ENetHost *server =
      enet_host_create(&address, MAX_PLAYERS, NUM_CHANNELS, 0, 0);
  if (!server) {
    fprintf(stderr, "[SERVER] Error creando host ENet.\n");
    return 1;
  }

  printf(
      "[SERVER] Escuchando en puerto %d (UDP/ENet) — Esperando jugadores...\n",
      SERVER_PORT);

  // Game Loop — no bloqueamos esperando conexiones, las manejamos dentro
  bool running = true;
  long long prevTime = get_ns();

  while (running) {
    long long frameStart = get_ns();
    float dt = (float)(frameStart - prevTime) / 1e9f;
    if (dt > 0.1f)
      dt = 0.1f;
    prevTime = frameStart;

    // 1. Procesar eventos ENet
    {
      // Buffer temporal para acumular el último input de cada jugador este tick
      InputPacket lastInputs[MAX_PLAYERS];
      bool gotInput[MAX_PLAYERS];
      for (int i = 0; i < MAX_PLAYERS; i++) {
        memset(&lastInputs[i], 0, sizeof(InputPacket));
        lastInputs[i].attackTarget = -1;
        lastInputs[i].aimTargetId = -1;
        gotInput[i] = false;
      }

      ENetEvent event;
      while (enet_host_service(server, &event, 0) > 0) {
        switch (event.type) {

        case ENET_EVENT_TYPE_CONNECT: {
          // Nuevo jugador conectado
          int slotIdx = find_free_slot();
          if (slotIdx < 0) {
            printf("[SERVER] Servidor lleno — rechazando conexión.\n");
            enet_peer_disconnect(event.peer, 0);
            break;
          }

          // Crear entidad Mongo para el nuevo jugador
          Entity playerEnt;
          Mongo_Init(&playerEnt, &g_projMgr);

          // Spawn en círculo para no superponer
          float angle = (float)slotIdx * (2.0f * PI / (float)MAX_PLAYERS);
          float spawnRadius = 5.0f;
          playerEnt.position = (Vector3){cosf(angle) * spawnRadius, 1.0f,
                                         sinf(angle) * spawnRadius};

          // Alternar equipos: slots pares → BLUE, impares → RED
          playerEnt.team = (slotIdx % 2 == 0) ? TEAM_BLUE : TEAM_RED;

          int netId = register_entity(&playerEnt);
          if (netId < 0) {
            printf("[SERVER] Sin espacio para más entidades.\n");
            enet_peer_disconnect(event.peer, 0);
            break;
          }

          g_players[slotIdx].peer = event.peer;
          g_players[slotIdx].netId = netId;
          g_playerCount++;

          // Guardar slotIdx en el peer data para lookup rápido
          event.peer->data = (void *)(intptr_t)slotIdx;

          // Enviar ACK (reliable)
          ConnectAck ack = {.playerNetId = netId};
          send_reliable(event.peer, PKT_CONNECT_ACK, &ack, sizeof(ack));
          enet_host_flush(server);

          printf("[SERVER] Jugador conectado → slot=%d netId=%d equipo=%s "
                 "(%d/%d jugadores)\n",
                 slotIdx, netId, playerEnt.team == TEAM_BLUE ? "BLUE" : "RED",
                 g_playerCount, MAX_PLAYERS);
          break;
        }

        case ENET_EVENT_TYPE_RECEIVE: {
          int slotIdx = (int)(intptr_t)event.peer->data;
          if (slotIdx < 0 || slotIdx >= MAX_PLAYERS ||
              g_players[slotIdx].peer != event.peer) {
            enet_packet_destroy(event.packet);
            break;
          }

          PacketHeader hdr;
          InputPacket inp;
          int pktType =
              parse_packet(event.packet, &hdr, &inp, sizeof(InputPacket));
          if (pktType == PKT_INPUT && hdr.length == (int)sizeof(InputPacket)) {
            if (inp.rightClick || inp.keyQ || inp.keyW) {
              lastInputs[slotIdx] = inp;
              gotInput[slotIdx] = true;
            }
          }
          enet_packet_destroy(event.packet);
          break;
        }

        case ENET_EVENT_TYPE_DISCONNECT: {
          int slotIdx = (int)(intptr_t)event.peer->data;
          if (slotIdx >= 0 && slotIdx < MAX_PLAYERS &&
              g_players[slotIdx].peer == event.peer) {
            int netId = g_players[slotIdx].netId;
            if (netId >= 0 && netId < g_entityCount) {
              g_entities[netId].entity.active = false;
            }
            printf("[SERVER] Jugador desconectado — slot=%d netId=%d "
                   "(%d/%d jugadores)\n",
                   slotIdx, netId, g_playerCount - 1, MAX_PLAYERS);
            g_players[slotIdx].peer = NULL;
            g_players[slotIdx].netId = -1;
            g_playerCount--;
          }
          break;
        }

        default:
          break;
        }
      }

      // Aplicar inputs acumulados de cada jugador
      for (int i = 0; i < MAX_PLAYERS; i++) {
        if (gotInput[i]) {
          apply_input(g_players[i].netId, &lastInputs[i]);
        }
      }
    }

    // 2. Actualizar movimiento de todos los jugadores conectados
    for (int i = 0; i < MAX_PLAYERS; i++) {
      if (g_players[i].peer != NULL && g_players[i].netId >= 0) {
        update_movement(g_players[i].netId, dt);
      }
    }

    // 2b. Respawn timer — contar y respawnear jugadores muertos
    for (int i = 0; i < MAX_PLAYERS; i++) {
      if (g_players[i].peer == NULL || g_players[i].netId < 0)
        continue;
      Entity *ent = &g_entities[g_players[i].netId].entity;
      MovingEntity *me = &g_entities[g_players[i].netId];
      if (!ent->isDead)
        continue;

      ent->respawnTimer -= dt;
      if (ent->respawnTimer <= 0.0f) {
        // Respawnear
        ent->isDead = false;
        ent->respawnTimer = 0.0f;
        ent->health = ent->maxHealth;
        ent->targetEntity = NULL;
        me->pathLength = 0;
        me->pathIndex = 0;

        // Posición de spawn por equipo
        if (ent->team == TEAM_BLUE) {
          ent->position = (Vector3){-8.0f, 1.0f, -8.0f};
        } else {
          ent->position = (Vector3){8.0f, 1.0f, 8.0f};
        }
        printf("[SERVER] Jugador netId=%d respawneado\n", g_players[i].netId);
      }
    }

    // 3. Actualizar entidades (cooldowns, dashes, etc.)
    for (int i = 0; i < g_entityCount; i++) {
      Entity_Update(&g_entities[i].entity, dt);
    }

    // 4. Actualizar proyectiles — construir lista de targets (todas las
    // entidades activas)
    {
      Entity *projTargets[MAX_SERVER_ENTITIES];
      int projTargetCount = 0;
      for (int i = 0; i < g_entityCount; i++) {
        if (g_entities[i].entity.active) {
          projTargets[projTargetCount++] = &g_entities[i].entity;
        }
      }
      Proj_Update(&g_projMgr, dt, projTargets, projTargetCount);
    }

    // 5. Enviar snapshot a todos los peers conectados (unreliable)
    if (g_playerCount > 0) {
      StateSnapshot snap;
      build_snapshot(&snap);
      for (int i = 0; i < MAX_PLAYERS; i++) {
        if (g_players[i].peer != NULL) {
          send_unreliable(g_players[i].peer, PKT_STATE_SNAPSHOT, &snap,
                          sizeof(snap));
        }
      }
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
