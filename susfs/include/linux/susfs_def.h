#ifndef KSU_SUSFS_DEF_H
#define KSU_SUSFS_DEF_H

#include <linux/bits.h>

/********/
/* ENUM */
/********/
/* shared with userspace ksu_susfs tool */
#define CMD_SUSFS_ADD_SUS_PATH 0x55550
#define CMD_SUSFS_SET_ANDROID_DATA_ROOT_PATH 0x55551
#define CMD_SUSFS_SET_SDCARD_ROOT_PATH 0x55552
#define CMD_SUSFS_ADD_SUS_PATH_LOOP 0x55553
#define CMD_SUSFS_ADD_SUS_MOUNT 0x55560
#define CMD_SUSFS_ADD_SUS_KSTAT 0x55570
#define CMD_SUSFS_UPDATE_SUS_KSTAT 0x55571
#define CMD_SUSFS_ADD_SUS_KSTAT_STATICALLY 0x55572
#define CMD_SUSFS_ADD_TRY_UMOUNT 0x55580
#define CMD_SUSFS_SET_UNAME 0x55590
#define CMD_SUSFS_ENABLE_LOG 0x555a0
#define CMD_SUSFS_SET_CMDLINE_OR_BOOTCONFIG 0x555b0
#define CMD_SUSFS_ADD_OPEN_REDIRECT 0x555c0
#define CMD_SUSFS_RUN_UMOUNT_FOR_CURRENT_MNT_NS 0x555d0
#define CMD_SUSFS_SHOW_VERSION 0x555e1
#define CMD_SUSFS_SHOW_ENABLED_FEATURES 0x555e2
#define CMD_SUSFS_SHOW_VARIANT 0x555e3
#define CMD_SUSFS_SHOW_SUS_SU_WORKING_MODE 0x555e4
#define CMD_SUSFS_IS_SUS_SU_READY 0x555f0
#define CMD_SUSFS_SUS_SU 0x60000
#define CMD_SUSFS_ADD_SUS_MAP 0x60020

#define SUSFS_MAX_LEN_PATHNAME 256 // 256 should address many paths already unless you are doing some strange experimental stuff, then set your own desired length
#define SUSFS_FAKE_CMDLINE_OR_BOOTCONFIG_SIZE 4096

#define TRY_UMOUNT_DEFAULT 0 /* used by susfs_try_umount() */
#define TRY_UMOUNT_DETACH 1 /* used by susfs_try_umount() */

#define SUS_SU_DISABLED 0
#define SUS_SU_WITH_OVERLAY 1 /* deprecated */
#define SUS_SU_WITH_HOOKS 2

#define DEFAULT_SUS_MNT_ID 100000 /* used by mount->mnt_id */
#define DEFAULT_SUS_MNT_ID_FOR_KSU_PROC_UNSHARE 1000000 /* used by vfsmount->susfs_mnt_id_backup */
#define DEFAULT_SUS_MNT_GROUP_ID 1000 /* used by mount->mnt_group_id */

/*
 * inode->i_state => storing flag 'INODE_STATE_'
 * mount->mnt.susfs_mnt_id_backup => storing original mnt_id of normal mounts or custom sus mnt_id of sus mounts
 * task_struct->susfs_last_fake_mnt_id => storing last valid fake mnt_id
 * task_struct->susfs_task_state => storing flag 'TASK_STRUCT_'
 */

#define INODE_STATE_SUS_PATH BIT(24)
#define INODE_STATE_SUS_MOUNT BIT(25)
#define INODE_STATE_SUS_KSTAT BIT(26)
#define INODE_STATE_OPEN_REDIRECT BIT(27)
#define INODE_STATE_ANDROID_DATA_ROOT_DIR BIT(28)
#define INODE_STATE_SDCARD_ROOT_DIR BIT(29)
#define INODE_STATE_SUS_MAP BIT(30)

#define TASK_STRUCT_NON_ROOT_USER_APP_PROC BIT(24)

#define MAGIC_MOUNT_WORKDIR "/debug_ramdisk/workdir"
#define DATA_ADB_UMOUNT_FOR_ZYGOTE_SYSTEM_PROCESS "/data/adb/susfs_umount_for_zygote_system_process"
#define DATA_ADB_NO_AUTO_ADD_SUS_BIND_MOUNT "/data/adb/susfs_no_auto_add_sus_bind_mount"
#define DATA_ADB_NO_AUTO_ADD_SUS_KSU_DEFAULT_MOUNT "/data/adb/susfs_no_auto_add_sus_ksu_default_mount"
#define DATA_ADB_NO_AUTO_ADD_TRY_UMOUNT_FOR_BIND_MOUNT "/data/adb/susfs_no_auto_add_try_umount_for_bind_mount"

/*
 * KernelSU-Next v3.1.0-legacy-susfs's drivers/kernelsu/setuid_hook.c calls
 * susfs_set_current_proc_umounted(), which does not exist in this project's
 * vendored susfs4ksu kernel-4.14 branch (HEAD dated 2025-02-23, predates this
 * KSU-Next tag). Backported from susfs4ksu's gki-android12-5.10 branch's
 * susfs_def.h. TIF_PROC_UMOUNTED=33 verified safe on this kernel: every
 * existing TIF_* bit in arch/arm64/include/asm/thread_info.h tops out at 24
 * (TIF_TAGGED_ADDR), thread_info->flags is a 64-bit unsigned long on arm64,
 * and this flag is only ever explicitly tested/set via
 * test_thread_flag()/set_thread_flag() -- it is not part of any
 * _TIF_WORK_MASK-style auto-processed mask, so adding it cannot perturb
 * existing scheduler/signal behavior. set_thread_flag() comes from
 * <linux/thread_info.h>, which setuid_hook.c already includes before this
 * header.
 */
#define TIF_PROC_UMOUNTED 33

static inline void susfs_set_current_proc_umounted(void) {
	set_thread_flag(TIF_PROC_UMOUNTED);
}

/*
 * Companion getter for the setter above. KernelSU-Next's
 * kernel/supercalls.c (do_manage_mark(), under #ifdef CONFIG_KSU_SUSFS)
 * calls this directly -- this was the actual first symbol named by run
 * 31582626614's build log; the setter alone (added in the previous fix
 * round) was not sufficient.
 */
static inline bool susfs_is_current_proc_umounted(void) {
	return (likely(test_thread_flag(TIF_PROC_UMOUNTED)));
}

/*
 * The remaining defines/structs below are for supercalls.c dispatch
 * entries that compile unconditionally under #ifdef CONFIG_KSU_SUSFS (not
 * gated by any CONFIG_KSU_SUSFS_SUS_* sub-flag), so unlike SUS_PATH they
 * can't be scoped out via defconfig -- confirmed by reading
 * KernelSU-Next's kernel/supercalls.c at this tag directly (lines ~858-950).
 */
#define SUSFS_MAGIC 0xFAFAFAFA

#define CMD_SUSFS_HIDE_SUS_MNTS_FOR_NON_SU_PROCS 0x55561
#define CMD_SUSFS_ENABLE_AVC_LOG_SPOOFING 0x60010
#define SUSFS_ENABLED_FEATURES_SIZE 8192

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

#endif // #ifndef KSU_SUSFS_DEF_H
