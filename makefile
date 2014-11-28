CFLAGS = -Wall
OBJS = main.o sfs_api.o free_block_list.o bit_field.o disk_emu.o

sfs: ${OBJS}
	gcc ${OBJS} -o sfs
	
main.o: sfs_api.h main.c
	gcc -c main.c ${CFLAGS}
	
sfs_api.o: sfs_api.c sfs_api.h lib/disk_emu.h
	gcc -c sfs_api.c ${CFLAGS}

free_block_list.o: free_block_list.c free_block_list.h
	gcc -c free_block_list.c ${CFLAGS}

bit_field.o: bit_field.c bit_field.h
	gcc -c bit_field.c ${CFLAGS}

disk_emu.o: lib/disk_emu.c lib/disk_emu.h
	gcc -c lib/disk_emu.c ${CFLAGS}

clean:
	rm -f ${OBJS} sfs test.disk
