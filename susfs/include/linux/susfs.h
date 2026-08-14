#ifndef KSU_SUSFS_H
#define KSU_SUSFS_H

#include <linux/version.h>
#include <linux/types.h>
#include <linux/utsname.h>
#include <linux/hashtable.h>
#include <linux/path.h>
#include <linux/susfs_def.h>

#define SUSFS_VERSION "v1.5.5"
#if LINUX_VERSION_CODE < KERNEL_VERSION(5,0,0)
#define SUSFS_VARIANT "NON-GKI"
#else
#define SUSFS_VARIANT "GKI"
#endif

/*********/
/* MACRO */
/*********/
#define getname_safe(name) (name == NULL ? ERR_PTR(-EINVAL) : getname(name))
#define putname_safe(name) (IS_ERR(name) ? NULL : putname(name))

/**********/
/* STRUCT */
/**********/
/* sus_path */
#ifdef CONFIG_KSU_SUSFS_SUS_PATH
struct st_susfs_sus_path {
	unsigned long                    target_ino;
	char                             target_pathname[SUSFS_MAX_LEN_PATHNAME];
	unsigned int                     i_uid;
	int                              err;
};

struct st_susfs_sus_path_hlist {
	unsigned long                    target_ino;
	char                             target_pathname[SUSFS_MAX_LEN_PATHNAME];
	struct hlist_node                node;
};

struct st_susfs_sus_path_list {
	struct list_head                 list;
	struct st_susfs_sus_path         info;
	char                             target_pathname[SUSFS_MAX_LEN_PATHNAME];
	size_t                           path_len;
};

struct st_external_dir {
	char                             target_pathname[SUSFS_MAX_LEN_PATHNAME];
	bool                             is_inited;
	int                              cmd;
	int                              err;
};
#endif

/* sus_mount */
#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
struct st_susfs_sus_mount {
	char                    target_pathname[SUSFS_MAX_LEN_PATHNAME];
	unsigned long           target_dev;
};

struct st_susfs_sus_mount_list {
	struct list_head                        list;
	struct st_susfs_sus_mount               info;
};
#endif

