#include "Server/Structs/PlayerStruct.h"
#include "Util/Enums.h"
#include "Util/Log.h"
#include "Util/Uthash.h"
#include <Server/Grenade.h>
#include <Server/IntelTent.h>
#include <Server/Master.h>
#include <Server/Packets/Packets.h>
#include <Server/ParseConvert.h>
#include <Server/Structs/ServerStruct.h>
#include <Util/Checks/PlayerChecks.h>
#include <Util/Checks/PositionChecks.h>
#include <Util/Checks/TimeChecks.h>
#include <Util/JSONHelpers.h>
#include <Util/Nanos.h>
#include <Util/Notice.h>
#include <Util/Physics.h>
#include <Util/Utlist.h>
#include <enet/enet.h>
#include <json-c/json_util.h>
#include <math.h>
#include <stdint.h>
#include <time.h>

static uint8_t _on_connect(server_t* server)
{
    if (server->protocol.num_players == server->protocol.max_players) {
        return 0xFF;
    }
    uint8_t player_id;
    for (player_id = 0; player_id < server->protocol.max_players; ++player_id) {
        player_t* player;
        HASH_FIND(hh, server->players, &player_id, sizeof(player_id), player);
        if (player == NULL) {
            break;
        }
    }
    server->protocol.num_players++;
    return player_id;
}

void free_all_players(server_t* server)
{
    player_t *player, *tmp;
    HASH_ITER(hh, server->players, player, tmp) {
        HASH_DEL(server->players, player);
        free(player);
    }
}

void update_movement_and_grenades(server_t* server)
{
    physics_set_globals((server->global_timers.update_time - server->global_timers.time_since_start) / 1000000000.f,
                        (server->global_timers.update_time - server->global_timers.last_update_time) / 1000000000.f);
    player_t *player, *tmp;
    HASH_ITER(hh, server->players, player, tmp) {
        if (player->state == STATE_READY) {
            long falldamage = 0;
            falldamage      = physics_move_player(server, player);
            if (falldamage > 0) {
                vector3f_t zero = {0, 0, 0};
                send_set_hp(server, player, player, falldamage, 0, 4, 5, 0, zero);
            }
            if (server->protocol.current_gamemode != GAME_MODE_ARENA) {
                handleTentAndIntel(server, player);
            }
        }
        handle_grenade(server, player);
    }
}

uint8_t get_player_unstuck(server_t* server, player_t* player)
{
    for (float z = player->movement.prev_legit_pos.z - 1;
         z <= player->movement.prev_legit_pos.z + 1;
         z++)
    {
        if (valid_player_pos(server,
                         player,
                         player->movement.prev_legit_pos.x,
                         player->movement.prev_legit_pos.y,
                         z))
        {
            player->movement.prev_legit_pos.z = z;
            player->movement.position         = player->movement.prev_legit_pos;
            return 1;
        }
        for (float x = player->movement.prev_legit_pos.x - 1;
             x <= player->movement.prev_legit_pos.x + 1;
             x++)
        {
            for (float y = player->movement.prev_legit_pos.y - 1;
                 y <= player->movement.prev_legit_pos.y + 1;
                 y++)
            {
                if (valid_player_pos(server, player, x, y, z)) {
                    player->movement.prev_legit_pos.x = x;
                    player->movement.prev_legit_pos.y = y;
                    player->movement.prev_legit_pos.z = z;
                    player->movement.position = player->movement.prev_legit_pos;
                    return 1;
                }
            }
        }
    }
    return 0;
}

void set_player_respawn_point(server_t* server, player_t* player)
{
    if (player->team != TEAM_SPECTATOR) {
        quad3d_t* spawn = server->protocol.spawns + player->team;

        float dx = spawn->to.x - spawn->from.x;
        float dy = spawn->to.y - spawn->from.y;

        player->movement.position.x = spawn->from.x + dx * ((float) rand() / (float) RAND_MAX);
        player->movement.position.y = spawn->from.y + dy * ((float) rand() / (float) RAND_MAX);
        player->movement.position.z =
        mapvxl_find_top_block(&server->s_map.map,
                              player->movement.position.x,
                              player->movement.position.y) -
        2.36f;

        player->movement.forward_orientation.x = 0.f;
        player->movement.forward_orientation.y = 0.f;
        player->movement.forward_orientation.z = 0.f;
    }
}

