#define _POSIX_C_SOURCE 200809L
#include "carmw/car_log_cleanup.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

static unsigned failures;
#define CHECK(x) do { if(!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); ++failures; } } while(0)
static void touch(const char *path)
{
    FILE *f = fopen(path, "w");
    CHECK(f != NULL);
    if(f) { fputs("keep or delete", f); fclose(f); }
}
int main(void)
{
    char root[] = "/tmp/carmw-cleanup-XXXXXX";
    CHECK(mkdtemp(root) != NULL);
    char log[512], file[512], nested[512], keep[512], link[512], special[512];
    snprintf(log, sizeof(log), "%s/log", root);
    snprintf(file, sizeof(file), "%s/log/event.txt", root);
    snprintf(nested, sizeof(nested), "%s/log/nested", root);
    snprintf(keep, sizeof(keep), "%s/setting.cfg", root);
    snprintf(link, sizeof(link), "%s/log/link", root);
    snprintf(special, sizeof(special), "%s/log/pipe", root);
    touch(keep);
    CHECK(car_log_cleanup_run(root)); /* no logs */
    CHECK(!car_log_cleanup_run("/"));
    CHECK(!car_log_cleanup_run("/tmp/../tmp"));
    CHECK(!car_log_cleanup_run("relative"));
    CHECK(symlink(root, log) == 0);
    CHECK(!car_log_cleanup_run(root)); /* never follow log symlink */
    CHECK(access(keep, F_OK) == 0);
    CHECK(unlink(log) == 0);
    CHECK(mkdir(log, 0700) == 0);
    CHECK(mkdir(nested, 0700) == 0);
    touch(file);
    CHECK(symlink(root, link) == 0);
    CHECK(car_log_cleanup_start(root));
    car_log_cleanup_shutdown();
    CHECK(car_log_cleanup_status() == CAR_LOG_CLEANUP_DONE);
    CHECK(access(file, F_OK) != 0);
    CHECK(access(nested, F_OK) != 0);
    CHECK(access(keep, F_OK) == 0);
    CHECK(access(log, F_OK) == 0);
    CHECK(mkfifo(special, 0600) == 0);
    CHECK(car_log_cleanup_start(root));
    car_log_cleanup_shutdown();
    CHECK(car_log_cleanup_status() == CAR_LOG_CLEANUP_FAILED);
    CHECK(access(special, F_OK) == 0);
    CHECK(access(keep, F_OK) == 0);
    CHECK(unlink(special) == 0);
    CHECK(rmdir(log) == 0);
    CHECK(unlink(keep) == 0);
    CHECK(rmdir(root) == 0);
    return failures ? 1 : 0;
}