/* sus_kstat */
#ifdef CONFIG_KSU_SUSFS_SUS_KSTAT
struct st_susfs_sus_kstat {
	int                     is_statically;
	unsigned long           target_ino; // the ino after bind mounted or overlayed
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

struct st_susfs_sus_kstat_hlist {
	unsigned long                           target_ino;
	struct st_susfs_sus_kstat               info;
	struct hlist_node                       node;
};
#endif

/* try_umount */
#ifdef CONFIG_KSU_SUSFS_TRY_UMOUNT
struct st_susfs_try_umount {
	char                    target_pathname[SUSFS_MAX_LEN_PATHNAME];
	int                     mnt_mode;
};

struct st_susfs_try_umount_list {
	struct list_head                        list;
	struct st_susfs_try_umount              info;
};
#endif

/* spoof_uname */
#ifdef CONFIG_KSU_SUSFS_SPOOF_UNAME
struct st_susfs_uname {
	char        release[__NEW_UTS_LEN+1];
	char        version[__NEW_UTS_LEN+1];
};
#endif

/* open_redirect */
#ifdef CONFIG_KSU_SUSFS_OPEN_REDIRECT
struct st_susfs_open_redirect {
	unsigned long                    target_ino;
	char                             target_pathname[SUSFS_MAX_LEN_PATHNAME];
	char                             redirected_pathname[SUSFS_MAX_LEN_PATHNAME];
	int                              err;
};

struct st_susfs_open_redirect_hlist {
	unsigned long                    target_ino;
	char                             target_pathname[SUSFS_MAX_LEN_PATHNAME];
	char                             redirected_pathname[SUSFS_MAX_LEN_PATHNAME];
	struct hlist_node                node;
};
#endif

/* sus_map */
#ifdef CONFIG_KSU_SUSFS_SUS_MAP
struct st_susfs_sus_map {
	char                             target_pathname[SUSFS_MAX_LEN_PATHNAME];
	int                              err;
};
#endif

/* sus_su */
#ifdef CONFIG_KSU_SUSFS_SUS_SU
struct st_sus_su {
	int         mode;
};
#endif

/***********************/
/* FORWARD DECLARATION */
/***********************/
/* sus_path */
#ifdef CONFIG_KSU_SUSFS_SUS_PATH
void susfs_add_sus_path(void __user **user_info);
void susfs_add_sus_path_loop(void __user **user_info);
void susfs_set_i_state_on_external_dir(void __user **user_info);
int susfs_sus_ino_for_filldir64(unsigned long ino);
#endif
/* sus_mount */
#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
int susfs_add_sus_mount(struct st_susfs_sus_mount* __user user_info);
#ifdef CONFIG_KSU_SUSFS_AUTO_ADD_SUS_BIND_MOUNT
int susfs_auto_add_sus_bind_mount(const char *pathname, struct path *path_target);
#endif // #ifdef CONFIG_KSU_SUSFS_AUTO_ADD_SUS_BIND_MOUNT
#ifdef CONFIG_KSU_SUSFS_AUTO_ADD_SUS_KSU_DEFAULT_MOUNT
void susfs_auto_add_sus_ksu_default_mount(const char __user *to_pathname);
#endif // #ifdef CONFIG_KSU_SUSFS_AUTO_ADD_SUS_KSU_DEFAULT_MOUNT
/*
 * Called from KernelSU-Next's own kernel/setuid_hook.c, externed and
 * called there under CONFIG_KSU_SUSFS_SUS_MOUNT (confirmed directly
 * against the real setuid_hook.c source at v3.1.0-legacy-susfs) -- NOT
 * CONFIG_KSU_SUSFS_TRY_UMOUNT, which is a separate, independently
 * toggleable flag. Documented no-op, see fs/susfs.c (Task 9, round 5
 * review fix; originally added under the wrong guard in round 4).
 */
void susfs_reorder_mnt_id(void);
#endif // #ifdef CONFIG_KSU_SUSFS_SUS_MOUNT

/* sus_kstat */
#ifdef CONFIG_KSU_SUSFS_SUS_KSTAT
/*
 * Task 9 fix (run 31584682068): KernelSU-Next v3.1.0-legacy-susfs's
 * kernel/supercalls.c calls every susfs ioctl handler as fn(arg), where
 * arg is uniformly typed void __user **, not the single struct pointer
 * our vendored (older) susfs4ksu source expected. Signature changed from
 * struct st_susfs_sus_kstat* __user to void __user ** to match the
 * caller; body dereferences *user_info. Same change applied below to
 * susfs_add_try_umount, susfs_set_uname, susfs_set_cmdline_or_bootconfig
 * -- the other 4 call sites that hit this exact
 * -Werror,-Wincompatible-pointer-types error in that run's log.
 */
int susfs_add_sus_kstat(void __user **user_info);
int susfs_update_sus_kstat(void __user **user_info);
void susfs_sus_ino_for_generic_fillattr(unsigned long ino, struct kstat *stat);
void susfs_sus_ino_for_show_map_vma(unsigned long ino, dev_t *out_dev, unsigned long *out_ino);
#endif
/* try_umount */
#ifdef CONFIG_KSU_SUSFS_TRY_UMOUNT
int susfs_add_try_umount(void __user **user_info);
void susfs_try_umount(uid_t target_uid);
/*
 * Thin alias for KernelSU-Next's fs/namespace.c call site (added by
 * 50_add_susfs_in_kernel-4.14-mtk.patch) -- naming-only gap vs the
 * susfs_try_umount() this file already defines (Task 9, round 4 link-fix).
 */
void susfs_try_umount_all(uid_t uid);
#ifdef CONFIG_KSU_SUSFS_AUTO_ADD_TRY_UMOUNT_FOR_BIND_MOUNT
void susfs_auto_add_try_umount_for_bind_mount(struct path *path);
#endif // #ifdef CONFIG_KSU_SUSFS_AUTO_ADD_TRY_UMOUNT_FOR_BIND_MOUNT
#endif // #ifdef CONFIG_KSU_SUSFS_TRY_UMOUNT
/* spoof_uname */
#ifdef CONFIG_KSU_SUSFS_SPOOF_UNAME
int susfs_set_uname(void __user **user_info);
void susfs_spoof_uname(struct new_utsname* tmp);
#endif
/* set_log */
#ifdef CONFIG_KSU_SUSFS_ENABLE_LOG
struct st_susfs_log {
	bool	enabled;
	int	err;
};
#endif
void susfs_set_log(bool enabled);
void susfs_enable_log(void __user **user_info);
#endif
/* spoof_cmdline_or_bootconfig */
#ifdef CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG
int susfs_set_cmdline_or_bootconfig(void __user **user_fake_boot_config);
int susfs_spoof_cmdline_or_bootconfig(struct seq_file *m);
#endif
/* open_redirect */
#ifdef CONFIG_KSU_SUSFS_OPEN_REDIRECT
void susfs_add_open_redirect(void __user **user_info);
struct filename* susfs_get_redirected_path(unsigned long ino);
#endif
/* sus_map */
#ifdef CONFIG_KSU_SUSFS_SUS_MAP
void susfs_add_sus_map(void __user **user_info);
#endif
/* sus_su */
#ifdef CONFIG_KSU_SUSFS_SUS_SU
int susfs_get_sus_su_working_mode(void);
int susfs_sus_su(struct st_sus_su* __user user_info);
#endif
/*
 * These 5 are called unconditionally by KernelSU-Next's
 * kernel/supercalls.c whenever CONFIG_KSU_SUSFS=y (not gated behind any
 * CONFIG_KSU_SUSFS_SUS_* sub-flag) -- added for the v3.1.0-legacy-susfs
 * pin (Task 9), since our vendored susfs4ksu kernel-4.14 source predates
 * this KSU-Next tag and never defined them.
 */
void susfs_set_hide_sus_mnts_for_non_su_procs(void __user **user_info);
void susfs_set_avc_log_spoofing(void __user **user_info);
void susfs_get_enabled_features(void __user **user_info);
void susfs_show_variant(void __user **user_info);
void susfs_show_version(void __user **user_info);
/* susfs_init */
void susfs_init(void);

#endif
