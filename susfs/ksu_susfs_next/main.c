/*
 * ksu_susfs_next -- userspace control tool for susfs on KernelSU-Next
 * v3.1.0-legacy-susfs (this kernel: shas-dream-oc-mt6768-a11, branch
 * shas-susfs-next).
 *
 * Adapted from susfs4ksu's kernel-4.14 ksu_susfs/jni/main.c (prctl(2)
 * transport) for Task 11 of the KSU-Next migration. That prctl-based tool
 * is byte-for-byte useless on this kernel: v3.1.0-legacy-susfs's susfs
 * dispatch is exclusively reachable via a hijacked reboot(2) syscall
 * (kernel/supercalls.c:ksu_handle_sys_reboot(), wired into
 * kernel/reboot.c's SYSCALL_DEFINE4(reboot,...) by
 * susfs/hooks/apply_ksu_next_hooks.py) -- there is zero prctl() usage
 * anywhere in this tag.
 *
 * ==========================================================================
 * IMPORTANT -- read before "fixing" the error handling in this file
 * ==========================================================================
 * The reboot(2) hijack is inserted as the VERY FIRST statement of
 * SYSCALL_DEFINE4(reboot, ...), before the kernel's own capability check
 * (CAP_SYS_BOOT) and before its real magic1/magic2 validation
 * (LINUX_REBOOT_MAGIC1/2). ksu_handle_sys_reboot()'s return value is
 * DISCARDED by the call site (`ksu_handle_sys_reboot(magic1, magic2, cmd,
 * &arg);` -- a bare statement, not assigned to `ret`). That means after our
 * susfs command has already been dispatched and executed inside the
 * kernel, execution ALWAYS falls through into the real reboot() body,
 * which then rejects our bogus magic1 (KERNEL_SU_OPTION == 0xDEADBEEF, not
 * LINUX_REBOOT_MAGIC1 == 0xfee1dead) with -EINVAL.
 *
 * Net effect: every call in this file is expected to return -1 with
 * errno == EINVAL, REGARDLESS of whether the susfs operation itself
 * succeeded. Do not add code that treats a nonzero syscall(__NR_reboot,...)
 * return as a susfs failure -- it isn't one, it's just the real reboot()
 * rejecting our deliberately-fake magic number after our payload already
 * ran.
 *
 * The only real in-band success/failure signal available is the `err`
 * field that a handful of NEWER commands (added natively by this
 * KernelSU-Next tag, not present in the older susfs4ksu protocol this tool
 * was adapted from) write back via copy_to_user(): hide_sus_mnts_for_non
 * _su_procs, avc_log_spoofing, show version/variant/enabled_features. For
 * every other reachable command (sus_kstat, try_umount, uname,
 * cmdline_or_bootconfig) the kernel handler's own int return value is
 * ALSO discarded by supercalls.c's dispatcher (e.g.
 * `susfs_add_sus_kstat(arg); return 0;`), so there is genuinely no error
 * feedback path for those at all -- this tool can only report that the
 * request was sent, not that it was applied successfully. This is a real
 * regression in observability versus the old prctl-based tool (which had
 * an explicit 5th out-param), inherent to how this KernelSU-Next tag wires
 * susfs into the syscall, not a bug in this adaptation. Verified by
 * reading kernel/supercalls.c's ksu_handle_sys_reboot() dispatcher and
 * kernel/reboot.c's hook call site directly, both on shas-susfs-next.
 * ==========================================================================
 *
 * Transport constants (both re-verified against live source this session):
 *   KERNEL_SU_OPTION == KSU_INSTALL_MAGIC1 (kernel/supercalls.h)  = 0xDEADBEEF
 *   SUSFS_MAGIC       (susfs/include/linux/susfs_def.h)           = 0xFAFAFAFA
 *
 * Reachable commands on THIS kernel build (susfs/ksu-next-susfs-verification.md
 * + kernel/supercalls.c's actual #ifdef CONFIG_KSU_SUSFS_* guards, cross
 * checked against build-kernel.yml's defconfig-append step on
 * shas-susfs-next): SUS_MOUNT (hide_sus_mnts_for_non_su_procs only --
 * add_sus_mount itself has NO dispatch case in this kernel at all, verified
 * by grepping the live supercalls.c: there is no
 * "cmd == CMD_SUSFS_ADD_SUS_MOUNT" anywhere in it), SUS_KSTAT, TRY_UMOUNT,
 * SPOOF_UNAME, SPOOF_CMDLINE_OR_BOOTCONFIG, plus the two commands that are
 * unconditionally compiled whenever CONFIG_KSU_SUSFS=y regardless of any
 * sub-flag (avc_log_spoofing, show version/variant/enabled_features).
 * SUS_PATH, ENABLE_LOG, OPEN_REDIRECT, SUS_MAP are off in this build's
 * defconfig -- their CLI verbs are kept but stubbed to a clear
 * "not supported on this build" error rather than silently no-op'ing.
 * sus_su and run_try_umount (present in the old prctl tool) have NO
 * dispatch case in this kernel's supercalls.c at all -- not even
 * Kconfig-gated, just structurally absent from this KSU-Next tag's susfs
 * port -- so they are stubbed the same way.
 */
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/vfs.h>
#include <sys/syscall.h>
#include <sys/reboot.h>

