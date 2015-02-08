
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/pagemap.h>
#include <linux/mm.h>

#include "sshfs.h"

#define DEBUG
#define SSHFS_DEFAULT_MODE 0755

static const struct inode_operations sshfs_dir_inode_operations;
static const struct super_operations sshfs_ops;

struct inode *sshfs_get_inode(struct super_block *sb,
                              const struct inode *dir,
                              umode_t mode,
                              dev_t dev)
{
    struct inode *inode = new_inode(sb);

    if (inode)
    {
        inode->i_ino = get_next_ino();
        inode_init_owner(inode, dir, mode);
        inode->i_atime
            = inode->i_ctime
            = inode->i_mtime
            = CURRENT_TIME;

        // initialize inode with different modes
        switch (mode & S_IFMT)
        {
            default:
                init_special_inode(inode, mode, dev);
                break;
            case S_IFREG:
                inode->i_op  = &sshfs_file_inode_operations;
                inode->i_fop = &sshfs_file_operations;
                break;
            case S_IFDIR:
                inode->i_op = &sshfs_dir_inode_operations;
                inode->i_fop = &simple_dir_operations;
                inc_nlink(inode);
                break;
            case S_IFLNK:
                inode->i_op = &page_symlink_inode_operations;
                break;
        }
    }
    return inode;
}

static int sshfs_mknod(struct inode *dir,
                       struct dentry *dentry,
                       umode_t mode,
                       dev_t dev)
{
    struct inode* inode = sshfs_get_inode(dir->i_sb, dir, mode, dev);
    int err = -ENOSPC;

    if (inode)
    {
        // fill the dentry by inode's data
        d_instantiate(dentry, inode);
        // SPECIAL: PIN THE DENTRY IN CORE;
        dget(dentry);
        err = 0;
        dir->i_mtime = dir->i_ctime = CURRENT_TIME;
    }
    return err;
}

static int sshfs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode)
{
    int retval = sshfs_mknod(dir, dentry, mode | S_IFDIR, 0);
    if (!retval)
        inc_nlink(dir);
    return retval;
}

static int sshfs_create(struct inode *dir,
                        struct dentry *dentry,
                        umode_t mode,
                        bool excl)
{
    return sshfs_mknod(dir, dentry, mode | S_IFREG, 0);
}

static const struct inode_operations sshfs_dir_inode_operations = {
    .mknod  = sshfs_mknod,
    .mkdir  = sshfs_mkdir,
    .create = sshfs_create,
    .lookup = simple_lookup,
    .link   = simple_link,
    .unlink = simple_unlink,
    .rmdir  = simple_rmdir,
    .rename = simple_rename,
};

static const struct super_operations sshfs_ops = {
    .statfs       = simple_statfs,
    .drop_inode   = generic_delete_inode,
    .show_options = generic_show_options,
};

int sshfs_fill_super(struct super_block *sb, void *data, int silent)
{
    struct inode *inode;
    //int err;

    sb->s_maxbytes       = MAX_LFS_FILESIZE;
    sb->s_blocksize      = PAGE_CACHE_SIZE;
    sb->s_blocksize_bits = PAGE_CACHE_SHIFT;
    sb->s_magic          = SSHFS_MAGIC;
    sb->s_op             = &sshfs_ops;

    inode = sshfs_get_inode(sb, NULL, S_IFDIR|SSHFS_DEFAULT_MODE, 0);

    sb->s_root = d_make_root(inode);
    if (!sb->s_root)
        return -ENOMEM;

    return 0;
}

struct dentry *sshfs_mount(struct file_system_type *fs_type,
                           int flags,
                           const char *dev_name,
                           void *data)
{
    return mount_nodev(fs_type, flags, data, sshfs_fill_super);
}

static void sshfs_kill_sb(struct super_block *sb)
{
    kill_litter_super(sb);
}

static struct file_system_type sshfs_fs_type = {
    .name     = "kernelsshfs",
    .mount    = sshfs_mount,
    .kill_sb  = sshfs_kill_sb,
    .fs_flags = FS_USERNS_MOUNT, // what for?
};

static int __init init_sshfs_fs(void )
{
    int err;

    err = register_filesystem(&sshfs_fs_type);
#ifdef DEBUG
    if (err)
        printk(KERN_ERR "Failed to ragister sshfs: Error[%d]\n", err);
    else
        printk(KERN_INFO "Successfully registered sshfs.\n");
#endif

    return err;
}

static void __exit exit_sshfs_fs(void )
{
    int err;

    err = unregister_filesystem(&sshfs_fs_type);
#ifdef DEBUG
    if (err)
        printk(KERN_ERR "Failed to unregister sshfs: Error[%d]\n",
                err);
    else
        printk(KERN_INFO "Successfully unregistered sshfs.\n");
#endif

}

module_init(init_sshfs_fs);
module_exit(exit_sshfs_fs);
