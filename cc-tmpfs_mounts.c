// vim: set softtabstop=4 ts=4 sw=4 expandtab:
/*
 * Spank plugin cc-tmpfs_mounts
 */

/* Needs to be defined before first invocation of features.h so enable it early. */
#define _GNU_SOURCE             /* See feature_test_macros(7) */

#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <linux/fs.h>
#include <slurm/spank.h>
#include <slurm/slurm.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <unistd.h>
#include <sched.h>
#include <pwd.h>
#include <ftw.h>

#ifdef WITH_SELINUX
#include <selinux/selinux.h>
#define TMPFS_SELINUX_CONTEXT "user_u:object_r:user_tmp_t:s0"
#endif

SPANK_PLUGIN (cc-tmpfs_mounts, 1);

// Default
#define MAX_BINDSELF_DIRS 16
#define MAX_TMPFS_DIRS 16
#define MAX_BIND_DIRS 16

// Globals
int init_opts = 0;
int binded = 0;
int tmped = 0;
uid_t uid = (uid_t)-1;
gid_t gid = (gid_t)-1;
uint32_t jobid;
uint32_t restartcount;
struct passwd *user;
const char *bind_target = "/tmp";
char bind_target_full[PATH_MAX+1] = "";
char bind_self_full[PATH_MAX+1] = "";
struct stat st = {0};

char *bind_self[MAX_BINDSELF_DIRS];
char *tmpfs_dirs[MAX_TMPFS_DIRS];
char *bind_dirs[MAX_BIND_DIRS];
int bind_self_count = 0;
int tmpfs_dirs_count = 0;
int bind_dirs_count = 0;
// Globals

int _tmpdir_bind(spank_t sp, int ac, char **av);
int _tmpdir_tmpfs(spank_t sp, int ac, char **av);
int _tmpdir_task_init(spank_t sp, int ac, char **av);
int _tmpdir_init(spank_t sp, int ac, char **av);
int _tmpdir_init_opts(spank_t sp, int ac, char **av);
int _tmpdir_init_prolog(spank_t sp, int ac, char **av);

// Cleaner helper
int unlink_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
    int rv;

    if ((rv = remove(fpath))) {
        perror(fpath);
    }

    return rv;
}

int rmrf(char *path)
{
    return nftw(path, unlink_cb, 64, FTW_DEPTH | FTW_PHYS | FTW_MOUNT);
}
// End Cleaner helper

/*
 *  Called from both srun and slurmd.
 */
int slurm_spank_init(spank_t sp, int ac, char **av)
{
    return _tmpdir_init_opts(sp,ac,av);
}

int slurm_spank_init_post_opt(spank_t sp, int ac, char **av)
{
    if (!spank_remote(sp)) {
        return 0;
    }

    uint32_t stepid;
    if (spank_get_item(sp, S_JOB_STEPID, &stepid) != ESPANK_SUCCESS) {
        /* if unable to obtain stepid then don't handle */
        slurm_error("cc-tmpfs_mounts: spank_get_item( S_JOB_STEPID ) failed");
        return 0;
    }

    if (stepid > SLURM_MAX_NORMAL_STEP_ID && stepid != SLURM_BATCH_SCRIPT && stepid != SLURM_INTERACTIVE_STEP) {
        /* only handle 'batch', 'interactive', or 'regular' step, not 'extern', or 'pending' */
        return 0;
    }

    _tmpdir_tmpfs(sp,ac,av);
    return _tmpdir_bind(sp,ac,av);
}