/*************************
 ** Define Const Values **
 *************************/
#define TAG "ksu_susfs_next"

/* == KSU_INSTALL_MAGIC1 in kernel/supercalls.h (v3.1.0-legacy-susfs) */
#define KERNEL_SU_OPTION 0xDEADBEEF
/* susfs/include/linux/susfs_def.h on shas-susfs-next */
#define SUSFS_MAGIC 0xFAFAFAFA

/* Command IDs -- verified against susfs/include/linux/susfs_def.h on
 * shas-susfs-next (values unchanged from the old susfs4ksu protocol). Only
 * IDs this tool actually uses (reachable or explicitly stubbed) are
 * defined here. */
#define CMD_SUSFS_ADD_SUS_PATH 0x55550
#define CMD_SUSFS_ADD_SUS_MOUNT 0x55560
#define CMD_SUSFS_HIDE_SUS_MNTS_FOR_NON_SU_PROCS 0x55561
#define CMD_SUSFS_ADD_SUS_KSTAT 0x55570
#define CMD_SUSFS_UPDATE_SUS_KSTAT 0x55571
#define CMD_SUSFS_ADD_SUS_KSTAT_STATICALLY 0x55572
#define CMD_SUSFS_ADD_TRY_UMOUNT 0x55580
#define CMD_SUSFS_SET_UNAME 0x55590
#define CMD_SUSFS_ENABLE_LOG 0x555a0
#define CMD_SUSFS_SET_CMDLINE_OR_BOOTCONFIG 0x555b0
#define CMD_SUSFS_ADD_OPEN_REDIRECT 0x555c0
#define CMD_SUSFS_SHOW_VERSION 0x555e1
#define CMD_SUSFS_SHOW_ENABLED_FEATURES 0x555e2
#define CMD_SUSFS_SHOW_VARIANT 0x555e3
#define CMD_SUSFS_ENABLE_AVC_LOG_SPOOFING 0x60010
/* Note: CMD_SUSFS_ADD_SUS_MAP, CMD_SUSFS_ADD_SUS_PATH_LOOP,
 * CMD_SUSFS_SET_ANDROID_DATA_ROOT_PATH, CMD_SUSFS_SET_SDCARD_ROOT_PATH are
 * deliberately NOT defined here -- they don't exist in the vendored
 * susfs/include/linux/susfs_def.h on shas-susfs-next at all (grepped
 * directly, zero matches), only referenced by kernel/supercalls.c's SUS_PATH
 * dispatch block, which is itself dead code while SUS_PATH is off. Since
 * their stubs below never call susfs_syscall(), no wire-format constant is
 * needed for them, so none is fabricated here. */

#define SUSFS_MAX_LEN_PATHNAME 256
#define SUSFS_ENABLED_FEATURES_SIZE 8192

#ifndef __NEW_UTS_LEN
#define __NEW_UTS_LEN 64
#endif

/******************
 ** Define Macro **
 ******************/
#define log(fmt, msg...) printf(TAG ":" fmt, ##msg)
#define NOT_SUPPORTED(cmdname) \
	do { \
		log("[-] '%s' is not supported on this kernel build " \
			"(CONFIG_KSU_SUSFS_* for this feature is disabled, or " \
			"this KernelSU-Next tag never implemented it -- see the " \
			"comment block at the top of this file / " \
			"susfs/ksu-next-susfs-verification.md)\n", cmdname); \
	} while (0)

