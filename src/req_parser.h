#ifndef REQ_PARSER_H
#define REQ_PARSER_H

#include <stdbool.h>

#define REQ_STR_LEN 512

typedef struct {
    /* Extracted readable strings (HTML stripped) */
    char os_desc[REQ_STR_LEN];
    char cpu_desc[REQ_STR_LEN];
    char ram_desc[REQ_STR_LEN];
    char gpu_desc[REQ_STR_LEN];
    char storage_desc[REQ_STR_LEN];
    char directx_desc[REQ_STR_LEN];
    char notes_desc[REQ_STR_LEN];

    /* Structured parsed criteria */
    double ram_gb;
    double storage_gb;
    double vram_gb;
    double cpu_ghz;
    int cpu_cores;
    bool requires_64bit;
    bool requires_dedicated_gpu;
    int directx_version;
    bool requires_vulkan;
} RequirementSpec;

typedef struct {
    char game_name[REQ_STR_LEN];
    long appid;
    char short_desc[REQ_STR_LEN * 2];
    bool has_native_linux;
    bool using_native_reqs;

    bool has_minimum;
    RequirementSpec minimum;

    bool has_recommended;
    RequirementSpec recommended;
} GameRequirements;

int parse_requirement_html(const char *html, RequirementSpec *spec);

#endif /* REQ_PARSER_H */
