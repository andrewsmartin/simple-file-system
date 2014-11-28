#include <stdlib.h>

#include "sfs_api.h"

int main(int argc, char *argv[])
{
    mksfs(strtol(argv[1], NULL, 10));
    return 0;
}