#include <sys/socket.h>
#include <linux/netlink.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#define NETLINK_USER 31
#define MYPROTO NETLINK_USERSOCK

#define MAX_PAYLOAD 1024 /* maximum payload size*/
struct sockaddr_nl src_addr, dest_addr;
struct nlmsghdr *nlh = NULL;
struct iovec iov;
int sock_fd;
struct msghdr msg;
char temp[256] = {0};

int open_netlink(void)
{
    int sock1;
    struct sockaddr_nl addr1;
    int group = NETLINK_USER;

    sock1 = socket(AF_NETLINK, SOCK_RAW, MYPROTO);
    if (sock1 < 0) {
        printf("sock < 0.\n");
        return sock1;
    }

    memset((void *) &addr1, 0, sizeof(addr1));
    addr1.nl_family = AF_NETLINK;
    addr1.nl_pid = getpid();
    /* This doesn't work for some reason. See the setsockopt() below. */
    /* addr.nl_groups = MYMGRP; */

    if (bind(sock1, (struct sockaddr *) &addr1, sizeof(addr1)) < 0) {
        printf("bind < 0.\n");
        return -1;
    }

    if (setsockopt(sock1, 270, NETLINK_ADD_MEMBERSHIP, &group, sizeof(group)) < 0) {
        printf("setsockopt < 0\n");
        return -1;
    }

    return sock1;
}

void read_event(int sock)
{
    struct sockaddr_nl nladdr1;
    struct msghdr msg1;
    struct iovec iov1;
    char buffer1[65536];
    int ret1;

    iov1.iov_base = (void *) buffer1;
    iov1.iov_len = sizeof(buffer1);
    msg1.msg_name = (void *) &(nladdr1);
    msg1.msg_namelen = sizeof(nladdr1);
    msg1.msg_iov = &iov1;
    msg1.msg_iovlen = 1;

    printf("Ok, listening.\n");
    ret1 = recvmsg(sock, &msg1, 0);
    if (ret1 < 0)
        printf("ret < 0.\n");
    else
        printf("Received message payload: %s\n", NLMSG_DATA((struct nlmsghdr *) &buffer1));
        strcpy(temp, NLMSG_DATA((struct nlmsghdr *) &buffer1));
}


int main()
{
    int nls1;
    char buf[1000];
    char *buf1 = NULL;
    size_t size = 0;


FILE *file = fopen("backup.txt", "w+");
fclose(file);


sock_fd=socket(PF_NETLINK, SOCK_RAW, NETLINK_USER);
if(sock_fd<0)
return -1;

nls1 = open_netlink();
    if (nls1 < 0)
        return nls1;

memset(&src_addr, 0, sizeof(src_addr));
src_addr.nl_family = AF_NETLINK;
src_addr.nl_pid = getpid(); /* self pid */

bind(sock_fd, (struct sockaddr*)&src_addr, sizeof(src_addr));

while(1){
memset(&dest_addr, 0, sizeof(dest_addr));
memset(&dest_addr, 0, sizeof(dest_addr));
dest_addr.nl_family = AF_NETLINK;
dest_addr.nl_pid = 0; /* For Linux Kernel */
dest_addr.nl_groups = 0; /* unicast */

read_event(nls1);

        switch( temp[0] )
        {
            case 'l':
                file = fopen("backup.txt", "r");
                fseek(file, 0, SEEK_END); /* Go to end of file */
                size = ftell(file);
                rewind(file);
                buf1 = malloc((size + 1) * sizeof(*buf1));
                fread(buf1, size, 1, file);
                buf[size] = '\0';
                strcpy(temp,buf1);
                fclose(file);
                break;
            case 'r':
                printf("test\n");
                break;
            default :
                file = fopen("backup.txt", "a+");
                int len = strlen(temp);
                for(int i = 0;i < len;i++)
                fputc(temp[i], file);
                fprintf(file, "\n");
                strcpy(temp, "backup ok");
                fclose(file);
        }

nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_PAYLOAD));
memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));
nlh->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);
nlh->nlmsg_pid = getpid();
nlh->nlmsg_flags = 0;

strcpy(NLMSG_DATA(nlh), temp);

iov.iov_base = (void *)nlh;
iov.iov_len = nlh->nlmsg_len;
msg.msg_name = (void *)&dest_addr;
msg.msg_namelen = sizeof(dest_addr);
msg.msg_iov = &iov;
msg.msg_iovlen = 1;

printf("Sending message to kernel\n");
sendmsg(sock_fd,&msg,0);
}

close(sock_fd);
}
