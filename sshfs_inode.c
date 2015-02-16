
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/pagemap.h>
#include <linux/mm.h>
#include <linux/parser.h>
#include <linux/string.h>

#include "sshfs_netlink.h"
#include "sshfs.h"

#define DEBUG
#define SSHFS_DEFAULT_MODE 0755

static const struct inode_operations sshfs_dir_inode_operations;
static const struct super_operations sshfs_ops;

// quite unstable
static int sshfs_get_dentry_link(struct dentry *dentry, char *dentry_link)
{
    struct dentry *parent;
    int i;
    int len = 0;
    int idx = 1;
    char dentry_name[MAX_DENTRY_NAME_NUMBER][MAX_DENTRY_NAME_LENGTH];

    strcpy(dentry_name[0], dentry->d_name.name);
    parent = dentry->d_parent;
    while (parent->d_name.name[0] != '/')
    {
        strcpy(dentry_name[idx++], parent->d_name.name);
        parent = parent->d_parent;
    }

    for (i = idx-1; i >= 0; i--)
    {
        strcpy(dentry_link+len, dentry_name[i]);
        len += strlen(dentry_name[i]);
        if (i != 0)
            dentry_link[len] = '/';
        else
            dentry_link[len] = '\0';
        len ++;
    }

    return 0;
}

static int sshfs_remote_mkdir(struct dentry *dentry, umode_t mode)
{
    int err;
    char dentry_link[MAX_DENTRY_LINK_LENGTH];

    sshfs_get_dentry_link(dentry, dentry_link);

    err = netlink_send_command(Cmd_mkdir, dentry_link, NULL);

    return err;
}

static int sshfs_remote_create(struct dentry *dentry, umode_t mode)
{
    int err;
    char dentry_link[MAX_DENTRY_LINK_LENGTH];

    sshfs_get_dentry_link(dentry, dentry_link);

    err = netlink_send_command(Cmd_create, dentry_link, NULL);

    return err;
}

static int sshfs_remote_rmdir(struct dentry *dentry)
{
    int err;
    char dentry_link[MAX_DENTRY_LINK_LENGTH];
    sshfs_get_dentry_link(dentry, dentry_link);

    err = netlink_send_command(Cmd_rmdir, dentry_link, NULL);

    return err;
}

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

#ifdef DEBUG
    printk(KERN_INFO "Entered: %s\n", __FUNCTION__);
#endif

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
    int err;

#ifdef DEBUG
    printk(KERN_INFO "Entered: %s\n", __FUNCTION__);
#endif

    err = sshfs_mknod(dir, dentry, mode | S_IFDIR, 0);
    // the only 2 differences between the mkdir and the create is:
    // 1. one is regular file, the other one is dir
    // 2. one should increase number of links
    if (!err)
        inc_nlink(dir);

    err = sshfs_remote_mkdir(dentry, mode);

    return err;
}

static int sshfs_create(struct inode *dir,
                        struct dentry *dentry,
                        umode_t mode,
                        bool excl)
{
    int ret = sshfs_mknod(dir, dentry, mode | S_IFREG, 0);

#ifdef DEBUG
    printk(KERN_INFO "Entered: %s\n", __FUNCTION__);
#endif

    ret = sshfs_remote_create(dentry, mode);
    return ret;
}

static int sshfs_rmdir(struct inode *dir, struct dentry *dentry)
{
    int ret;

    if (!simple_empty(dentry))
        return -ENOTEMPTY;

    ret = sshfs_remote_rmdir(dentry);

    drop_nlink(dentry->d_inode);
    simple_unlink(dir, dentry);
    drop_nlink(dir);
    return ret;
}

static const struct inode_operations sshfs_dir_inode_operations = {
    .mknod  = sshfs_mknod,
    .mkdir  = sshfs_mkdir,
    .rmdir  = sshfs_rmdir,
    .create = sshfs_create,
    .lookup = simple_lookup,
    .link   = simple_link,
    .unlink = simple_unlink,
    .rename = simple_rename,
};

static const struct super_operations sshfs_ops = {
    .statfs       = simple_statfs,
    .drop_inode   = generic_delete_inode,
    .show_options = generic_show_options,
};

enum {
    Opt_host,
    Opt_err
};

static const match_table_t tokens = {
    {Opt_host, "host=%s"},
    {Opt_err, NULL}
};

struct sshfs_fs_info {
    char host[MAX_HOST_LENGTH];
};

static int sshfs_parse_options(char *data, struct sshfs_fs_info* fs_info)
{
    substring_t args[MAX_OPT_ARGS];
    int token;
    char *p;

#ifdef DEBUG
    printk(KERN_INFO "mount options are: %s\n", data);
#endif

    fs_info->host[0] = '\0';
    while ((p = strsep(&data, ",")) != NULL)
    {
        if (!*p)
            continue;
        token = match_token(p, tokens, args);
        switch (token)
        {
            case Opt_host:
                // for "host=" has length 5...
                strncpy(fs_info->host, p+5, MAX_HOST_LENGTH);
                break;
        }
    }


    return 0;
}

int sshfs_fill_super(struct super_block *sb, void *data, int silent)
{
    struct sshfs_fs_info *fsi;
    struct inode *inode;
    int err;

    // In sshfs, there're quite a number of options
    // or none of them...
    save_mount_options(sb, data);

    fsi = kzalloc(sizeof(struct sshfs_fs_info), GFP_KERNEL);
    if (!fsi)
        return -ENOMEM;

    err = sshfs_parse_options(data, fsi);
    if (err)
        return err;
    sb->s_fs_info = fsi;
#ifdef DEBUG
    printk(KERN_INFO "fs_info->host=%s\n",((struct sshfs_fs_info*)sb->s_fs_info)->host);
#endif

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

    cmd_msg_buffer.ready = false;
    nl_sk = netlink_kernel_create(&init_net, NETLINK_USERSOCK, &cfg);
    if (!nl_sk)
    {
        printk(KERN_ALERT "Error creating socket\n");
        return -10;
    }

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
    netlink_kernel_release(nl_sk);
}

module_init(init_sshfs_fs);
module_exit(exit_sshfs_fs);
