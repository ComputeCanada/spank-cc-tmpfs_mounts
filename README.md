# spank-cc-tmpfs_mounts
SPANK plugin for SLURM that allows you to create in-memory, in-cgroup private tmpfs mounts per each job of users. They will automatically clean up when the cgroup is released at the end of their job. Generally used for /tmp, /dev/shm and /var/tmp.

## build options

| Makefile | rpmbuild | Description | Requirements |
| -------  | -------- | ----------- | ------------ |
|`WITH_SELINUX=1` | `--with-selinux` | `cc-tmpfs_mount.so` plugin will try to set SELinux security context of the temporary user folders to `user_u:object_r:user_tmp_t:s0` | `libselinux-devel` |