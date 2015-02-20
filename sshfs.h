
#ifndef SSHFS_H__
#define SSHFS_H__

#define SSHFS_MAGIC 0x20150208

#define MAX_COMMAND_LENGTH 16000
#define MAX_HOST_LENGTH 20
#define MAX_DENTRY_LINK_LENGTH 512
#define MAX_DENTRY_NAME_LENGTH 64
#define MAX_DENTRY_NAME_NUMBER 8

static int sshfs_get_dentry_link(struct dentry *dentry, char *dentry_link);

static ssize_t sshfs_file_read(struct file* flip, char __user *buf, size_t len, loff_t *ppos)
{
    char dentry_link[MAX_DENTRY_LINK_LENGTH];
    char *params;
    int paramslen = 0;
    struct iovec *iov;
    ssize_t ret;

    printk(KERN_INFO "Entering %s\n", __FUNCTION__);
    printk(KERN_INFO "%d\n", len);

    sshfs_get_dentry_link(flip->f_path.dentry, dentry_link);

    paramslen += strlen(dentry_link);
    paramslen += 30;

    params = (char *) kzalloc(sizeof(char) * (paramslen+1), GFP_KERNEL);

    sprintf(params, "%s %lld", dentry_link, *ppos);

    printk(KERN_INFO "Params to sftp_read: %s\n", params);

    iov =
        (struct iovec *) kzalloc(sizeof(struct iovec), GFP_KERNEL);
    netlink_send_command(Cmd_read, params, iov);

    printk(KERN_INFO "Received Message: %s %d\n",(char*) iov->iov_base, iov->iov_len);

    if(copy_to_user(buf, (void*)iov->iov_base, iov->iov_len))
        return -EFAULT;

    *ppos += iov->iov_len;
    ret = (ssize_t) iov->iov_len - 1;

    kzfree(params);
    kzfree(iov);

    printk(KERN_INFO "%u\n", ret);

    return ret;
}

static ssize_t sshfs_file_write(struct file *flip, const char __user *buf, size_t size, loff_t *ppos)
{
    char dentry_link[MAX_DENTRY_LINK_LENGTH];
    char *params;
    char *kernbuf;
    int buflen;
    int paramslen = 0;
    struct iovec * iov;

    printk(KERN_INFO "Entering %s\n", __FUNCTION__);

    sshfs_get_dentry_link(flip->f_path.dentry, dentry_link);

    buflen = size;
    if (buflen >= MAX_COMMAND_LENGTH)
        buflen = MAX_COMMAND_LENGTH+1;

    kernbuf = (char *) kzalloc(sizeof(char) * buflen, GFP_KERNEL);

    copy_from_user((void *) kernbuf, buf, buflen);

    kernbuf[buflen] = '\0';

    paramslen += strlen(dentry_link);
    paramslen += buflen;
    paramslen += 30;

    params = (char *) kzalloc(sizeof(char) * (paramslen+1), GFP_KERNEL);

    sprintf(params, "%s %lld %s", dentry_link, *ppos, kernbuf);

    params[paramslen] = '\0';

    printk(KERN_INFO "Params: %s\n", params);

    iov =
        (struct iovec *) kzalloc(sizeof(struct iovec), GFP_KERNEL);
    netlink_send_command(Cmd_write, params, iov);

    printk(KERN_INFO "Received Message: %s %d\n",(char*) iov->iov_base, iov->iov_len);

    *ppos += buflen;

    return (size_t) buflen;
}

static loff_t sshfs_llseek(struct file *flip, loff_t offset, int orig)
{
    printk(KERN_INFO "Entering %s\n", __FUNCTION__);

    return 0;
}

const struct file_operations sshfs_file_operations = {
    .read    = sshfs_file_read,
    .write   = sshfs_file_write,
    .llseek  = sshfs_llseek,
    .fsync   = noop_fsync,
};

const struct inode_operations sshfs_file_inode_operations = {
    .setattr = simple_setattr,
    .getattr = simple_getattr,
};
#endif
