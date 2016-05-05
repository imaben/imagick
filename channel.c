#include "channel.h"


void imagick_close_channel(int *sockfd)
{
    close(sockfd[0]);
    close(sockfd[1]);
}
