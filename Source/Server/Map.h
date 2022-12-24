// Copyright DarkNeutrino 2021
#ifndef MAP_H
#define MAP_H

#include <Server/Structs/ServerStruct.h>
#include <Util/Queue.h>
#include <Util/Types.h>
#include <Server/Classicgen.h>

uint8_t map_load(server_t* server, const char* path, int map_size[3]);
uint8_t map_classicgen(server_t* server, classicgen_opt_t options);

#endif
