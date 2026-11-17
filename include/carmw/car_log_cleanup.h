#ifndef CARMW_CAR_LOG_CLEANUP_H
#define CARMW_CAR_LOG_CLEANUP_H

#include <stdbool.h>

/* A trusted deployment directory; only its direct log child is eligible. */
#define CAR_LOG_CLEANUP_ROOT "/opt/car_dashboard"
typedef enum {
    CAR_LOG_CLEANUP_IDLE,
    CAR_LOG_CLEANUP_RUNNING,
    CAR_LOG_CLEANUP_DONE,
    CAR_LOG_CLEANUP_FAILED,
    CAR_LOG_CLEANUP_UNAVAILABLE
} car_log_cleanup_status_t;

/* root must be absolute with no dot components or symbolic-link ancestors.
 * The caller owns the policy for root; never pass user-entered paths here.
 * Missing log directory is a successful no-op. Never removes root or log. */
bool car_log_cleanup_run(const char *root);
bool car_log_cleanup_start(const char *root);
car_log_cleanup_status_t car_log_cleanup_status(void);
void car_log_cleanup_shutdown(void);

#endif
