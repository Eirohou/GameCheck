#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "checker.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void safe_copy(char *dest, size_t dest_size, const char *src) {
    if (!dest || dest_size == 0) return;
    if (!src) {
        dest[0] = '\0';
        return;
    }
    snprintf(dest, dest_size, "%s", src);
}

static const char *status_badge(CheckStatus st) {
    switch (st) {
        case STATUS_PASS:
            return utils_is_color_enabled() ? "\033[32;1m[ PASS ]\033[0m" : "[ PASS ]";
        case STATUS_WARN:
            return utils_is_color_enabled() ? "\033[33;1m[ WARN ]\033[0m" : "[ WARN ]";
        case STATUS_FAIL:
            return utils_is_color_enabled() ? "\033[31;1m[ FAIL ]\033[0m" : "[ FAIL ]";
        case STATUS_UNKNOWN:
        default:
            return utils_is_color_enabled() ? "\033[36;1m[ ?? ]\033[0m"   : "[  ?   ]";
    }
}

static const char *status_str(CheckStatus st) {
    switch (st) {
        case STATUS_PASS: return "PASS";
        case STATUS_WARN: return "WARN";
        case STATUS_FAIL: return "FAIL";
        case STATUS_UNKNOWN:
        default: return "UNKNOWN";
    }
}

