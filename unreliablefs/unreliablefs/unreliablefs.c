#define FUSE_USE_VERSION 29

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>

#include <fuse.h>

#include "unreliablefs_ops.h"
#include "wiscAFS_ops.h"
#include "unreliablefs.h"

extern struct err_inj_q *config_init(const char* conf_path);
extern void config_delete(struct err_inj_q *config);

struct unreliablefs_config conf;

static struct fuse_operations unreliable_ops = {
    .getattr     = wiscAFS_getattr,
    .readlink    = unreliable_readlink,
    .mknod       = unreliable_mknod,
    .mkdir       = wiscAFS_mkdir,
    .unlink      = wiscAFS_unlink,
    .rmdir       = wiscAFS_rmdir,
    .symlink     = unreliable_symlink,
    .rename      = wiscAFS_rename,
    .link        = unreliable_link,
    .chmod       = wiscAFS_chmod,
    .chown       = wiscAFS_chown,
    .truncate    = wiscAFS_truncate,
    .open	 = wiscAFS_open,
    .read	 = wiscAFS_read,
    .write       = wiscAFS_write,
    .statfs      = unreliable_statfs,
    .flush       = wiscAFS_flush,
    .release     = wiscAFS_release,
    .fsync       = unreliable_fsync,
#ifdef HAVE_XATTR
    .setxattr    = unreliable_setxattr,
    .getxattr    = wiscAFS_getxattr,
    .listxattr   = unreliable_listxattr,
    .removexattr = unreliable_removexattr,
#endif /* HAVE_XATTR */
    .opendir     = wiscAFS_opendir,
    .readdir     = wiscAFS_readdir,
    .releasedir  = unreliable_releasedir,
    .fsyncdir    = unreliable_fsyncdir,

    .init        = wiscAFS_init,
    .destroy     = unreliable_destroy,
    .access      = wiscAFS_access,
    .create      = wiscAFS_create,
    .fgetattr    = wiscAFS_fgetattr,
    .lock        = unreliable_lock,
#if !defined(__OpenBSD__)
    .ioctl       = unreliable_ioctl,
#endif /* __OpenBSD__ */
#ifdef HAVE_FLOCK
    .flock       = unreliable_flock,
#endif /* HAVE_FLOCK */
#ifdef HAVE_FALLOCATE
    .fallocate   = unreliable_fallocate,
#endif /* HAVE_FALLOCATE */
#ifdef HAVE_UTIMENSAT
    .utimens     = unreliable_utimens,
#endif /* HAVE_UTIMENSAT */
};

enum {
     KEY_HELP,
     KEY_VERSION,
     KEY_DEBUG,
};

#define UNRELIABLEFS_OPT(t, p, v) { t, offsetof(struct unreliablefs_config, p), v }
#define UNRELIABLEFS_VERSION "0.1"

static struct fuse_opt unreliablefs_opts[] = {
    UNRELIABLEFS_OPT("-seed=%u",           seed, 0),
    UNRELIABLEFS_OPT("-basedir=%s",        basedir, 0),

    FUSE_OPT_KEY("-d",             KEY_DEBUG),
    FUSE_OPT_KEY("-V",             KEY_VERSION),
    FUSE_OPT_KEY("-v",             KEY_VERSION),
    FUSE_OPT_KEY("--version",      KEY_VERSION),
    FUSE_OPT_KEY("-h",             KEY_HELP),
    FUSE_OPT_KEY("--help",         KEY_HELP),
    FUSE_OPT_KEY("subdir",         FUSE_OPT_KEY_DISCARD),
    FUSE_OPT_KEY("modules=",       FUSE_OPT_KEY_DISCARD),
    FUSE_OPT_END
};

static int unreliablefs_opt_proc(void *data, const char *arg, int key, struct fuse_args *outargs)
{
    switch (key) {
    case KEY_HELP:
        fprintf(stderr,
            "usage: unreliablefs mountpoint [options]\n\n"
            "general options:\n"
            "    -h   --help            print help\n"
            "    -v   --version         print version\n"
            "    -d                     enable debug output (implies -f)\n"
            "    -f                     foreground operation\n\n"
            "unreliablefs options:\n"
            "    -seed=NUM              random seed\n"
            "    -basedir=STRING        directory to mount\n\n");
        exit(1);

    case KEY_VERSION:
        fprintf(stderr, "unreliablefs version %s\n", UNRELIABLEFS_VERSION);
        fuse_opt_add_arg(outargs, "--version");
        fuse_main(outargs->argc, outargs->argv, &unreliable_ops, NULL);
        exit(1);
    }
    return 1;
}

int is_dir(const char *path) {
    struct stat statbuf;
    if (stat(path, &statbuf) != 0) {
        return 0;
    }

    return S_ISDIR(statbuf.st_mode);
}

int main(int argc, char *argv[])
{
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    memset(&conf, 0, sizeof(conf));
    conf.seed = time(0);
    conf.basedir = "/";
    fuse_opt_parse(&args, &conf, unreliablefs_opts, unreliablefs_opt_proc);
    srand(conf.seed);
    fprintf(stdout, "random seed = %d\n", conf.seed);
    fprintf(stderr, "Did this work?\n");

    if (is_dir(conf.basedir) == 0) {
       fprintf(stderr, "basedir ('%s') is not a directory\n", conf.basedir);
       fuse_opt_free_args(&args);
       return EXIT_FAILURE;
    }
    char subdir_option[PATH_MAX];
    sprintf(subdir_option, "-omodules=subdir,subdir=%s", conf.basedir);
    fuse_opt_add_arg(&args, subdir_option);
    /* build config_path */
    char *real_path = realpath(conf.basedir, NULL);
    if (!real_path) {
        perror("realpath");
        fuse_opt_free_args(&args);
        return EXIT_FAILURE;
    }
    conf.basedir = real_path;
    size_t sz = strlen(DEFAULT_CONF_NAME) + strlen(conf.basedir) + 2;
    conf.config_path = malloc(sz);
    if (!conf.config_path) {
        perror("malloc");
        fuse_opt_free_args(&args);
        return EXIT_FAILURE;
    }
    /* read configuration file on start */
    snprintf(conf.config_path, sz, "%s/%s", conf.basedir, DEFAULT_CONF_NAME);
    conf.errors = config_init(conf.config_path);
    if (!conf.errors) {
        fprintf(stdout, "error injections are not configured!\n");
    }
    if (pthread_mutex_init(&conf.mutex, NULL) != 0) {
        fuse_opt_free_args(&args);
        perror("pthread_mutex_init");
        return EXIT_FAILURE;
    }

    fprintf(stdout, "starting FUSE filesystem unreliablefs\n");
    int ret = fuse_main(args.argc, args.argv, &unreliable_ops, NULL);

    /* cleanup */
    fuse_opt_free_args(&args);
    config_delete(conf.errors);
    if (conf.config_path)
        free(conf.config_path);
    if (!ret) {
        fprintf(stdout, "random seed = %d\n", conf.seed);
    }

    return ret;
}
