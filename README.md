# shas-dream-oc-mt6768-a11 — NOC Kernel for Redmi 9 (lancelot) / Redmi 9T (merlin)

Custom **no-overclock (NOC)** kernel for MediaTek MT6768 devices running **stock MIUI (Android 11)**.
Based on the `merlin-r-oss` (A11) kernel source, Linux **4.14.259**, with **KernelSU** root support.

> Branch `shas-noc` = stock clocks (no overclock). This is the branch that works with stock MIUI.
> The original overclocked build lives upstream (`Arafattex/shas-dream-oc-mt6768-a11` OC branch).

---

## Supported devices & firmware

| Device | Codename | Model | Confirmed firmware |
|---|---|---|---|
| Redmi 9 (Global) | `lancelot` | M2004J19C / M2004J19G / M2004J19I | MIUI **V12.5.6.0.RJCMIXM** (Android 11) |
| Redmi 9 Power / Note 9 4G | `lancelot` | — | A11 Global/India |
| Redmi 9T / Note 9 4G | `merlin` | — | A11 Global |

**Only Android 11 stock MIUI.** Do NOT flash on Android 12/13 (V13.x / S…) — bootloop guaranteed.

---

## TL;DR — known-good stack (do not change casually)

Verified working on Redmi 9 (lancelot), MIUI V12.5.6.0.RJCMIXM, A11:

| Component | Version | Why |
|---|---|---|
| Kernel source | `shas-noc` branch (commit `6af5519f` base) | stock clocks, boots MIUI |
| Root | **KernelSU v0.9.5** (tiann/KernelSU tag) | stable, boots fine |
| Root code path | **CONFIG_KPROBES=y + CONFIG_KALLSYMS=y** | **required** — v0.9.5 su hooks only load with kprobes |
| Compiler | **AOSP Clang 14** (`clang-r450784d`, android13-release) + LLD 14 | matches original working build |
| GCC | aarch64-linux-android-4.9 r35 + arm-linux-androideabi-4.9 r34 | standard A11 prebuilts |
| Manager app | **KernelSU_v0.9.5_11872-release.apk** (from tiann release v0.9.5) | must match kernel v0.9.5 |
| SELinux | **enforcing** (stock) | permissive NOT needed for this stack |

### ⚠️ Things that did NOT work (learned the hard way)

- **KernelSU-Next (legacy branch)** → **bootloop** on this MTK 4.14 kernel with stock MIUI.
  `CONFIG_KSU_KPROBES_HOOK` + selinux_hide patches are unstable here. Do not use KernelSU-Next.
- **KernelSU v0.9.5 WITHOUT kprobes** → boots, manager shows "Working", but **every root grant fails**
  ("Failed to grant root!"). v0.9.5 compiles its `su` hooks (`ksu_sucompat_init` / `ksu_ksud_init`)
  under `#ifdef CONFIG_KPROBES` only.
- **Permissive SELinux via `patch_cmdline`** → not needed for the v0.9.5 stack (kept commented).
- **Clang 11 (clang-r383902b1)** — replaced by Clang 14 to match the proven original build.

---

## Building (GitHub Actions)

The repo has a workflow **"Build Kernel with KernelSU"** (`.github/workflows/build-kernel.yml`).

1. Open **Actions** tab → select **Build Kernel with KernelSU** → **Run workflow**
2. Input `device`: `lancelot` or `merlin` (default lancelot)
3. Wait ~30–40 min (downloads toolchains, compiles Python 2.7 for MTK DCT, builds kernel)
4. Open the finished run → **Artifacts** → download

The workflow always:
- checks out `shas-noc`
- integrates KernelSU **v0.9.5** via `tiann/KernelSU/v0.9.5/kernel/setup.sh`
- appends to `<device>_defconfig`:
  ```
  CONFIG_KSU=y
  CONFIG_KALLSYMS=y
  CONFIG_KPROBES=y
  CONFIG_HAVE_KPROBES=y
  CONFIG_KRETPROBES=y
  CONFIG_KPROBE_EVENTS=y
  ```
- builds with Clang 14 (`clang-r450784d`) + GCC 4.9 prebuilts
- packages an AnyKernel3 zip and uploads it as an artifact

### Local build (Ubuntu)

```bash
git clone -b shas-noc https://github.com/MAXX-2628/shas-dream-oc-mt6768-a11
cd shas-dream-oc-mt6768-a11

# toolchains
mkdir -p toolchains/{clang-llvm,gcc64-aosp,gcc32-aosp}
wget -q https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+archive/refs/heads/android13-release/clang-r450784d.tar.gz
tar -C toolchains/clang-llvm -xzf clang-r450784d.tar.gz
wget -q https://android.googlesource.com/platform/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/+archive/refs/tags/android-11.0.0_r35.tar.gz
tar -C toolchains/gcc64-aosp -xzf android-11.0.0_r35.tar.gz
wget -q https://android.googlesource.com/platform/prebuilts/gcc/linux-x86/arm/arm-linux-androideabi-4.9/+archive/refs/tags/android-11.0.0_r34.tar.gz
tar -C toolchains/gcc32-aosp -xzf android-11.0.0_r34.tar.gz

# KernelSU v0.9.5 + config (same as CI)
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/v0.9.5/kernel/setup.sh" | bash -s v0.9.5
printf "CONFIG_KSU=y\nCONFIG_KALLSYMS=y\nCONFIG_KPROBES=y\nCONFIG_HAVE_KPROBES=y\nCONFIG_KRETPROBES=y\nCONFIG_KPROBE_EVENTS=y\n" >> arch/arm64/configs/lancelot_defconfig

# build (Python 2.7 must be installed; MTK DCT DrvGen.py needs it)
export ARCH=arm64 SUBARCH=arm64
export PATH="$PWD/toolchains/clang-llvm/bin:$PWD/toolchains/gcc64-aosp/bin:$PWD/toolchains/gcc32-aosp/bin:$PATH"
make O=out ARCH=arm64 lancelot_defconfig
make -j$(nproc --all) O=out ARCH=arm64 CC=clang \
  CLANG_TRIPLE=aarch64-linux-gnu- CROSS_COMPILE=aarch64-linux-android- \
  CROSS_COMPILE_ARM32=arm-linux-androideabi- LD=ld.lld NM=llvm-nm OBJCOPY=llvm-objcopy

# flashable zip
cd AnyKernel3-master && rm -f *.zip *-dtb
cp ../out/arch/arm64/boot/Image.gz-dtb Image.gz-dtb
zip -r9 Shas-Dream-KSU-lancelot-A11-$(date +%Y%m%d-%H%M).zip .
```

