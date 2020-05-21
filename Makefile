
all: cc-tmpfs_mounts.so

cc-tmpfs_mounts.so: cc-tmpfs_mounts.c
	gcc $(CPPFLAGS) -std=gnu99 -Wall -o cc-tmpfs_mounts.o -fPIC -c cc-tmpfs_mounts.c
	gcc -shared -o cc-tmpfs_mounts.so cc-tmpfs_mounts.o 

clean:
	rm -f cc-tmpfs_mounts.o cc-tmpfs_mounts.so
