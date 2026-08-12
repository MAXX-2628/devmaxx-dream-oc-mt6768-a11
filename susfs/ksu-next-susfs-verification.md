# KernelSU-Next `v3.1.0-legacy-susfs` — susfs integration verification

Clone verified: `%TEMP%\opencode\ksun-next`, `git describe --tags` = `v3.1.0-legacy-susfs`,
commit `ba4422f0556e10f40dda1887631d87a18ede4ec5`. `kernel/` is a **flat** layout (no
`core/hook/supercall/policy` split): `ksu.c`, `sucompat.c`, `setuid_hook.c`, `supercalls.c`,
`lsm_hooks.c`, `syscall_hook_manager.c`, `allowlist.c`, `apk_sign.c`, `throne_tracker.c`,
`kernel_umount.c`, etc. all sit directly under `kernel/`, plus a `kernel/selinux/` subdir
(`selinux.c`, `rules.c`, `sepolicy.c`) and `kernel/tools/`. No `core_hook.c` (KernelSU-Next never
had one, even here — see Step 3), but otherwise this is the pre-modularization layout tiann-style
tooling expects.

## Step 1 — Kconfig `KSU_SUSFS_*` flags: present vs. plan §4 list

`kernel/Kconfig` lines 45–141, submenu `"KernelSU - SUSFS"`:

| Plan §4 flag | Present here? |
|---|---|
| `CONFIG_KSU_SUSFS` | Yes (line 46) — **also gained `depends on THREAD_INFO_IN_TASK`**, not just `depends on KSU` |
| `_HAS_MAGIC_MOUNT` | **Missing** |
| `_SUS_PATH` | Yes (line 54) |
| `_SUS_MOUNT` | Yes (line 65) |
| `_AUTO_ADD_SUS_KSU_DEFAULT_MOUNT` | **Missing** |
| `_AUTO_ADD_SUS_BIND_MOUNT` | **Missing** |
| `_SUS_KSTAT` | Yes (line 74) |
| `_SUS_OVERLAYFS` | **Missing** |
| `_TRY_UMOUNT` | Yes (line 82) |
| `_AUTO_ADD_TRY_UMOUNT_FOR_BIND_MOUNT` | **Missing** |
| `_SPOOF_UNAME` | Yes (line 90) |
| `_HIDE_KSU_SUSFS_SYMBOLS` | Yes (line 105) |
| `_SPOOF_CMDLINE_OR_BOOTCONFIG` | Yes (line 113) |
| `_ENABLE_LOG` | Yes (line 98) |
| `_OPEN_REDIRECT` | Yes (line 121) |
| *(not in plan §4)* `_SUS_MAP` | **Extra** — present (line 130), not listed in the plan doc |

