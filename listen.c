#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#define handle_err(msg)     \
    do                      \
    {                       \
        perror(msg);        \
        exit(EXIT_FAILURE); \
    } while (0)
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
#define MAX_ACCEPT_THREAD 10
uint thread_sp = MAX_ACCEPT_THREAD;
int threads[MAX_ACCEPT_THREAD];
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
void create_thread_pool()
{
    int i = 0;
    for (i = 0; i < MAX_ACCEPT_THREAD; i++)
    {
        threads[i] = i;
    }
    process_thread = malloc(MAX_ACCEPT_THREAD * sizeof *process_thread);
}
int get_thread()
{
    while (1)
    {
        if (thread_sp > 0)
        {
            pthread_mutex_lock(&lock);
            thread_sp--;
            pthread_mutex_unlock(&lock);
            return threads[thread_sp];
        }
        printf("No_thread\n");
        sleep(1);
    }
}
int release_thread(struct client_thread *ct)
{
    if (thread_sp < MAX_ACCEPT_THREAD)
    {
        pthread_mutex_lock(&lock);
        threads[thread_sp++] = ct->thread_number;
        pthread_mutex_unlock(&lock);
    }
    return 0;
}
void *handle_client(void *arg)
{
    pthread_detach(pthread_self());
    char buffer[1024];
    memset(&buffer, 0, sizeof buffer);
    struct client_thread *ct = arg;
    //TODO Fix read failure
    read(ct->cfd, buffer, 1024);
    //puts(buffer);
    write(ct->cfd, &buffer, strlen(buffer));
    close(ct->cfd);
    //Create a dummy wait
    sleep((int)(ct->thread_number * 2));
    release_thread(ct);
    free(ct);
    return NULL;
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
        ct = malloc(sizeof *ct);
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
void dummy_wait()
{
    sigset_t myset;
    (void)sigemptyset(&myset);
    while (1)
    {
        (void)printf("TCPServer is running:\n");
        (void)sigsuspend(&myset);
    }
}
int main()
{
    create_thread_pool();
    /* 1- socket 2-bind 3-listen 4-accept*/
    pthread_t listen_thread[3];
    struct ip_args listen_addr[2];
    int ports[3] = {2000, 3000, 57000};
    int i, ip[3] = {INADDR_ANY, INADDR_ANY, INADDR_LOOPBACK};
    for (i = 0; i < 3; i++)
    {
        listen_addr[i].port = ports[i];
        listen_addr[i].ipaddr = ip[i];
        pthread_create(&listen_thread[i], NULL, start_listen, &listen_addr[i]);
    }
    dummy_wait();
    return 0;
}
