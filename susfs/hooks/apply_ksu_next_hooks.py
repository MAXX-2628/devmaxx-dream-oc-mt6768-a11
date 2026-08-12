#!/usr/bin/env python3
"""
apply_ksu_next_hooks.py

Inserts KernelSU-Next (tag v3.1.0-legacy-susfs) manual (non-kprobe) hook call
sites into a target kernel tree. This replaces the old KernelSU tiann v0.9.5
hook patcher used on the `shas-noc` -> `shas-susfs` branch.

WHY THIS EXISTS / WHY IT DIFFERS FROM THE OLD v0.9.5 6-HOOK SET
-----------------------------------------------------------------
The target device (Redmi 9/9T, MT6768) has kprobes broken at the SoC level
(`# CONFIG_KPROBES is not set`, see D:\\Trust\\SUSFS-HANDOFF.md secs 1/3), so
KernelSU-Next's `KSU_MANUAL_HOOK` mode (`kernel/Kconfig`:
`default y if !KPROBES`) is mandatory here, same as it was for v0.9.5.

However KernelSU-Next's manual-hook surface is NOT the same shape as v0.9.5's:
  - `kernel/lsm_hooks.c` in this tag registers `inode_permission`,
    `inode_rename`, and `task_fix_setuid` as real LSM hooks whenever
    `KSU_KPROBES_HOOK` is undefined (i.e. exactly the manual-hook case we're
    in). That makes the old fs/devpts/inode.c `ksu_handle_devpts` hook and a
    setresuid hook UNNECESSARY here -- they're wired automatically. The
    public `ksu_handle_devpts()` wrapper in `kernel/sucompat.c` is explicitly
    commented `// dead code: devpts handling` in this tag, confirming this.
  - This tag ALSO requires one hook v0.9.5 never had: `kernel/reboot.c`
    must call `ksu_handle_sys_reboot()`. This isn't optional -- KernelSU-
    Next's own `kernel/Kbuild` (lines 101-110) fails the build with
    `$(error -- KernelSU-Next: No hooks were defined...)` unless it can grep
    the literal string `ksu_handle_sys_reboot` out of the target kernel's
    `kernel/reboot.c` when `CONFIG_KSU_MANUAL_HOOK=y`. This is KSU-Next's own
    "toolkit"/susfs magic-number command channel (see
    `kernel/supercalls.c:847-950`, gated by `SUSFS_MAGIC`), and it does NOT
    register itself automatically in manual-hook mode: the kprobe that would
    otherwise attach to `reboot()` is compiled only under
    `#if defined(KSU_KPROBES_HOOK) && !defined(CONFIG_KSU_SUSFS)`
    (`kernel/supercalls.c:1141`) -- i.e. it is skipped BOTH because we are in
    manual-hook mode AND because CONFIG_KSU_SUSFS is enabled. So the reboot
    hook is mandatory for a susfs build in manual mode, full stop.

Net result: 6 real manual hooks, not the 5 the original plan hypothesized,
and not the same 6 as v0.9.5 either (input.c stays, devpts drops out, reboot
comes in):

  1. fs/exec.c            -> do_execveat_common()      -> ksu_handle_execveat()
  2. fs/open.c             -> SYSCALL_DEFINE3(faccessat) -> ksu_handle_faccessat()
  3. fs/stat.c             -> vfs_statx()                -> ksu_handle_stat()
  4. fs/read_write.c       -> vfs_read()                 -> ksu_handle_vfs_read()
  5. kernel/reboot.c       -> SYSCALL_DEFINE4(reboot)     -> ksu_handle_sys_reboot()  [build-mandatory]
  6. drivers/input/input.c -> input_handle_event()        -> ksu_handle_input_handle_event()

All real function signatures below were grepped verbatim from
%TEMP%\\opencode\\ksun-next (tag v3.1.0-legacy-susfs, commit
ba4422f0556e10f40dda1887631d87a18ede4ec5):
  kernel/ksu.c        : ksu_handle_execveat
  kernel/sucompat.c   : ksu_handle_faccessat, ksu_handle_stat
  kernel/ksud.c       : ksu_handle_vfs_read, ksu_handle_input_handle_event
  kernel/supercalls.c : ksu_handle_sys_reboot

Anchors were verified against the real fetched target-kernel files at
%TEMP%\\opencode\\shas-susfs-files\\{fs,kernel,drivers}\\... (from Task 2).

Idempotent: safe to run more than once per build; each patch is skipped if
its call site is already present.
"""