static void evaluate_spec(const SystemSpecs *specs, const RequirementSpec *req,
                          ComponentCheck *ram, ComponentCheck *storage,
                          ComponentCheck *cpu, ComponentCheck *gpu,
                          ComponentCheck *os) {
    /* --- 1. RAM Check --- */
    if (req->ram_gb > 0.0) {
        snprintf(ram->req_text, sizeof(ram->req_text), "%.1f GB", req->ram_gb);
        snprintf(ram->sys_text, sizeof(ram->sys_text), "%.1f GB (Physical: ~%.0f GB)",
                 specs->ram_total_gb, specs->ram_norm_gb);

        if (specs->ram_norm_gb >= req->ram_gb || specs->ram_total_gb >= req->ram_gb * 0.90) {
            ram->status = STATUS_PASS;
            snprintf(ram->detail, sizeof(ram->detail), "System meets RAM requirements");
        } else if (specs->ram_total_gb >= req->ram_gb * 0.70) {
            ram->status = STATUS_WARN;
            snprintf(ram->detail, sizeof(ram->detail), "Slightly low RAM (%.1f GB vs %.1f GB required)",
                     specs->ram_total_gb, req->ram_gb);
        } else {
            ram->status = STATUS_FAIL;
            snprintf(ram->detail, sizeof(ram->detail), "Insufficient RAM (%.1f GB vs %.1f GB required)",
                     specs->ram_total_gb, req->ram_gb);
        }
    } else {
        safe_copy(ram->req_text, sizeof(ram->req_text), req->ram_desc[0] ? req->ram_desc : "Unspecified");
        snprintf(ram->sys_text, sizeof(ram->sys_text), "%.1f GB", specs->ram_total_gb);
        ram->status = STATUS_PASS;
        snprintf(ram->detail, sizeof(ram->detail), "Standard RAM detected");
    }

    /* --- 2. Storage Check --- */
    if (req->storage_gb > 0.0) {
        snprintf(storage->req_text, sizeof(storage->req_text), "%.1f GB", req->storage_gb);
        snprintf(storage->sys_text, sizeof(storage->sys_text), "%.1f GB Free", specs->storage_avail_gb);

        if (specs->storage_avail_gb >= req->storage_gb) {
            storage->status = STATUS_PASS;
            snprintf(storage->detail, sizeof(storage->detail), "Plenty of free storage space");
        } else if (specs->storage_avail_gb >= req->storage_gb * 0.85) {
            storage->status = STATUS_WARN;
            snprintf(storage->detail, sizeof(storage->detail), "Storage is very tight (%.1f GB free)", specs->storage_avail_gb);
        } else {
            storage->status = STATUS_FAIL;
            snprintf(storage->detail, sizeof(storage->detail), "Not enough disk space (need %.1f GB, have %.1f GB)",
                     req->storage_gb, specs->storage_avail_gb);
        }
    } else {
        safe_copy(storage->req_text, sizeof(storage->req_text), req->storage_desc[0] ? req->storage_desc : "Unspecified");
        snprintf(storage->sys_text, sizeof(storage->sys_text), "%.1f GB Free", specs->storage_avail_gb);
        storage->status = STATUS_PASS;
        snprintf(storage->detail, sizeof(storage->detail), "Sufficient storage");
    }

    /* --- 3. CPU Check --- */
    safe_copy(cpu->req_text, sizeof(cpu->req_text), req->cpu_desc[0] ? req->cpu_desc : "Any modern CPU");
    if (specs->cpu_ghz > 0.0) {
        snprintf(cpu->sys_text, sizeof(cpu->sys_text), "%d threads @ ~%.2f GHz", specs->cpu_threads, specs->cpu_ghz);
    } else {
        snprintf(cpu->sys_text, sizeof(cpu->sys_text), "%d threads", specs->cpu_threads);
    }

    if (req->cpu_cores > 0) {
        if (specs->cpu_threads >= req->cpu_cores) {
            if (req->cpu_ghz > 0.0 && specs->cpu_ghz > 0.0 && specs->cpu_ghz < req->cpu_ghz * 0.80) {
                cpu->status = STATUS_WARN;
                snprintf(cpu->detail, sizeof(cpu->detail), "Core count sufficient, clock speed slightly below spec");
            } else {
                cpu->status = STATUS_PASS;
                snprintf(cpu->detail, sizeof(cpu->detail), "CPU cores and threads exceed requirement");
            }
        } else if (specs->cpu_threads >= req->cpu_cores - 2) {
            cpu->status = STATUS_WARN;
            snprintf(cpu->detail, sizeof(cpu->detail), "CPU thread count is slightly below requirement");
        } else {
            cpu->status = STATUS_FAIL;
            snprintf(cpu->detail, sizeof(cpu->detail), "CPU significantly below required core/thread count");
        }
    } else if (req->cpu_ghz > 0.0) {
        if (specs->cpu_ghz >= req->cpu_ghz * 0.85 || specs->cpu_threads >= 4) {
            cpu->status = STATUS_PASS;
            snprintf(cpu->detail, sizeof(cpu->detail), "Modern multi-core CPU meets clock requirement");
        } else {
            cpu->status = STATUS_WARN;
            snprintf(cpu->detail, sizeof(cpu->detail), "CPU clock speed may be below target");
        }
    } else {
        cpu->status = STATUS_PASS;
        snprintf(cpu->detail, sizeof(cpu->detail), "Compatible x86_64 CPU");
    }

    /* --- 4. GPU Check --- */
    const GpuInfo *gpu_info = &specs->gpus[specs->primary_gpu_idx];
    safe_copy(gpu->req_text, sizeof(gpu->req_text), req->gpu_desc[0] ? req->gpu_desc : "DirectX/Vulkan GPU");
    snprintf(gpu->sys_text, sizeof(gpu->sys_text), "%s (%s)",
             gpu_info->model[0] ? gpu_info->model : "Linux Display Controller",
             gpu_info->is_integrated ? "Integrated" : "Dedicated");

    bool is_demanding_gpu = req->requires_dedicated_gpu || (req->vram_gb >= 2.0);
    bool is_legacy_card = str_contains_ci(req->gpu_desc, "8600") ||
                          str_contains_ci(req->gpu_desc, "9600") ||
                          str_contains_ci(req->gpu_desc, "hd2600") ||
                          str_contains_ci(req->gpu_desc, "x800") ||
                          str_contains_ci(req->gpu_desc, "7600") ||
                          str_contains_ci(req->gpu_desc, "hd 2000") ||
                          str_contains_ci(req->gpu_desc, "128 mb") ||
                          str_contains_ci(req->gpu_desc, "256 mb") ||
                          str_contains_ci(req->gpu_desc, "pixel shader 2") ||
                          str_contains_ci(req->gpu_desc, "shader model 2");

    if (gpu_info->is_integrated) {
        if (is_legacy_card) {
            gpu->status = STATUS_PASS;
            snprintf(gpu->detail, sizeof(gpu->detail), "Modern integrated graphics easily surpass legacy GPU spec");
        } else if (is_demanding_gpu) {
            if (req->vram_gb >= 4.0 || str_contains_ci(req->gpu_desc, "gtx 1060") ||
                str_contains_ci(req->gpu_desc, "rx 580") || str_contains_ci(req->gpu_desc, "rtx")) {
                gpu->status = STATUS_FAIL;
                snprintf(gpu->detail, sizeof(gpu->detail), "Demands dedicated GPU (e.g. GTX 1060/RX 580). Integrated GPU will struggle severely");
            } else {
                gpu->status = STATUS_WARN;
                snprintf(gpu->detail, sizeof(gpu->detail), "Game targets dedicated GPU; integrated graphics may yield low FPS");
            }
        } else {
            gpu->status = STATUS_PASS;
            snprintf(gpu->detail, sizeof(gpu->detail), "Integrated graphics sufficient for entry graphics spec");
        }
    } else {
        /* User has a dedicated GPU */
        if (req->vram_gb > 0.0 && gpu_info->vram_gb > 0.0) {
            if (gpu_info->vram_gb >= req->vram_gb) {
                gpu->status = STATUS_PASS;
                snprintf(gpu->detail, sizeof(gpu->detail), "Dedicated GPU VRAM (%.1f GB) satisfies requirement", gpu_info->vram_gb);
            } else {
                gpu->status = STATUS_WARN;
                snprintf(gpu->detail, sizeof(gpu->detail), "Dedicated GPU VRAM (%.1f GB) lower than suggested (%.1f GB)",
                         gpu_info->vram_gb, req->vram_gb);
            }
        } else {
            gpu->status = STATUS_PASS;
            snprintf(gpu->detail, sizeof(gpu->detail), "Dedicated gaming GPU detected");
        }
    }

    /* --- 5. OS & Architecture Check --- */
    safe_copy(os->req_text, sizeof(os->req_text), req->os_desc[0] ? req->os_desc : (req->requires_64bit ? "64-bit OS" : "Any OS"));
    snprintf(os->sys_text, sizeof(os->sys_text), "%.120s (%.60s)", specs->os_name, specs->arch);

    if (req->requires_64bit && !specs->is_64bit) {
        os->status = STATUS_FAIL;
        snprintf(os->detail, sizeof(os->detail), "Requires 64-bit OS/processor");
    } else {
        os->status = STATUS_PASS;
        snprintf(os->detail, sizeof(os->detail), "64-bit Linux kernel supported");
    }
}

