// Copyright DarkNeutrino 2021
#ifndef COMMANDS_H
#define COMMANDS_H

#include <Server/Structs/ServerStruct.h>
#include <libmapvxl/libmapvxl.h>

typedef enum {
    ROLE_TRUSTED     = 1,  // 00000001
    ROLE_GUARD       = 2,  // 00000010
    ROLE_MOD         = 4,  // 00000100
    ROLE_ADMIN       = 8,  // 00001000
    ROLE_MANAGER     = 16, // 00010000

    PERM_ANY         = 0,  // 00000000
    PERM_MIN_TRUSTED = 31, // 00011111
    PERM_MIN_GUARD   = 30, // 00011110
    PERM_MIN_MOD     = 28, // 00011100
    PERM_MIN_ADMIN   = 24  // 00011000
} command_permission_t;

uint8_t player_has_permission(player_t* player, uint8_t console, uint32_t permission);
void    command_handle(server_t* server, player_t* player, char* message, uint8_t console);
void    command_populate_all(server_t* server);
void    free_all_commands(server_t* server);
void    command_create(server_t* server,
                       uint8_t   parse_args,
                       void (*command)(void* server, command_args_t arguments),
                       char     id[30],
                       char     description[1024],
                       uint32_t permissions);
void    command_free(server_t* server, command_t* command);

#endif
