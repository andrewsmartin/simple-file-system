#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "sfs_api.h"

int main(int argc, char *argv[])
{
    int i = strtol(argv[1], NULL, 10);
    char *buf = "Some text for the file. ASDALSKFJHSDFJKLSDKFJSDFH SDF\
        SDFSDJFHSDKJFLSDKFJHSDFJDSKLFKSDJFHDJKLSKJDFHDJSKFLSKDJFHD\
        SDFSDFHSDFJKLSDKFJSDFHDSJKFLSDKFJHDSFJKSLDFKJSDHFSJDKFLSDKJFHDS\
        SDFKJSDHFJDSKLFSDKJFHDSJFKLSDKJFHDJKLSDKJFHDJKFLSDKJFHDJSKLDF\
        SDFLKJSDFHDSJFLSDKJFDSHFJKSLDKFJHDSFJKLSDKFJHDSJFKLSDKJFHDJKLS\
        DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD\
        DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD\
        DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD";
    if (i == 1) {
        mksfs(i);
        int fileID = sfs_fopen("test1.txt");
        sfs_fwrite(fileID, buf, strlen(buf));
        sfs_fclose(fileID);
    }
    else {
        mksfs(i);
        int fileID = sfs_fopen("test1.txt");
        char *tmp = malloc(strlen(buf));
        sfs_fread(fileID, tmp, strlen(buf));
        sfs_fclose(fileID);
        printf("%s\n", tmp);
    }
    
    return 0;
}