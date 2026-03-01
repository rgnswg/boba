/*
 * client.c — Cliente Gráfico de Boba MOBA (ENet/UDP)
 *
 * Predicción local del movimiento + servidor autoritativo.
 * Inputs se envían throttled a 60Hz via ENet (unreliable).
 * Snapshots se reciben unreliable — si se pierde uno, el siguiente reemplaza.
 *
 * INTERPOLACIÓN: Los jugadores remotos se renderizan interpolando entre
 * los 2 últimos snapshots recibidos, dando movimiento suave a cualquier FPS.
 */

#include "map.h"
#include "movement.h"
#include "net.h"
#include "pathfinding.h"
#include "raylib.h"
#include "raymath.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---- Estado ----

static int g_playerNetId = -1;

// Predicción local
static MovingEntity g_localPlayer;
static GameMap g_localMap;
static bool g_localReady = false;
static bool g_initialSynced = false; // ¿ya sincronizamos posición inicial?

// Estado de muerte (leído del snapshot)
static bool g_localDead = false;
static float g_localRespawnTimer = 0.0f;

// Debug HUD
static bool g_debugHud = false;

// ---- Snapshot Buffer para Interpolación ----

// Almacenamos los 2 últimos snapshots con sus timestamps de recepción.
// Los jugadores remotos se renderizan interpolando entre prev y curr.
static StateSnapshot g_snapPrev;
static StateSnapshot g_snapCurr;
static double g_snapPrevTime = 0.0; // GetTime() al recibir snapPrev
static double g_snapCurrTime = 0.0; // GetTime() al recibir snapCurr
static int g_snapCount = 0; // cuántos snapshots hemos recibido (0, 1, 2+)
static double g_interpDelay =
    1.0 / 60.0; // delay de interpolación (auto-ajustado)

// ---- Interpolación ----

// Busca una entidad por netId en un snapshot. Retorna NULL si no la encuentra.
static const NetEntity *find_net_entity(const StateSnapshot *snap, int netId) {
  for (int i = 0; i < snap->entityCount; i++) {
    if (snap->entities[i].netId == netId)
      return &snap->entities[i];
  }
  return NULL;
}

// Calcula la posición interpolada de una entidad remota.
// renderTime = GetTime() actual.
// Si solo tenemos 1 snapshot, devolvemos posición directa (sin interp).
// renderTime debe ser now - g_interpDelay para que t quede en [0, 1]
static Vector3 interp_entity_pos(int netId, double renderTime) {
  const NetEntity *curr = find_net_entity(&g_snapCurr, netId);
  if (!curr)
    return (Vector3){0, 0, 0};

  Vector3 currPos = {curr->x, curr->y, curr->z};

  if (g_snapCount < 2)
    return currPos; // Solo 1 snapshot, no podemos interpolar

  const NetEntity *prev = find_net_entity(&g_snapPrev, netId);
  if (!prev)
    return currPos; // Entidad nueva, no existe en snap anterior

  Vector3 prevPos = {prev->x, prev->y, prev->z};

  // Calcular t: fracción del intervalo entre snapshots
  double interval = g_snapCurrTime - g_snapPrevTime;
  if (interval <= 0.0001)
    return currPos; // Evitar división por 0

  float t = (float)((renderTime - g_snapPrevTime) / interval);

  // Clampear a [0, 1] — interpolación pura, sin extrapolación
  if (t < 0.0f)
    t = 0.0f;
  if (t > 1.0f)
    t = 1.0f;

  return Vector3Lerp(prevPos, currPos, t);
}

// ---- Colores ----

static Color team_color(int team, bool isPlayer) {
  if (isPlayer)
    return BLUE;
  switch (team) {
  case 1:
    return SKYBLUE;
  case 2:
    return ORANGE;
  default:
    return GRAY;
  }
}

// ---- Render ----

