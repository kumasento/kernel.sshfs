#ifndef NETLINK_COMMON_H__
#define NETLINK_COMMON_H__

#include <sys/socket.h>
#include <linux/netlink.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_PAYLOAD 1023 /* maximum payload size*/

struct sockaddr_nl src_addr;
struct sockaddr_nl dest_addr;
int sock_fd;
struct msghdr msg;
struct nlmsghdr *nlh;
struct iovec iov;

int init_netlink_portal()
{
    // set sock_fd
    sock_fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_USERSOCK);
    if(sock_fd < 0)
        return -1;

    // set src_addr
    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.nl_family = AF_NETLINK;
    src_addr.nl_pid    = getpid(); /* self pid */

    bind(sock_fd, (struct sockaddr*)&src_addr, sizeof(src_addr));

    // set dest_addr
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.nl_family = AF_NETLINK;
    dest_addr.nl_pid    = 0; /* For Linux Kernel */
    dest_addr.nl_groups = 0; /* unicast */

    return 0;
}

char * send_recv_netlink_portal(char *msg_str, int msg_len)
{
    nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_PAYLOAD));
    memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));
    nlh->nlmsg_len   = NLMSG_SPACE(MAX_PAYLOAD);
    nlh->nlmsg_pid   = getpid();
    nlh->nlmsg_flags = 0;

    strncpy(NLMSG_DATA(nlh), msg_str, msg_len);

    iov.iov_base    = (void *)nlh;
    iov.iov_len     = nlh->nlmsg_len;

    msg.msg_name    = (void *)&dest_addr;
    msg.msg_namelen = sizeof(dest_addr);
    msg.msg_iov     = &iov;
    msg.msg_iovlen  = 1;

    printf("Sending message to kernel\n");
    sendmsg(sock_fd, &msg, 0);
    printf("Waiting for message from kernel\n");

    recvmsg(sock_fd, &msg, 0);
    printf("Received message payload: %s\n", (char *)NLMSG_DATA(nlh));

    return (char *)NLMSG_DATA(nlh);
}

void close_netlink_portal()
{
    close(sock_fd);
}


#endif