void evaluate_compatibility(const SystemSpecs *specs, const GameRequirements *game,
                            EvaluationReport *min_rep, EvaluationReport *rec_rep) {
    if (!specs || !game) return;

    if (min_rep) {
        memset(min_rep, 0, sizeof(EvaluationReport));
        if (game->has_minimum) {
            evaluate_spec(specs, &game->minimum,
                          &min_rep->ram, &min_rep->storage,
                          &min_rep->cpu, &min_rep->gpu,
                          &min_rep->os);

            bool has_fail = (min_rep->ram.status == STATUS_FAIL ||
                             min_rep->storage.status == STATUS_FAIL ||
                             min_rep->os.status == STATUS_FAIL ||
                             min_rep->gpu.status == STATUS_FAIL ||
                             min_rep->cpu.status == STATUS_FAIL);

            bool has_warn = (min_rep->ram.status == STATUS_WARN ||
                             min_rep->storage.status == STATUS_WARN ||
                             min_rep->gpu.status == STATUS_WARN ||
                             min_rep->cpu.status == STATUS_WARN);

            if (has_fail) {
                min_rep->verdict = TIER_BELOW_MINIMUM;
                snprintf(min_rep->verdict_headline, sizeof(min_rep->verdict_headline), "BELOW MINIMUM REQUIREMENTS");
                snprintf(min_rep->verdict_details, sizeof(min_rep->verdict_details),
                         "Your PC falls short on one or more critical requirements to run this game smoothly.");
            } else if (has_warn) {
                min_rep->verdict = TIER_BORDERLINE;
                snprintf(min_rep->verdict_headline, sizeof(min_rep->verdict_headline), "BORDERLINE / PLAYABLE ON LOW");
                snprintf(min_rep->verdict_details, sizeof(min_rep->verdict_details),
                         "Your PC can run the game, but hardware limitations (e.g. integrated GPU or tight RAM) may require low settings or lower resolution.");
            } else {
                min_rep->verdict = TIER_MEETS_MINIMUM;
                snprintf(min_rep->verdict_headline, sizeof(min_rep->verdict_headline), "CAN RUN (Meets Minimum Requirements)");
                snprintf(min_rep->verdict_details, sizeof(min_rep->verdict_details),
                         "Your PC comfortably satisfies all minimum system specifications.");
            }

            /* Contextual Linux recommendations */
            char rec_buf[512] = "";
            if (!game->has_native_linux) {
                strncat(rec_buf, "• Windows-only title: Runs on Linux via Steam Proton (DXVK/VKD3D).\n", sizeof(rec_buf) - strlen(rec_buf) - 1);
            }
            if (min_rep->gpu.status == STATUS_FAIL || min_rep->gpu.status == STATUS_WARN) {
                strncat(rec_buf, "• Consider using FSR (FidelityFX Super Resolution) or 720p resolution to boost frame rates.\n", sizeof(rec_buf) - strlen(rec_buf) - 1);
            }
            if (min_rep->storage.status == STATUS_FAIL || min_rep->storage.status == STATUS_WARN) {
                strncat(rec_buf, "• Free up disk space before initiating download on Steam.\n", sizeof(rec_buf) - strlen(rec_buf) - 1);
            }
            safe_copy(min_rep->recommendations, sizeof(min_rep->recommendations), rec_buf);
        } else {
            min_rep->verdict = TIER_NO_REQUIREMENTS;
            snprintf(min_rep->verdict_headline, sizeof(min_rep->verdict_headline), "NO MINIMUM REQUIREMENTS LISTED");
        }
    }

    if (rec_rep) {
        memset(rec_rep, 0, sizeof(EvaluationReport));
        if (game->has_recommended) {
            evaluate_spec(specs, &game->recommended,
                          &rec_rep->ram, &rec_rep->storage,
                          &rec_rep->cpu, &rec_rep->gpu,
                          &rec_rep->os);

            bool has_fail = (rec_rep->ram.status == STATUS_FAIL ||
                             rec_rep->storage.status == STATUS_FAIL ||
                             rec_rep->os.status == STATUS_FAIL ||
                             rec_rep->gpu.status == STATUS_FAIL ||
                             rec_rep->cpu.status == STATUS_FAIL);

            bool has_warn = (rec_rep->ram.status == STATUS_WARN ||
                             rec_rep->storage.status == STATUS_WARN ||
                             rec_rep->gpu.status == STATUS_WARN ||
                             rec_rep->cpu.status == STATUS_WARN);

            if (!has_fail && !has_warn) {
                rec_rep->verdict = TIER_MEETS_RECOMMENDED;
                snprintf(rec_rep->verdict_headline, sizeof(rec_rep->verdict_headline), "OPTIMAL (Meets Recommended Specifications)");
                snprintf(rec_rep->verdict_details, sizeof(rec_rep->verdict_details),
                         "Your PC matches or exceeds the recommended hardware specs for a smooth high-fidelity gaming experience.");
            } else {
                rec_rep->verdict = TIER_MEETS_MINIMUM;
                snprintf(rec_rep->verdict_headline, sizeof(rec_rep->verdict_headline), "BELOW RECOMMENDED (Playable at Medium/Low)");
                snprintf(rec_rep->verdict_details, sizeof(rec_rep->verdict_details),
                         "Your PC meets minimum requirements but does not reach the recommended target.");
            }
        } else {
            rec_rep->verdict = TIER_NO_REQUIREMENTS;
            snprintf(rec_rep->verdict_headline, sizeof(rec_rep->verdict_headline), "NO RECOMMENDED SPECS LISTED");
        }
    }
}