int slurm_spank_job_prolog(spank_t sp, int ac, char **av)
{
    int i;

    if(_tmpdir_init_prolog(sp,ac,av)) {
        return -1;
    }
    if(stat(bind_target_full, &st) == -1) {
        if(mkdir(bind_target_full,0750)) {
            slurm_error("cc-tmpfs_mounts: mkdir(\"%s\",0750): %m", bind_target_full);
            return -1;
        }
#ifdef WITH_SELINUX
        if(setfilecon(bind_target_full, TMPFS_SELINUX_CONTEXT)) {
            slurm_debug("cc-tmpfs_mounts: Unable to set %s SELinux context", bind_target_full);
        }
#endif
    }
    if(chown(bind_target_full,uid,gid)) {
        slurm_error("cc-tmpfs_mounts: chown(%s,%u,%u): %m", bind_target_full,uid,gid);
        return -1;
    }
    // bindself directories need creating
    if(strlen(user->pw_name)>0) {
        for(i=0;i<bind_self_count;i++) {
            int c = snprintf(bind_self_full, sizeof(bind_self_full), "%s/%s.%d.%d", bind_self[i], user->pw_name, jobid, restartcount);
            if( c < 0 || c > sizeof(bind_self_full) - 1 ) {
                slurm_error("cc-tmpfs_mounts: \"%s/%s.%d.%d\" too large. Aborting",bind_self[i],user->pw_name,jobid,restartcount);
                return -1;
            }
            if(stat(bind_self_full, &st) == -1) {
                if(mkdir(bind_self_full,0750)) {
                    slurm_error("cc-tmpfs_mounts: mkdir(\"%s\",0750): %m", bind_self_full);
                    return -1;
                }
#ifdef WITH_SELINUX
                if(setfilecon(bind_self_full, TMPFS_SELINUX_CONTEXT)) {
                    slurm_debug("cc-tmpfs_mounts: Unable to set %s SELinux context", bind_self_full);
                }
#endif
            }
            if(chown(bind_self_full,uid,gid)) {
                slurm_error("cc-tmpfs_mounts: chown(%s,%u,%u): %m", bind_self_full,uid,gid);
                return -1;
            }
        }
    }
    // this _tmpdir_tmpfs call is local, won't do anything
    return _tmpdir_tmpfs(sp,ac,av);
}

int slurm_spank_job_epilog(spank_t sp, int ac, char **av)
{
    int i;

    if(_tmpdir_init_prolog(sp,ac,av)) {
        return -1;
    }
    if(strlen(user->pw_name)>0) {
        for(i=0;i<bind_self_count;i++) {
            int c = snprintf(bind_self_full, sizeof(bind_self_full), "%s/%s.%d.%d", bind_self[i], user->pw_name, jobid, restartcount);
            if( c < 0 || c > sizeof(bind_self_full) - 1 ) {
                slurm_error("cc-tmpfs_mounts: \"%s/%s.%d.%d\" too large. Aborting",bind_self[i],user->pw_name,jobid,restartcount);
                return -1;
            }
            if(strlen(bind_self_full) > 5 && stat(bind_self_full, &st) > -1) {
                if(S_ISDIR(st.st_mode)) {
                    slurm_debug("Removing directory: %s", bind_self_full);
                    if(rmrf(bind_self_full) != 0) {
                        slurm_error("cc-tmpfs_mounts: rmrf(\"%s\"): %m", bind_self_full);
                        //return -1;
                    }
                }
            }
        }
    }
    if(strlen(bind_target_full) > 5 && stat(bind_target_full, &st) > -1) {
        if(S_ISDIR(st.st_mode)) {
            slurm_debug("Removing directory: %s", bind_target_full);
            if(rmrf(bind_target_full) != 0) {
                slurm_error("cc-tmpfs_mounts: rmrf(\"%s\"): %m", bind_target_full);
                //return -1;
            }
        }
    }
    return 0;
}

int slurm_spank_local_user_init(spank_t sp, int ac, char **av)
{
    return _tmpdir_tmpfs(sp,ac,av);
}

int slurm_spank_task_init_privileged(spank_t sp, int ac, char **av)
{
    // should be unnecessary after slurm_spank_init_post_opt, kept just
    // in case
    _tmpdir_tmpfs(sp,ac,av);
    return _tmpdir_bind(sp,ac,av);
}

int slurm_spank_task_init(spank_t sp, int ac, char **av)
{
    return _tmpdir_task_init(sp,ac,av);
}

