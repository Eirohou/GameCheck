#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "sysinfo.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>

static void safe_copy(char *dest, size_t dest_size, const char *src) {
    if (!dest || dest_size == 0) return;
    if (!src) {
        dest[0] = '\0';
        return;
    }
    snprintf(dest, dest_size, "%s", src);
}

static void parse_os_release(char *dest, size_t dest_size) {
    FILE *f = fopen("/etc/os-release", "r");
    if (!f) {
        snprintf(dest, dest_size, "Linux");
        return;
    }

    char line[512];
    char name[256] = "";
    char pretty[256] = "";

    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "PRETTY_NAME=", 12) == 0) {
            char *val = line + 12;
            if (*val == '"') val++;
            safe_copy(pretty, sizeof(pretty), val);
            char *end = strchr(pretty, '"');
            if (end) *end = '\0';
            end = strchr(pretty, '\n');
            if (end) *end = '\0';
        } else if (strncmp(line, "NAME=", 5) == 0) {
            char *val = line + 5;
            if (*val == '"') val++;
            safe_copy(name, sizeof(name), val);
            char *end = strchr(name, '"');
            if (end) *end = '\0';
            end = strchr(name, '\n');
            if (end) *end = '\0';
        }
    }
    fclose(f);

    if (pretty[0]) {
        safe_copy(dest, dest_size, pretty);
    } else if (name[0]) {
        safe_copy(dest, dest_size, name);
    } else {
        safe_copy(dest, dest_size, "Linux");
    }
}

static void detect_cpu(SystemSpecs *specs) {
    specs->cpu_model[0] = '\0';
    specs->cpu_cores = 0;
    specs->cpu_threads = (int)sysconf(_SC_NPROCESSORS_ONLN);
    specs->cpu_ghz = 0.0;

    FILE *f = fopen("/proc/cpuinfo", "r");
    if (f) {
        char line[512];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "model name", 10) == 0 || strncmp(line, "Hardware", 8) == 0) {
                char *colon = strchr(line, ':');
                if (colon && !specs->cpu_model[0]) {
                    safe_copy(specs->cpu_model, sizeof(specs->cpu_model), str_trim(colon + 1));
                }
            } else if (strncmp(line, "cpu cores", 9) == 0) {
                char *colon = strchr(line, ':');
                if (colon && specs->cpu_cores == 0) {
                    specs->cpu_cores = atoi(str_trim(colon + 1));
                }
            } else if (strncmp(line, "cpu MHz", 7) == 0 && specs->cpu_ghz == 0.0) {
                char *colon = strchr(line, ':');
                if (colon) {
                    double mhz = atof(str_trim(colon + 1));
                    if (mhz > 0.0) specs->cpu_ghz = mhz / 1000.0;
                }
            }
        }
        fclose(f);
    }

    if (!specs->cpu_model[0]) {
        snprintf(specs->cpu_model, sizeof(specs->cpu_model), "Unknown x86 CPU");
    }

    /* If cores count wasn't found in cpuinfo, default to threads / 2 or 1 */
    if (specs->cpu_cores == 0) {
        specs->cpu_cores = (specs->cpu_threads > 1) ? specs->cpu_threads / 2 : 1;
    }

    /* Try sysfs max frequency for a more accurate gaming clock speed */
    FILE *ffreq = fopen("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq", "r");
    if (!ffreq) {
        ffreq = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq", "r");
    }
    if (ffreq) {
        long khz = 0;
        if (fscanf(ffreq, "%ld", &khz) == 1 && khz > 0) {
            specs->cpu_ghz = (double)khz / 1000000.0;
        }
        fclose(ffreq);
    }

    /* If still 0, parse from model name string (e.g. "@ 3.00GHz") */
    if (specs->cpu_ghz == 0.0) {
        char *at = strchr(specs->cpu_model, '@');
        if (at) {
            specs->cpu_ghz = parse_cpu_ghz(at);
        }
    }
}

