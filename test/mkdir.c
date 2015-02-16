
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

int main(int argc, char* argv[])
{
    if (argc <= 1)
    {
        fprintf(stderr, "Usage: %s [dirname]\n", argv[0]);
        return 1;
    }

    int status;
    status = mkdir(argv[1], S_IRWXU);

    if (status)
        fprintf(stderr, "Failed to create: %s\n", argv[1]);

    return 0;
}