int _tmpdir_task_init(spank_t sp, int ac, char **av)
{
        char cwd[PATH_MAX+1];
        char restartcount_s[6] = "";

        // This basically does a `cd $(pwd)`
        // The reason is that when we change /tmp to /tmp/$user.$jobid.$restarcount via bindmount
        // if the user uses sbatch --chdir=/tmp or they launch their job via salloc or sbatch while in /tmp
        // then it will cause them to exist in the real /tmp rather than the new bindmounted one, until they
        // do a `cd /tmp` for example.  So `ls` would be different than `ls $(pwd)` in this case
        // We fix it by doing a `cd $(pwd)` for them, as them, to ensure they land in the bindmounted location
        // and makes sure that we actually clean up their job data at the end of the job rather than missing it.
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            if (chdir(cwd) != 0) {
              slurm_error("cc-tmpfs_mounts: chdir() error");
              return -1;
            }
        } else {
            slurm_error("cc-tmpfs_mounts: getcwd() error");
            return -1;
        }
        sprintf(restartcount_s, "%u", restartcount);
        if(spank_setenv(sp, "SLURM_RESTART_COUNT", restartcount_s, 1) != ESPANK_SUCCESS) {
            slurm_error("cc-tmpfs_mounts: Unable to set job's SLURM_RESTART_COUNT env variable");
            return -1;
        }
        if(spank_setenv(sp, "SLURM_TMPDIR", bind_target_full, 1) != ESPANK_SUCCESS) {
            slurm_error("cc-tmpfs_mounts: Unable to set job's SLURM_TMPDIR env variable");
            return -1;
        }
        if(spank_setenv(sp, "XDG_RUNTIME_DIR", bind_target_full, 1) != ESPANK_SUCCESS) {
            slurm_error("cc-tmpfs_mounts: Unable to set job's XDG_RUNTIME_DIR env variable");
            return -1;
        }
        return 0;
}

int _tmpdir_bind(spank_t sp, int ac, char **av)
{
    int i;

    // only on cluster nodes
    if(!spank_remote(sp)) {
        return 0;
    }

    // We have done this already
    if(binded) {
        return 0;
    }
    // Don't do this anymore
    binded = 1;

    // Init dirs
    if(_tmpdir_init_prolog(sp,ac,av)) {
        return -1;
    }

    if(strlen(user->pw_name)>0) {
        for(i=0;i<bind_dirs_count;i++) {
            slurm_debug("cc-tmpfs_mounts: mounting: %s %s", bind_target_full, bind_dirs[i]);
            if(mount(bind_target_full, bind_dirs[i], "none", MS_BIND, NULL)) {
                //mount(bind_dirs[i], bind_target_full, "none", MS_BIND, NULL);
                slurm_error("cc-tmpfs_mounts: failed to mount %s for job: %u, %m", bind_dirs[i], jobid);
                return -1;
            }
        }
        for(i=0;i<bind_self_count;i++) {
            int c = snprintf(bind_self_full, sizeof(bind_self_full), "%s/%s.%d.%d", bind_self[i], user->pw_name, jobid, restartcount);
            if( c < 0 || c > sizeof(bind_self_full) - 1 ) {
                slurm_error("cc-tmpfs_mounts: \"%s/%s.%d.%d\" too large. Aborting",bind_self[i],user->pw_name,jobid,restartcount);
                return -1;
            }
            slurm_debug("cc-tmpfs_mounts: mounting: %s %s", bind_self_full, bind_self[i]);
            if(mount(bind_self_full, bind_self[i], "none", MS_BIND, NULL)) {
                slurm_error("cc-tmpfs_mounts: failed to mount %s for job: %u, %m", bind_self[i], jobid);
                return -1;
            }
        }
    }

    return 0;
}