static void detect_ram(SystemSpecs *specs) {
    specs->ram_total_gb = 0.0;
    specs->ram_avail_gb = 0.0;
    specs->ram_norm_gb = 0.0;

    FILE *f = fopen("/proc/meminfo", "r");
    if (f) {
        char line[256];
        long total_kb = 0;
        long avail_kb = 0;

        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "MemTotal:", 9) == 0) {
                sscanf(line + 9, "%ld", &total_kb);
            } else if (strncmp(line, "MemAvailable:", 13) == 0) {
                sscanf(line + 13, "%ld", &avail_kb);
            }
        }
        fclose(f);

        specs->ram_total_gb = (double)total_kb / (1024.0 * 1024.0);
        specs->ram_avail_gb = (double)avail_kb / (1024.0 * 1024.0);
    } else {
        struct sysinfo si;
        if (sysinfo(&si) == 0) {
            specs->ram_total_gb = (double)(si.totalram * si.mem_unit) / (1024.0 * 1024.0 * 1024.0);
            specs->ram_avail_gb = (double)(si.freeram * si.mem_unit) / (1024.0 * 1024.0 * 1024.0);
        }
    }

    /* Normalization heuristic: hardware reserved RAM often leaves e.g. 7.4GB on an 8GB machine */
    double t = specs->ram_total_gb;
    if (t >= 3.0 && t <= 4.5)        specs->ram_norm_gb = 4.0;
    else if (t > 4.5 && t <= 8.5)    specs->ram_norm_gb = 8.0;
    else if (t > 8.5 && t <= 12.5)   specs->ram_norm_gb = 12.0;
    else if (t > 12.5 && t <= 17.0)  specs->ram_norm_gb = 16.0;
    else if (t > 17.0 && t <= 25.0)  specs->ram_norm_gb = 24.0;
    else if (t > 25.0 && t <= 34.0)  specs->ram_norm_gb = 32.0;
    else if (t > 34.0 && t <= 68.0)  specs->ram_norm_gb = 64.0;
    else                             specs->ram_norm_gb = t;
}

static void detect_storage(SystemSpecs *specs) {
    const char *path = getenv("HOME");
    if (!path || access(path, W_OK) != 0) {
        path = "/";
    }
    safe_copy(specs->storage_path, sizeof(specs->storage_path), path);

    struct statvfs sv;
    if (statvfs(path, &sv) == 0) {
        specs->storage_avail_gb = (double)sv.f_bavail * sv.f_frsize / (1024.0 * 1024.0 * 1024.0);
        specs->storage_total_gb = (double)sv.f_blocks * sv.f_frsize / (1024.0 * 1024.0 * 1024.0);
    } else {
        specs->storage_avail_gb = 0.0;
        specs->storage_total_gb = 0.0;
    }
}

