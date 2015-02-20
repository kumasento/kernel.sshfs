
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

int main(int argc, char *argv[])
{
    if (argc <= 1)
    {
        fprintf(stderr, "Usage: %s [filename]\n", argv[0]);
        return 1;
    }

    int fd;
    char *filename = argv[1];
    fd = open(filename, O_RDONLY, 0);

    printf("%d\n", fd);
    if (fd < 0)
    {
        fprintf(stderr, "Failed to open file: %s[%d]\n", filename, fd);
        return 1;
    }

    char buf[100];
    int nbytes = read(fd, (void *) buf, 100);

    printf("%d %s\n", nbytes, buf);

    return 0;
}