/*******************
 ** Define Struct **
 *******************/
/* Every struct below is field-for-field verified against
 * susfs/include/linux/susfs.h on shas-susfs-next (Task 11, Step 2). The
 * ONLY divergence found from the old susfs4ksu main.c's inline copies:
 * st_susfs_sus_kstat.is_statically was `bool` (1 byte) in the old tool but
 * is `int` (4 bytes) in the kernel's authoritative struct -- fixed below to
 * `int` per the kernel's definition (the kernel's struct is what actually
 * gets copy_from_user'd, so it is authoritative). Everything else matched
 * exactly: same field order, same types, same SUSFS_MAX_LEN_PATHNAME (256).
 */
struct st_susfs_sus_kstat {
	int                     is_statically;
	unsigned long           target_ino;
	char                    target_pathname[SUSFS_MAX_LEN_PATHNAME];
	unsigned long           spoofed_ino;
	unsigned long           spoofed_dev;
	unsigned int            spoofed_nlink;
	long long               spoofed_size;
	long                    spoofed_atime_tv_sec;
	long                    spoofed_mtime_tv_sec;
	long                    spoofed_ctime_tv_sec;
	long                    spoofed_atime_tv_nsec;
	long                    spoofed_mtime_tv_nsec;
	long                    spoofed_ctime_tv_nsec;
	unsigned long           spoofed_blksize;
	unsigned long long      spoofed_blocks;
};

struct st_susfs_try_umount {
	char                    target_pathname[SUSFS_MAX_LEN_PATHNAME];
	int                     mnt_mode;
};

struct st_susfs_uname {
	char                    release[__NEW_UTS_LEN+1];
	char                    version[__NEW_UTS_LEN+1];
};

/* --- The following are NOT in the old susfs4ksu protocol at all -- they
 * are new to this KernelSU-Next tag's native susfs port (verified against
 * susfs/include/linux/susfs_def.h on shas-susfs-next). Note the `err`
 * field: the kernel copy_to_user()'s the WHOLE struct back, including
 * `err`, which is the only real success/failure signal this transport
 * offers (see the big comment block at the top of this file). */
struct st_susfs_hide_sus_mnts_for_non_su_procs {
	bool enabled;
	int err;
};

struct st_susfs_avc_log_spoofing {
	bool enabled;
	int err;
};

struct st_susfs_enabled_features {
	char enabled_features[SUSFS_ENABLED_FEATURES_SIZE];
	int err;
};

struct st_susfs_variant {
	char susfs_variant[16];
	int err;
};

struct st_susfs_version {
	char susfs_version[16];
	int err;
};

/**********************
 ** Define Functions **
 **********************/
void pre_check(void) {
	if (getuid() != 0) {
		log("[-] Must run as root\n");
		exit(1);
	}
}

int get_file_stat(char *pathname, struct stat *sb) {
	if (stat(pathname, sb) != 0) {
		return 1;
	}
	return 0;
}

void copy_stat_to_sus_kstat(struct st_susfs_sus_kstat *info, struct stat *sb) {
	info->spoofed_ino = sb->st_ino;
	info->spoofed_dev = sb->st_dev;
	info->spoofed_nlink = sb->st_nlink;
	info->spoofed_size = sb->st_size;
	info->spoofed_atime_tv_sec = sb->st_atime;
	info->spoofed_mtime_tv_sec = sb->st_mtime;
	info->spoofed_ctime_tv_sec = sb->st_ctime;
	info->spoofed_atime_tv_nsec = sb->st_atime_nsec;
	info->spoofed_mtime_tv_nsec = sb->st_mtime_nsec;
	info->spoofed_ctime_tv_nsec = sb->st_ctime_nsec;
	info->spoofed_blksize = sb->st_blksize;
	info->spoofed_blocks = sb->st_blocks;
}

/*
 * Transport: reboot(2) hijack. Return value is intentionally discarded by
 * every caller below -- see the big comment block at the top of this file
 * for why it is meaningless (the real reboot() syscall body always runs
 * afterward and rejects our fake magic1 with -EINVAL).
 */
static void susfs_syscall(unsigned int cmd, void *arg) {
	(void)syscall(__NR_reboot, KERNEL_SU_OPTION, SUSFS_MAGIC, cmd, arg);
}