static void detect_gpus(SystemSpecs *specs) {
    specs->gpu_count = 0;
    specs->primary_gpu_idx = 0;

    DIR *dir = opendir("/sys/bus/pci/devices");
    if (!dir) return;

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL && specs->gpu_count < MAX_GPUS) {
        if (ent->d_name[0] == '.') continue;

        char class_path[512];
        snprintf(class_path, sizeof(class_path), "/sys/bus/pci/devices/%s/class", ent->d_name);
        FILE *fc = fopen(class_path, "r");
        if (!fc) continue;

        char class_str[32] = "";
        if (fgets(class_str, sizeof(class_str), fc)) {
            /* 0x0300xx = VGA, 0x0302xx = 3D controller, 0x0380xx = Display controller */
            if (strncmp(class_str, "0x0300", 6) == 0 ||
                strncmp(class_str, "0x0302", 6) == 0 ||
                strncmp(class_str, "0x0380", 6) == 0) {

                GpuInfo *gpu = &specs->gpus[specs->gpu_count];
                memset(gpu, 0, sizeof(GpuInfo));
                safe_copy(gpu->slot, sizeof(gpu->slot), ent->d_name);

                /* Read vendor & device IDs */
                char vendor_path[512], dev_path[512];
                snprintf(vendor_path, sizeof(vendor_path), "/sys/bus/pci/devices/%s/vendor", ent->d_name);
                snprintf(dev_path, sizeof(dev_path), "/sys/bus/pci/devices/%s/device", ent->d_name);

                FILE *fv = fopen(vendor_path, "r");
                FILE *fd = fopen(dev_path, "r");
                char vendor_hex[32] = "", dev_hex[32] = "";
                if (fv) { if (fgets(vendor_hex, sizeof(vendor_hex), fv)) str_trim(vendor_hex); fclose(fv); }
                if (fd) { if (fgets(dev_hex, sizeof(dev_hex), fd)) str_trim(dev_hex); fclose(fd); }

                const char *v_clean = vendor_hex[0] ? (strncmp(vendor_hex, "0x", 2) == 0 ? vendor_hex + 2 : vendor_hex) : "0000";
                const char *d_clean = dev_hex[0] ? (strncmp(dev_hex, "0x", 2) == 0 ? dev_hex + 2 : dev_hex) : "0000";
                snprintf(gpu->pci_id, sizeof(gpu->pci_id), "%s:%s", v_clean, d_clean);

                /* Vendor identification */
                if (strcasecmp(vendor_hex, "0x8086") == 0) {
                    safe_copy(gpu->vendor, sizeof(gpu->vendor), "Intel");
                } else if (strcasecmp(vendor_hex, "0x10de") == 0) {
                    safe_copy(gpu->vendor, sizeof(gpu->vendor), "NVIDIA");
                } else if (strcasecmp(vendor_hex, "0x1002") == 0) {
                    safe_copy(gpu->vendor, sizeof(gpu->vendor), "AMD");
                } else {
                    safe_copy(gpu->vendor, sizeof(gpu->vendor), "Other");
                }

                /* Driver identification */
                char driver_path[512];
                snprintf(driver_path, sizeof(driver_path), "/sys/bus/pci/devices/%s/driver", ent->d_name);
                char link_target[512];
                ssize_t len = readlink(driver_path, link_target, sizeof(link_target) - 1);
                if (len > 0) {
                    link_target[len] = '\0';
                    char *slash = strrchr(link_target, '/');
                    safe_copy(gpu->driver, sizeof(gpu->driver), slash ? slash + 1 : link_target);
                } else {
                    safe_copy(gpu->driver, sizeof(gpu->driver), "unknown");
                }

                /* Query friendly name via lspci if available */
                char lspci_cmd[512];
                snprintf(lspci_cmd, sizeof(lspci_cmd), "lspci -s %s 2>/dev/null", ent->d_name);
                FILE *lp = popen(lspci_cmd, "r");
                if (lp) {
                    char buf[512];
                    if (fgets(buf, sizeof(buf), lp)) {
                        char *nl = strchr(buf, '\n');
                        if (nl) *nl = '\0';
                        char *colon = strchr(buf, ':');
                        if (colon) {
                            /* Skip device class description if second colon exists */
                            char *second_colon = strchr(colon + 1, ':');
                            char *src_name = second_colon ? second_colon + 1 : colon + 1;
                            safe_copy(gpu->model, sizeof(gpu->model), str_trim(src_name));
                        }
                    }
                    pclose(lp);
                }

                if (!gpu->model[0]) {
                    char tmp_v[64], tmp_id[32];
                    safe_copy(tmp_v, sizeof(tmp_v), gpu->vendor);
                    safe_copy(tmp_id, sizeof(tmp_id), gpu->pci_id);
                    snprintf(gpu->model, sizeof(gpu->model), "%s Graphics [%s]", tmp_v, tmp_id);
                }

                /* Determine integrated vs dedicated */
                if (strcmp(gpu->vendor, "Intel") == 0) {
                    if (str_contains_ci(gpu->model, "Arc")) {
                        gpu->is_integrated = false;
                    } else {
                        gpu->is_integrated = true;
                    }
                } else if (strcmp(gpu->vendor, "NVIDIA") == 0) {
                    gpu->is_integrated = false;
                } else if (strcmp(gpu->vendor, "AMD") == 0) {
                    if (str_contains_ci(gpu->model, "Radeon RX") ||
                        str_contains_ci(gpu->model, "Radeon Pro") ||
                        str_contains_ci(gpu->model, "FirePro")) {
                        gpu->is_integrated = false;
                    } else if (str_contains_ci(gpu->model, "Vega") ||
                               str_contains_ci(gpu->model, "Radeon Graphics")) {
                        gpu->is_integrated = true;
                    } else {
                        gpu->is_integrated = false;
                    }
                } else {
                    gpu->is_integrated = true;
                }

                /* Attempt to detect VRAM */
                gpu->vram_gb = 0.0;
                char vram_path[512];
                snprintf(vram_path, sizeof(vram_path), "/sys/bus/pci/devices/%s/drm/card*/device/mem_info_vram_total", ent->d_name);
                FILE *fvram = fopen(vram_path, "r");
                if (fvram) {
                    unsigned long long bytes = 0;
                    if (fscanf(fvram, "%llu", &bytes) == 1 && bytes > 0) {
                        gpu->vram_gb = (double)bytes / (1024.0 * 1024.0 * 1024.0);
                    }
                    fclose(fvram);
                }

                /* If NVIDIA, attempt nvidia-smi */
                if (gpu->vram_gb == 0.0 && strcmp(gpu->vendor, "NVIDIA") == 0) {
                    FILE *nsmi = popen("nvidia-smi --query-gpu=memory.total --format=csv,noheader,nounits 2>/dev/null", "r");
                    if (nsmi) {
                        int mb = 0;
                        if (fscanf(nsmi, "%d", &mb) == 1 && mb > 0) {
                            gpu->vram_gb = (double)mb / 1024.0;
                        }
                        pclose(nsmi);
                    }
                }

                specs->gpu_count++;
            }
        }
        fclose(fc);
    }
    closedir(dir);

    /* Prefer dedicated GPU as primary gaming GPU */
    for (int i = 0; i < specs->gpu_count; i++) {
        if (!specs->gpus[i].is_integrated) {
            specs->primary_gpu_idx = i;
            break;
        }
    }
}

