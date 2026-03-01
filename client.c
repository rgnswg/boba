/*
 * client.c — Cliente Gráfico de Boba MOBA (ENet/UDP)
 *
 * Predicción local del movimiento + servidor autoritativo.
 * Inputs se envían throttled a 30Hz via ENet (unreliable).
 * Snapshots se reciben unreliable — si se pierde uno, el siguiente reemplaza.
 */

#include "map.h"
#include "movement.h"
#include "net.h"
#include "pathfinding.h"
#include "raylib.h"
#include "raymath.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---- Estado ----

static StateSnapshot g_snapshot;
static bool g_hasSnapshot = false;
static int g_playerNetId = -1;
static float g_timeSinceSnap = 0.0f; // para extrapolar proyectiles

// Predicción local
static MovingEntity g_localPlayer;
static GameMap g_localMap;
static bool g_localReady = false;

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
                          Camera3D camera) {
  for (int i = 0; i < snap->entityCount; i++) {
    const NetEntity *e = &snap->entities[i];
    if (!e->active)
      continue;

    bool isPlayer = (e->netId == playerNetId);
    Vector3 pos;
    if (isPlayer && g_localReady) {
      pos = g_localPlayer.entity.position;
    } else {
      pos = (Vector3){e->x, e->y, e->z};
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

  for (int i = 0; i < snap->projectileCount; i++) {
    const NetProjectile *p = &snap->projectiles[i];
    if (!p->active)
      continue;
    // Extrapolar posición con velocidad entre snapshots (30Hz → suave)
    Vector3 pos = {p->x + p->vx * g_timeSinceSnap,
                   p->y + p->vy * g_timeSinceSnap,
                   p->z + p->vz * g_timeSinceSnap};
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

  // Inicializar predicción local
  Map_InitHeadless(&g_localMap);
  Entity_Init(&g_localPlayer.entity);
  g_localPlayer.entity.position = (Vector3){0.0f, 1.0f, 0.0f};
  g_localPlayer.entity.speed = 10.0f;
  g_localPlayer.entity.radius = 0.8f;
  MovingEntity_Init(&g_localPlayer);
  g_localReady = true;

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

// Throttle envío a 30Hz
#define SEND_INTERVAL (1.0f / 30.0f)
  float sendTimer = 0.0f;
  InputPacket pendingInp = {0};
  pendingInp.attackTarget = -1;
  pendingInp.aimTargetId = -1;
  bool hasPendingAction = false;

  while (!shouldClose && !WindowShouldClose()) {
    float dt = GetFrameTime();
    sendTimer += dt;

    // ===== 1. INPUT =====
    if (IsKeyPressed(KEY_ESCAPE))
      menuActive = !menuActive;

    if (!menuActive) {
      Ray mouseRay = GetMouseRay(GetMousePosition(), camera);
      int hoveredId = g_hasSnapshot
                          ? pick_entity(&g_snapshot, mouseRay, g_playerNetId)
                          : -1;

      // Cursor
      if (hoveredId >= 0 && g_hasSnapshot) {
        const NetEntity *he = NULL;
        for (int i = 0; i < g_snapshot.entityCount; i++) {
          if (g_snapshot.entities[i].netId == hoveredId) {
            he = &g_snapshot.entities[i];
            break;
          }
        }
        if (he) {
          int playerTeam = 1;
          for (int i = 0; i < g_snapshot.entityCount; i++) {
            if (g_snapshot.entities[i].netId == g_playerNetId) {
              playerTeam = g_snapshot.entities[i].team;
              break;
            }
          }
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

    // ===== 1b. ENVIAR INPUT (throttled 30Hz, unreliable) =====
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
            g_snapshot = snap;
            g_hasSnapshot = true;
            g_timeSinceSnap = 0.0f; // reset para extrapolación de proyectiles
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

      // Reconciliación mínima: solo hard-reset para divergencias imposibles.
      // NO sincronizar cuando el path termina: la predicción local ya llegó
      // al destino correcto. El servidor converge naturalmente.
      // El LERP causaba rubberbanding: tiraba al jugador ATRÁS porque
      // el server está 1-2 ticks detrás.
      if (g_hasSnapshot && g_localReady) {
        for (int i = 0; i < g_snapshot.entityCount; i++) {
          if (g_snapshot.entities[i].netId == g_playerNetId) {
            Vector3 srvPos = {g_snapshot.entities[i].x,
                              g_snapshot.entities[i].y,
                              g_snapshot.entities[i].z};
            Vector3 diff =
                Vector3Subtract(srvPos, g_localPlayer.entity.position);
            diff.y = 0;
            // Solo resetear para teleports/spawns (>10m de divergencia)
            if (Vector3Length(diff) > 10.0f) {
              g_localPlayer.entity.position = srvPos;
              g_localPlayer.pathLength = 0;
              g_localPlayer.pathIndex = 0;
            }
            break;
          }
        }
      }
    }

    // ===== 3. SIMULAR PREDICCIÓN LOCAL + avanzar extrapolación =====
    g_timeSinceSnap += dt;
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
    if (g_hasSnapshot)
      draw_entities(&g_snapshot, g_playerNetId, camera);
    EndMode3D();

    DrawText(TextFormat("FPS: %d", GetFPS()), screenW - 100, 10, 20, WHITE);
    DrawText(TextFormat("NetId: %d", g_playerNetId), 10, 10, 16, GRAY);
    DrawText("Q", 20, screenH - 60, 30, WHITE);
    DrawText("W", 100, screenH - 60, 30, WHITE);

    if (!g_hasSnapshot) {
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
