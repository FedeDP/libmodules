#include "poll_priv.h"
#include <sys/signalfd.h>
#include <sys/timerfd.h>
#include <sys/inotify.h>
#include <sys/eventfd.h>
#include <sys/types.h>
#include <linux/version.h>
#include <limits.h>
#include <unistd.h>
#include <signal.h>
#include <sys/syscall.h>

/* Inotify related defines */
#define BUF_LEN (sizeof(struct inotify_event) + NAME_MAX + 1)

static void create_timerfd(ev_src_t *tmp);
static void create_signalfd(ev_src_t *tmp);
static void create_inotifyfd(ev_src_t *tmp);
static void create_pidfd(ev_src_t *tmp);
static void create_eventfd(ev_src_t *tmp);

static void create_timerfd(ev_src_t *tmp) {
    tmp->tmr_src.f.fd = timerfd_create(tmp->tmr_src.its.clock_id, TFD_NONBLOCK | TFD_CLOEXEC);
    struct itimerspec timerValue = {{0}};
    timerValue.it_value.tv_sec = tmp->tmr_src.its.ms / 1000;
    timerValue.it_value.tv_nsec = (tmp->tmr_src.its.ms % 1000) * 1000 * 1000;
    if (!(tmp->flags & M_SRC_ONESHOT)) {
        /* Set interval */
        timerValue.it_interval.tv_sec = tmp->tmr_src.its.ms / 1000;
        timerValue.it_interval.tv_nsec = (tmp->tmr_src.its.ms % 1000) * 1000 * 1000;
    }
    const int abs_fl = tmp->flags & M_SRC_TMR_ABSOLUTE ? TFD_TIMER_ABSTIME : 0;
    timerfd_settime(tmp->tmr_src.f.fd, abs_fl, &timerValue, NULL);
}

static void create_signalfd(ev_src_t *tmp) {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, tmp->sgn_src.sgs.signo);
    sigprocmask(SIG_BLOCK, &mask, NULL);
    tmp->sgn_src.f.fd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
}

static void create_inotifyfd(ev_src_t *tmp) {
    tmp->path_src.f.fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    inotify_add_watch(tmp->path_src.f.fd, tmp->path_src.pt.path, tmp->path_src.pt.events);
}

static void create_pidfd(ev_src_t *tmp) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0)
    tmp->pid_src.f.fd = syscall(__NR_pidfd_open, tmp->pid_src.pid.pid, 0);
#endif
}

static void create_eventfd(ev_src_t *tmp) {
    tmp->task_src.f.fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
}

int poll_notify_userevent(poll_priv_t *priv, ev_src_t *src) {
    uint64_t u = 1;
    /* task_src and thresh_src share memory layout, thus using task_src.f.fd is ok */
    if (write(src->task_src.f.fd, &u, sizeof(uint64_t)) == sizeof(uint64_t)) {
        return 0;
    }
    return -errno;
}

void create_priv_fd(ev_src_t *tmp) {
    switch (tmp->type) {
        case M_SRC_TYPE_TMR:
            create_timerfd(tmp);
            break;
        case M_SRC_TYPE_SGN:
            create_signalfd(tmp);
            break;
        case M_SRC_TYPE_PATH:
            create_inotifyfd(tmp);
            break;
        case M_SRC_TYPE_PID:
            create_pidfd(tmp);
            break;
        case M_SRC_TYPE_TASK:
        case M_SRC_TYPE_THRESH:
            create_eventfd(tmp);
            break;
        default:
            break;
    }
}

int poll_consume_sgn(poll_priv_t *priv, const int idx, ev_src_t *src, m_evt_sgn_t *sgn_msg) {
    struct signalfd_siginfo fdsi;
    const size_t s = read(src->sgn_src.f.fd, &fdsi, sizeof(struct signalfd_siginfo));
    if (s == sizeof(struct signalfd_siginfo)) {
        return 0;
    }
    return -errno;
}

int poll_consume_tmr(poll_priv_t *priv, const int idx, ev_src_t *src, m_evt_tmr_t *tm_msg) {
    uint64_t t;
    if (read(src->tmr_src.f.fd, &t, sizeof(uint64_t)) == sizeof(uint64_t)) {
        return 0;
    }
    return -errno;
}

int poll_consume_pt(poll_priv_t *priv, const int idx, ev_src_t *src, m_evt_path_t *pt_msg) {
    char buffer[BUF_LEN];
    const size_t length = read(src->path_src.f.fd, buffer, BUF_LEN);
    if (length > 0) {
        struct inotify_event *event = (struct inotify_event *) buffer;
        if (event->len) {
            pt_msg->events = event->mask;
            return 0;
        }
    }
    return -errno;
}

int poll_consume_pid(poll_priv_t *priv, const int idx, ev_src_t *src, m_evt_pid_t *pid_msg) {
    pid_msg->events = 0;
    return 0; // nothing to do
}

int poll_consume_task(poll_priv_t *priv, const int idx, ev_src_t *src, m_evt_task_t *task_msg) {
    uint64_t u;
    if (read(src->task_src.f.fd, &u, sizeof(uint64_t)) == sizeof(uint64_t)) {
        task_msg->retval = src->task_src.retval;
        return 0;
    }
    return -errno;
}

int poll_consume_thresh(poll_priv_t *priv, const int idx, ev_src_t *src, m_evt_thresh_t *thresh_msg) {
    uint64_t u;
    if (read(src->task_src.f.fd, &u, sizeof(uint64_t)) == sizeof(uint64_t)) {
        return 0;
    }
    return -errno;
}