**Finding**: 5 of the plan's 15 flags do not exist in this tag's Kconfig
(`_HAS_MAGIC_MOUNT`, `_AUTO_ADD_SUS_KSU_DEFAULT_MOUNT`, `_AUTO_ADD_SUS_BIND_MOUNT`,
`_SUS_OVERLAYFS`, `_AUTO_ADD_TRY_UMOUNT_FOR_BIND_MOUNT`) — all of them are the "automatic"
mount-tagging conveniences that susfs4ksu's newer `kernel_patches/KernelSU/
10_enable_susfs_for_ksu.patch` (tiann-targeted, in the `susfs4ksu-414` clone) *does* define. This
tag also adds one flag the plan doesn't list (`_SUS_MAP`) and tightens `KSU_SUSFS`'s own
dependency to require `THREAD_INFO_IN_TASK`. **Practical implication for the target device**: if
Redmi 9/9T's MT6768 4.14 kernel config doesn't set `THREAD_INFO_IN_TASK`, `KSU_SUSFS` won't even
be selectable here — this needs a config check before Task 5/6 proceed, it is not guaranteed.
Also: without `_AUTO_ADD_SUS_KSU_DEFAULT_MOUNT`/`_AUTO_ADD_SUS_BIND_MOUNT`, any module/bind mounts
KSU or Magisk-style modules create will **not** be auto-hidden — they'd need manual
`susfs add_sus_mount` calls from userspace instead of being automatic, a real behavioral
difference from what the plan assumed.

## Step 2 — kernel-side `susfs_*`/`st_susfs_*` symbols actually referenced

Full recursive symbol list from `kernel/` (34 distinct `susfs_*` identifiers, 0 `st_susfs_*`):

```
susfs_add_open_redirect        susfs_add_sus_kstat            susfs_add_sus_map
susfs_add_sus_path              susfs_add_sus_path_loop        susfs_add_try_umount
susfs_enable_log                susfs_get_current_sid          susfs_get_enabled_features
susfs_get_sid_from_name         susfs_init                     susfs_init_sid
susfs_is_boot_completed_triggered              susfs_is_current_init_domain
susfs_is_current_ksu_domain     susfs_is_current_proc_umounted susfs_is_current_zygote_domain
susfs_is_sid_equal              susfs_ksu_sid                  susfs_priv_app_sid
susfs_reorder_mnt_id            susfs_run_sus_path_loop        susfs_set_avc_log_spoofing
susfs_set_cmdline_or_bootconfig susfs_set_current_proc_umounted
susfs_set_hide_sus_mnts_for_non_su_procs        susfs_set_i_state_on_external_dir
susfs_set_init_sid              susfs_set_ksu_sid              susfs_set_priv_app_sid
susfs_set_sid                   susfs_set_uname                susfs_set_zygote_sid
susfs_show_variant              susfs_show_version              susfs_try_umount
susfs_update_sus_kstat          susfs_zygote_sid
```

Cross-referenced against every `susfs_*`/`st_susfs_*` identifier in susfs4ksu-414's
`kernel_patches/KernelSU/10_enable_susfs_for_ksu.patch` (the tiann-targeted patch — 41 distinct
identifiers). **Real drift, not just naming**:

- **Called here but not in the tiann patch** (16): `susfs_add_sus_map`,
  `susfs_add_sus_path_loop`, `susfs_enable_log` (patch calls `susfs_set_log` instead — different
  name for what looks like the same operation), `susfs_get_enabled_features` (patch computes the
  bitmask inline instead of calling a function), `susfs_is_boot_completed_triggered`,
  `susfs_is_current_proc_umounted`, `susfs_reorder_mnt_id`, `susfs_run_sus_path_loop`,
  `susfs_set_avc_log_spoofing`, `susfs_set_current_proc_umounted`,
  `susfs_set_hide_sus_mnts_for_non_su_procs`, `susfs_set_i_state_on_external_dir`,
  `susfs_set_priv_app_sid`, `susfs_show_variant`/`susfs_show_version` (patch inlines these via
  `SUSFS_VARIANT`/`SUSFS_VERSION` string constants instead of calling functions).
- **Called in the tiann patch but NOT here** (14): `susfs_add_sus_mount` (see below —
  significant), `susfs_is_allow_su`, `susfs_is_log_enabled`, `susfs_is_mnt_devname_ksu`,
  `susfs_is_umount_for_zygote_system_process_enabled`, `susfs_is_auto_add_sus_bind_mount_enabled`,
  `susfs_is_auto_add_sus_ksu_default_mount_enabled`,
  `susfs_is_auto_add_try_umount_for_bind_mount_enabled` (these last three track directly to the
  three missing `_AUTO_ADD_*`/`_HAS_MAGIC_MOUNT`-adjacent Kconfig flags from Step 1),
  `susfs_on_post_fs_data`, `susfs_run_try_umount_for_current_mnt_ns`, `susfs_set_log`,
  `susfs_task_state`, `susfs_try_umount_all`.
- **No `struct st_susfs_*` casts anywhere** in this tag's code — every susfs call here passes the
  raw `void __user *arg`/`unsigned long arg` straight through uncast (e.g.
  `susfs_add_sus_path(arg)`), whereas the tiann patch explicitly casts
  (`(struct st_susfs_sus_path __user*)arg3`). Functionally equivalent at the ABI level (the callee
  does the interpretation either way) but it means grepping this codebase for struct names won't
  find susfs's own type definitions — those only exist in susfs4ksu's `fs/`/`include/` patch
  (Task 4's territory), not in `ksun-next`.
- **`susfs_add_sus_mount` is not called anywhere in this tag.** `CONFIG_KSU_SUSFS_SUS_MOUNT` is a
  real, enabled Kconfig flag here, and `setuid_hook.c`/`supercalls.c` do call
  `susfs_reorder_mnt_id()`, `susfs_try_umount()`, and `susfs_set_hide_sus_mnts_for_non_su_procs()`
  under that guard — but there is no `CMD_SUSFS_ADD_SUS_MOUNT` command and no call to
  `susfs_add_sus_mount()` (confirmed via `grep -n "susfs_add_sus_mount\|SUS_MOUNT"` across
  `supercalls.c`, `setuid_hook.c`, `selinux/*.c`). **Task 4 needs to confirm** whether
  susfs4ksu's `kernel-4.14` branch `fs/susfs.c` still requires an explicit `add_sus_mount` call to
  populate its hide-list, or whether `susfs_set_hide_sus_mnts_for_non_su_procs()` is this
  version's equivalent mechanism — if the former, sus-mount hiding may be non-functional out of
  the box on this tag without additional wiring.

This confirms KernelSU-Next's team built this tag against a **different, non-contemporaneous
snapshot** of susfs4ksu's API than what `susfs4ksu-414`'s `kernel_patches/KernelSU/
10_enable_susfs_for_ksu.patch` targets (that patch is written for tiann/KernelSU directly, a
different, always-slightly-ahead lineage). Task 4 should verify `fs/susfs.c` on the
`kernel-4.14` branch actually **exports all 34** symbols this tag calls (not just the ones the
tiann patch happens to also use) before assuming link-compatibility.

## Step 3 — command-dispatch mechanism and hook-anchor files

**Correction to the brief's Step 3 assumption**: this tag is *not* prctl-based.
`grep -n "prctl|SYSCALL_DEFINE" kernel/ksu.c` returns **zero matches** — there is no `prctl()`
dispatch anywhere in this tag's `kernel/` tree at all (checked recursively, not just `ksu.c`).

What's actually there — **two coexisting dispatch mechanisms**, both already present at this
"legacy" tag (i.e. the ioctl-table rewrite found in the original Task 3 `v3.3.0` investigation
did *not* introduce ioctl dispatch — it only modularized files that already used it):

1. **A typed ioctl table** (`kernel/supercalls.c:755`, `static const struct ksu_ioctl_cmd_map
   ksu_ioctl_handlers[] = { { .cmd = KSU_IOCTL_GRANT_ROOT, ... }, ... }`) — same shape,
   same `KSU_IOCTL_*` naming convention, as what the original `v3.3.0` investigation found in
   `supercall/dispatch.c`. Handles `GRANT_ROOT`, `GET_INFO`, `SET_SEPOLICY`, allow/deny-list
   management, app profiles, etc. **susfs commands are not registered here.**
2. **A hijacked `reboot(2)` syscall magic-number channel**
   (`kernel/supercalls.c:847`, `int ksu_handle_sys_reboot(int magic1, int magic2, unsigned int
   cmd, void __user **arg4)`, wired up via `register_kprobe(&reboot_kp)` at
   `kernel/supercalls.c:1142`, gated by `KSU_KPROBES_HOOK`) — this is where susfs actually lives:
   `if (magic2 == SUSFS_MAGIC && current_uid().val == 0) { if (cmd ==
   CMD_SUSFS_ADD_SUS_PATH) {...} ... }` (`kernel/supercalls.c:860-950`), alongside other
   "toolkit extension" magic numbers (`KSU_INSTALL_MAGIC2`, `CHANGE_MANAGER_UID`,
   `GET_SULOG_DUMP_V2`, `CHANGE_KSUVER`, `CHANGE_SPOOF_UNAME`). **This is the tiann-style
   magic-number convention the brief expected — it's just carried over `reboot()`, not
   `prctl()`.** Userspace `ksu_susfs` tooling built against tiann/KernelSU's `reboot()`-hijack
   convention should be directly compatible here; tooling built against `prctl()` would not be.

Real hook-anchor files (confirmed present, not just guessed from names):

- **`kernel/setuid_hook.c`** — `ksu_handle_setresuid(uid_t ruid, uid_t euid, uid_t suid)` (line
  73) is the real setuid hook entry point, and it already contains susfs wiring under
  `CONFIG_KSU_SUSFS_SUS_MOUNT`/`CONFIG_KSU_SUSFS_SUS_PATH`/`CONFIG_KSU_SUSFS_TRY_UMOUNT`:
  `susfs_is_sid_equal()`, `susfs_try_umount()`, `susfs_reorder_mnt_id()`,
  `susfs_run_sus_path_loop()` are all called directly from this function (lines 57-63 declare the
  externs, the calls happen in the body).
- **`kernel/sucompat.c`** — `ksu_handle_faccessat`, `ksu_handle_stat`,
  `ksu_handle_execve_sucompat`, `ksu_handle_execveat_sucompat`, `ksu_handle_devpts` — the
  syscall-level su-hiding hooks. No direct susfs calls found here (susfs's own path-hiding hooks
  into the VFS layer live in susfs4ksu's separate `fs/`-side patch, not in this file).
- **`kernel/syscall_hook_manager.c`** — present, manages kprobe/manual-hook registration
  generically; no susfs-specific code found in it directly.
- **`kernel/lsm_hooks.c`** — present (LSM hook table registration), no direct susfs references.
- **`kernel/ksu.c`** — the module init/exit entry point; calls `susfs_init()` unconditionally
  under `#ifdef CONFIG_KSU_SUSFS` right after `ksu_throne_tracker_init()` (line 65), before
  `ksu_ksud_init()`.
- **`kernel/selinux/rules.c`** — `apply_kernelsu_rules()` calls
  `susfs_set_priv_app_sid()`/`susfs_set_init_sid()`/`susfs_set_ksu_sid()`/`susfs_set_zygote_sid()`
  under `#ifdef CONFIG_KSU_SUSFS` right before committing the new sepolicy (lines 137-143).
- **`kernel/selinux/selinux.c`** / **`selinux.h`** — define and declare the susfs SID globals
  and helper functions (`susfs_ksu_sid`, `susfs_zygote_sid`, `susfs_init_sid`,
  `susfs_priv_app_sid`, `susfs_set_*_sid()`, `susfs_is_sid_equal()`,
  `susfs_is_current_*_domain()`), all under `#ifdef CONFIG_KSU_SUSFS`, all cleanly appended after
  the existing (non-susfs) SID-cache code rather than replacing it.

**For Tasks 5-6**: the manual-hook install points are `setuid_hook.c` (setuid/umount path,
already susfs-aware) and `sucompat.c` (su-hiding syscall hooks, susfs-agnostic — susfs's own path
hiding is expected to come from the separate `fs/`-side patch Task 4 applies, not from anything in
`sucompat.c`). Neither file needs new manual-hook plumbing added for susfs itself — it's already
wired. What Task 4 needs to get right is making sure `fs/susfs.c` on `kernel-4.14` actually
defines all 34 symbols enumerated in Step 2, with the exact signatures these call sites expect
(untyped `unsigned long`/`void __user *` args throughout, per the "no `st_susfs_*` casts" finding
above).