int detect_system_specs(SystemSpecs *specs) {
    if (!specs) return -1;
    memset(specs, 0, sizeof(SystemSpecs));

    /* OS & Kernel */
    struct utsname un;
    if (uname(&un) == 0) {
        safe_copy(specs->kernel, sizeof(specs->kernel), un.release);
        safe_copy(specs->arch, sizeof(specs->arch), un.machine);
        specs->is_64bit = (strcmp(un.machine, "x86_64") == 0 || strcmp(un.machine, "aarch64") == 0);
    } else {
        safe_copy(specs->kernel, sizeof(specs->kernel), "unknown");
        safe_copy(specs->arch, sizeof(specs->arch), "unknown");
        specs->is_64bit = true;
    }

    parse_os_release(specs->os_name, sizeof(specs->os_name));

    /* Hardware */
    detect_cpu(specs);
    detect_ram(specs);
    detect_storage(specs);
    detect_gpus(specs);

    return 0;
}

void print_system_specs(const SystemSpecs *specs) {
    if (!specs) return;

    printf("%s%s=== Linux System Hardware Specifications ===%s\n", col_bold(), col_cyan(), col_reset());
    printf("  %sOS:%s       %s (%s, %s)\n", col_bold(), col_reset(), specs->os_name, specs->kernel, specs->arch);
    printf("  %sCPU:%s      %s\n", col_bold(), col_reset(), specs->cpu_model);
    printf("            %d Cores, %d Threads", specs->cpu_cores, specs->cpu_threads);
    if (specs->cpu_ghz > 0.0) {
        printf(" (Max ~%.2f GHz)\n", specs->cpu_ghz);
    } else {
        printf("\n");
    }
    printf("  %sRAM:%s      %.2f GiB Total (%.2f GiB Available) [Tier: %.0f GB]\n",
           col_bold(), col_reset(), specs->ram_total_gb, specs->ram_avail_gb, specs->ram_norm_gb);
    printf("  %sStorage:%s  %.2f GiB Free (Mount: %s, Total: %.2f GiB)\n",
           col_bold(), col_reset(), specs->storage_avail_gb, specs->storage_path, specs->storage_total_gb);

    if (specs->gpu_count > 0) {
        printf("  %sGPU:%s      ", col_bold(), col_reset());
        for (int i = 0; i < specs->gpu_count; i++) {
            const GpuInfo *g = &specs->gpus[i];
            if (i > 0) printf("            ");
            printf("%s (%s%s%s", g->model,
                   g->is_integrated ? "Integrated" : "Dedicated",
                   g->vram_gb > 0.0 ? ", " : "",
                   g->vram_gb > 0.0 ? "" : "");
            if (g->vram_gb > 0.0) {
                printf("%.1f GB VRAM", g->vram_gb);
            }
            printf(", driver: %s)%s\n", g->driver, (i == specs->primary_gpu_idx && specs->gpu_count > 1) ? " *" : "");
        }
    } else {
        printf("  %sGPU:%s      Generic Linux DRM display\n", col_bold(), col_reset());
    }
    printf("%s==============================================%s\n\n", col_dim(), col_reset());
}

