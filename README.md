<div align="center">

# 📱 shas-dream-oc-mt6768-a11 — NOC Kernel

### by dev-maxx, for J💗K

### The Story of a Kernel That Wouldn't Let Us Have Root

![Branch](https://img.shields.io/badge/branch-shas--noc-important)
![Kernel](https://img.shields.io/badge/kernel-4.14.259-blue)
![Device](https://img.shields.io/badge/device-Redmi%209%20(lancelot)%20%2F%209T%20(merlin)-green)
![Android](https://img.shields.io/badge/Android-11%20(stock%20MIUI)-orange)
![Root](https://img.shields.io/badge/root-KernelSU%20v0.9.5-success)
![Status](https://img.shields.io/badge/status-%E2%9C%85%20ROOT%20WORKS-brightgreen)

</div>

## ðŸ¿ What is this? (for total beginners)

A phone is like a little computer. Before it can do *anything*, it needs a program called a **kernel** to wake up its brain.

This repo is a custom **kernel** made for the **Redmi 9** (and Redmi 9T). It does two things:

1. âš¡ Runs at **stock (normal) clock speeds** â€” like the phone originally shipped with. Safe, no overheating.
2. ðŸ”“ Has **KernelSU** built in â€” so you can get **root** (super-powers over your own phone).

> **Root** = permission to use your phone 100%. Without it, the phone decides what you can and can't do. With it, *you* decide.

---

## ðŸ“‹ The whole story in 30 seconds

| # | What we tried | What happened |
|---|---|---|
| 1 | KernelSU-Next (a newer root tool) | ðŸ’¥ Bootloop â€” phone restarts forever |
| 2 | Same, but "permissive" security | âœ… Boots, but root is flaky/not clean |
| 3 | KernelSU v0.9.5 (the old, faithful one) | âœ… Boots, but **root refused to work** |
| 4 | KernelSU v0.9.5 + **kprobes** on | ðŸ’¥ Bootloop â€” crashed the whole kernel |
| 5 | Same + KASLR disabled (defense mode off) | ðŸ’¥ Still bootloop |
| 6 | ðŸ” **Investigation**: kprobes is *broken* on MediaTek 4.14 kernels. It can never work here. | ðŸ’¡ |
| 7 | ðŸŽ‰ KernelSU v0.9.5 with **manual hooks** (no kprobes) | âœ… **BOOTS + ROOT WORKS!** |

Scroll down for the **full story with every detail**, crash logs, and all the detective work.

---

## ðŸ“± Supported devices

| Device | Codename | Model | Confirmed firmware |
|---|---|---|---|
| Redmi 9 (Global) | `lancelot` | M2004J19C / M2004J19G / M2004J19I | MIUI **V12.5.6.0.RJCMIXM** (Android 11) |
| Redmi 9 Power / Note 9 4G | `lancelot` | â€” | A11 Global / India |
| Redmi 9T / Note 9 4G | `merlin` | â€” | A11 Global |

> âš ï¸ **Only Android 11 stock MIUI.** Flashing this on Android 12/13 = **guaranteed bootloop**. This is not a threat, this is a promise.

---

## ðŸ† TL;DR â€” the stack that WORKS (don't change it casually)

| Component | Version | Why |
|---|---|---|
| Kernel source | `shas-noc` branch (base commit `6af5519f`) | Stock clocks, proven to boot stock MIUI |
| Root | **KernelSU v0.9.5** (`tiann/KernelSU` tag) | Last version that supports non-GKI (old-style) kernels |
| Hook mode | **MANUAL hooks** (kprobes **OFF**!) | See "The Big Discovery" below â€” kprobes crash this kernel |
| Compiler | **AOSP Clang 14** (`clang-r450784d`, android13-release) + LLD 14 | Matches the original known-good build |
| GCC | aarch64-linux-android-4.9 r35 + arm-linux-androideabi-4.9 r34 | Standard Android 11 prebuilts |
| Manager app | `KernelSU_v0.9.5_11872-release.apk` (exactly this one!) | Manager and kernel must speak the same language |
| SELinux | **enforcing** (stock) | Because this is a clean root, we don't need to weaken it |

---

## ðŸ” How KernelSU grants super-powers (for kids AND engineers)

KernelSU gives an app root by **standing at the door** of the phone's most important functions. When an app tries to do something special, KernelSU checks: *"Is this app allowed?"* If yes â€” super-powers granted! ðŸ¦¸

There are **two ways** KernelSU can stand at the door:

<details>
<summary><b>ðŸ› ï¸ Way 1: KPROBES â€” "the spy that rewrites instructions" (ðŸ’¥ BROKEN on our phone)</b></summary>

Kprobes works by **sneaking little spy instructions** into running kernel code. Every time the kernel runs a spy instruction, the spy reports back to KernelSU.

**Why it's broken on MediaTek 4.14 kernels (ours):**

- The kernel has a security feature (`CONFIG_RANDOMIZE_BASE`, aka "**KASLR**") that moves code around randomly at boot.
- To plant a spy, the kernel must **change an instruction** in memory â€” and on this chipset that triggers a crash (`do_undefinstr` â€” the CPU sees an "illegal instruction").
- Even with KASLR turned off, the crash just changed shape (`IABT` â€” the CPU tried to *run* code from an empty address). The spy mechanism itself is faulty on this chip.

**What the crash logs looked like** (we kept them!):

```text
[    0.71] Unable to handle kernel paging request at ... (IABT)
             pc : 0xffffff80011e3fc8
             lr : SyS_access+0x18/0x30
             Kernel Offset: disabled
```

`IABT` = "Instruction Abort" = the processor tried to fetch the *next* instruction from an address where there IS NO INSTRUCTION. Imagine a spy hiding in a wall, but the wall isn't there. ðŸ’¥

**Verdict:** kprobes is *fundamentally broken* on this kernel. No configuration can save it.
</details>

<details>
<summary><b>ðŸ“– Way 2: MANUAL HOOKS â€” "call the spy directly" (âœ… WORKS!)</b></summary>

Manually-hooked KernelSU doesn't rewrite any instructions. Instead, the kernel source code itself calls KernelSU's helper functions at the right moments â€” like adding "call your mom" to your own to-do list instead of hiring a spy to watch you.

The official KernelSU docs have a page for this exact situation:
ðŸ‘‰ [Integrate for non-GKI devices](https://kernelsu.org/guide/how-to-integrate-for-non-gki.html)

KernelSU's own config makes this automatic: when `KPROBES` is **off**, it switches to "manual hook mode" by itself.

```kconfig
config KSU_MANUAL_HOOK
	bool "KernelSU manual hook mode."
	depends on KSU && KSU != m
	default y if !KPROBES      # â† no kprobes = manual mode, automatically
```

We patched **6 places** in the kernel source. Each one is a tiny phone call.

ðŸ“ž **The 6 phone calls (the 6 hooks):**

| File | Function patched | What this hook does |
|---|---|---|
| `fs/exec.c` | `do_execveat_common` | Catches every app launch (`exec`) â€” lets allowed apps run as root, redirects `su` |
| `fs/open.c` | `faccessat` | Catches apps checking if `/system/bin/su` exists â€” hides it from unallowed apps |
| `fs/read_write.c` | `vfs_read` | While booting, secretly injects KernelSU's startup script into an Android config file |
| `fs/stat.c` | `vfs_statx` | Catches apps asking for `su` file details â€” same hiding trick, different spy window |
| `fs/devpts/inode.c` | `devpts_get_priv` | Fixes `pm` (package manager) inside a root shell |
| `drivers/input/input.c` | `input_handle_event` | Listens for **Volume-Down Ã— 3** at boot â€” enables KernelSU **Safe Mode** (boot rescue) |

</details>

---

## ðŸ•µï¸ The FULL detective story â€” every attempt, in order

> Real logs, real commits, real workflow runs. This is what actually happened.

### ðŸ§ª Attempt 1 â€” KernelSU-Next (legacy branch) â†’ ðŸ’¥ BOOTLOOP
*Workflow run `31324017848`*

- **What we did:** Tried the modern fork "KernelSU-Next" on the legacy branch, with `CONFIG_KSU_KPROBES_HOOK` (kprobe-based hooks).
- **Result:** The phone entered a bootloop â€” restarting forever. The zip comparison proved the kernel source was identical, so the root cause was KernelSU-Next + kprobes + Clang 11.
- **Lesson learned:** Never enable kprobe hooks on this kernel.

### ðŸ§ª Attempt 2 â€” KernelSU-Next + permissive SELinux â†’ âœ… BOOTS (but not recommended)
*Workflow run `31359616375`*

- **What we did:** Same as above but forced SELinux to "permissive" (all rules off).
- **Result:** It booted! But permissive SELinux is unsafe and insecure â€” it's like turning off all the locks in your house. Not acceptable as a final solution.
- **Additional lesson:** Some roots fail because SELinux blocks them â€” but that doesn't mean weakening SELinux is the answer.

### ðŸ§ª Attempt 3 â€” KernelSU v0.9.5 + Clang 14 â†’ âœ… BOOTS, âŒ ROOT FAILS
*Workflow runs `31361347555` + `31362131229`*

- **What we did:** Switched to the good old **KernelSU v0.9.5** (the last version supporting non-GKI kernels), compiled with **Clang 14** (`clang-r450784d`, AOSP android13-release).
- **Result:** The kernel booted into MIUI fine! But when the KernelSU manager tried to grant root, it said **"Failed to grant root!"**
- **Why:** We read the v0.9.5 source code (`kernel/ksu.c`) and found the su hooks only load under `#ifdef CONFIG_KPROBES`:

```c
#ifdef CONFIG_KPROBES
	ksu_sucompat_init();
	ksu_ksud_init();
#else
	pr_alert("KPROBES is disabled, KernelSU may not work, please check ...");
#endif
```

- **So we thought:** "Easy! Just enable kprobes!" ðŸ˜…

### ðŸ§ª Attempt 4 â€” v0.9.5 + kprobes ON â†’ ðŸ’¥ BOOTLOOP AGAIN
*Commit `ad24fee9`, workflow run `31365845786`*

- **What we did:** Added `CONFIG_KPROBES=y`, `CONFIG_HAVE_KPROBES=y`, `CONFIG_KRETPROBES=y`, `CONFIG_KPROBE_EVENTS=y` to the defconfig.
- **Result:** Bootloop. We pulled the crash log (`/proc/last_kmsg`):

```text
[0.66] do_undefinstr ... el1_undef ... SyS_access+0x18
       pc : [<ffffff8009562e88>] lr : [<ffffff80088f8010>] pstate: ...
       Kernel Offset: 0x32000000 (enabled)
```

- **Analysis:** `do_undefinstr` = CPU hit an "illegal instruction". The CPU was told to execute a kprobe spy instruction but it didn't understand it. The crash happened during `kernel_init` â€” right when kprobes were being registered.

### ðŸ§ª Attempt 5 â€” v0.9.5 + kprobes + NO KASLR â†’ ðŸ’¥ STILL BOOTLOOP
*Commit `25dd7a03`, workflow run `31371351178`*

- **What we did:** Disabled KASLR (`CONFIG_RANDOMIZE_BASE` off) â€” reasoning: with code at fixed addresses, the kprobe machinery might work.
- **Result:** Different crash, same bootloop:

```text
[0.71] Unable to handle kernel paging request at ffffff80011e3fc8 (IABT)
       pc : 0xffffff80011e3fc8
       lr : SyS_access+0x18/0x30
       Kernel Offset: disabled
```

- **Analysis:** `IABT` = "Instruction Abort at a non-executable address". The PC (program counter) pointed inside an empty page â€” the CPU tried to *run* instructions that don't exist. This was **not** a KASLR problem. The kprobe mechanism itself is broken on this MTK 4.14 kernel.

### ðŸ§ª Attempt 6 â€” The Investigation ðŸ”¬ *(the turning point)*

We stopped building and started reading:

1. **We read KernelSU-Next's Kconfig** and discovered the hook modes:

```kconfig
config KSU_MANUAL_HOOK
	bool "KernelSU manual hook mode."
	default y if !KPROBES        # â† this line!

config KSU_KPROBES_HOOK
	bool "KernelSU tracepoint+kretprobe hook"
	depends on KRETPROBES && KPROBES && HAVE_SYSCALL_TRACEPOINTS
	default y if !KSU_MANUAL_HOOK
```

2. **We found the smoking gun in the WORKING OC kernel:** The original overclocked kernel (which <u>had working root</u>!) printed **"KPROBES is disabled"** in its boot logs! The OC kernel's KernelSU was *never* using kprobes!

3. **We read the official guide** â†’ [Integrate for non-GKI devices](https://kernelsu.org/guide/how-to-integrate-for-non-gki.html) â€” for kernels older than ~5.10 or with broken kprobes, the recommended way is **manual hooks**: patch the kernel source so *it calls* KernelSU's functions.

### ðŸ† Attempt 7 â€” v0.9.5 + MANUAL HOOKS (NO kprobes!) â†’ âœ… âœ… âœ… IT WORKS!
*Commit `bceb45eb`, workflow run `31375278717`*

- **What we did:**
  1. Kept `CONFIG_KSU=y` but made sure kprobes stay **off** (`# CONFIG_KPROBES is not set`)
  2. Added a build step that patches **6 kernel source files** with the official manual-hook code (see table above)
  3. Built with the proven Clang 14 toolchain
- **Result:** **Boots into MIUI âœ” AND grants root âœ”**
- **Why it works:** No instruction rewriting, no spies â€” just direct phone calls from the kernel source. The working OC kernel did the same thing all along.

---

## ðŸŽ® Interactive Quiz â€” did you understand the story?

<details>
<summary><b>Question 1:</b> Why did the kernel bootloop with kprobes enabled?</summary>

Because kprobes works by rewriting instructions inside the running kernel, and on MediaTek 4.14 that triggers a hardware crash (`do_undefinstr` / `IABT`). The CPU literally doesn't understand the spy instructions.
</details>

<details>
<summary><b>Question 2:</b> What did the WORKING OC kernel teach us?</summary>

Its boot log said **"KPROBES is disabled"** â€” proving kprobes were never needed. The OC kernel's KernelSU used source-level (manual) hooks, exactly like our final build.
</details>

<details>
<summary><b>Question 3:</b> Why did root fail on Attempt 3 (no kprobes)?</summary>

Because KernelSU v0.9.5 compiles its `su` hooks under `#ifdef CONFIG_KPROBES` â€” without kprobes the hooks were compiled out, so no app could ever be granted root.
</details>

<details>
<summary><b>Question 4:</b> What are the two "hook modes" in KernelSU?</summary>

**Kprobe mode** (automatic, rewrites instructions â€” broken here) and **manual hook mode** (kernel source calls the hooks directly â€” works here). KernelSU switches to manual mode automatically when `!KPROBES`.
</details>

---

## ðŸ› ï¸ Building (GitHub Actions â€” the easy way)

The repo has the workflow **"Build Kernel with KernelSU"** (`.github/workflows/build-kernel.yml`).

1. Go to the **Actions** tab â†’ select **Build Kernel with KernelSU** â†’ click **Run workflow**
2. Input `device`: `lancelot` or `merlin` (default `lancelot`)
3. Wait ~30â€“40 minutes (it downloads toolchains, compiles Python 2.7 for MTK's DCT, then builds the kernel)
4. Open the finished run â†’ **Artifacts** â†’ download

**What the workflow does automatically:**
- Checks out the `shas-noc` branch
- Integrates **KernelSU v0.9.5** via `tiann/KernelSU/v0.9.5/kernel/setup.sh`
- Appends to `<device>_defconfig`:
  ```text
  CONFIG_KSU=y
  # CONFIG_KPROBES is not set     â† THE key line (manual hook mode)
  ```
- **Patches the 6 hook points** in the kernel source (see the hook table above)
- Builds with Clang 14 + GCC 4.9 prebuilts
- Packages an **AnyKernel3** flashable zip and uploads it as an artifact

### ðŸ’» Local build (advanced, Ubuntu)

```bash
git clone -b shas-noc https://github.com/MAXX-2628/shas-dream-oc-mt6768-a11
cd shas-dream-oc-mt6768-a11

# 1) toolchains
mkdir -p toolchains/{clang-llvm,gcc64-aosp,gcc32-aosp}
wget -q https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+archive/refs/heads/android13-release/clang-r450784d.tar.gz
tar -C toolchains/clang-llvm -xzf clang-r450784d.tar.gz
wget -q https://android.googlesource.com/platform/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/+archive/refs/tags/android-11.0.0_r35.tar.gz
tar -C toolchains/gcc64-aosp -xzf android-11.0.0_r35.tar.gz
wget -q https://android.googlesource.com/platform/prebuilts/gcc/linux-x86/arm/arm-linux-androideabi-4.9/+archive/refs/tags/android-11.0.0_r34.tar.gz
tar -C toolchains/gcc32-aosp -xzf android-11.0.0_r34.tar.gz

# 2) KernelSU v0.9.5 (NO kprobes!)
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/v0.9.5/kernel/setup.sh" | bash -s v0.9.5
printf "CONFIG_KSU=y\n# CONFIG_KPROBES is not set\n" >> arch/arm64/configs/lancelot_defconfig

# 3) apply the 6 manual hooks (see workflow .github/workflows/build-kernel.yml for the full python script)
#    (fs/exec.c, fs/open.c, fs/read_write.c, fs/stat.c, fs/devpts/inode.c, drivers/input/input.c)

# 4) build (Python 2.7 required - MTK DCT DrvGen.py)
export ARCH=arm64 SUBARCH=arm64
export PATH="$PWD/toolchains/clang-llvm/bin:$PWD/toolchains/gcc64-aosp/bin:$PWD/toolchains/gcc32-aosp/bin:$PATH"
make O=out ARCH=arm64 lancelot_defconfig
make -j$(nproc --all) O=out ARCH=arm64 CC=clang \
  CLANG_TRIPLE=aarch64-linux-gnu- CROSS_COMPILE=aarch64-linux-android- \
  CROSS_COMPILE_ARM32=arm-linux-androideabi- LD=ld.lld NM=llvm-nm OBJCOPY=llvm-objcopy

# 5) flashable zip
cd AnyKernel3-master && rm -f *.zip *-dtb
cp ../out/arch/arm64/boot/Image.gz-dtb Image.gz-dtb
zip -r9 Shas-Dream-KSU-lancelot-A11-$(date +%Y%m%d-%H%M).zip .
```

---

## ðŸ“² Flashing

### âš ï¸ Critical: unzip the artifact TWICE

GitHub wraps artifacts in an extra zip layer. The download contains a **folder** with the *real* flashable zip inside.

- Extract the artifact â†’ open the folder â†’ copy the **inner** zip to the phone
- **Never flash the outer artifact zip** â€” recovery will say `Invalid zip file format`

### Steps

1. Copy the **inner** zip to the phone storage
2. Boot **custom recovery** (TWRP / OrangeFox) â€” stock MIUI recovery rejects unsigned zips
3. Install the zip (it only replaces the *boot* partition â€” no data wipe)
4. Reboot

> Requirements: unlocked bootloader, backup your current `boot.img` first (`dd if=/dev/block/by-name/boot` in TWRP terminal), battery above ~70%.

---

## ðŸ”“ Root setup (kernel-side root, no APK needed to boot)

1. Flash the kernel zip (above)
2. Install **KernelSU Manager v0.9.5** â€” this exact APK:
   https://github.com/tiann/KernelSU/releases/download/v0.9.5/KernelSU_v0.9.5_11872-release.apk
3. Open the manager â†’ **Home** must show `Working` + the kernel version
4. First root request will prompt in the manager â†’ allow (or make the app an "allowlist" entry)
5. Reboot once if grants keep failing

**Manager/kernel pairing rules (very important):**
- v0.9.5 kernel â†’ **tiann KernelSU Manager v0.9.5** only
- KernelSU-Next kernel â†’ KernelSU-Next Manager (different protocol â€” do NOT mix!)
- Official KernelSU v1.x managers are incompatible with v0.9.5 kernels

> MIUI note: `adb install` fails with `INSTALL_FAILED_USER_RESTRICTED` unless
> **Settings â†’ Developer options â†’ Install via USB** is enabled.

---

## ðŸ†˜ Safe Mode (your rescue button!)

KernelSU 0.9.5 has a built-in **Safe Mode**: press **Volume-Down 3 times during boot**, and KernelSU will boot with all root features disabled â€” handy if a module breaks your boot.

Our 6th hook (`drivers/input/input.c`) is what enables this. And because we use **manual hooks** (kprobes off), the official warning about accidental Safe Mode triggering doesn't apply to us.

---

## ðŸ—„ï¸ Full history â€” workflow runs & commits

| Run ID | What happened | Result |
|---|---|---|
| `31324017848` | KernelSU-Next legacy + kprobes, first NOC build | ðŸ’¥ bootloop |
| `31359616375` | KernelSU-Next + permissive SELinux | âœ… boots, insecure |
| `31361347555` | v0.9.5 + Clang 14 (first try, broken toolchain URL) | âš ï¸ toolchain fixed later |
| `31362131229` | v0.9.5 + Clang 14 (AOSP clang-r450784d) | âœ… boots, âŒ root fails |
| `31365845786` | v0.9.5 + kprobes ON (`ad24fee9`) | ðŸ’¥ bootloop (`do_undefinstr`) |
| `31371351178` | v0.9.5 + kprobes + KASLR off (`25dd7a03`) | ðŸ’¥ bootloop (`IABT`) |
| **`31375278717`** | **v0.9.5 + MANUAL HOOKS, no kprobes (`bceb45eb`)** | âœ… **BOOTS + ROOT WORKS** |

**Key commits on `shas-noc`:**
- `d531e16b` / `7096b6cc` â€” permissive SELinux experiment (reverted)
- `ab7117f2` â€” workflow with v0.9.5 + Proton Clang 14 (bad toolchain URL â€” failed)
- `639b95a9` â€” fixed toolchain to AOSP Clang 14 (`clang-r450784d`)
- `ad24fee9` â€” added CONFIG_KPROBES to defconfig (bootloop!)
- `25dd7a03` â€” disabled KASLR (still bootloop!)
- `bceb45eb` â€” **THE FIX**: manual hooks in the build workflow, kprobes off

---

## ðŸ“œ Credits / sources

- Kernel source base: `merlin-r-oss` (Xiaomi MT6768 A11 OSS release)
- KernelSU: https://github.com/tiann/KernelSU (tag **v0.9.5**)
- Official non-GKI integration guide: https://kernelsu.org/guide/how-to-integrate-for-non-gki.html
- AnyKernel3: https://github.com/osm0sis/AnyKernel3
- Toolchains: AOSP Clang `clang-r450784d`, GCC 4.9 android-11 prebuilts
- Crash logs: MediaTek `CONFIG_PSTORE_RAM` / `CONFIG_MTK_RAM_CONSOLE` (`/proc/last_kmsg`)

## susfs (branch `shas-susfs` only)

This branch adds [susfs4ksu](https://gitlab.com/simonpunk/susfs4ksu) (branch
`kernel-4.14`) on top of the proven `shas-noc` recipe above: KSU stays pinned
at **v0.9.5** with kprobes off and the same 6 manual hooks, plus SELinux
domain-spoofing (zygote/init/ksu SID games), sus_path/sus_mount/sus_kstat
hiding, and uname/cmdline spoofing.

### Why two extra patches are hand-adapted, not stock

Neither of susfs's two kernel patches applies cleanly here, so this repo
carries adapted copies under `susfs/patches/` and the CI workflow always
uses those (never fetches patches from GitLab at build time):

- `10_enable_susfs_for_ksu-v095.patch` - the stock patch targets KSU's
  post-v1.0 non-GKI-dropped source layout; on v0.9.5 it needs 3 hand fixes
  (the `escape_to_root`/`setup_selinux` renames plus the susfs SELinux SID
  helper block in `kernel/selinux/selinux.c`).
- `50_add_susfs_in_kernel-4.14-mtk.patch` - the stock patch targets vanilla
  4.14; this MTK tree has extra vendor code (an F2FS fake-version override in
  `kernel/sys.c`, an IN_ALL_EVENTS mask calc in `fs/notify/fdinfo.c`, and a
  missing `is_pid` parameter on `show_map_vma` in `fs/proc/task_mmu.c`) that
  shifts the patch context in 3 places. Both were re-verified with
  `git apply --check` on a fresh KSU v0.9.5 clone / a synthetic tree built
  from the 21 stock files, respectively - clean applies, no `.rej`.

### Installing

1. Flash the kernel zip from this branch's CI artifact (`Shas-Dream-KSU-<device>-A11`) exactly like the `shas-noc` build - **unzip TWICE**, same as before.
2. Flash the `susfs-module-<device>` artifact zip as a KernelSU module (KSU
   Manager -> Modules -> Install from storage), then reboot. This installs
   the `ksu_susfs` CLI tool and `service.sh` automation from
   `susfs/ksu_module_susfs/`.

### Per-app / dynamic permissive

There is no `set_permissive` verb in the susfs tool - permissive toggling is
a KernelSU App Profile setting, not a susfs one:

KSU Manager -> tap the target app under Superuser -> App Profile -> toggle
**SELinux: Permissive** for that app's domain only. Everything else stays
enforcing. Susfs's job is orthogonal: hiding the su/module/mount footprint
from whichever apps you've granted root or profiled, via the SID games and
path/mount/kstat hiding above.

### Verifying susfs from adb shell

```sh
# from a root shell (adb shell su)
ksu_susfs add_sus_path /data/adb/modules
ksu_susfs add_sus_mount /data/adb/modules
ksu_susfs add_sus_kstat_statically /data/adb/ksu /data/system 0 0 755
```

See `susfs/ksu_module_susfs/README.md` (`ksu_susfs/jni/main.c` upstream) for
the full command list: `add_sus_path`, `add_sus_mount`,
`add_sus_kstat_statically`, `add_sus_kstat`, `update_sus_kstat`,
`update_sus_kstat_full_clone`, `add_try_umount`.

### Safe mode still works

Volume-Down x3 during boot still forces KSU safe mode (all root features off)
exactly as on `shas-noc` - susfs doesn't touch that hook.

### Rebranded manager (Device Sync)

`manager-rebrand/DeviceSync-v0.9.5.apk` on this branch is a repackaged copy
of the stock v0.9.5 manager APK - renamed to `com.deviceutil.sync` / "Device
Sync" (package, app label, launcher/webui activities, custom permission),
re-signed with a dedicated keystore - because some banking apps fingerprint
the stock KernelSU manager via `PackageManager` component scanning even when
susfs hides the rest of the root footprint. The kernel build's
`KSU_EXPECTED_SIZE`/`KSU_EXPECTED_HASH` (in `build-kernel.yml`) are set to
this APK's certificate, so the kernel only trusts this rebranded manager, not
the stock one.

Two gotchas if you ever need to redo this rebrand yourself:
- KernelSU identifies its manager purely by **APK signature**, not package
  name (`kernel/apk_sign.c`) - renaming the package/label is safe on its own.
- The compiled `libkernelsu.so` exports its JNI functions name-mangled with
  the **Java package** the `Natives` class lives in
  (`Java_me_weishu_kernelsu_Natives_becomeManager` etc). Renaming that one
  class's package breaks root (`UnsatisfiedLinkError`) unless you also
  recompile the native lib from source. Since `Natives` isn't declared in
  `AndroidManifest.xml`, the working fix is to leave just that class (and its
  `Natives$Profile` inner class) under `me.weishu.kernelsu` while rebranding
  everything manifest-visible (Application, Activities, FileProvider, the
  custom `DYNAMIC_RECEIVER_NOT_EXPORTED_PERMISSION`) - PackageManager scans
  never see the untouched class, so detection stays defeated either way.

### Known-good companion module: ReZygisk

Stock [ReZygisk](https://github.com/PerformanC/ReZygisk) v1.0.0 fails to
install on KSU v0.9.5 with `Unable to apply SELinux patches! Your kernel may
not support SELinux patch fully` - **this is not a kernel/susfs bug.** Root
cause: v0.9.5's `ksud sepolicy check` parser doesn't skip `#`-comment lines
in `sepolicy.rule`, and upstream's file has them (written for newer KSU
parsers). Fixed build (comments stripped, natively rebuilt from source, not
hand-patched) is published at
[MAXX-2628/ReZygisk-KernelSU release v1.0.0-ksu095-1](https://github.com/MAXX-2628/ReZygisk-KernelSU/releases/tag/v1.0.0-ksu095-1) -
use that instead of the stock release on this kernel.

### Is this kernel overclocked? No - here's the actual investigation

Traced the MT6768 CPU frequency selection path directly
(`drivers/misc/mediatek/base/power/cpufreq_v1/src/mach/mt6768/mtk_cpufreq_platform.c`,
`_mt_cpufreq_get_cpu_level()`): the OPP table is chosen at runtime from a
hardware efuse read (the SoC's factory-programmed segment code), not a
kernel config choice. A boosted "PRO"/"PRO_v7" OPP table does exist in the
header (2202MHz), which is worth being suspicious of - but the actual
selection function's `if/else` chain only ever assigns the standard levels;
PRO is unreachable dead code regardless of what the efuse reports. Governor
is `schedutil` (not `performance`), no max-freq override in the defconfig.
Ceiling actually reachable: **2000MHz big cluster / 1700MHz little
cluster** - this is the chip's genuine stock spec for this SoC, not
something inflated by this kernel.

If you're still seeing more heat/battery drain than expected, it's more
likely the extra background load from root + susfs + any Zygisk modules
than the clock ceiling - but `cpu-control.zip` (flashable module, this
branch) is provided as an **optional, fully reversible** way to cap below
stock anyway if you want that tradeoff: edit `MAX_FREQ_BIG`/`MAX_FREQ_LITTLE`
in its `service.sh` before flashing (defaults are the stock values = no-op),
or just uninstall the module via KSU Manager to instantly revert.

## License

Kernel code under GPLv2 (upstream). See COPYING.