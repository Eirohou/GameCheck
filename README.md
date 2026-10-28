# GameCompChecker (`gamecomp`)

An **extremely lightweight** Steam game compatibility checker written in C for **Linux**.  
Quickly checks whether your specific Linux PC can run any game on Steam against both **Minimum** and **Recommended** system requirements.

**No ownership or installation required**: You can check any game available on the Steam Store simply by typing its name or Steam AppID.

---

## Highlights & Features

- ⚡ **Ultra-Low Weight**:
  - Compiled binary is only **~75 KB** (stripped).
  - Uses **< 5 MB RAM** at runtime.
  - No Electron, no Python, no GUI overhead.
  - Embedded JSON parser (`cJSON`), requiring only standard C libraries and `libcurl`.
- 🐧 **Linux-Native Hardware Inspection**:
  - Direct Linux kernel & sysfs inspection: reads `/proc/cpuinfo`, `/proc/meminfo`, `sysinfo()`, `statvfs()`, and `/sys/bus/pci/devices/`.
  - Automatic detection of CPU cores, online threads, and maximum clock frequency.
  - Normalized RAM tiering (accounts for integrated graphics and hardware-reserved memory).
  - Free disk space calculation on your home/target mount.
  - GPU detection identifying integrated graphics (Intel UHD/Iris, AMD APU) vs. dedicated graphics (NVIDIA GeForce, AMD Radeon RX, Intel Arc), driver, and VRAM.
- 🎮 **Steam Requirements Integration**:
  - Queries the official Steam Store public API on the fly.
  - Automatically identifies whether a game has a **Native Linux** build or runs via **Steam Proton / Wine**.
  - Evaluates against native Linux requirements when present; falls back cleanly to PC requirements for Proton gaming.
- 🔍 **Flexible Game Selection**:
  - Interactive search mode with numbered selection menu.
  - Command-line game title search (e.g. `./gamecomp "elden ring"`).
  - Direct Steam AppID lookup (e.g. `./gamecomp 1091500`).
- 📊 **Clear & Actionable Verdicts**:
  - Formatted side-by-side comparison table with color badges (`[ PASS ]`, `[ WARN ]`, `[ FAIL ]`).
  - Actionable Linux advice (Proton recommendations, FSR / resolution tips, storage alerts).
  - Machine-readable `--json` output support for scripting and automation.

---

## Requirements

- **Linux** (x86_64, aarch64, or any Linux distro: Arch, Debian, Ubuntu, Fedora, Alpine, etc.)
- `gcc` or `clang`
- `make`
- `libcurl` (development headers: `curl-devel` or `libcurl4-openssl-dev`)

---

## Building

Clone or navigate to the repository directory and run:

```bash
make
```

The resulting binary `gamecomp` will be compiled with `-O2` optimizations and stripped for minimal file size.

### Optional Installation

Install system-wide to `/usr/local/bin`:

```bash
sudo make install
```

To remove:
```bash
sudo make uninstall
```

---

## Usage

### 1. Interactive Mode
Run without arguments to inspect your hardware and interactively search and test games:

```bash
./gamecomp
```

Example session:
```text
=== Steam Game Compatibility Checker for Linux ===
Tests if any game on Steam can run on your specific Linux PC
No Steam ownership or local game installation required.

=== Linux System Hardware Specifications ===
  OS:       Arch Linux (7.2.11-arch1-1, x86_64)
  CPU:      11th Gen Intel(R) Core(TM) i3-1115G4 @ 3.00GHz
            2 Cores, 4 Threads (Max ~4.10 GHz)
  RAM:      7.38 GiB Total (2.81 GiB Available) [Tier: 8 GB]
  Storage:  27.69 GiB Free (Mount: /home/user, Total: 107.20 GiB)
  GPU:      Intel Corporation Tiger Lake-LP GT2 [UHD Graphics G4] (Integrated, driver: i915)
==============================================

Enter game title or Steam AppID (or 'q' to quit): portal 2
```

