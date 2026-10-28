#ifndef SYSINFO_H
#define SYSINFO_H

#include <stdbool.h>

#define MAX_STR_LEN 256
#define MAX_GPUS 4

typedef struct {
    char slot[32];          // PCI slot e.g. "0000:00:02.0"
    char model[MAX_STR_LEN];// Friendly model name e.g. "Intel UHD Graphics G4"
    char vendor[64];        // "Intel", "NVIDIA", "AMD", etc.
    char driver[64];        // "i915", "nvidia", "amdgpu", etc.
    char pci_id[32];        // "8086:9a78"
    bool is_integrated;     // true if integrated GPU, false if dedicated
    double vram_gb;         // Dedicated VRAM in GB if detectable, 0 if shared/unknown
} GpuInfo;

typedef struct {
    /* OS Information */
    char os_name[MAX_STR_LEN];
    char kernel[MAX_STR_LEN];
    char arch[64];
    bool is_64bit;

    /* CPU Information */
    char cpu_model[MAX_STR_LEN];
    int cpu_cores;
    int cpu_threads;
    double cpu_ghz;

    /* RAM Information */
    double ram_total_gb;
    double ram_avail_gb;
    double ram_norm_gb;    // Normalized physical tier (e.g., 8.0 for a ~7.4GB reported system)

    /* Storage Information */
    char storage_path[MAX_STR_LEN];
    double storage_avail_gb;
    double storage_total_gb;

    /* GPU Information */
    int gpu_count;
    GpuInfo gpus[MAX_GPUS];
    int primary_gpu_idx;   // Best gaming GPU index (prefers dedicated)
} SystemSpecs;

int detect_system_specs(SystemSpecs *specs);
void print_system_specs(const SystemSpecs *specs);

#endif /* SYSINFO_H */