static void print_table_row(const char *component, const char *req, const char *sys, CheckStatus st) {
    char req_buf[32];
    char sys_buf[32];

    snprintf(req_buf, sizeof(req_buf), "%-30.30s", req);
    snprintf(sys_buf, sizeof(sys_buf), "%-30.30s", sys);

    printf("  %-11s %s %s  %s\n", component, req_buf, sys_buf, status_badge(st));
}

void print_compatibility_report(const SystemSpecs *specs, const GameRequirements *game,
                                const EvaluationReport *min_rep, const EvaluationReport *rec_rep) {
    (void)specs;
    if (!game) return;

    printf("\n%s================================================================================%s\n", col_cyan(), col_reset());
    printf("  %s%sGAME COMPATIBILITY CHECK REPORT%s\n", col_bold(), col_white(), col_reset());
    printf("%s================================================================================%s\n", col_cyan(), col_reset());

    /* Game Summary */
    printf("  %sGame Title:%s     %s%s%s (Steam AppID: %ld)\n",
           col_bold(), col_reset(), col_bold(), game->game_name, col_reset(), game->appid);
    printf("  %sLinux Native:%s   %s%s%s\n",
           col_bold(), col_reset(),
           game->has_native_linux ? col_green() : col_yellow(),
           game->has_native_linux ? "YES (Native Linux Build)" : "NO (Runs via Steam Proton/Wine)",
           col_reset());
    printf("  %sTesting Basis:%s  %s\n",
           col_bold(), col_reset(),
           game->using_native_reqs ? "Steam Native Linux Requirements" : "Steam PC Requirements (Proton / DXVK)");

    if (game->short_desc[0]) {
        printf("  %sAbout:%s          %.90s%s\n", col_dim(), col_reset(), game->short_desc,
               strlen(game->short_desc) > 90 ? "..." : "");
    }
    printf("\n");

    /* Minimum Requirements Table */
    if (game->has_minimum && min_rep) {
        printf("%s--------------------------------------------------------------------------------%s\n", col_dim(), col_reset());
        printf("  %s[MINIMUM SYSTEM REQUIREMENTS CHECK]%s\n", col_bold(), col_reset());
        printf("  %-11s %-30s %-30s  %s\n", "Component", "Required Spec", "Your System Spec", "Status");
        printf("  ------------------------------------------------------------------------------\n");

        print_table_row("RAM", min_rep->ram.req_text, min_rep->ram.sys_text, min_rep->ram.status);
        print_table_row("Storage", min_rep->storage.req_text, min_rep->storage.sys_text, min_rep->storage.status);
        print_table_row("CPU", min_rep->cpu.req_text, min_rep->cpu.sys_text, min_rep->cpu.status);
        print_table_row("GPU", min_rep->gpu.req_text, min_rep->gpu.sys_text, min_rep->gpu.status);
        print_table_row("OS / Arch", min_rep->os.req_text, min_rep->os.sys_text, min_rep->os.status);
        printf("\n");
    }

    /* Recommended Requirements Table (if present) */
    if (game->has_recommended && rec_rep) {
        printf("%s--------------------------------------------------------------------------------%s\n", col_dim(), col_reset());
        printf("  %s[RECOMMENDED SYSTEM REQUIREMENTS CHECK]%s\n", col_bold(), col_reset());
        printf("  %-11s %-30s %-30s  %s\n", "Component", "Recommended Spec", "Your System Spec", "Status");
        printf("  ------------------------------------------------------------------------------\n");

        print_table_row("RAM", rec_rep->ram.req_text, rec_rep->ram.sys_text, rec_rep->ram.status);
        print_table_row("Storage", rec_rep->storage.req_text, rec_rep->storage.sys_text, rec_rep->storage.status);
        print_table_row("CPU", rec_rep->cpu.req_text, rec_rep->cpu.sys_text, rec_rep->cpu.status);
        print_table_row("GPU", rec_rep->gpu.req_text, rec_rep->gpu.sys_text, rec_rep->gpu.status);
        print_table_row("OS / Arch", rec_rep->os.req_text, rec_rep->os.sys_text, rec_rep->os.status);
        printf("\n");
    }

    /* Final Verdict Section */
    printf("%s================================================================================%s\n", col_cyan(), col_reset());
    if (min_rep && min_rep->verdict != TIER_NO_REQUIREMENTS) {
        const char *vcolor = col_green();
        if (min_rep->verdict == TIER_BELOW_MINIMUM) vcolor = col_red();
        else if (min_rep->verdict == TIER_BORDERLINE) vcolor = col_yellow();

        printf("  %sVERDICT: %s%s%s%s\n", col_bold(), vcolor, min_rep->verdict_headline, col_reset(), col_reset());
        printf("  %s\n", min_rep->verdict_details);

        if (min_rep->recommendations[0]) {
            printf("\n  %sLinux Gaming Notes & Suggestions:%s\n%s", col_bold(), col_reset(), min_rep->recommendations);
        }
    } else {
        printf("  %sVERDICT: System requirements not published by developer on Steam.%s\n", col_yellow(), col_reset());
    }
    printf("%s================================================================================%s\n\n", col_cyan(), col_reset());
}