static int parse_bool_arg(const char *s, bool *out) {
	if (!strcmp(s, "1")) { *out = true; return 0; }
	if (!strcmp(s, "0")) { *out = false; return 0; }
	return 1;
}

static void print_help(void) {
	log(" usage: %s <CMD> [CMD options]\n", TAG);
	log("    <CMD>:\n");
	log("        add_sus_kstat_statically </path/of/file_or_directory> <ino> <dev> <nlink> <size>\\\n");
	log("                                 <atime> <atime_nsec> <mtime> <mtime_nsec> <ctime> <ctime_nsec>\\\n");
	log("                                 <blocks> <blksize>\n");
	log("         |--> Pass 'default' for any numeric field to keep its original stat value\n");
	log("\n");
	log("        add_sus_kstat </path/of/file_or_directory>\n");
	log("         |--> Store original stat info in kernel memory BEFORE bind mount/overlay; complete with update_sus_kstat after\n");
	log("\n");
	log("        update_sus_kstat </path/of/file_or_directory>\n");
	log("        update_sus_kstat_full_clone </path/of/file_or_directory>\n");
	log("         |--> Complete the kstat spoofing started by add_sus_kstat\n");
	log("\n");
	log("        add_try_umount </path/of/file_or_directory> <mode>\n");
	log("         |--> <mode>: 0 -> umount with no flags, 1 -> umount with MNT_DETACH\n");
	log("\n");
	log("        set_uname <release> <version>\n");
	log("         |--> Pass 'default' to keep the kernel's original value for that field\n");
	log("\n");
	log("        set_cmdline_or_bootconfig </path/to/fake_cmdline_file/or/fake_bootconfig_file>\n");
	log("\n");
	log("        set_hide_sus_mnts_for_non_su_procs <0|1>\n");
	log("         |--> New in this KernelSU-Next tag (not present in the old prctl tool):\n");
	log("              hide sus mounts from /proc/self/mount* for non-su-allowed processes\n");
	log("\n");
	log("        enable_avc_log_spoofing <0|1>\n");
	log("         |--> New in this KernelSU-Next tag. NOTE: verified against fs/susfs.c that this\n");
	log("              kernel build has no code anywhere that actually reads this flag once set --\n");
	log("              the command is accepted and its `err` field will report success, but it has\n");
	log("              no behavioral effect (pre-existing gap in this tag's own kernel-side port, not\n");
	log("              introduced by this tool).\n");
	log("\n");
	log("        show <version|enabled_features|variant>\n");
	log("\n");
	log("    -- Not supported on this kernel build (Kconfig-disabled or absent from this KSU-Next\n");
	log("       tag's susfs port entirely) -- these will print an error and exit 1, not silently no-op:\n");
	log("        add_sus_path, add_sus_path_loop, add_sus_mount, add_open_redirect, enable_log,\n");
	log("        add_sus_map, set_android_data_root_path, set_sdcard_root_path, sus_su, run_try_umount\n");
}

/*******************
 ** Main Function **
 *******************/
