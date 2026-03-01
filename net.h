#ifndef NET_H
#define NET_H

#include <enet/enet.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// --- Configuración de Red ---
#define SERVER_HOST "127.0.0.1"
#define SERVER_PORT 7777

// --- Canales ENet ---
// Canal 0: Unreliable — snapshots (server→client), inputs (client→server)
// Canal 1: Reliable   — ConnectAck, game events (muerte, compra, etc.)
#define CHAN_UNRELIABLE 0
#define CHAN_RELIABLE 1
#define NUM_CHANNELS 2

// --- Tipos de Paquete ---
typedef enum {
  PKT_NONE = 0,
  PKT_STATE_SNAPSHOT = 1, // Server → Client: estado completo del juego
  PKT_INPUT = 2,          // Client → Server: inputs del frame
  PKT_CONNECT_ACK = 3,    // Server → Client: confirmación de conexión
} PacketType;

// --- Cabecera genérica de paquete ---
typedef struct {
  int32_t type;   // PacketType
  int32_t length; // tamaño del payload en bytes
} PacketHeader;

// --- Snapshot de entidad (serializable, sin punteros) ---
typedef struct {
  int32_t netId;
  float x, y, z;
  float health, maxHealth;
  float radius;
  int32_t team;   // 0=NEUTRAL, 1=BLUE, 2=RED
  int32_t active; // bool como int
} NetEntity;

// --- Snapshot de proyectil ---
typedef struct {
  float x, y, z;
  float vx, vy, vz; // velocidad para extrapolación client-side
  float radius;
  int32_t colorR, colorG, colorB;
  int32_t active;
} NetProjectile;

#define NET_MAX_ENTITIES 32
#define NET_MAX_PROJECTILES 100

// --- Snapshot completo del estado del juego ---
typedef struct {
  int32_t entityCount;
  NetEntity entities[NET_MAX_ENTITIES];
  int32_t projectileCount;
  NetProjectile projectiles[NET_MAX_PROJECTILES];
} StateSnapshot;

// --- Input del jugador ---
typedef struct {
  int32_t rightClick;
  float targetX, targetZ;
  int32_t attackTarget; // netId (-1 si ninguno)
  int32_t keyQ;
  int32_t keyW;
  float aimX, aimZ;
  int32_t aimTargetId; // netId (-1 si ninguno)
} InputPacket;

// --- ACK de conexión ---
typedef struct {
  int32_t playerNetId;
} ConnectAck;

// --- Helpers de envío ENet ---

// Envía un paquete por ENet. channel y flags controlan reliable/unreliable.
static inline bool enet_send_packet(ENetPeer *peer, PacketType type,
                                    const void *payload, int payloadLen,
                                    int channel, enet_uint32 flags) {
  int totalLen = (int)sizeof(PacketHeader) + payloadLen;
  ENetPacket *packet = enet_packet_create(NULL, totalLen, flags);
  if (!packet)
    return false;

  PacketHeader hdr;
  hdr.type = (int32_t)type;
  hdr.length = payloadLen;
  memcpy(packet->data, &hdr, sizeof(PacketHeader));
  if (payloadLen > 0 && payload) {
    memcpy(packet->data + sizeof(PacketHeader), payload, payloadLen);
  }

  return enet_peer_send(peer, channel, packet) == 0;
}

// Envía unreliable (para snapshots e inputs — si se pierde, el siguiente
// reemplaza)
static inline bool send_unreliable(ENetPeer *peer, PacketType type,
                                   const void *payload, int payloadLen) {
  return enet_send_packet(peer, type, payload, payloadLen, CHAN_UNRELIABLE,
                          ENET_PACKET_FLAG_UNSEQUENCED);
}

// Envía reliable (para ConnectAck, game events)
static inline bool send_reliable(ENetPeer *peer, PacketType type,
                                 const void *payload, int payloadLen) {
  return enet_send_packet(peer, type, payload, payloadLen, CHAN_RELIABLE,
                          ENET_PACKET_FLAG_RELIABLE);
}

// Parsea un paquete recibido de ENet. Devuelve el tipo y copia el payload.
static inline int parse_packet(const ENetPacket *packet, PacketHeader *hdrOut,
                               void *payloadOut, int maxPayload) {
  if (packet->dataLength < sizeof(PacketHeader))
    return PKT_NONE;

  memcpy(hdrOut, packet->data, sizeof(PacketHeader));
  int payloadLen = hdrOut->length;

  if (payloadLen < 0 || payloadLen > maxPayload)
    return PKT_NONE;
  if ((int)packet->dataLength < (int)sizeof(PacketHeader) + payloadLen)
    return PKT_NONE;

  if (payloadLen > 0 && payloadOut) {
    memcpy(payloadOut, packet->data + sizeof(PacketHeader), payloadLen);
  }

  return hdrOut->type;
}

#endif // NET_H
