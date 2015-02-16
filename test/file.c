
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
    fd = open(filename, O_RDWR, S_IREAD | S_IWRITE);
    if (fd < 0)
    {
        fprintf(stderr, "Failed to open file: %s[%d]\n", filename, fd);
        return 1;
    }
    return 0;
}
