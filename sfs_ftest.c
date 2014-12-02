#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sfs_api.h"

/* The maximum file name length. We assume that filenames can contain
 * upper-case letters and periods ('.') characters. Feel free to
 * change this if your implementation differs.
 */
#define MAX_FNAME_LENGTH 12   /* Assume at most 12 characters (8.3) */

/* The maximum number of files to attempt to open or create.  NOTE: we
 * do not _require_ that you support this many files. This is just to
 * test the behavior of your code.
 */
#define MAX_FD 100 

/* The maximum number of bytes we'll try to write to a file. If you
 * support much shorter or larger files for some reason, feel free to
 * reduce this value.
 */
#define MAX_BYTES 30000 /* Maximum file size I'll try to create */
#define MIN_BYTES 10000         /* Minimum file size */

/* Just a random test string.
*/
static char test_str[] = "A mathematician is a machine for turning coffee into theorems.\n";

/* rand_name() - return a randomly-generated, but legal, file name.
 *
 * This function creates a filename of the form xxxxxxxx.xxx, where
 * each 'x' is a random upper-case letter (A-Z). Feel free to modify
 * this function if your implementation requires shorter filenames, or
 * supports longer or different file name conventions.
 * 
 * The return value is a pointer to the new string, which may be
 * released by a call to free() when you are done using the string.
 */

char *rand_name() 
{
    char fname[MAX_FNAME_LENGTH];
    int i;

    for (i = 0; i < MAX_FNAME_LENGTH; i++) {
        if (i != 8) {
            fname[i] = 'A' + (rand() % 26);
        }
        else {
            fname[i] = '.';
        }
    }
    fname[i] = '\0';
    return (strdup(fname));
}

/* The main testing program
*/
    int