---

## Flashing

### ⚠️ Critical: unzip the artifact twice

GitHub wraps artifacts in an extra zip layer. The download `Shas-Dream-KSU-lancelot-A11.zip` contains a
folder with the **real flashable zip** (`Shas-Dream-KSU-lancelot-A11-<date>.zip`).

- Extract the artifact → open the folder → copy the **inner** zip to the phone
- **Never flash the outer artifact zip** → recovery says `Invalid zip file format`

### Flash

1. Copy the inner zip to the phone storage
2. Boot **custom recovery** (TWRP / OrangeFox) — stock MIUI recovery rejects unsigned zips
3. Install the zip (it only replaces the boot partition — no data wipe)
4. Reboot

Requirements:
- Unlocked bootloader
- Backup your current `boot.img` first (TWRP backup or `dd if=/dev/block/by-name/boot`)
- Battery above ~70%

---

## Root setup (KernelSU v0.9.5)

1. Flash the kernel zip (above)
2. Install **KernelSU Manager v0.9.5** — only this exact APK:
   https://github.com/tiann/KernelSU/releases/download/v0.9.5/KernelSU_v0.9.5_11872-release.apk
3. Open the manager → Home must show `Working` + the kernel version
4. First root request will prompt in the manager → allow
5. Reboot once if grants keep failing

**Manager/kernel pairing rules:**
- v0.9.5 kernel → tiann **KernelSU Manager v0.9.5** only
- KernelSU-Next kernel → KernelSU-Next Manager (different protocol — do NOT mix, grants will fail/bootloop)
- Newer official KernelSU v1.x managers are incompatible with v0.9.5 kernels

MIUI note: `adb install` fails with `INSTALL_FAILED_USER_RESTRICTED` unless
**Settings → Developer options → Install via USB** is enabled.

---

## Diagnosing a bootloop

The kernel has crash logging built in (`CONFIG_PSTORE_RAM`, `CONFIG_MTK_RAM_CONSOLE`,
`CONFIG_PANIC_TIMEOUT=1` — panics auto-reboot after 1 s, which is what makes it *loop*).

If a build bootloops:

1. **Capture the log BEFORE restoring stock boot** (evidence is overwritten by the next boot):
   - Boot to TWRP (Power + Vol Up during bootloop)
   - TWRP → Advanced → Terminal:
     ```
     cat /proc/last_kmsg > /sdcard/last_kmsg.txt
     cat /sys/fs/pstore/console* > /sdcard/pstore.txt
     ```
   - Or from PC: `adb pull /sdcard/last_kmsg.txt`
   - The backtrace shows the exact panicking function (kernel code vs root-hook code)
2. Restore stock boot to recover:
   - Download stock fastboot ROM **V12.5.6.0.RJCMIXM** (for lancelot) → extract `images/boot.img`
   - `fastboot flash boot boot.img`
3. Check the obvious first: **firmware must be Android 11** and the correct device build (`lancelot` vs `merlin` — wrong one bootloops on these MTK boards)

---

## How this repo got here (session summary)

1. Original project = overclocked kernel (`shas-dream-oc`). The OC zip (`a11r_oc_lancelot.zip`)
   **boots stock MIUI** but its root never worked (same missing-kprobes issue).
2. Aug 2026 experiments added a GitHub Actions workflow and switched root to **KernelSU-Next legacy**
   → first flashable NOC zip bootlooped. Byte-level comparison of the zips proved the kernel source was
   identical; the regression was KernelSU-Next + clang 11.
3. Reverted to the proven stack (**KernelSU v0.9.5 + Clang 14, enforcing**) → NOC zip boots MIUI.
4. Root still failed → read the v0.9.5 source: su hooks are `#ifdef CONFIG_KPROBES`.
   Enabling kprobes in the defconfig was the final fix (build `ad24fee9`).

**Session workflow runs for reference:**
- `31324017848` — first successful NOC build (KernelSU-Next, bootlooped)
- `31359616375` — permissive + KernelSU-Next (boots, not recommended)
- `31362131229` — v0.9.5 + Clang 14 (boots MIUI, root incomplete)
- `31365845786`+ — v0.9.5 + Clang 14 + **kprobes** (root-capable)

---

## Credits / sources

- Kernel source base: merlin-r-oss (Xiaomi MT6768 A11 OSS release)
- KernelSU: https://github.com/tiann/KernelSU (tag v0.9.5)
- AnyKernel3: https://github.com/osm0sis/AnyKernel3
- Toolchains: AOSP Clang `clang-r450784d`, GCC 4.9 android-11 prebuilts

## License

Kernel code under GPLv2 (upstream). See COPYING.