static void draw_entities(const StateSnapshot *snap, int playerNetId,
                          Camera3D camera, double renderTime) {
  for (int i = 0; i < snap->entityCount; i++) {
    const NetEntity *e = &snap->entities[i];
    if (!e->active)
      continue;

    bool isPlayer = (e->netId == playerNetId);
    Vector3 pos;
    if (isPlayer && g_localReady) {
      // Jugador local: predicción instantánea
      pos = g_localPlayer.entity.position;
    } else {
      // Jugador remoto / NPC: interpolación suave
      pos = interp_entity_pos(e->netId, renderTime);
    }

    Color bodyColor = team_color(e->team, isPlayer);
    Color wireColor = isPlayer ? DARKBLUE : (e->team == 2 ? MAROON : DARKGRAY);

    DrawCylinder(pos, e->radius, e->radius, 2.0f, 16, bodyColor);
    DrawCylinderWires(pos, e->radius, e->radius, 2.0f, 16, wireColor);

    Vector3 headPos = {pos.x, pos.y + 2.5f, pos.z};
    Vector2 screenPos = GetWorldToScreen(headPos, camera);
    float hpRatio = (e->maxHealth > 0) ? (e->health / e->maxHealth) : 0.0f;
    int barW = 60, barH = 8;
    int barX = (int)screenPos.x - barW / 2;
    int barY = (int)screenPos.y - 5;

    DrawRectangle(barX, barY, barW, barH, DARKGRAY);
    DrawRectangle(barX, barY, (int)(barW * hpRatio), barH,
                  hpRatio > 0.5f ? GREEN : (hpRatio > 0.25f ? YELLOW : RED));
    DrawRectangleLines(barX, barY, barW, barH, BLACK);
    if (isPlayer)
      DrawText("YOU", barX, barY - 16, 12, WHITE);
  }

  // Proyectiles: extrapolación por velocidad (ya funcionan bien)
  float timeSinceSnap = (float)(renderTime - g_snapCurrTime);
  if (timeSinceSnap < 0.0f)
    timeSinceSnap = 0.0f;

  for (int i = 0; i < snap->projectileCount; i++) {
    const NetProjectile *p = &snap->projectiles[i];
    if (!p->active)
      continue;
    Vector3 pos = {p->x + p->vx * timeSinceSnap, p->y + p->vy * timeSinceSnap,
                   p->z + p->vz * timeSinceSnap};
    DrawSphere(pos, p->radius,
               (Color){(unsigned char)p->colorR, (unsigned char)p->colorG,
                       (unsigned char)p->colorB, 255});
  }
}

// ---- Picking ----

static int pick_entity(const StateSnapshot *snap, Ray mouseRay,
                       int excludeNetId) {
  int bestId = -1;
  float bestDist = 1e9f;
  for (int i = 0; i < snap->entityCount; i++) {
    const NetEntity *e = &snap->entities[i];
    if (!e->active || e->netId == excludeNetId)
      continue;
    Vector3 min = {e->x - e->radius, e->y, e->z - e->radius};
    Vector3 max = {e->x + e->radius, e->y + 2.5f, e->z + e->radius};
    RayCollision hit = GetRayCollisionBox(mouseRay, (BoundingBox){min, max});
    if (hit.hit && hit.distance < bestDist) {
      bestDist = hit.distance;
      bestId = e->netId;
    }
  }
  return bestId;
}

static bool ray_hit_ground(Ray ray, float *outX, float *outZ) {
  if (ray.direction.y == 0)
    return false;
  float t = -ray.position.y / ray.direction.y;
  if (t < 0)
    return false;
  *outX = ray.position.x + ray.direction.x * t;
  *outZ = ray.position.z + ray.direction.z * t;
  return true;
}

// ---- Main ----

