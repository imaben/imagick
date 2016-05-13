#pragma once

#include "process.h"

void imagick_add_event(imagick_worker_ctx_t *ctx, int fd, int flags);
void imagick_delete_event(imagick_worker_ctx_t *ctx, int fd, int flags);
void imagick_modify_event(imagick_worker_ctx_t *ctx, int fd, int flags);


