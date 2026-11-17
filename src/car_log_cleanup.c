#define _POSIX_C_SOURCE 200809L
#include "carmw/car_log_cleanup.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static pthread_mutex_t s_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_t s_thread;
static bool s_joinable;
static car_log_cleanup_status_t s_status;
static char s_root[512];

static int open_root(const char *root)
{
    if(!root || root[0] != '/' || strlen(root) >= sizeof(s_root) || !root[1]) return -1;
    char path[512];
    strcpy(path, root + 1);
    int fd = open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    char *save = NULL;
    for(char *part = strtok_r(path, "/", &save); part; part = strtok_r(NULL, "/", &save)) {
        if(!strcmp(part, ".") || !strcmp(part, "..")) { close(fd); return -1; }
        int next = openat(fd, part, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        close(fd);
        if(next < 0) return -1;
        fd = next;
    }
    return fd;
}

static bool clear_dir(int fd, dev_t device, unsigned depth)
{
    if(depth > 32) { close(fd); return false; }
    DIR *dir = fdopendir(fd);
    if(!dir) { close(fd); return false; }
    bool ok = true;
    struct dirent *entry;
    for(;;) {
        errno = 0;
        entry = readdir(dir);
        if(!entry) { if(errno) ok = false; break; }
        if(!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        struct stat st;
        if(fstatat(fd, entry->d_name, &st, AT_SYMLINK_NOFOLLOW) != 0) { ok = false; continue; }
        /* Do not cross mounts or follow links; reject special files. */
        if(st.st_dev != device) { ok = false; continue; }
        if(S_ISDIR(st.st_mode)) {
            int child = openat(fd, entry->d_name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
            struct stat actual;
            if(child < 0) { ok = false; continue; }
            if(fstat(child, &actual) || actual.st_dev != device || actual.st_ino != st.st_ino) {
                close(child); ok = false; continue;
            }
            if(!clear_dir(child, device, depth + 1) || unlinkat(fd, entry->d_name, AT_REMOVEDIR)) ok = false;
        }
        else if(S_ISREG(st.st_mode) || S_ISLNK(st.st_mode)) {
            if(unlinkat(fd, entry->d_name, 0)) ok = false;
        }
        else ok = false;
    }
    if(closedir(dir)) ok = false;
    return ok;
}

bool car_log_cleanup_run(const char *root)
{
    int parent = open_root(root);
    if(parent < 0) return false;
    struct stat parent_st;
    if(fstat(parent, &parent_st)) { close(parent); return false; }
    int fd = openat(parent, "log", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    int error = errno;
    close(parent);
    if(fd < 0) return error == ENOENT;
    struct stat st;
    if(fstat(fd, &st) || st.st_dev != parent_st.st_dev) { close(fd); return false; }
    return clear_dir(fd, st.st_dev, 0);
}

static void *worker(void *unused)
{
    (void)unused;
    bool ok = car_log_cleanup_run(s_root);
    pthread_mutex_lock(&s_lock);
    s_status = ok ? CAR_LOG_CLEANUP_DONE : CAR_LOG_CLEANUP_FAILED;
    pthread_mutex_unlock(&s_lock);
    return NULL;
}

bool car_log_cleanup_start(const char *root)
{
    if(!root || strlen(root) >= sizeof(s_root)) return false;
    pthread_mutex_lock(&s_lock);
    if(s_status == CAR_LOG_CLEANUP_RUNNING) { pthread_mutex_unlock(&s_lock); return false; }
    if(s_joinable) { pthread_join(s_thread, NULL); s_joinable = false; }
    strcpy(s_root, root);
    s_status = CAR_LOG_CLEANUP_RUNNING;
    if(pthread_create(&s_thread, NULL, worker, NULL)) s_status = CAR_LOG_CLEANUP_FAILED;
    else s_joinable = true;
    bool started = s_joinable;
    pthread_mutex_unlock(&s_lock);
    return started;
}

car_log_cleanup_status_t car_log_cleanup_status(void)
{
    pthread_mutex_lock(&s_lock);
    car_log_cleanup_status_t status = s_status;
    pthread_mutex_unlock(&s_lock);
    return status;
}

void car_log_cleanup_shutdown(void)
{
    /* Called after the UI event loop has stopped submitting work. */
    if(s_joinable) { pthread_join(s_thread, NULL); s_joinable = false; }
}