import re
import sys
import os

# Each entry: (relative_path, function_anchor_regex, local_anchor_regex,
#              extern_decl, call_stmt, marker)
#
# `function_anchor_regex` locates the start of the target function (matched
# up to and including its opening brace).
# `local_anchor_regex` is searched only within a bounded window AFTER the
# function start, and locates the end of that function's local-variable
# declaration block (so the inserted call is a statement, not a declaration,
# and never sits ahead of other declarations).
# `marker` is the idempotency check: if this substring is already in the
# file, the patch is treated as already applied and skipped.

WINDOW = 1500  # chars searched after function-start match for the local anchor

HOOKS = [
    dict(
        path="fs/exec.c",
        name="do_execveat_common (execve/execveat)",
        func_re=re.compile(
            r"static\s+int\s+do_execveat_common\s*\(\s*int\s+fd\s*,"
            r"\s*struct\s+filename\s*\*\s*filename\s*,"
            r"[\s\S]*?int\s+flags\s*\)\s*\{"
        ),
        local_re=re.compile(
            r"struct\s+files_struct\s*\*\s*displaced\s*;\s*\n\s*int\s+retval\s*;\s*\n"
        ),
        extern_decl=(
            "extern int ksu_handle_execveat(int *fd, struct filename **filename_ptr,\n"
            "\t\t\t\tvoid *argv, void *envp, int *flags);"
        ),
        call_stmt="ksu_handle_execveat(&fd, &filename, &argv, &envp, &flags);",
        marker="ksu_handle_execveat(",
    ),
    dict(
        path="fs/open.c",
        name="SYSCALL_DEFINE3(faccessat)",
        func_re=re.compile(
            r"SYSCALL_DEFINE3\s*\(\s*faccessat\s*,\s*int\s*,\s*dfd\s*,"
            r"\s*const\s+char\s+__user\s*\*\s*,\s*filename\s*,"
            r"\s*int\s*,\s*mode\s*\)\s*\{"
        ),
        local_re=re.compile(
            r"unsigned\s+int\s+lookup_flags\s*=\s*LOOKUP_FOLLOW\s*;\s*\n"
        ),
        extern_decl=(
            "extern int ksu_handle_faccessat(int *dfd, const char __user **filename_user,\n"
            "\t\t\t\tint *mode, int *__unused_flags);"
        ),
        call_stmt="ksu_handle_faccessat(&dfd, &filename, &mode, NULL);",
        marker="ksu_handle_faccessat(",
    ),
    dict(
        path="fs/stat.c",
        name="vfs_statx (stat/lstat/newfstatat/statx common path)",
        func_re=re.compile(
            r"int\s+vfs_statx\s*\(\s*int\s+dfd\s*,\s*const\s+char\s+__user\s*\*\s*filename\s*,"
            r"\s*int\s+flags\s*,"
            r"[\s\S]*?u32\s+request_mask\s*\)\s*\{"
        ),
        local_re=re.compile(
            r"unsigned\s+int\s+lookup_flags\s*=\s*LOOKUP_FOLLOW\s*\|\s*LOOKUP_AUTOMOUNT\s*;\s*\n"
        ),
        extern_decl=(
            "extern int ksu_handle_stat(int *dfd, const char __user **filename_user,\n"
            "\t\t\t   int *flags);"
        ),
        call_stmt="ksu_handle_stat(&dfd, &filename, &flags);",
        marker="ksu_handle_stat(",
    ),
    dict(
        path="fs/read_write.c",
        name="vfs_read",
        func_re=re.compile(
            r"ssize_t\s+vfs_read\s*\(\s*struct\s+file\s*\*\s*file\s*,"
            r"\s*char\s+__user\s*\*\s*buf\s*,\s*size_t\s+count\s*,"
            r"\s*loff_t\s*\*\s*pos\s*\)\s*\{"
        ),
        local_re=re.compile(r"ssize_t\s+ret\s*;\s*\n"),
        extern_decl=(
            "extern int ksu_handle_vfs_read(struct file **file_ptr, char __user **buf_ptr,\n"
            "\t\t\t\tsize_t *count_ptr, loff_t **pos);"
        ),
        call_stmt="ksu_handle_vfs_read(&file, &buf, &count, &pos);",
        marker="ksu_handle_vfs_read(",
    ),
    dict(
        path="kernel/reboot.c",
        name="SYSCALL_DEFINE4(reboot) [build-mandatory anchor for KSU_MANUAL_HOOK]",
        func_re=re.compile(
            r"SYSCALL_DEFINE4\s*\(\s*reboot\s*,\s*int\s*,\s*magic1\s*,"
            r"\s*int\s*,\s*magic2\s*,\s*unsigned\s+int\s*,\s*cmd\s*,"
            r"[\s\S]*?void\s+__user\s*\*\s*,\s*arg\s*\)\s*\{"
        ),
        local_re=re.compile(r"int\s+ret\s*=\s*0\s*;\s*\n"),
        extern_decl=(
            "extern int ksu_handle_sys_reboot(int magic1, int magic2, unsigned int cmd,\n"
            "\t\t\t\t  void __user **arg);"
        ),
        call_stmt="ksu_handle_sys_reboot(magic1, magic2, cmd, &arg);",
        marker="ksu_handle_sys_reboot(",
    ),
    dict(
        path="drivers/input/input.c",
        name="input_handle_event",
        func_re=re.compile(
            r"static\s+void\s+input_handle_event\s*\(\s*struct\s+input_dev\s*\*\s*dev\s*,"
            r"\s*unsigned\s+int\s+type\s*,\s*unsigned\s+int\s+code\s*,\s*int\s+value\s*\)\s*\{"
        ),
        # `int disposition = input_get_disposition(dev, type, code, &value);`
        # is this function's one and only local declaration, and it is the
        # first thing in the block -- valid C89 as-is. Inserting our hook
        # block BEFORE it (as an earlier version of this script did, via a
        # zero-width local_re matched at the opening brace) makes it a
        # statement-then-declaration in the same scope, which is invalid
        # C89 ("mixed declarations and code") -- the exact bug class already
        # documented as a hard compile failure on this MTK tree in
        # D:\Trust\SUSFS-HANDOFF.md 10.5.2. Anchor after this declaration
        # instead, same as the other 5 hooks anchor after their last local
        # declaration.
        local_re=re.compile(
            r"int\s+disposition\s*=\s*input_get_disposition\([^;]*\)\s*;\s*\n"
        ),
        extern_decl=(
            "extern int ksu_handle_input_handle_event(unsigned int *type, unsigned int *code,\n"
            "\t\t\t\t\t  int *value);"
        ),
        call_stmt="ksu_handle_input_handle_event(&type, &code, &value);",
        marker="ksu_handle_input_handle_event(",
    ),
]