void init_player(server_t*  server,
                 player_t* player,
                 uint8_t    reset,
                 uint8_t    disconnect,
                 vector3f_t empty,
                 vector3f_t forward,
                 vector3f_t strafe,
                 vector3f_t height)
{
    if (reset == 0) {
        player->state  = STATE_DISCONNECTED;
        player->queues = NULL;
    }
    player->ups                          = 60;
    player->timers.time_since_last_wu    = get_nanos();
    player->input                        = 0;
    player->movement.eye_pos             = empty;
    player->movement.forward_orientation = forward;
    player->movement.strafe_orientation  = strafe;
    player->movement.height_orientation  = height;
    player->movement.position            = empty;
    player->movement.velocity            = empty;
    if (reset == 0 && disconnect == 0) {
        permissions_t roleList[5] = {{"manager", &server->manager_passwd, 4},
                                     {"admin", &server->admin_passwd, 3},
                                     {"mod", &server->mod_passwd, 2},
                                     {"guard", &server->guard_passwd, 1},
                                     {"trusted", &server->trusted_passwd, 0}};
        for (unsigned long x = 0; x < sizeof(roleList) / sizeof(permissions_t); ++x) {
            player->role_list[x] = roleList[x];
        }
        player->grenade = NULL;
    }
    player->airborne                             = 0;
    player->wade                                 = 0;
    player->lastclimb                            = 0;
    player->move_backwards                       = 0;
    player->move_forward                         = 0;
    player->move_left                            = 0;
    player->move_right                           = 0;
    player->jumping                              = 0;
    player->crouching                            = 0;
    player->sneaking                             = 0;
    player->sprinting                            = 0;
    player->primary_fire                         = 0;
    player->secondary_fire                       = 0;
    player->can_build                            = 1;
    player->allow_killing                        = 1;
    player->allow_team_killing                   = 0;
    player->muted                                = 0;
    player->told_to_master                       = 0;
    player->timers.since_last_base_enter         = 0;
    player->timers.since_last_base_enter_restock = 0;
    player->timers.since_last_3block_dest        = 0;
    player->timers.since_last_block_dest         = 0;
    player->timers.since_last_block_plac         = 0;
    player->timers.since_last_grenade_thrown     = 0;
    player->timers.since_last_shot               = 0;
    player->timers.time_since_last_wu            = 0;
    player->timers.since_last_weapon_input       = 0;
    player->hp                                   = 100;
    player->blocks                               = 50;
    player->grenades                             = 3;
    player->has_intel                            = 0;
    player->reloading                            = 0;
    player->client                               = ' ';
    player->version_minor                        = 0;
    player->version_major                        = 0;
    player->version_revision                     = 0;
    player->periodic_delay_index                 = 0;
    player->current_periodic_message             = server->periodic_messages;
    player->welcome_sent                         = 0;
    if (reset == 0) {
        player->permissions = 0;
    } else if (reset == 1) {
        grenade_t* grenade;
        grenade_t* tmp;
        DL_FOREACH_SAFE(player->grenade, grenade, tmp)
        {
            DL_DELETE(player->grenade, grenade);
            free(grenade);
        }
    }
    player->is_invisible = 0;
    player->kills        = 0;
    player->deaths       = 0;
    memset(player->name, 0, 17);
    memset(player->os_info, 0, 255);
}

void send_joining_data(server_t* server, player_t* player)
{
    player_t *receiver, *tmp;
    LOG_INFO("Sending state to %s (#%hhu)", player->name, player->id);
    HASH_ITER(hh, server->players, receiver, tmp) {
        if (receiver != player && is_past_join_screen(receiver)) {
            send_existing_player(server, player, receiver);
        }
    }
    send_state_data(server, player);
}

void on_player_update(server_t* server, player_t* player)
{
    switch (player->state) {
        case STATE_DISCONNECTED:
            break;
        case STATE_STARTING_MAP:
            player->blockBuffer = NULL;
            send_map_start(server, player);
            break;
        case STATE_LOADING_CHUNKS:
            send_map_chunks(server, player);
            break;
        case STATE_JOINING:
            send_joining_data(server, player);
            break;
        case STATE_SPAWNING:
            player->hp             = 100;
            player->grenades       = 3;
            player->blocks         = 50;
            player->item           = 2;
            player->input          = 0;
            player->move_forward   = 0;
            player->move_backwards = 0;
            player->move_left      = 0;
            player->move_right     = 0;
            player->jumping        = 0;
            player->crouching      = 0;
            player->sneaking       = 0;
            player->sprinting      = 0;
            player->primary_fire   = 0;
            player->secondary_fire = 0;
            player->alive          = 1;
            player->reloading      = 0;
            set_player_respawn_point(server, player);
            send_respawn(server, player);
            LOG_INFO("Player %s (#%hhu) spawning at: %f %f %f",
                     player->name,
                     player->id,
                     player->movement.position.x,
                     player->movement.position.y,
                     player->movement.position.z);
            break;
        case STATE_WAITING_FOR_RESPAWN:
        {
            if (time(NULL) - player->timers.start_of_respawn_wait >=
                player->respawn_time)
            {
                player->state = STATE_SPAWNING;
            }
            break;
        }
        case STATE_READY:
            // send data
            if (server->master.enable_master_connection == 1) {
                if (player->told_to_master == 0) {
                    master_update(server);
                    player->told_to_master = 1;
                }
            }
            break;
        default:
            // disconnected
            break;
    }
}