int main(void) {
  printf("[CLIENT] Iniciando (ENet/UDP)...\n");

  if (enet_initialize() != 0) {
    fprintf(stderr, "[CLIENT] Error inicializando ENet.\n");
    return 1;
  }

  // Crear host ENet (cliente, sin escuchar)
  ENetHost *client = enet_host_create(NULL, 1, NUM_CHANNELS, 0, 0);
  if (!client) {
    fprintf(stderr, "[CLIENT] Error creando host ENet.\n");
    return 1;
  }

  // Conectar al servidor
  ENetAddress serverAddr;
  enet_address_set_host(&serverAddr, SERVER_HOST);
  serverAddr.port = SERVER_PORT;

  ENetPeer *serverPeer =
      enet_host_connect(client, &serverAddr, NUM_CHANNELS, 0);
  if (!serverPeer) {
    fprintf(stderr, "[CLIENT] Error conectando.\n");
    return 1;
  }

  printf("[CLIENT] Conectando a %s:%d...\n", SERVER_HOST, SERVER_PORT);

  // Esperar conexión + ACK
  bool connected = false;
  for (int attempts = 0; attempts < 50 && !connected; attempts++) {
    ENetEvent event;
    if (enet_host_service(client, &event, 100) > 0) {
      if (event.type == ENET_EVENT_TYPE_CONNECT) {
        printf("[CLIENT] Conectado al servidor!\n");
      }
      if (event.type == ENET_EVENT_TYPE_RECEIVE) {
        PacketHeader hdr;
        ConnectAck ack;
        int pktType =
            parse_packet(event.packet, &hdr, &ack, sizeof(ConnectAck));
        if (pktType == PKT_CONNECT_ACK) {
          g_playerNetId = ack.playerNetId;
          printf("[CLIENT] Soy netId=%d\n", g_playerNetId);
          connected = true;
        }
        enet_packet_destroy(event.packet);
      }
    }
  }

  if (!connected) {
    fprintf(stderr, "[CLIENT] Timeout esperando ACK del servidor.\n");
    enet_host_destroy(client);
    enet_deinitialize();
    return 1;
  }

  // Inicializar predicción local (posición se sincroniza desde el primer
  // snapshot)
  Map_InitHeadless(&g_localMap);
  Entity_Init(&g_localPlayer.entity);
  g_localPlayer.entity.speed = 10.0f;
  g_localPlayer.entity.radius = 0.8f;
  MovingEntity_Init(&g_localPlayer);
  g_localReady = true;
  g_initialSynced = false;

  // Ventana
  const int screenW = 1280, screenH = 720;
  InitWindow(screenW, screenH, "Boba MOBA — Cliente");
  SetExitKey(0);

  Camera3D camera = {0};
  Vector3 cameraOffset = {20.0f, 20.0f, 20.0f};
  camera.up = (Vector3){0, 1, 0};
  camera.fovy = 22.0f;
  camera.projection = CAMERA_ORTHOGRAPHIC;
  camera.target = (Vector3){0, 0, 0};
  camera.position = Vector3Add(camera.target, cameraOffset);

  bool menuActive = false;
  Rectangle exitBtnBounds = {screenW / 2.0f - 100, screenH / 2.0f - 25, 200,
                             50};
  bool shouldClose = false;

// Throttle envío a 60Hz (match server tick rate)
#define SEND_INTERVAL (1.0f / 60.0f)
  float sendTimer = 0.0f;
  InputPacket pendingInp = {0};
  pendingInp.attackTarget = -1;
  pendingInp.aimTargetId = -1;
  bool hasPendingAction = false;

  while (!shouldClose && !WindowShouldClose()) {
    float dt = GetFrameTime();
    double now = GetTime();
    sendTimer += dt;

    // ===== 1. INPUT =====
    if (IsKeyPressed(KEY_ESCAPE))
      menuActive = !menuActive;

    if (!menuActive && !g_localDead) {
      Ray mouseRay = GetMouseRay(GetMousePosition(), camera);
      int hoveredId = g_snapCount > 0
                          ? pick_entity(&g_snapCurr, mouseRay, g_playerNetId)
                          : -1;

      // Cursor
      if (hoveredId >= 0 && g_snapCount > 0) {
        const NetEntity *he = find_net_entity(&g_snapCurr, hoveredId);
        if (he) {
          int playerTeam = 1;
          const NetEntity *me = find_net_entity(&g_snapCurr, g_playerNetId);
          if (me)
            playerTeam = me->team;
          SetMouseCursor(he->team == playerTeam ? MOUSE_CURSOR_POINTING_HAND
                                                : MOUSE_CURSOR_CROSSHAIR);
        }
      } else {
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
      }

      if (IsKeyPressed(KEY_Q)) {
        pendingInp.keyQ = 1;
        float gx, gz;
        if (hoveredId >= 0)
          pendingInp.aimTargetId = hoveredId;
        else if (ray_hit_ground(mouseRay, &gx, &gz)) {
          pendingInp.aimX = gx;
          pendingInp.aimZ = gz;
        }
        hasPendingAction = true;
      }
      if (IsKeyPressed(KEY_W)) {
        pendingInp.keyW = 1;
        if (hoveredId >= 0)
          pendingInp.aimTargetId = hoveredId;
        hasPendingAction = true;
      }
      if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) ||
          (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
           IsKeyDown(KEY_LEFT_CONTROL))) {
        pendingInp.rightClick = 1;
        if (hoveredId >= 0) {
          pendingInp.attackTarget = hoveredId;
        } else {
          float gx, gz;
          if (ray_hit_ground(mouseRay, &gx, &gz)) {
            pendingInp.targetX = gx;
            pendingInp.targetZ = gz;
          }
        }
        hasPendingAction = true;

        // PREDICCIÓN LOCAL: mover inmediatamente
        if (g_localReady && pendingInp.attackTarget < 0) {
          Vector3 dest = {pendingInp.targetX, 0.0f, pendingInp.targetZ};
          MovingEntity_MoveTo(&g_localPlayer, &g_localMap, dest);
        }
      }
    }

    // ===== 1b. ENVIAR INPUT (throttled 60Hz, unreliable) =====
    if (sendTimer >= SEND_INTERVAL) {
      sendTimer -= SEND_INTERVAL;
      if (hasPendingAction) {
        send_unreliable(serverPeer, PKT_INPUT, &pendingInp, sizeof(pendingInp));
      } else {
        InputPacket empty = {0};
        empty.attackTarget = -1;
        empty.aimTargetId = -1;
        send_unreliable(serverPeer, PKT_INPUT, &empty, sizeof(empty));
      }
      memset(&pendingInp, 0, sizeof(pendingInp));
      pendingInp.attackTarget = -1;
      pendingInp.aimTargetId = -1;
      hasPendingAction = false;
      enet_host_flush(client);
    }

    // ===== 2. RECIBIR SNAPSHOTS =====
    {
      ENetEvent event;
      while (enet_host_service(client, &event, 0) > 0) {
        switch (event.type) {
        case ENET_EVENT_TYPE_RECEIVE: {
          PacketHeader hdr;
          StateSnapshot snap;
          int pktType =
              parse_packet(event.packet, &hdr, &snap, sizeof(StateSnapshot));
          if (pktType == PKT_STATE_SNAPSHOT &&
              hdr.length == (int)sizeof(StateSnapshot)) {
            // Shift buffer: curr → prev, nuevo → curr
            g_snapPrev = g_snapCurr;
            g_snapPrevTime = g_snapCurrTime;
            g_snapCurr = snap;
            g_snapCurrTime = GetTime();
            g_snapCount++;

            // Auto-ajustar delay de interpolación al intervalo real
            if (g_snapCount >= 2) {
              double measured = g_snapCurrTime - g_snapPrevTime;
              if (measured > 0.001 && measured < 0.5) {
                // Suavizar para evitar saltos por jitter
                g_interpDelay = g_interpDelay * 0.9 + measured * 0.1;
              }
            }
          }
          enet_packet_destroy(event.packet);
          break;
        }
        case ENET_EVENT_TYPE_DISCONNECT:
          printf("[CLIENT] Servidor desconectado.\n");
          shouldClose = true;
          break;
        default:
          break;
        }
      }

      // Sincronización de posición
      if (g_snapCount > 0 && g_localReady) {
        const NetEntity *me = find_net_entity(&g_snapCurr, g_playerNetId);
        if (me) {
          Vector3 srvPos = {me->x, me->y, me->z};

          // Primera vez: sync total desde el servidor (spawn position)
          if (!g_initialSynced) {
            g_localPlayer.entity.position = srvPos;
            g_localPlayer.pathLength = 0;
            g_localPlayer.pathIndex = 0;
            g_initialSynced = true;
            printf("[CLIENT] Sync inicial: pos=(%.1f, %.1f, %.1f)\n", srvPos.x,
                   srvPos.y, srvPos.z);
          } else {
            // Reconciliación: reset si divergencia > 3m
            Vector3 diff =
                Vector3Subtract(srvPos, g_localPlayer.entity.position);
            diff.y = 0;
            if (Vector3Length(diff) > 3.0f) {
              g_localPlayer.entity.position = srvPos;
              g_localPlayer.pathLength = 0;
              g_localPlayer.pathIndex = 0;
            }
          }

          // Sync muerte/respawn desde el servidor
          if (me->isDead) {
            g_localDead = true;
            g_localRespawnTimer = me->respawnTimer;
          } else if (g_localDead) {
            // Estaba muerto y el servidor dice que ya no: respawneó
            g_localPlayer.entity.position = srvPos;
            g_localPlayer.pathLength = 0;
            g_localPlayer.pathIndex = 0;
            g_localDead = false;
            g_localRespawnTimer = 0.0f;
          }
        }
      }
    }

    // ===== 3. SIMULAR PREDICCIÓN LOCAL =====
    if (g_localReady)
      MovingEntity_Update(&g_localPlayer, dt);

    // ===== 4. CÁMARA =====
    if (g_localReady) {
      camera.target = g_localPlayer.entity.position;
      camera.position = Vector3Add(g_localPlayer.entity.position, cameraOffset);
    }

    // ===== 5. RENDER =====
    BeginDrawing();
    ClearBackground((Color){30, 30, 40, 255});
    BeginMode3D(camera);
    DrawGrid(40, 1.0f);
    if (g_snapCount > 0) {
      // Renderizar un tick en el pasado → interpolación pura, sin extrapolación
      double renderTime = now - g_interpDelay;
      draw_entities(&g_snapCurr, g_playerNetId, camera, renderTime);
    }
    EndMode3D();

    // ===== DEATH OVERLAY =====
    if (g_localDead) {
      // Overlay oscuro semi-transparente (simula escala de grises)
      DrawRectangle(0, 0, screenW, screenH, (Color){0, 0, 0, 150});

      // timer de respawn
      int timerVal = (int)ceilf(g_localRespawnTimer);
      if (timerVal < 0)
        timerVal = 0;
      const char *timerText = TextFormat("%d", timerVal);
      int timerFontSize = 120;
      int timerW = MeasureText(timerText, timerFontSize);
      DrawText(timerText, screenW / 2 - timerW / 2, screenH / 2 - 80,
               timerFontSize, WHITE);

      const char *deathMsg = "HAS MUERTO";
      int msgFontSize = 36;
      int msgW = MeasureText(deathMsg, msgFontSize);
      DrawText(deathMsg, screenW / 2 - msgW / 2, screenH / 2 + 50, msgFontSize,
               RED);
    }

    DrawText(TextFormat("FPS: %d", GetFPS()), screenW - 100, 10, 20, WHITE);
    DrawText(TextFormat("NetId: %d", g_playerNetId), 10, 10, 16, GRAY);
    DrawText("Q", 20, screenH - 60, 30, WHITE);
    DrawText("W", 100, screenH - 60, 30, WHITE);

    // ===== DEBUG HUD (F3) =====
    if (IsKeyPressed(KEY_F3))
      g_debugHud = !g_debugHud;

    if (g_debugHud && g_snapCount > 0) {
      int dy = 30;
      int y = 30;
      DrawRectangle(0, y - 2, 400, 18 + dy * (g_snapCurr.entityCount + 3),
                    (Color){0, 0, 0, 180});

      DrawText("[DEBUG - F3]", 10, y, 14, LIME);
      y += dy;

      // Local prediction vs server
      const NetEntity *me = find_net_entity(&g_snapCurr, g_playerNetId);
      if (me && g_localReady) {
        Vector3 lp = g_localPlayer.entity.position;
        DrawText(TextFormat("LOCAL  x:%.2f z:%.2f", lp.x, lp.z), 10, y, 14,
                 SKYBLUE);
        y += dy;
        DrawText(TextFormat("SERVER x:%.2f z:%.2f", me->x, me->z), 10, y, 14,
                 ORANGE);
        y += dy;
        float dx = lp.x - me->x;
        float dz = lp.z - me->z;
        float delta = sqrtf(dx * dx + dz * dz);
        Color deltaColor = delta < 0.5f ? GREEN : (delta < 2.0f ? YELLOW : RED);
        DrawText(TextFormat("DELTA  %.3f m", delta), 10, y, 14, deltaColor);
        y += dy;
      }

      DrawText(TextFormat("InterpDelay: %.1f ms", g_interpDelay * 1000.0), 10,
               y, 14, GRAY);
      y += dy;

      // Todas las entidades
      for (int i = 0; i < g_snapCurr.entityCount; i++) {
        const NetEntity *e = &g_snapCurr.entities[i];
        if (!e->active)
          continue;
        const char *tag = (e->netId == g_playerNetId) ? "YOU" : "   ";
        const char *team = (e->team == 1) ? "B" : (e->team == 2) ? "R" : "N";
        DrawText(TextFormat("%s id:%d [%s] x:%.1f z:%.1f hp:%.0f", tag,
                            e->netId, team, e->x, e->z, e->health),
                 10, y, 12, (e->netId == g_playerNetId) ? SKYBLUE : WHITE);
        y += dy - 8;
      }
    }

    if (g_snapCount == 0) {
      DrawText("Esperando servidor...", screenW / 2 - 120, screenH / 2, 20,
               YELLOW);
    }
    if (menuActive) {
      bool bh = CheckCollisionPointRec(GetMousePosition(), exitBtnBounds);
      DrawRectangleRec(exitBtnBounds, bh ? RED : MAROON);
      DrawRectangleLinesEx(exitBtnBounds, 2, WHITE);
      int tw = MeasureText("SALIR", 20);
      DrawText("SALIR", exitBtnBounds.x + (exitBtnBounds.width - tw) / 2,
               exitBtnBounds.y + 15, 20, WHITE);
    }
    EndDrawing();
  }

  enet_peer_disconnect(serverPeer, 0);
  // Allow a short window for the disconnect to be sent
  {
    ENetEvent event;
    while (enet_host_service(client, &event, 200) > 0) {
      if (event.type == ENET_EVENT_TYPE_RECEIVE)
        enet_packet_destroy(event.packet);
      if (event.type == ENET_EVENT_TYPE_DISCONNECT)
        break;
    }
  }

  CloseWindow();
  enet_host_destroy(client);
  enet_deinitialize();
  Map_Unload(&g_localMap);
  printf("[CLIENT] Saliendo.\n");
  return 0;
}