void print_json_report(const SystemSpecs *specs, const GameRequirements *game,
                       const EvaluationReport *min_rep, const EvaluationReport *rec_rep) {
    printf("{\n");
    printf("  \"game\": {\n");
    printf("    \"appid\": %ld,\n", game->appid);
    printf("    \"name\": \"%s\",\n", game->game_name);
    printf("    \"native_linux\": %s,\n", game->has_native_linux ? "true" : "false");
    printf("    \"used_native_reqs\": %s\n", game->using_native_reqs ? "true" : "false");
    printf("  },\n");

    printf("  \"system\": {\n");
    printf("    \"os\": \"%s\",\n", specs->os_name);
    printf("    \"kernel\": \"%s\",\n", specs->kernel);
    printf("    \"arch\": \"%s\",\n", specs->arch);
    printf("    \"cpu\": \"%s\",\n", specs->cpu_model);
    printf("    \"cpu_threads\": %d,\n", specs->cpu_threads);
    printf("    \"ram_total_gb\": %.2f,\n", specs->ram_total_gb);
    printf("    \"storage_avail_gb\": %.2f,\n", specs->storage_avail_gb);
    if (specs->gpu_count > 0) {
        printf("    \"gpu\": \"%s\",\n", specs->gpus[specs->primary_gpu_idx].model);
        printf("    \"gpu_integrated\": %s\n", specs->gpus[specs->primary_gpu_idx].is_integrated ? "true" : "false");
    } else {
        printf("    \"gpu\": \"Unknown\",\n");
        printf("    \"gpu_integrated\": false\n");
    }
    printf("  },\n");

    if (min_rep && game->has_minimum) {
        printf("  \"minimum_check\": {\n");
        printf("    \"verdict\": \"%s\",\n", min_rep->verdict_headline);
        printf("    \"ram\": \"%s\",\n", status_str(min_rep->ram.status));
        printf("    \"storage\": \"%s\",\n", status_str(min_rep->storage.status));
        printf("    \"cpu\": \"%s\",\n", status_str(min_rep->cpu.status));
        printf("    \"gpu\": \"%s\",\n", status_str(min_rep->gpu.status));
        printf("    \"os\": \"%s\"\n", status_str(min_rep->os.status));
        printf("  }%s\n", (rec_rep && game->has_recommended) ? "," : "");
    }

    if (rec_rep && game->has_recommended) {
        printf("  \"recommended_check\": {\n");
        printf("    \"verdict\": \"%s\",\n", rec_rep->verdict_headline);
        printf("    \"ram\": \"%s\",\n", status_str(rec_rep->ram.status));
        printf("    \"storage\": \"%s\",\n", status_str(rec_rep->storage.status));
        printf("    \"cpu\": \"%s\",\n", status_str(rec_rep->cpu.status));
        printf("    \"gpu\": \"%s\",\n", status_str(rec_rep->gpu.status));
        printf("    \"os\": \"%s\"\n", status_str(rec_rep->os.status));
        printf("  }\n");
    }

    printf("}\n");
}
