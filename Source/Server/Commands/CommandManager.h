#ifndef COMMANDSMANAGER_H
#define COMMANDSMANAGER_H

#include <Server/Server.h>

void cmd_generate_ban(server_t* server, command_args_t arguments, float time, ip_t ip, char* reason);

#define DECLARE_COMMAND(name) void name(void* p_server, command_args_t arguments);

DECLARE_COMMAND(cmd_admin)
DECLARE_COMMAND(cmd_ban_custom)
DECLARE_COMMAND(cmd_ban_range)
DECLARE_COMMAND(cmd_admin_mute)
DECLARE_COMMAND(cmd_clin)
DECLARE_COMMAND(cmd_help)
DECLARE_COMMAND(cmd_intel)
DECLARE_COMMAND(cmd_inv)
DECLARE_COMMAND(cmd_kick)
DECLARE_COMMAND(cmd_kill)
DECLARE_COMMAND(cmd_login)
DECLARE_COMMAND(cmd_logout)
DECLARE_COMMAND(cmd_master)
DECLARE_COMMAND(cmd_mute)
DECLARE_COMMAND(cmd_pm)
DECLARE_COMMAND(cmd_ratio)
DECLARE_COMMAND(cmd_reset)
DECLARE_COMMAND(cmd_say)
DECLARE_COMMAND(cmd_server)
DECLARE_COMMAND(cmd_toggle_build)
DECLARE_COMMAND(cmd_toggle_kill)
DECLARE_COMMAND(cmd_toggle_team_kill)
DECLARE_COMMAND(cmd_tp)
DECLARE_COMMAND(cmd_tpc)
DECLARE_COMMAND(cmd_unban)
DECLARE_COMMAND(cmd_unban_range)
DECLARE_COMMAND(cmd_undo_ban)
DECLARE_COMMAND(cmd_ups)
DECLARE_COMMAND(cmd_shutdown)

#endif
