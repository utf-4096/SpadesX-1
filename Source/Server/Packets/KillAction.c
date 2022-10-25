#include <Server/Packets/Packets.h>
#include <Server/Server.h>
#include <Util/Checks/PlayerChecks.h>

void send_kill_action_packet(server_t* server,
                             player_t*   killer,
                             player_t*   player,
                             uint8_t   killReason,
                             uint8_t   respawnTime,
                             uint8_t   makeInvisible)
{
    if (server->protocol.num_players == 0) {
        return;
    }
    if (player->alive == 0) {
        return; // Cant kill player if they are dead
    }
    ENetPacket* packet = enet_packet_create(NULL, 5, ENET_PACKET_FLAG_RELIABLE);
    stream_t    stream = {packet->data, packet->dataLength, 0};
    stream_write_u8(&stream, PACKET_TYPE_KILL_ACTION);
    stream_write_u8(&stream, player->id);   // Player that died.
    stream_write_u8(&stream, killer->id);    // Player that killed.
    stream_write_u8(&stream, killReason);  // Killing reason (1 is headshot)
    stream_write_u8(&stream, respawnTime); // Time before respawn happens
    uint8_t sent = 0;
    player_t *connected_player, *tmp;
    HASH_ITER(hh, server->players, connected_player, tmp) {
        uint8_t isPast = is_past_state_data(connected_player);
        if ((makeInvisible && connected_player->id != player->id && isPast) || (isPast && !makeInvisible)) {
            if (enet_peer_send(connected_player->peer, 0, packet) == 0) {
                sent = 1;
            }
        }
    }
    if (sent == 0) {
        enet_packet_destroy(packet);
        return; // Do not kill the player since sending the packet failed
    }
    if (!makeInvisible && player->is_invisible == 0) {
        if (killer != player) {
            killer->kills++;
        }
        player->deaths++;
        player->alive                        = 0;
        player->respawn_time                 = respawnTime;
        player->timers.start_of_respawn_wait = time(NULL);
        player->state                        = STATE_WAITING_FOR_RESPAWN;
        switch (player->weapon) {
            case 0:
                player->weapon_reserve  = 50;
                player->weapon_clip     = 10;
                player->default_clip    = RIFLE_DEFAULT_CLIP;
                player->default_reserve = RIFLE_DEFAULT_RESERVE;
                break;
            case 1:
                player->weapon_reserve  = 120;
                player->weapon_clip     = 30;
                player->default_clip    = SMG_DEFAULT_CLIP;
                player->default_reserve = SMG_DEFAULT_RESERVE;
                break;
            case 2:
                player->weapon_reserve  = 48;
                player->weapon_clip     = 6;
                player->default_clip    = SHOTGUN_DEFAULT_CLIP;
                player->default_reserve = SHOTGUN_DEFAULT_RESERVE;
                break;
        }
    }
    if (player->has_intel) {
        send_intel_drop(server, player);
    }
}
