WITH_SELINUX?=0
ifeq ($(WITH_SELINUX), 1)
SELINUX_LDLIBS:=-lselinux
else
SELINUX_LDLIBS:=
endif
all: cc-tmpfs_mounts.so

cc-tmpfs_mounts.so: cc-tmpfs_mounts.c
	gcc $(CPPFLAGS) -std=gnu99 -Wall -o cc-tmpfs_mounts.o -fPIC -c cc-tmpfs_mounts.c -D WITH_SELINUX=$(WITH_SELINUX)
	gcc -shared -o cc-tmpfs_mounts.so cc-tmpfs_mounts.o $(SELINUX_LDLIBS)

clean:
	rm -f cc-tmpfs_mounts.o cc-tmpfs_mounts.so
