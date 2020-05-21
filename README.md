# spank-cc-tmpfs_mounts
SPANK plugin for SLURM that allows you to create in-memory, in-cgroup private tmpfs mounts per each job of users. They will automatically clean up when the cgroup is released at the end of their job. Generally used for /tmp, /dev/shm and /var/tmp.
