
#ifndef SFTP_COMMON_H__
#define SFTP_COMMON_H__

#define MAX_COMMAND_HEADER_LENGTH 128

#include <libssh/sftp.h>
#include <libssh/libssh.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <fcntl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

sftp_session init_sftp_session(ssh_session ssh_s)
{
    sftp_session sftp;
    int rc;

    sftp = sftp_new(ssh_s);
    if (sftp == NULL)
    {
        fprintf(stderr, "Error allocating SFTP session: %s\n",
                ssh_get_error(ssh_s));
        exit(1);
    }

    rc = sftp_init(sftp);
    if (rc != SSH_OK)
    {
        fprintf(stderr, "Error initializing: %s\n",
                ssh_get_error(sftp));
        sftp_free(sftp);
        exit(1);
    }
    return sftp;
}

typedef enum {
    SFTP_COMMAND_MKDIR,
    SFTP_COMMAND_RMDIR,
    SFTP_COMMAND_CREATE,
    SFTP_COMMAND_READ,
    SFTP_COMMAND_UNRECOGNIZED
} sftp_command_type;

sftp_command_type sftp_match_command_type(char *cmdhdr)
{
    if (cmdhdr == NULL || strlen(cmdhdr) <= 0)
        return SFTP_COMMAND_UNRECOGNIZED;

    if (!strcmp(cmdhdr, "mkdir"))
        return SFTP_COMMAND_MKDIR;
    if (!strcmp(cmdhdr, "create"))
        return SFTP_COMMAND_CREATE;
    if (!strcmp(cmdhdr, "rmdir"))
        return SFTP_COMMAND_RMDIR;

    return SFTP_COMMAND_UNRECOGNIZED;
}

struct sftp_command
{
    sftp_command_type cmdtype;
    char *cmdstr;
};

struct sftp_command* sftp_parse_command(char *cmdstr)
{
    // First extract the command header
    if (cmdstr == NULL || strlen(cmdstr) <= 0)
        return NULL;
    char cmdhdr[MAX_COMMAND_HEADER_LENGTH];
    char c = cmdstr[0];
    int i;

    for (i = 0; i < strlen(cmdstr) && c != ' '; i++)
        cmdhdr[i] = c, c = cmdstr[i+1];
    cmdhdr[i] = '\0';

    struct sftp_command * sftp_cmd =
        (struct sftp_command *) malloc(sizeof(struct sftp_command *));

    sftp_cmd->cmdtype = sftp_match_command_type(cmdhdr);

    int cmdstr_len = strlen(cmdstr);
    cmdstr_len -= strlen(cmdhdr) + 1;

    sftp_cmd->cmdstr = (char *) malloc(sizeof(char) * (cmdstr_len+1));
    memcpy(sftp_cmd->cmdstr, cmdstr+strlen(cmdhdr)+1, sizeof(char) * cmdstr_len);
    sftp_cmd->cmdstr[cmdstr_len] = '\0';
    return sftp_cmd;
}

int sftp_execute_mkdir(ssh_session ssh, sftp_session sftp, char *dirname)
{
    int rc;

    rc = sftp_mkdir(sftp, dirname, S_IRWXU);
    if (rc != SSH_OK)
    {
        if (sftp_get_error(sftp) != SSH_FX_FILE_ALREADY_EXISTS)
        {
            fprintf(stderr, "Can't create directory: %s\n",
                    ssh_get_error(ssh));
            return rc;
        }
    }

    return rc;
}

int sftp_execute_rmdir(ssh_session ssh, sftp_session sftp, char *dirname)
{
    int rc;

    rc = sftp_rmdir(sftp, dirname);
    if (rc != SSH_OK)
    {
        fprintf(stderr, "Can't remove directory: %s\n",
                ssh_get_error(ssh));
        return rc;
    }

    return rc;
}

int sftp_execute_create(ssh_session ssh, sftp_session sftp, char *filename)
{
    int rc, nwritten;
    int access_type = O_CREAT;
    sftp_file file;

    file = sftp_open(sftp, filename, access_type, S_IRWXU);
    if (file == NULL)
    {
        fprintf(stderr, "Can't open file: %s\n", ssh_get_error(ssh));
        return SSH_ERROR;
    }

    rc = sftp_close(file);
    if (rc != SSH_OK)
    {
        fprintf(stderr, "Can't close the file: %s\n",
                ssh_get_error(ssh));
        return rc;
    }
    return SSH_OK;
}

struct iovec *sftp_execute(ssh_session ssh, sftp_session sftp, char *cmdstr)
{
    struct sftp_command* sftp_cmd = sftp_parse_command(cmdstr);

    printf("%d %s\n", sftp_cmd->cmdtype, sftp_cmd->cmdstr);

    struct iovec *iov =
        (struct iovec *) malloc(sizeof(struct iovec));

    char *msg;
    switch (sftp_cmd->cmdtype)
    {
        case SFTP_COMMAND_MKDIR:
            if (sftp_execute_mkdir(ssh, sftp, sftp_cmd->cmdstr)
                    != SSH_OK)
                msg = "Cannot create directory";
            else
                msg = "Directory Created";
            break;
        case SFTP_COMMAND_RMDIR:
            if (sftp_execute_rmdir(ssh, sftp, sftp_cmd->cmdstr)
                    != SSH_OK)
                msg = "Cannot remove directory";
            else
                msg = "Directory Removed";
            break;
        case SFTP_COMMAND_CREATE:
            if (sftp_execute_create(ssh, sftp, sftp_cmd->cmdstr)
                    != SSH_OK)
                msg = "Cannot create file";
            else
                msg = "File Created";
            break;
        default:
            return NULL;
    }

    iov->iov_base = msg;
    iov->iov_len = strlen(msg);
    return iov;
}

#endif