int _tmpdir_tmpfs(spank_t sp, int ac, char **av)
{
    int i;

    // only on cluster nodes
    if(!spank_remote(sp)) {
        return 0;
    }

    // We have done this already
    if(tmped) {
        return 0;
    }
    // Don't do this anymore
    tmped = 1;

    // Init dirs
    if(_tmpdir_init(sp,ac,av)) {
        return -1;
    }

    // Make / share (propagate) mounts (same as mount --make-rshared /)
    if(mount("", "/", "notapplicable", MS_REC|MS_SHARED, "")) {
        slurm_error("cc-tmpfs_mounts: failed to 'mount --make-rshared /' for job: %u, %m", jobid);
        return -1;
    }

    // Create our own namespace
    if(unshare(CLONE_NEWNS)) {
        slurm_error("cc-tmpfs_mounts: failed to unshare mounts for job: %u, %m", jobid);
        return -1;
    }

    // Make / slave (same as mount --make-rslave /)
    if(mount("", "/", "notapplicable", MS_REC|MS_SLAVE, "")) {
            slurm_error("cc-tmpfs_mounts: failed to 'mount --make-rslave /' for job: %u, %m", jobid);
            return -1;
    }
    char opts[21] = "";
    sprintf(opts, "mode=0750,uid=%d", uid);
    for(i=0;i<tmpfs_dirs_count;i++) {
        char *mount_type = "tmpfs";
        slurm_debug("cc-tmpfs_mounts: mounting: %s %s as type: %s", tmpfs_dirs[i], opts, mount_type);
        if(mount(mount_type, tmpfs_dirs[i], mount_type, 0, opts)) {
            slurm_error("cc-tmpfs_mounts: failed to mount %s for job: %u, %m", tmpfs_dirs[i], jobid);
            return -1;
        }
    }

    return 0;
}

int _tmpdir_init_prolog(spank_t sp, int ac, char **av)
{
    if(_tmpdir_init_opts(sp,ac,av)) {
        return 0;
    }

    // Get JobID
    if(spank_get_item(sp, S_JOB_ID, &jobid) != ESPANK_SUCCESS) {
        slurm_error("cc-tmpfs_mounts: Failed to get jobid from SLURM");
        return -1;
    }

    // Get UID
    if(spank_get_item(sp, S_JOB_UID, &uid) != ESPANK_SUCCESS) {
        slurm_error("cc-tmpfs_mounts: Unable to get job's user id");
        return -1;
    }

    // Get GID
    if(spank_get_item(sp, S_JOB_GID, &gid) != ESPANK_SUCCESS) {
        slurm_debug("cc-tmpfs_mounts: Unable to get job's group id");
        gid = 0;
    }
    // Get RestartCount
    if(spank_get_item(sp, S_SLURM_RESTART_COUNT, &restartcount) != ESPANK_SUCCESS) {
        slurm_debug("cc-tmpfs_mounts: Unable to get job's restart count");
        restartcount = 0;
    }
    // Get Username
    if((user = getpwuid(uid)) == NULL) {
        slurm_error("cc-tmpfs_mounts: Unable to get job's username");
        return -1;
    }
    if(gid==0) {
        if((gid = user->pw_gid) == 0) {
            slurm_error("cc-tmpfs_mounts: Unable to get job's group id");
            return -1;
        }
    }
    int c = snprintf(bind_target_full, sizeof(bind_target_full), "%s/%s.%d.%d", bind_target, user->pw_name, jobid, restartcount);
    if( c < 0 || c > sizeof(bind_target_full) - 1 ) {
        slurm_error("cc-tmpfs_mounts: \"%s/%s.%d.%d\" too large. Aborting",bind_target,user->pw_name,jobid,restartcount);
        return -1;
    }

    return 0;
}

int _tmpdir_init(spank_t sp, int ac, char **av)
{

    if(_tmpdir_init_opts(sp,ac,av)) {
        return 0;
    }

    // Get JobID
    if(spank_get_item(sp, S_JOB_ID, &jobid) != ESPANK_SUCCESS) {
        slurm_error("cc-tmpfs_mounts: Failed to get jobid from SLURM");
        return -1;
    }

    // Get UID
    if(spank_get_item(sp, S_JOB_UID, &uid) != ESPANK_SUCCESS) {
        slurm_error("cc-tmpfs_mounts: Unable to get job's user id");
        return -1;
    }

    // Get GID
    if(spank_get_item(sp, S_JOB_GID, &gid) != ESPANK_SUCCESS) {
        slurm_debug("cc-tmpfs_mounts: Unable to get job's group id");
        gid = 0;
    }

    return 0;
}