### 2. Check Game by Title
Search directly from the command line:

```bash
./gamecomp "portal 2"
./gamecomp "hades"
./gamecomp "witcher 3"
```

If multiple titles match, an interactive numbered menu lets you pick:
```text
Found 10 matching games:
  [ 1] Portal 2                                (AppID: 620, Native Linux)
  [ 2] Portal 2 Soundtrack                     (AppID: 323180, Native Linux)
  [ 3] Portal 2: Community Edition             (AppID: 440000, Native Linux)
Select a game [1-10, default 1, or 0 to cancel]: 1
```

### 3. Check Game Directly by Steam AppID
Skip the search step and evaluate a specific AppID immediately:

```bash
./gamecomp 1091500    # Cyberpunk 2077
./gamecomp 1245620    # ELDEN RING
./gamecomp 413150     # Stardew Valley
```

### 4. Display System Hardware Specifications Only
```bash
./gamecomp --sysinfo
```

### 5. Machine-Readable JSON Output
Ideal for integration into scripts, status bars, or frontends:

```bash
./gamecomp --json 413150
```

Sample JSON:
```json
{
  "game": {
    "appid": 413150,
    "name": "Stardew Valley",
    "native_linux": true,
    "used_native_reqs": true
  },
  "system": {
    "os": "Arch Linux",
    "kernel": "7.2.11-arch1-1",
    "arch": "x86_64",
    "cpu": "11th Gen Intel(R) Core(TM) i3-1115G4 @ 3.00GHz",
    "cpu_threads": 4,
    "ram_total_gb": 7.38,
    "storage_avail_gb": 27.69,
    "gpu": "Intel Corporation Tiger Lake-LP GT2 [UHD Graphics G4] (rev 01)",
    "gpu_integrated": true
  },
  "minimum_check": {
    "verdict": "CAN RUN (Meets Minimum Requirements)",
    "ram": "PASS",
    "storage": "PASS",
    "cpu": "PASS",
    "gpu": "PASS",
    "os": "PASS"
  }
}
```

### 6. Command Line Options Summary

| Option | Description |
|---|---|
| `[GAME_NAME]` | Search Steam Store for title and test compatibility |
| `[APPID]` | Check game directly by numerical Steam AppID |
| `--sysinfo` | Print detected Linux hardware information and exit |
| `--json` | Output results in JSON format |
| `--no-color` | Disable ANSI terminal color escape codes |
| `-h`, `--help` | Display command usage and help |

---

## Compatibility Verdicts

- **CAN RUN (Meets Recommended Requirements)**: PC hardware meets or exceeds recommended specifications for high-quality gameplay.
- **CAN RUN (Meets Minimum Requirements)**: PC hardware satisfies all minimum specs for playable framerates (may require medium/low graphic presets).
- **BORDERLINE / PLAYABLE ON LOW**: System meets most requirements, but limitations like integrated graphics or lower clock speeds may require 720p or performance scaling (FSR).
- **BELOW MINIMUM REQUIREMENTS**: System falls short on critical requirements (insufficient RAM, insufficient disk storage, or unsupported GPU).

---

## Project Structure

```
gameCompChecker/
├── Makefile            # Portable build configuration
├── README.md           # Documentation
└── src/
    ├── main.c          # CLI handling, interactive prompt loop
    ├── sysinfo.h/c     # Linux hardware detection (/proc, /sys, sysinfo, statvfs)
    ├── steam_api.h/c   # Steam Store Search and AppDetails API client
    ├── req_parser.h/c  # HTML requirements parser & data extractor
    ├── checker.h/c     # Hardware vs. Game compatibility comparison engine
    ├── utils.h/c       # String helpers, ANSI styling, unit parsers
    └── cJSON.h/c       # Embedded lightweight JSON parser
```

---

## License

MIT License. See individual file headers for cJSON copyright details.
