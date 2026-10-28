#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "sysinfo.h"
#include "steam_api.h"
#include "req_parser.h"
#include "checker.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

static void print_usage(const char *progname) {
    printf("%sSteam Game Compatibility Checker for Linux%s\n", col_bold(), col_reset());
    printf("Usage: %s [OPTIONS] [GAME_NAME | APPID]\n\n", progname);
    printf("Options:\n");
    printf("  --sysinfo         Display your Linux PC's detected hardware and exit\n");
    printf("  --json            Output compatibility results in machine-readable JSON\n");
    printf("  --no-color        Disable colored terminal output\n");
    printf("  -h, --help        Display this help message\n\n");
    printf("Examples:\n");
    printf("  %s                    # Launch interactive prompt\n", progname);
    printf("  %s \"portal 2\"          # Check game by title\n", progname);
    printf("  %s 1091500            # Check game directly by Steam AppID (Cyberpunk 2077)\n", progname);
    printf("  %s --sysinfo          # Show Linux hardware info\n", progname);
}

static bool is_numeric_str(const char *s) {
    if (!s || !*s) return false;
    for (; *s; s++) {
        if (!isdigit((unsigned char)*s)) return false;
    }
    return true;
}

static int run_check_for_appid(long appid, const SystemSpecs *specs, bool json_mode) {
    if (!json_mode) {
        printf("%sFetching Steam system requirements for AppID %ld...%s\n", col_dim(), appid, col_reset());
    }

    GameRequirements game;
    int res = steam_get_game_details(appid, &game);
    if (res != 0) {
        if (res == -2) {
            fprintf(stderr, "%sError:%s Game with AppID %ld not found or restricted.\n", col_red(), col_reset(), appid);
        } else {
            fprintf(stderr, "%sError:%s Failed to fetch details from Steam Store API. Please check your internet connection.\n",
                    col_red(), col_reset());
        }
        return 1;
    }

    EvaluationReport min_rep, rec_rep;
    evaluate_compatibility(specs, &game, &min_rep, &rec_rep);

    if (json_mode) {
        print_json_report(specs, &game, &min_rep, &rec_rep);
    } else {
        print_compatibility_report(specs, &game, &min_rep, &rec_rep);
    }

    return 0;
}

static int run_search_and_check(const char *query, const SystemSpecs *specs, bool json_mode) {
    if (!json_mode) {
        printf("%sSearching Steam Store for \"%s\"...%s\n", col_dim(), query, col_reset());
    }

    SearchResultSet results;
    int count = steam_search_games(query, &results);
    if (count < 0) {
        fprintf(stderr, "%sError:%s Failed to communicate with Steam API. Check your internet connection.\n",
                col_red(), col_reset());
        return 1;
    }

    if (count == 0) {
        fprintf(stderr, "\n%sNo games found matching \"%s\".%s Try a different title or search term.\n\n",
                col_yellow(), query, col_reset());
        return 1;
    }

    long chosen_appid = -1;
    if (count == 1 || json_mode || !isatty(STDIN_FILENO)) {
        chosen_appid = results.items[0].appid;
        if (!json_mode && count > 1) {
            printf("%sSelected top match:%s %s (AppID: %ld)\n",
                   col_bold(), col_reset(), results.items[0].name, chosen_appid);
        }
    } else {
        printf("\nFound %s%d matching games%s:\n", col_bold(), count, col_reset());
        for (int i = 0; i < count; i++) {
            printf("  %s[%2d]%s %-50s %s(AppID: %ld, %s)%s\n",
                   col_cyan(), i + 1, col_reset(),
                   results.items[i].name,
                   col_dim(),
                   results.items[i].appid,
                   results.items[i].has_linux ? "Native Linux" : "Windows/Proton",
                   col_reset());
        }

        printf("\nSelect a game [1-%d, default 1, or 0 to cancel]: ", count);
        fflush(stdout);

        char input[64];
        if (fgets(input, sizeof(input), stdin)) {
            char *trimmed = str_trim(input);
            if (!*trimmed || strcmp(trimmed, "1") == 0) {
                chosen_appid = results.items[0].appid;
            } else {
                int choice = atoi(trimmed);
                if (choice == 0) {
                    printf("Cancelled.\n");
                    return 0;
                } else if (choice >= 1 && choice <= count) {
                    chosen_appid = results.items[choice - 1].appid;
                } else {
                    printf("Invalid selection. Defaulting to [1].\n");
                    chosen_appid = results.items[0].appid;
                }
            }
        } else {
            chosen_appid = results.items[0].appid;
        }
    }

    return run_check_for_appid(chosen_appid, specs, json_mode);
}

int main(int argc, char *argv[]) {
    bool json_mode = false;
    bool sysinfo_only = false;
    char target_arg[256] = "";

    /* Automatically disable ANSI colors if output is redirected */
    if (!isatty(STDOUT_FILENO)) {
        utils_set_color_enabled(false);
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--no-color") == 0) {
            utils_set_color_enabled(false);
        } else if (strcmp(argv[i], "--json") == 0) {
            json_mode = true;
            utils_set_color_enabled(false);
        } else if (strcmp(argv[i], "--sysinfo") == 0) {
            sysinfo_only = true;
        } else if (argv[i][0] != '-') {
            if (target_arg[0]) {
                strncat(target_arg, " ", sizeof(target_arg) - strlen(target_arg) - 1);
            }
            strncat(target_arg, argv[i], sizeof(target_arg) - strlen(target_arg) - 1);
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    /* Hardware Detection */
    SystemSpecs specs;
    if (detect_system_specs(&specs) != 0) {
        fprintf(stderr, "Failed to inspect Linux system hardware.\n");
        return 1;
    }

    if (sysinfo_only) {
        print_system_specs(&specs);
        return 0;
    }

    /* CLI argument provided directly */
    if (target_arg[0]) {
        if (is_numeric_str(target_arg)) {
            return run_check_for_appid(atol(target_arg), &specs, json_mode);
        } else {
            return run_search_and_check(target_arg, &specs, json_mode);
        }
    }

    /* Interactive Mode */
    printf("\n%s%s=== Steam Game Compatibility Checker for Linux ===%s\n", col_bold(), col_cyan(), col_reset());
    printf("Tests if any game on Steam can run on your specific Linux PC\n");
    printf("No Steam ownership or local game installation required.\n\n");

    print_system_specs(&specs);

    char input_buf[256];
    while (1) {
        printf("%sEnter game title or Steam AppID%s (or 'q' to quit): ", col_bold(), col_reset());
        fflush(stdout);

        if (!fgets(input_buf, sizeof(input_buf), stdin)) {
            break;
        }

        char *query = str_trim(input_buf);
        if (!*query) continue;
        if (strcmp(query, "q") == 0 || strcmp(query, "quit") == 0 || strcmp(query, "exit") == 0) {
            printf("Goodbye!\n");
            break;
        }

        if (is_numeric_str(query)) {
            run_check_for_appid(atol(query), &specs, false);
        } else {
            run_search_and_check(query, &specs, false);
        }

        printf("\n");
    }

    return 0;
}
