#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#define handle_err(msg)     \
    do                      \
    {                       \
        perror(msg);        \
        exit(EXIT_FAILURE); \
    } while (0)
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
#define MAX_ACCEPT_THREAD 200
uint empty_thread_sp = 1;
int empty_thread[MAX_ACCEPT_THREAD];
pthread_t *process_thread;
struct ip_args
{
    int port;
    int ipaddr;
};
struct client_thread
{
    unsigned int cfd;
    int thread_number;
};
int create_thread_pool()
{
    int i = 0;
    for (i = 0; i < MAX_ACCEPT_THREAD; i++)
    {
        empty_thread[i] = i;
    }
    process_thread =malloc(MAX_ACCEPT_THREAD * sizeof *process_thread);
}
int get_thread()
{

    if (empty_thread_sp < MAX_ACCEPT_THREAD)
    {
        pthread_mutex_lock(&lock);
        empty_thread_sp++;
        pthread_mutex_unlock(&lock);
        return empty_thread[empty_thread_sp];
    }
    else
    {
        return get_thread();
    }
}
int release_thread(struct client_thread *ct)
{
    if (empty_thread_sp > 0)
    {
        pthread_mutex_lock(&lock);
        empty_thread[--empty_thread_sp] = ct->thread_number;
        pthread_mutex_unlock(&lock);
    }
    else
    {
        return 0;
    }
}
void *handle_client(void *arg)
{
    pthread_detach(pthread_self());
    int no;
    char buffer[1024];
    //TODO is this ok?
    memset(&buffer,0,sizeof buffer);
    struct client_thread *ct = arg;
    int n;
    n = read(ct->cfd, buffer, 1024);
    printf("%s\n", buffer);
    write(ct->cfd, &buffer, strlen(buffer));
    close(ct->cfd);
    release_thread(ct);
    no = ct->thread_number;
    free(ct);
    
}
void *start_listen(void *args)
{
    struct ip_args *listen_addr = args;
    unsigned int sfd;
    struct client_thread *ct; 
    struct sockaddr_in my_addr;
    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd == -1)
        handle_err("socket");
    else
        printf("start listening on port: %d\n", listen_addr->port);
    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_addr.s_addr = htonl(listen_addr->ipaddr);
    my_addr.sin_port = htons(listen_addr->port);
    struct sockaddr_in peer_addr;
    if (bind(sfd, (struct sockaddr *)&my_addr, sizeof(my_addr)) < 0)
        handle_err("bind");
    if (listen(sfd, 10) == -1)
        handle_err("listen");

    while (1)
    {
        ct= malloc(sizeof *ct); 
        socklen_t addr_size = sizeof peer_addr;
        ct->cfd = accept(sfd, (struct sockaddr *)&peer_addr, &addr_size);
        if (ct->cfd == 1)
            handle_err("accept");

        ct->thread_number = 0;
        ct->thread_number = get_thread();


        if (pthread_create(&process_thread[ct->thread_number], NULL, handle_client, (void *)ct) != 0)
            handle_err("pthread");
    }
}
int main()
{
    create_thread_pool();
    /* 1- socket 2-bind 3-listen 4-accept*/
    struct ip_args listen_addr1, listen_addr2, control_addr;
    listen_addr1.ipaddr = INADDR_ANY;
    listen_addr1.port = 2000;
    listen_addr2.ipaddr = INADDR_ANY;
    listen_addr2.port = 3000;
    control_addr.ipaddr = INADDR_LOOPBACK;
    control_addr.port = 57000;
    pthread_t listen_thread[3];
    pthread_create(&listen_thread[0], NULL, start_listen, (void *)&listen_addr1);
    pthread_create(&listen_thread[1], NULL, start_listen, (void *)&listen_addr2);
    //pthread_create(&listen_thread[2], NULL, start_listen, (void *)&control_addr);
    start_listen((void *)&control_addr);
    return 0;
}
