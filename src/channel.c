#include <stdio.h>
#include "channel.h"
#include "log.h"
#include "log.h"

#define check_fd_valid(fd) (fd != -1)

int imagick_write_channel(imagick_channel_t *ch, imagick_channel_cmd_t *cmd)
{
    if (!check_fd_valid(ch->fd)) {
        return -1;
    }
    int r = write(ch->fd, cmd, sizeof(imagick_channel_cmd_t));
    if (r == -1) {
        imagick_log_error("Failed to write channel:%d, code:%d", ch->slot, r);
        return -1;
    }
    return 0;
}

void imagick_close_channel(int *sockfd)
{
    close(sockfd[0]);
    close(sockfd[1]);
}
