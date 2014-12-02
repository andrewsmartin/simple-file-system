CFLAGS = -Wall
OBJS = sfs_ftest.o sfs_api.o sblock_cache.o dir_cache.o fat_cache.o free_block_list.o file_descriptor.o bit_field.o disk_emu.o

sfs: ${OBJS}
	gcc ${OBJS} -o sfs

sfs_ftest.o: sfs_ftest.c
	gcc -c sfs_ftest.c ${CFLAGS}
	
sfs_api.o: sfs_api.c
	gcc -c sfs_api.c ${CFLAGS}

sblock_cache.o: sblock_cache.c
	gcc -c sblock_cache.c ${CFLAGS}

dir_cache.o: dir_cache.c
	gcc -c dir_cache.c ${CFLAGS}

fat_cache.o: fat_cache.c
	gcc -c fat_cache.c ${CFLAGS}

free_block_list.o: free_block_list.c
	gcc -c free_block_list.c ${CFLAGS}

file_descriptor.o: file_descriptor.c
	gcc -c file_descriptor.c ${CFLAGS}

bit_field.o: bit_field.c
	gcc -c bit_field.c ${CFLAGS}

disk_emu.o: lib/disk_emu.c
	gcc -c lib/disk_emu.c ${CFLAGS}

clean:
	rm -f ${OBJS} sfs test.disk