def patch_file(hook, root):
    path = os.path.join(root, hook["path"])
    if not os.path.isfile(path):
        print(f"ERROR: file not found: {path}", file=sys.stderr)
        sys.exit(1)

    with open(path, "r", encoding="utf-8", errors="surrogateescape") as f:
        content = f.read()

    if hook["marker"] in content:
        print(f"SKIP  {hook['path']}: '{hook['name']}' already patched")
        return

    func_match = hook["func_re"].search(content)
    if not func_match:
        print(
            f"ERROR: anchor function not found in {path} for hook "
            f"'{hook['name']}' (regex: {hook['func_re'].pattern!r})",
            file=sys.stderr,
        )
        sys.exit(1)

    window_start = func_match.end()
    window_end = min(len(content), window_start + WINDOW)
    window = content[window_start:window_end]

    local_match = hook["local_re"].search(window)
    if not local_match:
        print(
            f"ERROR: local insertion anchor not found in {path} within "
            f"{WINDOW} chars after '{hook['name']}' (regex: {hook['local_re'].pattern!r})",
            file=sys.stderr,
        )
        sys.exit(1)

    insert_at = window_start + local_match.end()

    block = (
        "\n#ifdef CONFIG_KSU\n"
        f"\t{hook['extern_decl']}\n"
        f"\t{hook['call_stmt']}\n"
        "#endif\n"
    )

    content = content[:insert_at] + block + content[insert_at:]

    with open(path, "w", encoding="utf-8", errors="surrogateescape") as f:
        f.write(content)

    print(f"OK    {hook['path']}: inserted '{hook['name']}' hook")


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else "."
    for hook in HOOKS:
        patch_file(hook, root)
    print("apply_ksu_next_hooks.py: all hooks processed.")


if __name__ == "__main__":
    main()