main(int argc, char **argv)
{
    int i, j, k;
    int chunksize;
    char *buffer;
    char fixedbuf[1024];
    int fds[MAX_FD];
    char *names[MAX_FD];
    int filesize[MAX_FD];
    int nopen;                    /* Number of files simultaneously open */
    int ncreate;                  /* Number of files created in directory */
    int error_count = 0;
    int tmp;

    mksfs(1);                     /* Initialize the file system. */

    /* First we open two files and attempt to write data to them.
    */
    for (i = 0; i < 2; i++) {
        names[i] = rand_name();
        fds[i] = sfs_fopen(names[i]);
        if (fds[i] < 0) {
            fprintf(stderr, "ERROR: creating first test file %s\n", names[i]);
            error_count++;
        }
        tmp = sfs_fopen(names[i]);
        if (tmp >= 0 && tmp != fds[i]) {
            fprintf(stderr, "ERROR: file %s was opened twice\n", names[i]);
            error_count++;
        }
        filesize[i] = (rand() % (MAX_BYTES-MIN_BYTES)) + MIN_BYTES;
    }

    for (i = 0; i < 2; i++) {
        for (j = i + 1; j < 2; j++) {
            if (fds[i] == fds[j]) {
                fprintf(stderr, "Warning: the file descriptors probably shouldn't be the same?\n");
            }
        }
    }

    printf("Two files created with zero length:\n");
    sfs_ls();
    printf("\n");

    for (i = 0; i < 2; i++) {
        for (j = 0; j < filesize[i]; j += chunksize) {
            if ((filesize[i] - j) < 10) {
                chunksize = filesize[i] - j;
            }
            else {
                chunksize = (rand() % (filesize[i] - j)) + 1;
            }

            if ((buffer = malloc(chunksize)) == NULL) {
                fprintf(stderr, "ABORT: Out of memory!\n");
                exit(-1);
            }
            for (k = 0; k < chunksize; k++) {
                buffer[k] = (char) (j+k);
            }
            printf("file: %d, chunksize: %d\n", i, chunksize);
            sfs_ls();
            sfs_fwrite(fds[i], buffer, chunksize);
	        free(buffer);
        }
    }

    sfs_fclose(fds[1]);

    printf("File %s now has length %d and %s now has length %d:\n",
            names[0], filesize[0], names[1], filesize[1]);
    sfs_ls();

    fds[1] = sfs_fopen(names[1]);

    printf("before doing reads\n\n");

    for (i = 0; i < 2; i++) {
        for (j = 0; j < filesize[i]; j += chunksize) {
            if ((filesize[i] - j) < 10) {
                chunksize = filesize[i] - j;
            }
            else {
                chunksize = (rand() % (filesize[i] - j)) + 1;
            }
            if ((buffer = malloc(chunksize)) == NULL) {
                fprintf(stderr, "ABORT: Out of memory!\n");
                exit(-1);
            }
            printf("before sfs_read\n\n");
            sfs_fread(fds[i], buffer, chunksize);
            printf("After sfs_read\n\n");
            /*
            for (k = 0; k < chunksize; k++) {
                if (buffer[k] != (char)(j+k)) {
                    fprintf(stderr, "ERROR: data error at offset %d in file %s (%d,%d)\n",
                            j+k, names[i], buffer[k], (char)(j+k));
                    error_count++;
                    break;
                }
            }
            */
            free(buffer);
            printf("After doing reads %d\n\n", i);
        }
    }
    printf("After doing reads\n\n");

    for (i = 0; i < 2; i++) {
        sfs_fclose(fds[i]);
        if (sfs_remove(names[i]) != 0) {
            fprintf(stderr, "ERROR: deleting file %s\n", names[i]);
            error_count++;
        }
        printf("After deleting file %s:\n", names[i]);
        sfs_ls();
    }

    /* Now try to close and delete the closed and deleted files. Don't
     * care about the return codes, really, but just want to make sure
     * this doesn't cause a problem.
     */
    for (i = 0; i < 2; i++) {
        sfs_fclose(fds[i]);
        if (sfs_remove(names[i]) == 0) {
            fprintf(stderr, "Warning: deleting already deleted file %s\n", names[i]);
        }
        //free(names[i]);
        names[i] = NULL;
    }

    /* Now just try to open up a bunch of files.
    */
    ncreate = 0;
    for (i = 0; i < MAX_FD; i++) {
        names[i] = rand_name();
        fds[i] = sfs_fopen(names[i]);
        if (fds[i] < 0) {
            break;
        }
        sfs_fclose(fds[i]);
        ncreate++;
    }

    printf("Created %d files in the root directory\n", ncreate);

    nopen = 0;
    for (i = 0; i < ncreate; i++) {
        fds[i] = sfs_fopen(names[i]);
        if (fds[i] < 0) {
            break;
        }
        nopen++;
    }
    printf("Simultaneously opened %d files\n", nopen);

    for (i = 0; i < nopen; i++) {
        sfs_fwrite(fds[i], test_str, strlen(test_str));
        sfs_fclose(fds[i]);
    }

    /* Re-open in reverse order */
    for (i = nopen-1; i >= 0; i--) {
        fds[i] = sfs_fopen(names[i]);
        if (fds[i] < 0) {
            fprintf(stderr, "ERROR: can't re-open file %s\n", names[i]);
        }
    }

    /* Now test the file contents.
    */
    for (j = 0; j < strlen(test_str); j++) {
        for (i = 0; i < nopen; i++) {
            char ch;

            sfs_fread(fds[i], &ch, 1);
            if (ch != test_str[j]) {
                fprintf(stderr, "ERROR: Read wrong byte from %s at %d (%c,%c)\n", 
                        names[i], j, ch, test_str[j]);
                error_count++;
                break;
            }
        }
    }

    /* Now close all of the open file handles.
    */
    for (i = 0; i < nopen; i++) {
        sfs_fclose(fds[i]);
    }

    /* Now we try to re-initialize the system.
    */
    mksfs(0);

    for (i = 0; i < nopen; i++) {
        fds[i] = sfs_fopen(names[i]);
        if (fds[i] >= 0) {
            sfs_fread(fds[i], fixedbuf, sizeof(fixedbuf));

            for (j = 0; j < strlen(test_str); j++) {
                if (test_str[j] != fixedbuf[j]) {
                    fprintf(stderr, "ERROR: Wrong byte in %s at %d (%d,%d)\n", 
                            names[i], j, fixedbuf[j], test_str[j]);
                    error_count++;
                    break;
                }
            }

            sfs_fclose(fds[i]);
        }
    }

    for (i = 0; i < ncreate; i++) {
        sfs_remove(names[i]);
        free(names[i]);
        names[i] = NULL;
    }

    //-------- The following part tests sfs_fseek

    printf("Tests sfs_fseek\n");

    // Prepare a file
    char* f_name = rand_name();
    int f_id = sfs_fopen(f_name);
    buffer = malloc(100);

    for(i = 0; i < 10; i++)
    {
        sfs_fwrite(f_id, "0123456789", 10);
    }

    // sfs_fwrite shouldn't change read pointer
    for(i = 0; i < 10; i++)
    {
        sfs_fread(f_id, buffer, 10);
        if(0 != strncmp(buffer, "0123456789", 10))
        {
            fprintf(stderr, "ERROR: should read '0123456789'\n");
            error_count++;
        }
    }

    // sys_seek changes read pointer
    for(i = 0; i < 100; i += 7)
    {
        sfs_fseek(f_id, i);
        sfs_fread(f_id, buffer, 1);
        if (buffer[0] - 48 != i % 10) {
            fprintf(stderr, "ERROR: postion %d shoud be %c\n", i, i % 10);
            error_count++;
        }
    }

    sfs_fseek(f_id, 80);
    sfs_fwrite(f_id, "9876543210", 10);
    sfs_fseek(f_id, 85);
    sfs_fread(f_id, buffer, 10);
    if(0 != strncmp(buffer, "4321001234", 10))
    {
        fprintf(stderr, "ERROR: should read '4321001234'\n");
        error_count++;
    }

    //free(buffer);

    fprintf(stderr, "Test program exiting with %d errors\n", error_count);
    return (error_count);
}
