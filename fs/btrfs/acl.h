/* SPDX-License-Identifier: GPL-2.0 */

#ifndef BTRFS_ACL_H
#define BTRFS_ACL_H

#include <linux/types.h>

struct posix_acl;
struct inode;
struct btrfs_trans_handle;

#ifdef CONFIG_BTRFS_FS_POSIX_ACL

struct mnt_idmap;
struct dentry;

struct posix_acl *btrfs_get_acl(struct inode *inode, int type, bool rcu);
int btrfs_set_acl(struct mnt_idmap *idmap, struct dentry *dentry,
		  struct posix_acl *acl, int type);
int __btrfs_set_acl(struct btrfs_trans_handle *trans, struct inode *inode,
		    struct posix_acl *acl, int type);

#else

#include <linux/errno.h>

struct btrfs_trans_handle;

#define btrfs_get_acl NULL
#define btrfs_set_acl NULL
static inline int __btrfs_set_acl(struct btrfs_trans_handle *trans,
				  struct inode *inode, struct posix_acl *acl,
				  int type)
{
	return -EOPNOTSUPP;
}

#endif

#endif
