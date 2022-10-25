#include <Server/Server.h>
#include <Util/Checks/PlayerChecks.h>

void send_world_update(server_t* server, player_t* player)
{
    if (server->protocol.num_players == 0) {
        return;
    }
    ENetPacket* packet = enet_packet_create(NULL, 1 + (32 * 24), 0);
    stream_t    stream = {packet->data, packet->dataLength, 0};
    stream_write_u8(&stream, PACKET_TYPE_WORLD_UPDATE);

    player_t *connected_player, *tmp;
    HASH_ITER(hh, server->players, connected_player, tmp) {
        if (player_to_player_visibile(player, connected_player) && connected_player->is_invisible == 0) {
            /*float    dt       = (getNanos() - server->globalTimers.lastUpdateTime) / 1000000000.f;
            Vector3f position = {server->player[j].movement.velocity.x * dt + server->player[j].movement.position.x,
                                 server->player[j].movement.velocity.y * dt + server->player[j].movement.position.y,
                                 server->player[j].movement.velocity.z * dt + server->player[j].movement.position.z};
            WriteVector3f(&stream, position);*/
            stream_write_vector3f(&stream, connected_player->movement.position);
            stream_write_vector3f(&stream, connected_player->movement.forward_orientation);
        } else {
            vector3f_t empty;
            empty.x = 0;
            empty.y = 0;
            empty.z = 0;
            stream_write_vector3f(&stream, empty);
            stream_write_vector3f(&stream, empty);
        }
    }
    if (enet_peer_send(player->peer, 0, packet) != 0) {
        enet_packet_destroy(packet);
    }
}
