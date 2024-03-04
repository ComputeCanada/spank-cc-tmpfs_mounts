WITH_SELINUX?=0
ifeq ($(WITH_SELINUX), 1)
SELINUX_CFLAGS:=-D WITH_SELINUX
SELINUX_LDLIBS:=-lselinux
else
SELINUX_CFLAGS:=
SELINUX_LDLIBS:=
endif
all: cc-tmpfs_mounts.so

cc-tmpfs_mounts.so: cc-tmpfs_mounts.c
	gcc $(CPPFLAGS) -std=gnu99 -Wall -o cc-tmpfs_mounts.o -fPIC -c cc-tmpfs_mounts.c $(SELINUX_CFLAGS)
	gcc -shared -o cc-tmpfs_mounts.so cc-tmpfs_mounts.o $(SELINUX_LDLIBS)

clean:
	rm -f cc-tmpfs_mounts.o cc-tmpfs_mounts.so
