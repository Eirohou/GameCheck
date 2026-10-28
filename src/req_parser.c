#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "req_parser.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

int parse_requirement_html(const char *html, RequirementSpec *spec) {
    if (!html || !spec) return -1;
    memset(spec, 0, sizeof(RequirementSpec));

    /* Allocate buffer for clean text */
    size_t len = strlen(html);
    char *clean = malloc(len * 2 + 1024);
    if (!clean) return -1;

    strip_html(html, clean, len * 2 + 1024);

    /* Split clean text line by line */
    char *saveptr = NULL;
    char *line = strtok_r(clean, "\n", &saveptr);

    while (line) {
        char *trimmed = str_trim(line);
        if (*trimmed) {
            /* Check for 64-bit requirement anywhere in text */
            if (str_contains_ci(trimmed, "64-bit") || str_contains_ci(trimmed, "64 bit") || str_contains_ci(trimmed, "x64")) {
                spec->requires_64bit = true;
            }

            /* Check for Vulkan anywhere in text */
            if (str_contains_ci(trimmed, "vulkan")) {
                spec->requires_vulkan = true;
            }

            /* Category line identification */
            if (str_starts_with_ci(trimmed, "OS:") || str_starts_with_ci(trimmed, "OS *:") ||
                str_starts_with_ci(trimmed, "Operating System:")) {
                char *colon = strchr(trimmed, ':');
                if (colon) {
                    strncpy(spec->os_desc, str_trim(colon + 1), sizeof(spec->os_desc) - 1);
                }
            } else if (str_starts_with_ci(trimmed, "Processor:") || str_starts_with_ci(trimmed, "CPU:")) {
                char *colon = strchr(trimmed, ':');
                if (colon) {
                    strncpy(spec->cpu_desc, str_trim(colon + 1), sizeof(spec->cpu_desc) - 1);
                    spec->cpu_ghz = parse_cpu_ghz(spec->cpu_desc);
                    spec->cpu_cores = parse_cpu_cores(spec->cpu_desc);
                }
            } else if (str_starts_with_ci(trimmed, "Memory:") || str_starts_with_ci(trimmed, "RAM:")) {
                char *colon = strchr(trimmed, ':');
                if (colon) {
                    strncpy(spec->ram_desc, str_trim(colon + 1), sizeof(spec->ram_desc) - 1);
                    spec->ram_gb = parse_size_in_gb(spec->ram_desc);
                }
            } else if (str_starts_with_ci(trimmed, "Graphics:") || str_starts_with_ci(trimmed, "Video Card:") ||
                       str_starts_with_ci(trimmed, "Video:") || str_starts_with_ci(trimmed, "GPU:")) {
                char *colon = strchr(trimmed, ':');
                if (colon) {
                    strncpy(spec->gpu_desc, str_trim(colon + 1), sizeof(spec->gpu_desc) - 1);
                    spec->vram_gb = parse_size_in_gb(spec->gpu_desc);

                    /* Check if requires dedicated GPU */
                    if (str_contains_ci(spec->gpu_desc, "gtx") ||
                        str_contains_ci(spec->gpu_desc, "rtx") ||
                        str_contains_ci(spec->gpu_desc, "radeon rx") ||
                        str_contains_ci(spec->gpu_desc, "arc a") ||
                        str_contains_ci(spec->gpu_desc, "arc b") ||
                        str_contains_ci(spec->gpu_desc, "vega 56") ||
                        str_contains_ci(spec->gpu_desc, "vega 64") ||
                        str_contains_ci(spec->gpu_desc, "discrete") ||
                        str_contains_ci(spec->gpu_desc, "dedicated") ||
                        spec->vram_gb >= 2.0) {
                        spec->requires_dedicated_gpu = true;
                    }
                }
            } else if (str_starts_with_ci(trimmed, "Storage:") || str_starts_with_ci(trimmed, "Hard Disk Space:") ||
                       str_starts_with_ci(trimmed, "Hard Drive Space:") || str_starts_with_ci(trimmed, "Hard Drive:") ||
                       str_starts_with_ci(trimmed, "Disk Space:")) {
                char *colon = strchr(trimmed, ':');
                if (colon) {
                    strncpy(spec->storage_desc, str_trim(colon + 1), sizeof(spec->storage_desc) - 1);
                    spec->storage_gb = parse_size_in_gb(spec->storage_desc);
                }
            } else if (str_starts_with_ci(trimmed, "DirectX:") || str_starts_with_ci(trimmed, "DirectX®:")) {
                char *colon = strchr(trimmed, ':');
                if (colon) {
                    strncpy(spec->directx_desc, str_trim(colon + 1), sizeof(spec->directx_desc) - 1);
                    if (str_contains_ci(spec->directx_desc, "12")) spec->directx_version = 12;
                    else if (str_contains_ci(spec->directx_desc, "11")) spec->directx_version = 11;
                    else if (str_contains_ci(spec->directx_desc, "10")) spec->directx_version = 10;
                    else if (str_contains_ci(spec->directx_desc, "9")) spec->directx_version = 9;
                }
            } else if (str_starts_with_ci(trimmed, "Additional Notes:")) {
                char *colon = strchr(trimmed, ':');
                if (colon) {
                    strncpy(spec->notes_desc, str_trim(colon + 1), sizeof(spec->notes_desc) - 1);
                    /* Notes might mention VRAM or 64-bit */
                    if (str_contains_ci(spec->notes_desc, "vram") && spec->vram_gb == 0.0) {
                        spec->vram_gb = parse_size_in_gb(spec->notes_desc);
                        if (spec->vram_gb >= 2.0) spec->requires_dedicated_gpu = true;
                    }
                }
            }
        }
        line = strtok_r(NULL, "\n", &saveptr);
    }

    free(clean);
    return 0;
}
