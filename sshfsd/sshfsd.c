
#include <libssh/libssh.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/uio.h>

#define SSH_DEBUG

#include "sftp_common.h"
#include "libssh_common.h"
#include "netlink_common.h"

ssh_session init_ssh_session(const char *host)
{
    int verbosity = SSH_LOG_PROTOCOL;
    int port = 22;

    ssh_session my_ssh_session = ssh_new();
    if (my_ssh_session == NULL)
        exit(-1);

    ssh_options_set(my_ssh_session, SSH_OPTIONS_HOST, host);
    ssh_options_set(my_ssh_session, SSH_OPTIONS_LOG_VERBOSITY, &verbosity);
    ssh_options_set(my_ssh_session, SSH_OPTIONS_PORT, &port);

    int rc = ssh_connect(my_ssh_session);
    if (rc != SSH_OK)
    {
        fprintf(stderr, "Error connecting to localhost: %s\n",
                ssh_get_error(my_ssh_session));
        exit(-1);
    }

    if (verify_knownhost(my_ssh_session) < 0)
    {
        ssh_disconnect(my_ssh_session);
        ssh_free(my_ssh_session);
        exit(-1);
    }

    if (authenticate_pubkey(my_ssh_session) != SSH_AUTH_SUCCESS)
    {
        ssh_disconnect(my_ssh_session);
        ssh_free(my_ssh_session);
        exit(-1);
    }

    return my_ssh_session;
}
int main(int argc, char *argv[])
{
    if (argc <= 1)
    {
        fprintf(stderr, "Usage: %s [host]", argv[0]);
        exit(1);
    }

    ssh_session ssh_s = init_ssh_session(argv[1]);
    sftp_session sftp_s = init_sftp_session(ssh_s);
    init_netlink_portal();

    while (1)
    {
    #ifdef SSH_DEBUG
        puts("Ask for request: Seding READY...");
    #endif
        char *recv = send_recv_netlink_portal("READY", strlen("READY"));
        if (!strcmp(recv, "ERROR"))
        {
            fprintf(stderr, "Something wrong\n");
            exit(1);
        }
        else if (!strcmp(recv, "NOTHING"))
        {
        #ifdef SSH_DEBUG
            puts("Nothing to do");
        #endif
            sleep(1);
        }
        else
        {
        #ifdef SSH_DEBUG
            printf("Received Command: %s\n", recv);
        #endif

            struct iovec *iov = sftp_execute(ssh_s, sftp_s, recv);

            if (iov == NULL)
                exit(1);

            int cur_idx;
            for (cur_idx = 0;
                    cur_idx == 0 || cur_idx+MAX_PAYLOAD < iov->iov_len;
                    cur_idx+=MAX_PAYLOAD)
            {
                char *base = iov->iov_base + cur_idx;
                int len = iov->iov_len - cur_idx;

                printf("chunk: %s len: %d\n", base, len);
                recv = send_recv_netlink_portal(base,len);
                if (strcmp(recv, "COPIED"))
                    exit(1);
            }
            recv = send_recv_netlink_portal("DONE", strlen("DONE"));
            if (strcmp(recv, "OK"))
                exit(1);
        }
    }

    close_netlink_portal();

    ssh_disconnect(ssh_s);
    ssh_free(ssh_s);
}
