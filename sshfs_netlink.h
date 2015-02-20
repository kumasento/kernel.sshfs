#ifndef NETLINK_H__
#define NETLINK_H__

#include <linux/sched.h>
#include <linux/netlink.h>
#include <linux/delay.h>
#include <asm/spinlock.h>
#include <net/sock.h>
#include <net/net_namespace.h>

#define NLDEBUG
#define NETLINK_SEND_COMMAND_LENGTH 16383
#define NETLINK_SEND_COMMAND_QUEUE_SIZE 128

static struct sock *nl_sk = NULL;

struct command_msg_buffer
{
    char *cmd;
    char *msg;
    int cmdlen;
    int msglen;
    bool exec;
    bool ready;
};

struct command_msg_buffer cmd_msg_buffer;

typedef enum {
    Cmd_mkdir,
    Cmd_rmdir,
    Cmd_create,
    Cmd_read,
    Cmd_write
} cmdtype;

static int netlink_send_command(cmdtype cmd,
                                char *params,
                                struct iovec *iov)
{
    char *cmdhdr = NULL;
    char *cmdstr = NULL;
    int cmdstr_len = 0;

    switch (cmd)
    {
        case Cmd_mkdir:
            cmdhdr = (char *) "mkdir";
            break;
        case Cmd_rmdir:
            cmdhdr = (char *) "rmdir";
            break;
        case Cmd_create:
            cmdhdr = (char *) "create";
            break;
        case Cmd_read:
            cmdhdr = (char *) "read";
            break;
        case Cmd_write:
            cmdhdr = (char *) "write";
            break;
    }

    if (cmdhdr == NULL)
    {
    #ifdef NLDEBUG
        printk(KERN_ERR "No such command header: %d\n", cmd);
    #endif
        return -1;
    }

    cmdstr_len += strlen(cmdhdr);
    cmdstr_len += strlen(params);
    cmdstr_len ++;
    cmdstr = kzalloc(sizeof(char) * cmdstr_len, GFP_KERNEL);
    sprintf(cmdstr, "%s %s", cmdhdr, params);

#ifdef NLDEBUG
    printk(KERN_INFO "Command to remote host: %s\n", cmdstr);
#endif

    cmd_msg_buffer.cmd    = kzalloc(sizeof(char) * cmdstr_len, GFP_KERNEL);
    cmd_msg_buffer.cmdlen = cmdstr_len;
    cmd_msg_buffer.exec   = false;
    cmd_msg_buffer.msglen = 0;
    cmd_msg_buffer.msg    = NULL;

    strcpy(cmd_msg_buffer.cmd, cmdstr);

    cmd_msg_buffer.ready  = true;
    while(!cmd_msg_buffer.exec)
        msleep(1000);

#ifdef NLDEBUG
    printk(KERN_INFO "Received Message: %s\n", cmd_msg_buffer.msg);
#endif
    switch (cmd)
    {
        case Cmd_read:
            if (iov != NULL)
            {
                iov->iov_len = cmd_msg_buffer.msglen+1;
                iov->iov_base =
                    (char *) kzalloc(sizeof(char)*(iov->iov_len), GFP_KERNEL);
                strcpy(iov->iov_base, cmd_msg_buffer.msg);
            }
            break;
        default:
            break;
    }

#ifdef NLDEBUG
    printk(KERN_INFO "Done\n");
#endif

    kzfree(cmd_msg_buffer.msg);
    cmd_msg_buffer.msg = NULL;

    return 0;
}

static const char * recv_ready_str = "READY";
static const char * recv_done_str = "DONE";

static void netlink_recv_msg(struct sk_buff *skb)
{
    struct nlmsghdr *nlh;
    int pid;
    struct sk_buff *skb_out;

    char *recvmsg;
    char *msg;
    int msg_size;
    int res;

    int recvlen;
    int newmsglen;
    char *buf;
#ifdef NLDEBUG
    printk(KERN_INFO "Entering: %s\n", __FUNCTION__);
#endif

    nlh = (struct nlmsghdr*) skb->data;
    pid = nlh->nlmsg_pid;
#ifdef NLDEBUG
    printk(KERN_INFO "Netlink received msg payload: %s\n",
            (char *)nlmsg_data(nlh));
#endif
    recvmsg = (char *) nlmsg_data(nlh);

    if (!strcmp(recvmsg, recv_ready_str))
    {
        if (!cmd_msg_buffer.ready)
        {
            msg = (char *) "NOTHING";
            msg_size = strlen(msg);
        }
        else
        {
            msg = cmd_msg_buffer.cmd;
            msg_size = cmd_msg_buffer.cmdlen;
        }
    }
    else if (!strcmp(recvmsg, recv_done_str))
    {
        kzfree(cmd_msg_buffer.cmd);
        cmd_msg_buffer.cmd = NULL;

        cmd_msg_buffer.exec = true;
        cmd_msg_buffer.ready = false;

        msg = (char *) "OK";
        msg_size = strlen(msg);
    }
    else
    {
        recvlen = strlen(recvmsg);
        newmsglen = recvlen + cmd_msg_buffer.msglen;

        buf = (char *) kzalloc(sizeof(char) * (newmsglen+1), GFP_KERNEL);
        if (cmd_msg_buffer.msg != NULL)
            strcpy(buf, cmd_msg_buffer.msg);
        strcpy(buf+cmd_msg_buffer.msglen, recvmsg);
        kzfree(cmd_msg_buffer.msg);
        cmd_msg_buffer.msg = buf;
        cmd_msg_buffer.msglen = strlen(buf);

        msg = (char *) "COPIED";
        msg_size = strlen(msg);
    }

    skb_out = nlmsg_new(msg_size, 0);
    if (!skb_out)
    {
        printk(KERN_ERR "Failed to allocate new skb\n");
        return ;
    }
    nlh = nlmsg_put(skb_out, 0, 0, NLMSG_DONE, msg_size, 0);
    NETLINK_CB(skb_out).dst_group = 0;
    strncpy(nlmsg_data(nlh), msg, msg_size);

    res = nlmsg_unicast(nl_sk, skb_out, pid);

    if (res < 0)
        printk(KERN_INFO "Error while sending bak to user\n");
}

static struct netlink_kernel_cfg cfg = {
    .input = netlink_recv_msg,
};

#endif
