#ifndef STEAM_API_H
#define STEAM_API_H

#include "req_parser.h"
#include <stdbool.h>

#define MAX_SEARCH_RESULTS 15

typedef struct {
    long appid;
    char name[REQ_STR_LEN];
    bool has_win;
    bool has_mac;
    bool has_linux;
} SteamSearchResult;

typedef struct {
    int total_results;
    SteamSearchResult items[MAX_SEARCH_RESULTS];
} SearchResultSet;

int steam_search_games(const char *query, SearchResultSet *results);
int steam_get_game_details(long appid, GameRequirements *details);

#endif /* STEAM_API_H */