void on_new_player_connection(server_t* server, ENetEvent* event)
{
    uint8_t banned_user = 0;
    uint8_t player_id;
    if (event->data != VERSION_0_75) {
        enet_peer_disconnect_now(event->peer, REASON_WRONG_PROTOCOL_VERSION);
        return;
    }

    struct json_object* root = json_object_from_file("Bans.json");
    if (root == NULL) {
        FILE* fp;
        fp = fopen("Bans.json", "w+");
        if (fp == NULL) {
            perror("Unable to open/create Bans.json with error: ");
            exit(EXIT_FAILURE);
        }
        fclose(fp);
        root = json_object_new_object();
        json_object_object_add(root, "Bans", json_object_new_array());
        json_object_to_file("Bans.json", root);
    }
    ip_t hostIP;
    hostIP.cidr = 24;
    hostIP.ip32 = event->peer->address.host;
    struct json_object* array;
    json_object_object_get_ex(root, "Bans", &array);
    int count = json_object_array_length(array);
    for (int i = 0; i < count; ++i) {
        struct json_object* object_at_index = json_object_array_get_idx(array, i);
        const char*         IP;
        const char*         start_of_range_string;
        const char*         end_of_range_string;
        READ_STR_FROM_JSON(object_at_index, start_of_range_string, start_of_range, "start of range", "0.0.0.0", 1);
        READ_STR_FROM_JSON(object_at_index, end_of_range_string, end_of_range, "end of range", "0.0.0.0", 1);
        READ_STR_FROM_JSON(object_at_index, IP, IP, "IP", "0.0.0.0", 1);
        ip_t ipStruct;
        ip_t start_of_range, end_of_range;
        if (format_str_to_ip((char*) IP, &ipStruct) &&
            (format_str_to_ip((char*) start_of_range_string, &start_of_range) &&
             format_str_to_ip((char*) end_of_range_string, &end_of_range)))
        {
            if (ip_in_range(hostIP, ipStruct, start_of_range, end_of_range)) {
                const char* name_of_player;
                const char* reason;
                double      time    = 0.0f;
                uint64_t    timeNow = get_nanos();
                READ_STR_FROM_JSON(object_at_index, name_of_player, Name, "Name", "Deuce", 0);
                READ_STR_FROM_JSON(object_at_index, reason, Reason, "Reason", "None", 0);
                READ_DOUBLE_FROM_JSON(object_at_index, time, Time, "Time", 0.0f, 0);
                if (((long double) timeNow / NANO_IN_MINUTE) > time && time != 0) {
                    json_object_array_del_idx(array, i, 1);
                    json_object_to_file("Bans.json", root);
                    continue; // Continue searching for bans and delete all the old ones that match host IP
                              // or its range
                } else {
                    enet_peer_disconnect(event->peer, REASON_BANNED);

                    LOG_WARNING("Banned user %s tried to join with IP: %hhu.%hhu.%hhu.%hhu Banned for: %s",
                                name_of_player,
                                hostIP.ip[0],
                                hostIP.ip[1],
                                hostIP.ip[2],
                                hostIP.ip[3],
                                reason);
                    banned_user       = 1;
                    event->peer->data = (void*) ((size_t) server->protocol.max_players - 1);
                    break;
                }
            }
        }
    }
    json_object_put(root);

    if (banned_user) {
        return;
    }
    // check peer
    // ...
    // find next free ID
    player_id = _on_connect(server);
    if (player_id == 0xFF) {
        enet_peer_disconnect_now(event->peer, REASON_SERVER_FULL);
        LOG_WARNING("Server full. Kicking player");
        return;
    }
    player_t* player = calloc(1, sizeof(player_t));
    vector3f_t empty   = {0, 0, 0};
    vector3f_t forward = {1, 0, 0};
    vector3f_t height  = {0, 0, 1};
    vector3f_t strafe  = {0, 1, 0};
    init_player(server, player, 0, 0, empty, forward, strafe, height);
    player->id = player_id;
    player->peer    = event->peer;
    event->peer->data                 = (void*) ((size_t) player_id);
    player->hp      = 100;
    player->ip.ip32 = event->peer->address.host;

    format_ip_to_str(player->name, player->ip);
    snprintf(player->name, 6, "Limbo");
    char ipString[17];
    format_ip_to_str(ipString, player->ip);
    LOG_INFO("Player %s (%s, #%hhu) connected", player->name, ipString, player_id);

    player->timers.since_periodic_message = get_nanos();
    player->current_periodic_message      = server->periodic_messages;
    player->periodic_delay_index          = 0;
    player->state = STATE_STARTING_MAP;
    HASH_ADD(hh, server->players, id, sizeof(uint8_t), player);
}

