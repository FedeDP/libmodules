#pragma once

#include "priv.h"
#include <fcntl.h>

/* Useful macros to smooth away differences between supported OS */
enum op_type { ADD, RM };

int poll_create(poll_priv_t *priv);
int poll_set_new_evt(poll_priv_t *priv, ev_src_t *tmp, const enum op_type flag);
int poll_init(poll_priv_t *priv);
int poll_wait(poll_priv_t *priv, const int timeout);
ev_src_t *poll_recv(poll_priv_t *priv, const int idx);
int poll_get_fd(const poll_priv_t *priv);
int poll_clear(poll_priv_t *priv);
int poll_destroy(poll_priv_t *priv);

int poll_consume_sgn(poll_priv_t *priv, const int idx, ev_src_t *src, m_evt_sgn_t *msg);
int poll_consume_tmr(poll_priv_t *priv, const int idx, ev_src_t *src, m_evt_tmr_t *msg);
int poll_consume_pt(poll_priv_t *priv, const int idx, ev_src_t *src, m_evt_path_t *pt_msg);
int poll_consume_pid(poll_priv_t *priv, const int idx, ev_src_t *src, m_evt_pid_t *pid_msg);
int poll_consume_task(poll_priv_t *priv, const int idx, ev_src_t *src, m_evt_task_t *task_msg);
int poll_consume_thresh(poll_priv_t *priv, const int idx, ev_src_t *src, m_evt_thresh_t *thresh_msg);

int poll_notify_userevent(poll_priv_t *priv, ev_src_t *src);