
#ifndef SSHFS_H__
#define SSHFS_H__

#define SSHFS_MAGIC 0x20150208

const struct file_operations sshfs_file_operations = {
    .read         = do_sync_read,
    .write        = do_sync_write,
    .aio_read     = generic_file_aio_read,
    .aio_write    = generic_file_aio_write,
    .mmap         = generic_file_mmap,
    .fsync        = noop_fsync,
    .splice_read  = generic_file_splice_read,
    .splice_write = generic_file_splice_write,
    .llseek       = generic_file_llseek,
};

const struct inode_operations sshfs_file_inode_operations = {
    .setattr = simple_setattr,
    .getattr = simple_getattr,
};
#endif