int _tmpdir_init_opts(spank_t sp, int ac, char **av)
{
    int i;

    if(init_opts) {
        return 0;
    }
    init_opts = 1;

    // Init
    bzero(tmpfs_dirs,sizeof(tmpfs_dirs));
    bzero(bind_dirs,sizeof(bind_dirs));
    bzero(bind_self,sizeof(bind_self));

    // for each argument in plugstack.conf
    for(i = 0; i < ac; i++) {
        if(strncmp("target=", av[i], 7) == 0) {
            const char *optarg = av[i] + 7;
            if(!strlen(optarg)) {
                slurm_error("cc-tmpfs_mounts: no argument given to target= option");
                return -1;
            }
            bind_target = strdup(optarg);
            if(!bind_target) {
                slurm_error("cc-tmpfs_mounts: can't malloc :-(");
                return -1;
            }
            continue;
        }
        if(strncmp("bind=", av[i], 5) == 0) {
            const char *optarg = av[i] + 5;
            if(bind_dirs_count == MAX_BIND_DIRS) {
                slurm_error("cc-tmpfs_mounts: Reached MAX_BIND_DIRS (%d)",MAX_BIND_DIRS);
                return -1;
            }
            if(optarg[0] != '/') {
                slurm_error("cc-tmpfs_mounts: bind= option must start with a '/': (%s)",optarg);
                return -1;
            }
            if(!strlen(optarg)) {
                slurm_error("cc-tmpfs_mounts: no argument given to bind= option");
                return -1;
            }
            bind_dirs[bind_dirs_count] = strdup(optarg);
            if(!bind_dirs[bind_dirs_count]) {
                slurm_error("cc-tmpfs_mounts: can't malloc :-(");
                return -1;
            }
            bind_dirs_count++;
            continue;
        }
        if(strncmp("bindself=", av[i], 9) == 0) {
            const char *optarg = av[i] + 9;
            if(bind_self_count == MAX_BINDSELF_DIRS) {
                slurm_error("cc-tmpfs_mounts: Reached MAX_BINDSELF_DIRS (%d)",MAX_BINDSELF_DIRS);
                return -1;
            }
            if(optarg[0] != '/') {
                slurm_error("cc-tmpfs_mounts: bindself= option must start with a '/': (%s)",optarg);
                return -1;
            }
            if(!strlen(optarg)) {
                slurm_error("cc-tmpfs_mounts: no argument given to bindself= option");
                return -1;
            }
            bind_self[bind_self_count] = strdup(optarg);
            if(!bind_self[bind_self_count]) {
                slurm_error("cc-tmpfs_mounts: can't malloc :-(");
                return -1;
            }
            bind_self_count++;
            continue;
        }
        if(strncmp("tmpfs=", av[i], 6) == 0) {
            const char *optarg = av[i] + 6;
            if(tmpfs_dirs_count == MAX_TMPFS_DIRS) {
                slurm_error("cc-tmpfs_mounts: Reached MAX_TMPFS_DIRS (%d)",MAX_TMPFS_DIRS);
                return -1;
            }
            if(optarg[0] != '/') {
                slurm_error("cc-tmpfs_mounts: tmpfs= option must start with a '/': (%s)",optarg);
                return -1;
            }
            if(!strlen(optarg)) {
                slurm_error("cc-tmpfs_mounts: no argument given to tmpfs= option");
                return -1;
            }
            tmpfs_dirs[tmpfs_dirs_count] = strdup(optarg);
            if(!tmpfs_dirs[tmpfs_dirs_count]) {
                slurm_error("cc-tmpfs_mounts: can't malloc :-(");
                return -1;
            }
            tmpfs_dirs_count++;
            continue;
        }
        slurm_error("cc-tmpfs_mounts: Invalid option \"%s\"", av[i]);
        return -1;
    }
    return 0;
}