int main(int argc, char *argv[]) {
	pre_check();

	/* ---- Reachable commands ---- */

	if (argc == 15 && !strcmp(argv[1], "add_sus_kstat_statically")) {
		struct st_susfs_sus_kstat info = {0};
		struct stat sb;
		char *endptr;
		unsigned long ino, dev, nlink, size, atime, atime_nsec, mtime, mtime_nsec, ctime, ctime_nsec, blksize;
		long blocks;

		if (get_file_stat(argv[2], &sb)) {
			log("[-] Failed to get stat from path: '%s'\n", argv[2]);
			return 1;
		}

		info.is_statically = 1;
		if (strcmp(argv[3], "default")) {
			ino = strtoul(argv[3], &endptr, 10);
			if (*endptr != '\0') { print_help(); return 1; }
			info.target_ino = sb.st_ino;
			sb.st_ino = ino;
		} else {
			info.target_ino = sb.st_ino;
		}
		if (strcmp(argv[4], "default")) {
			dev = strtoul(argv[4], &endptr, 10);
			if (*endptr != '\0') { print_help(); return 1; }
			sb.st_dev = dev;
		}
		if (strcmp(argv[5], "default")) {
			nlink = strtoul(argv[5], &endptr, 10);
			if (*endptr != '\0') { print_help(); return 1; }
			sb.st_nlink = nlink;
		}
		if (strcmp(argv[6], "default")) {
			size = strtoul(argv[6], &endptr, 10);
			if (*endptr != '\0') { print_help(); return 1; }
			sb.st_size = size;
		}
		if (strcmp(argv[7], "default")) {
			atime = strtol(argv[7], &endptr, 10);
			if (*endptr != '\0') { print_help(); return 1; }
			sb.st_atime = atime;
		}
		if (strcmp(argv[8], "default")) {
			atime_nsec = strtoul(argv[8], &endptr, 10);
			if (*endptr != '\0') { print_help(); return 1; }
			sb.st_atimensec = atime_nsec;
		}
		if (strcmp(argv[9], "default")) {
			mtime = strtol(argv[9], &endptr, 10);
			if (*endptr != '\0') { print_help(); return 1; }
			sb.st_mtime = mtime;
		}
		if (strcmp(argv[10], "default")) {
			mtime_nsec = strtoul(argv[10], &endptr, 10);
			if (*endptr != '\0') { print_help(); return 1; }
			sb.st_mtimensec = mtime_nsec;
		}
		if (strcmp(argv[11], "default")) {
			ctime = strtol(argv[11], &endptr, 10);
			if (*endptr != '\0') { print_help(); return 1; }
			sb.st_ctime = ctime;
		}
		if (strcmp(argv[12], "default")) {
			ctime_nsec = strtoul(argv[12], &endptr, 10);
			if (*endptr != '\0') { print_help(); return 1; }
			sb.st_ctimensec = ctime_nsec;
		}
		if (strcmp(argv[13], "default")) {
			blocks = strtoul(argv[13], &endptr, 10);
			if (*endptr != '\0') { print_help(); return 1; }
			sb.st_blocks = blocks;
		}
		if (strcmp(argv[14], "default")) {
			blksize = strtoul(argv[14], &endptr, 10);
			if (*endptr != '\0') { print_help(); return 1; }
			sb.st_blksize = blksize;
		}
		strncpy(info.target_pathname, argv[2], SUSFS_MAX_LEN_PATHNAME - 1);
		copy_stat_to_sus_kstat(&info, &sb);
		susfs_syscall(CMD_SUSFS_ADD_SUS_KSTAT_STATICALLY, &info);
		log("[i] add_sus_kstat_statically request sent for '%s' (no error feedback available on this transport)\n", argv[2]);
		return 0;

	} else if (argc == 3 && !strcmp(argv[1], "add_sus_kstat")) {
		struct st_susfs_sus_kstat info = {0};
		struct stat sb;

		if (get_file_stat(argv[2], &sb)) {
			log("[-] Failed to get stat from path: '%s'\n", argv[2]);
			return 1;
		}
		strncpy(info.target_pathname, argv[2], SUSFS_MAX_LEN_PATHNAME - 1);
		info.is_statically = 0;
		info.target_ino = sb.st_ino;
		copy_stat_to_sus_kstat(&info, &sb);
		susfs_syscall(CMD_SUSFS_ADD_SUS_KSTAT, &info);
		log("[i] add_sus_kstat request sent for '%s'\n", argv[2]);
		return 0;

	} else if (argc == 3 && !strcmp(argv[1], "update_sus_kstat")) {
		struct st_susfs_sus_kstat info = {0};
		struct stat sb;

		if (get_file_stat(argv[2], &sb)) {
			log("[-] Failed to get stat from path: '%s'\n", argv[2]);
			return 1;
		}
		strncpy(info.target_pathname, argv[2], SUSFS_MAX_LEN_PATHNAME - 1);
		info.is_statically = 0;
		info.target_ino = sb.st_ino;
		info.spoofed_size = sb.st_size;
		info.spoofed_blocks = sb.st_blocks;
		susfs_syscall(CMD_SUSFS_UPDATE_SUS_KSTAT, &info);
		log("[i] update_sus_kstat request sent for '%s'\n", argv[2]);
		return 0;

	} else if (argc == 3 && !strcmp(argv[1], "update_sus_kstat_full_clone")) {
		struct st_susfs_sus_kstat info = {0};
		struct stat sb;

		if (get_file_stat(argv[2], &sb)) {
			log("[-] Failed to get stat from path: '%s'\n", argv[2]);
			return 1;
		}
		strncpy(info.target_pathname, argv[2], SUSFS_MAX_LEN_PATHNAME - 1);
		info.is_statically = 0;
		info.target_ino = sb.st_ino;
		susfs_syscall(CMD_SUSFS_UPDATE_SUS_KSTAT, &info);
		log("[i] update_sus_kstat_full_clone request sent for '%s'\n", argv[2]);
		return 0;

	} else if (argc == 4 && !strcmp(argv[1], "add_try_umount")) {
		struct st_susfs_try_umount info = {0};
		char *endptr;
		char abs_path[PATH_MAX], *p_abs_path;

		strncpy(info.target_pathname, argv[2], SUSFS_MAX_LEN_PATHNAME - 1);
		p_abs_path = realpath(info.target_pathname, abs_path);
		if (p_abs_path == NULL) {
			perror("realpath");
			return 1;
		}
		if (!strcmp(p_abs_path, "/system") ||
		    !strcmp(p_abs_path, "/vendor") ||
		    !strcmp(p_abs_path, "/product") ||
		    !strcmp(p_abs_path, "/data/adb/modules") ||
		    !strcmp(p_abs_path, "/debug_ramdisk") ||
		    !strcmp(p_abs_path, "/sbin")) {
			log("[-] %s cannot be added to try_umount, because it will be umounted by ksu lastly\n", p_abs_path);
			return 1;
		}
		if (strcmp(argv[3], "0") && strcmp(argv[3], "1")) {
			print_help();
			return 1;
		}
		info.mnt_mode = (int)strtol(argv[3], &endptr, 10);
		if (*endptr != '\0') { print_help(); return 1; }
		susfs_syscall(CMD_SUSFS_ADD_TRY_UMOUNT, &info);
		log("[i] add_try_umount request sent for '%s'\n", p_abs_path);
		return 0;

	} else if (argc == 4 && !strcmp(argv[1], "set_uname")) {
		struct st_susfs_uname info = {0};

		strncpy(info.release, argv[2], __NEW_UTS_LEN);
		strncpy(info.version, argv[3], __NEW_UTS_LEN);
		susfs_syscall(CMD_SUSFS_SET_UNAME, &info);
		log("[i] set_uname request sent\n");
		return 0;

	} else if (argc == 3 && !strcmp(argv[1], "set_cmdline_or_bootconfig")) {
		char abs_path[PATH_MAX], *p_abs_path, *buffer;
		FILE *file;
		long file_size;
		size_t result;

		/*
		 * IMPORTANT: the kernel handler does a raw
		 * strncpy_from_user(fake_cmdline_or_bootconfig, *user_info, ...)
		 * on whatever pointer we pass -- it does NOT open/read a file on
		 * our behalf. The pointer we pass must therefore point at the
		 * actual desired /proc/cmdline or /proc/bootconfig TEXT CONTENT,
		 * not at a path string (confirmed by reading
		 * fs/susfs.c:susfs_set_cmdline_or_bootconfig() directly). We must
		 * read the file ourselves and pass its contents, exactly like the
		 * old prctl-based tool did -- passing the path string itself
		 * (an earlier draft of this file's mistake) would set the fake
		 * cmdline to the literal path text instead of its contents.
		 */
		p_abs_path = realpath(argv[2], abs_path);
		if (p_abs_path == NULL) {
			perror("realpath");
			return 1;
		}
		file = fopen(p_abs_path, "rb");
		if (file == NULL) {
			perror("Error opening file");
			return 1;
		}
		fseek(file, 0, SEEK_END);
		file_size = ftell(file);
		rewind(file);
		buffer = (char *)malloc((size_t)file_size + 1);
		if (buffer == NULL) {
			perror("No enough memory");
			fclose(file);
			return 1;
		}
		result = fread(buffer, 1, (size_t)file_size, file);
		if (result != (size_t)file_size) {
			perror("Reading error");
			fclose(file);
			free(buffer);
			return 1;
		}
		buffer[file_size] = '\0';
		fclose(file);
		susfs_syscall(CMD_SUSFS_SET_CMDLINE_OR_BOOTCONFIG, buffer);
		free(buffer);
		log("[i] set_cmdline_or_bootconfig request sent (source file '%s')\n", p_abs_path);
		return 0;

	} else if (argc == 3 && !strcmp(argv[1], "set_hide_sus_mnts_for_non_su_procs")) {
		struct st_susfs_hide_sus_mnts_for_non_su_procs info = {0};
		bool enabled;

		if (parse_bool_arg(argv[2], &enabled)) { print_help(); return 1; }
		info.enabled = enabled;
		susfs_syscall(CMD_SUSFS_HIDE_SUS_MNTS_FOR_NON_SU_PROCS, &info);
		if (info.err) {
			log("[-] set_hide_sus_mnts_for_non_su_procs failed, err=%d\n", info.err);
			return 1;
		}
		log("[+] hide_sus_mnts_for_non_su_procs set to %d\n", enabled);
		return 0;

	} else if (argc == 3 && !strcmp(argv[1], "enable_avc_log_spoofing")) {
		struct st_susfs_avc_log_spoofing info = {0};
		bool enabled;

		if (parse_bool_arg(argv[2], &enabled)) { print_help(); return 1; }
		info.enabled = enabled;
		susfs_syscall(CMD_SUSFS_ENABLE_AVC_LOG_SPOOFING, &info);
		if (info.err) {
			log("[-] enable_avc_log_spoofing failed, err=%d\n", info.err);
			return 1;
		}
		log("[+] avc_log_spoofing set to %d\n", enabled);
		return 0;

	} else if (argc == 3 && !strcmp(argv[1], "show")) {
		if (!strcmp(argv[2], "version")) {
			struct st_susfs_version info = {0};
			susfs_syscall(CMD_SUSFS_SHOW_VERSION, &info);
			if (info.err) {
				log("[-] show version failed, err=%d\n", info.err);
				return 1;
			}
			printf("%s\n", info.susfs_version);
			return 0;
		} else if (!strcmp(argv[2], "variant")) {
			struct st_susfs_variant info = {0};
			susfs_syscall(CMD_SUSFS_SHOW_VARIANT, &info);
			if (info.err) {
				log("[-] show variant failed, err=%d\n", info.err);
				return 1;
			}
			printf("%s\n", info.susfs_variant);
			return 0;
		} else if (!strcmp(argv[2], "enabled_features")) {
			struct st_susfs_enabled_features *info = calloc(1, sizeof(*info));
			if (!info) {
				perror("calloc");
				return 1;
			}
			susfs_syscall(CMD_SUSFS_SHOW_ENABLED_FEATURES, info);
			if (info->err) {
				log("[-] show enabled_features failed, err=%d\n", info->err);
				free(info);
				return 1;
			}
			printf("%s", info->enabled_features);
			free(info);
			return 0;
		}
		print_help();
		return 1;

	/* ---- Not reachable on this kernel build: Kconfig-disabled ---- */

	} else if (argc >= 2 && !strcmp(argv[1], "add_sus_path")) {
		NOT_SUPPORTED("add_sus_path"); return 1;
	} else if (argc >= 2 && !strcmp(argv[1], "add_sus_path_loop")) {
		NOT_SUPPORTED("add_sus_path_loop"); return 1;
	} else if (argc >= 2 && !strcmp(argv[1], "set_android_data_root_path")) {
		NOT_SUPPORTED("set_android_data_root_path"); return 1;
	} else if (argc >= 2 && !strcmp(argv[1], "set_sdcard_root_path")) {
		NOT_SUPPORTED("set_sdcard_root_path"); return 1;
	} else if (argc >= 2 && !strcmp(argv[1], "add_sus_mount")) {
		NOT_SUPPORTED("add_sus_mount"); return 1;
	} else if (argc >= 2 && !strcmp(argv[1], "enable_log")) {
		NOT_SUPPORTED("enable_log"); return 1;
	} else if (argc >= 2 && !strcmp(argv[1], "add_open_redirect")) {
		NOT_SUPPORTED("add_open_redirect"); return 1;
	} else if (argc >= 2 && !strcmp(argv[1], "add_sus_map")) {
		NOT_SUPPORTED("add_sus_map"); return 1;

	/* ---- Not reachable: absent from this KSU-Next tag's susfs port
	 * entirely (no dispatch case in kernel/supercalls.c at all, verified
	 * directly -- not even Kconfig-gated) ---- */

	} else if (argc >= 2 && !strcmp(argv[1], "sus_su")) {
		NOT_SUPPORTED("sus_su"); return 1;
	} else if (argc >= 2 && !strcmp(argv[1], "run_try_umount")) {
		NOT_SUPPORTED("run_try_umount"); return 1;

	} else {
		print_help();
	}
	return 0;
}