void for_players(server_t* server)
{
    player_t *player, *tmp;
    HASH_ITER(hh, server->players, player, tmp) {
        if (is_past_join_screen(player)) {
            uint64_t timeNow = get_nanos();
            if (player->primary_fire == 1 && player->reloading == 0) {
                if (player->weapon_clip > 0) {
                    uint64_t milliseconds = 0;
                    switch (player->weapon) {
                        case WEAPON_RIFLE:
                        {
                            milliseconds = 500;
                            break;
                        }
                        case WEAPON_SMG:
                        {
                            milliseconds = 100;
                            break;
                        }
                        case WEAPON_SHOTGUN:
                        {
                            milliseconds = 1000;
                            break;
                        }
                    }
                    if (diff_is_older_then(timeNow,
                                           &player->timers.since_last_weapon_input,
                                           NANO_IN_MILLI * milliseconds))
                    {
                        player->weapon_clip--;
                    }
                }
            } else if (player->primary_fire == 0 && player->reloading == 1) {
                switch (player->weapon) {
                    case WEAPON_RIFLE:
                    case WEAPON_SMG:
                    {
                        if (diff_is_older_then(timeNow,
                                               &player->timers.since_reload_start,
                                               NANO_IN_MILLI * (uint64_t) 2500))
                        {
                            uint8_t defaultAmmo = RIFLE_DEFAULT_CLIP;
                            if (player->weapon == WEAPON_SMG) {
                                defaultAmmo = SMG_DEFAULT_CLIP;
                            }
                            double newReserve = fmax(0,
                                                     player->weapon_reserve -
                                                     (defaultAmmo - player->weapon_clip));
                            player->weapon_clip +=
                            player->weapon_reserve - newReserve;
                            player->weapon_reserve = newReserve;
                            player->reloading      = 0;
                            send_weapon_reload(server, player, 0, 0, 0);
                        }
                        break;
                    }
                    case WEAPON_SHOTGUN:
                    {
                        if (diff_is_older_then(timeNow,
                                               &player->timers.since_reload_start,
                                               NANO_IN_MILLI * (uint64_t) 500))
                        {
                            if (player->weapon_reserve == 0 ||
                                player->weapon_clip == SHOTGUN_DEFAULT_CLIP)
                            {
                                player->reloading = 0;
                                break;
                            }
                            player->weapon_clip++;
                            player->weapon_reserve--;
                            if (player->weapon_clip == SHOTGUN_DEFAULT_CLIP) {
                                player->reloading = 0;
                            }
                            send_weapon_reload(server, player, 0, 0, 0);
                        }
                        break;
                    }
                }
            }
            if (diff_is_older_then(
                timeNow,
                &player->timers.since_periodic_message,
                (uint64_t) (server->periodic_delays[player->periodic_delay_index] * 60) *
                NANO_IN_SECOND))
            {
                string_node_t* message;
                DL_FOREACH(server->periodic_messages, message)
                {
                    send_server_notice(player, 0, message->string);
                }
                player->periodic_delay_index =
                fmin(player->periodic_delay_index + 1, 4);
            }
        } else if (player->state == STATE_PICK_SCREEN) {
            block_node_t *node, *tmp;
            LL_FOREACH_SAFE(player->blockBuffer, node, tmp)
            {
                player_t* sender;
                HASH_FIND(hh, server->players, &node->sender_id, sizeof(node->sender_id), sender);
                if (sender == NULL) {
                    goto not_found;
                }
                if (node->type == 10) {
                    send_set_color_to_player(server, sender, player, node->color);
                    send_block_line_to_player(server, sender, player, node->position, node->position_end);
                    send_set_color_to_player(server, sender, player, player->color);
                } else {
                    send_set_color_to_player(server, sender, player, node->color);
                    send_block_action_to_player(server,
                                                sender,
                                                player,
                                                node->type,
                                                node->position.x,
                                                node->position.y,
                                                node->position.z);
                    send_set_color_to_player(server, sender, player, player->color);
                }
not_found:
                LL_DELETE(player->blockBuffer, node);
                free(node);
            }
        } else if (is_past_state_data(player)) {
        }
    }
}
