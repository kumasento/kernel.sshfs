
#ifndef SSHFS_H__
#define SSHFS_H__

#define SSHFS_MAGIC 0x20150208
#define MAX_HOST_LENGTH 20
#define MAX_DENTRY_LINK_LENGTH 512
#define MAX_DENTRY_NAME_LENGTH 64
#define MAX_DENTRY_NAME_NUMBER 8

ssize_t sshfs_file_read(struct file* flip, char __user *buf, size_t len, loff_t *ppos)
{
    printk(KERN_INFO "Entering %s\n", __FUNCTION__);
    return 0;
}

const struct file_operations sshfs_file_operations = {
    .read    = sshfs_file_read,
};

const struct inode_operations sshfs_file_inode_operations = {
    .setattr = simple_setattr,
    .getattr = simple_getattr,
};
#endif
