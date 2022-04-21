/* J. David's webserver */
/* This is a simple webserver.
 * Created November 1999 by J. David Blackstone.
 * CSE 4344 (Network concepts), Prof. Zeigler
 * University of Texas at Arlington
 */
 /* This program compiles for Sparc Solaris 2.6.
  * To compile for Linux:
  *  1) Comment out the #include <pthread.h> line.
  *  2) Comment out the line that defines the variable newthread.
  *  3) Comment out the two lines that run pthread_create().
  *  4) Uncomment the line that runs accept_request().
  *  5) Remove -lsocket from the Makefile.
  */

#include <stdio.h>      //输入输出
#include <ctype.h>      //字符处理
#include <string.h>     //字符串相关函数
#include <strings.h>    //同上, 可以认为没用了
#include <stdlib.h>     //各种通用工具函数&内存分配函数

#include <sys/socket.h> //套接字
#include <sys/types.h>  //基本系统数据类型
#include <sys/stat.h>   //文件状态
#include <sys/wait.h>   //进程控制

#include <netinet/in.h> //internet地址族
#include <arpa/inet.h>  //internet定义
#include <unistd.h>     //符号常量
#include <pthread.h>    //线程库

#define ISspace(x) isspace((int)(x))
/*
  作用: 主要用于检查参数c是否为空白字符, 也就是判断是否为空格(' ')、水平定位字符
  ('\t')、归位键('\r')、换行('\n')、垂直定位字符('\v')或翻页('\f')的情况
  reutrn: 如果是返回非零, 否则返回0
*/

#define SERVER_STRING "Server: jdbhttpd/0.1.0\r\n"
  //server_string: 响应头

void* accept_request(void*); // 接受请求
/*
void accept_request(int);
*/
void bad_request(int);
void cat(int, FILE*);
void cannot_execute(int);
void error_die(const char*);
void execute_cgi(int, const char*, const char*, const char*);
int get_line(int, char*, int);
void headers(int, const char*);
void not_found(int);
void serve_file(int, const char*);
int startup(u_short*);
void unimplemented(int);

/**********************************************************************/
/* A request has caused a call to accept() on the server port to
 * return.  Process the request appropriately.
 * Parameters: the socket connected to the client */

 /**********************************************************************/
 /*
 请求调用服务器端口上的accept()来返回。适当处理请求。
参数:客户端连接的socket
 */
 /*
 void *accept_request(void *client1)
 */
void* accept_request(void* client1)  {
    int client = *(int*)client1;
    char buf[1024];
    int numchars;
    char method[255];
    char url[255];
    char path[512];
    size_t i, j;
    struct stat st;
    /*
    struct stat {
        mode_t     st_mode;       //文件对应的模式，文件，目录等
        ino_t      st_ino;       //inode节点号
        dev_t      st_dev;        //设备号码
        dev_t      st_rdev;       //特殊设备号码
        nlink_t    st_nlink;      //文件的连接数
        uid_t      st_uid;        //文件所有者
        gid_t      st_gid;        //文件所有者对应的组
        off_t      st_size;       //普通文件，对应的文件字节数
        time_t     st_atime;      //文件最后被访问的时间
        time_t     st_mtime;      //文件内容最后被修改的时间
        time_t     st_ctime;      //文件状态改变时间
        blksize_t st_blksize;    //文件内容对应的块大小
        blkcnt_t   st_blocks;     //文件内容对应的块数量
    };
    */
    int cgi = 0;      /* becomes true if server decides this is a CGI program */
    char* query_string = NULL;

    numchars = get_line(client, buf, sizeof(buf));
    i = 0; j = 0;
    while (!ISspace(buf[j]) && (i < sizeof(method) - 1))
    {
        method[i] = buf[j];
        i++; j++;
    }
    method[i] = '\0';

    if (strcasecmp(method, "GET") && strcasecmp(method, "POST"))
    {
        unimplemented(client);
        return NULL;
        /*
        return -> return NULL
        */
    }

    if (strcasecmp(method, "POST") == 0)
        cgi = 1;

    i = 0;
    while (ISspace(buf[j]) && (j < sizeof(buf)))
        j++;
    while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < sizeof(buf)))
    {
        url[i] = buf[j];
        i++; j++;
    }
    url[i] = '\0';

    if (strcasecmp(method, "GET") == 0)
    {
        query_string = url;
        while ((*query_string != '?') && (*query_string != '\0'))
            query_string++;
        if (*query_string == '?')
        {
            cgi = 1;
            *query_string = '\0';
            query_string++;
        }
    }

    sprintf(path, "htdocs%s", url);
    if (path[strlen(path) - 1] == '/')
        strcat(path, "index.html");
    if (stat(path, &st) == -1) {
        while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
            numchars = get_line(client, buf, sizeof(buf));
        not_found(client);
    }
    else
    {
        if ((st.st_mode & S_IFMT) == S_IFDIR)
            strcat(path, "/index.html");
        if ((st.st_mode & S_IXUSR) ||
            (st.st_mode & S_IXGRP) ||
            (st.st_mode & S_IXOTH))
            cgi = 1;
        if (!cgi)
            serve_file(client, path);
        else
            execute_cgi(client, path, method, query_string);
    }

    close(client);
    return NULL;
    /*
    -> return NULL
    */
}

/**********************************************************************/
/* Inform the client that a request it has made has a problem.
 * Parameters: client socket */
 /**********************************************************************/
void bad_request(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "<P>Your browser sent a bad request, ");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "such as a POST without a Content-Length.\r\n");
    send(client, buf, sizeof(buf), 0);
}

/**********************************************************************/
/* Put the entire contents of a file out on a socket.  This function
 * is named after the UNIX "cat" command, because it might have been
 * easier just to do something like pipe, fork, and exec("cat").
 * Parameters: the client socket descriptor
 *             FILE pointer for the file to cat */
 /**********************************************************************/
void cat(int client, FILE* resource)
{
    char buf[1024];

    fgets(buf, sizeof(buf), resource);
    while (!feof(resource))
    {
        send(client, buf, strlen(buf), 0);
        fgets(buf, sizeof(buf), resource);
    }
}

/**********************************************************************/
/* Inform the client that a CGI script could not be executed.
 * Parameter: the client socket descriptor. */
 /**********************************************************************/
void cannot_execute(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Print out an error message with perror() (for system errors; based
 * on value of errno, which indicates system call errors) and exit the
 * program indicating an error. */
 /**********************************************************************/
void error_die(const char* sc)
{
    perror(sc);
    exit(1);
}

/**********************************************************************/
/* Execute a CGI script.  Will need to set environment variables as
 * appropriate.
 * Parameters: client socket descriptor
 *             path to the CGI script */
 /**********************************************************************/
void execute_cgi(int client, const char* path,
    const char* method, const char* query_string)
{
    char buf[1024];
    int cgi_output[2];
    int cgi_input[2];
    pid_t pid;
    int status;
    int i;
    char c;
    int numchars = 1;
    int content_length = -1;

    buf[0] = 'A'; buf[1] = '\0';
    if (strcasecmp(method, "GET") == 0)
        while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
            numchars = get_line(client, buf, sizeof(buf));
    else    /* POST */
    {
        numchars = get_line(client, buf, sizeof(buf));
        while ((numchars > 0) && strcmp("\n", buf))
        {
            buf[15] = '\0';
            if (strcasecmp(buf, "Content-Length:") == 0)
                content_length = atoi(&(buf[16]));
            numchars = get_line(client, buf, sizeof(buf));
        }
        if (content_length == -1) {
            bad_request(client);
            return;
        }
    }

    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);

    if (pipe(cgi_output) < 0) {
        cannot_execute(client);
        return;
    }
    if (pipe(cgi_input) < 0) {
        cannot_execute(client);
        return;
    }

    if ((pid = fork()) < 0) {
        cannot_execute(client);
        return;
    }
    if (pid == 0)  /* child: CGI script */
    {
        char meth_env[255];
        char query_env[255];
        char length_env[255];

        dup2(cgi_output[1], 1);
        dup2(cgi_input[0], 0);
        close(cgi_output[0]);
        close(cgi_input[1]);
        sprintf(meth_env, "REQUEST_METHOD=%s", method);
        putenv(meth_env);
        if (strcasecmp(method, "GET") == 0) {
            sprintf(query_env, "QUERY_STRING=%s", query_string);
            putenv(query_env);
        }
        else {   /* POST */
            sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
            putenv(length_env);
        }
        execl(path, path, NULL);
        exit(0);
    }
    else {    /* parent */
        close(cgi_output[1]);
        close(cgi_input[0]);
        if (strcasecmp(method, "POST") == 0)
            for (i = 0; i < content_length; i++) {
                recv(client, &c, 1, 0);
                write(cgi_input[1], &c, 1);
            }
        while (read(cgi_output[0], &c, 1) > 0)
            send(client, &c, 1, 0);

        close(cgi_output[0]);
        close(cgi_input[1]);
        waitpid(pid, &status, 0);
    }
}

/**********************************************************************/
/* Get a line from a socket, whether the line ends in a newline,
 * carriage return, or a CRLF combination.  Terminates the string read
 * with a null character.  If no newline indicator is found before the
 * end of the buffer, the string is terminated with a null.  If any of
 * the above three line terminators is read, the last character of the
 * string will be a linefeed and the string will be terminated with a
 * null character.
 * Parameters: the socket descriptor
 *             the buffer to save the data in
 *             the size of the buffer
 * Returns: the number of bytes stored (excluding null) */
 /**********************************************************************/
/*
从socket中的得到一行, 无论该行是否以换行符结束/回车/ctrl组合键.
终止字符串读取null字符, 如果在缓冲结束之前, 没有找到换行符, 那么字符串以null结束.
如果任何前面三个结束符被读到了, 最后的字符是换行符, 并且字符串将会以null字符结束.
参数: socket描述符
      保存数据的缓冲区
      缓冲区的大小
return: 存储的字节数(不包括 null)
*/
int get_line(int sock, char* buf, int size)
{
    int i = 0;
    char c = '\0';
    int n;

    while ((i < size - 1) && (c != '\n'))
    {
        n = recv(sock, &c, 1, 0);
        /*
        函数原型: ssize_t recv(int sockfd, void *buf, size_t len, int flags);

        从TCP连接的另一端接收数据。
        参数1: sockfd 指定接收端套接字描述符；
        参数2: buf 指明一个缓冲区，该缓冲区用来存放recv函数接收到的数据；
        参数3: len 指明buf的长度；
        参数4: flag 一般置0。
        
        如果把flags设置为MSG_PEEK，仅仅是把tcp 缓冲区中的数据读取到buf中，没有把已读取的数据从tcp 缓冲区中移除，如果再次调用recv()函数仍然可以读到刚才读到的数据。
        */
        /* DEBUG printf("%02X\n", c); */
        if (n > 0)
        {
            if (c == '\r')
            {
                n = recv(sock, &c, 1, MSG_PEEK);
                /* DEBUG printf("%02X\n", c); */
                if ((n > 0) && (c == '\n'))
                    recv(sock, &c, 1, 0);
                else
                    c = '\n';
            }
            buf[i] = c;
            i++;
        }
        else
            c = '\n';
    }
    buf[i] = '\0';

    return(i);
}

/**********************************************************************/
/* Return the informational HTTP headers about a file. */
/* Parameters: the socket to print the headers on
 *             the name of the file */
 /**********************************************************************/
void headers(int client, const char* filename)
{
    char buf[1024];
    (void)filename;  /* could use filename to determine file type */

    strcpy(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Give a client a 404 not found status message. */
/**********************************************************************/
void not_found(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "your request because the resource specified\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "is unavailable or nonexistent.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Send a regular file to the client.  Use headers, and report
 * errors to client if they occur.
 * Parameters: a pointer to a file structure produced from the socket
 *              file descriptor
 *             the name of the file to serve */
 /**********************************************************************/
void serve_file(int client, const char* filename)
{
    FILE* resource = NULL;
    int numchars = 1;
    char buf[1024];

    buf[0] = 'A'; buf[1] = '\0';
    while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
        numchars = get_line(client, buf, sizeof(buf));

    resource = fopen(filename, "r");
    if (resource == NULL)
        not_found(client);
    else
    {
        headers(client, filename);
        cat(client, resource);
    }
    fclose(resource);
}

/**********************************************************************/
/* This function starts the process of listening for web connections
 * on a specified port.  If the port is 0, then dynamically allocate a
 * port and modify the original port variable to reflect the actual
 * port.
 * Parameters: pointer to variable containing the port to connect on
 * Returns: the socket */
 /**********************************************************************/
int startup(u_short* port)
{
    int httpd = 0;
    struct sockaddr_in name;

    httpd = socket(PF_INET, SOCK_STREAM, 0);
    /*
    socket函数: 创建套接字!
    函数原型: int socket(int af, int type, int protocol);
    参数1. af为IP地址类型: AF_INET(表示IPv4), AF_INET6(表示IPv6), PF_INET(=AF_INET), PF_INET6(=AF_INET6), 等
    参数2. type: 数据传输方式/套接字类型: SOCK_STREAM（流格式套接字/面向连接的套接字), SOCK_DGRAM（数据报套接字/无连接的套接字, 等
    参数3. protocol: IPPROTO_TCP(TCP), IPPTOTO_UDP(UDP), 0(如果设为0, 意思是使用的协议可以通过参数1和2推断出来)
    return: 成功就返回新建的socket的文件描述符, 失败返回-1
    */
    if (httpd == -1)
        error_die("socket");
    memset(&name, 0, sizeof(name));
    name.sin_family = AF_INET;
    name.sin_port = htons(*port);
    name.sin_addr.s_addr = htonl(INADDR_ANY); 
    /*
    HBO和NBO的概念:
    主机字节顺序HBO（Host Byte Order）
    网络字节顺序NBO（Network Byte Order）
    先转换再用

    四个转换函数:
    htonl()--"Host to Network Long"
    htons()--"Host to Network Short"
    ntohl()--"Network to Host Long"
    ntohs()--"Network to Host Short"

    INADDR_ANY 0.0.0.0泛指本机
    作用: 如果有多个网卡, 只需要管理一个套接字, 接受某端口传输的数据就可以了
    */
    if (bind(httpd, (struct sockaddr*) & name, sizeof(name)) < 0)
        error_die("bind");
    /*
    函数原型: int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
    作用: 将指定了通信协议的套接字文件与自己的IP和端口绑定起来
    return : 成功返回0, 失败返回-1

    */
    if (*port == 0)  /* if dynamically allocating a port 如果动态分配了一个端口*/
    {
        /*
        int -> socklen_t
        */
        socklen_t namelen = sizeof(name);
        if (getsockname(httpd, (struct sockaddr*) & name, &namelen) == -1)
            error_die("getsockname");
        /*
        getsockname函数用于获取与某个套接字关联的本地协议地址
        getpeername函数用于获取与某个套接字关联的外地协议地址
        
        函数原型:
        int getsockname(int sockfd, struct sockaddr *localaddr, socklen_t *addrlen);
        int getpeername(int sockfd, struct sockaddr *peeraddr, socklen_t *addrlen);
        
        return: 成功返回1, 失败返回-1
        */
        *port = ntohs(name.sin_port);

    }
    if (listen(httpd, 5) < 0)
        error_die("listen");

    /*
    函数原型: int listen(int fd, int backlog);
    参数1: fd 一个已绑定未被连接的套接字描述符
    参数2: backlog 连接请求队列(queue of pending connections)
    作用: 一个套接字处于监听到来的连接请求的状态
    listen函数使用主动连接套接字变为被连接套接口，使得一个进程可以接受其它进程的请求，从而成为一个服务器进程。
    在TCP服务器编程中listen函数把进程变为一个服务器，并指定相应的套接字变为被动连接。
    */
    return(httpd);
}

/**********************************************************************/
/* Inform the client that the requested web method has not been
 * implemented.
 * Parameter: the client socket */
 /**********************************************************************/
void unimplemented(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</TITLE></HEAD>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/

int main(void)
{
    int server_sock = -1;               //
    u_short port = 0;                   //端口号
    int client_sock = -1;               //
    struct sockaddr_in client_name;
    /*
    struct sockaddr和struct sockaddr_in这两个结构体用来处理网络通信的地址
    sockadd_in 是 sockaddr的升级版, 更改了某些缺陷: sa_data把目标地址和端口信息混在一起了.
    sockaddr => <sys/socket.h>
    sockaddr => <netinet/in.h> / <arpa/inet.h>

    struct sockaddr_in {
        sa_family_t     sin_family;    //地址族
        uint16_t        sin_port;      //16位TCP/UDP端口号
        struct in_addr  sin_addr;      //32位IP地址
        char            sin_zero[8];   //不用
    }

    struct in_addr {
        In_addr_t       s_addr;        //32位IPv4地址
    }

    ps: sin_port和sin_addr都必须是网络字节序（NBO），一般可视化的数字都是主机字节序（HBO）


    */
    /*
    int -> socklen_t
    */
    socklen_t client_name_len = sizeof(client_name);
    /*
    typedef int socklen_t;
    原版是int, 直接用int会报warning.
    */
    pthread_t newthread;
    /*
    typedef unsigned long int pthread_t;
    蜜汁定义!
    pthread_t用于声明线程ID
    */

    server_sock = startup(&port);
    /*
        int类型
    */
    printf("httpd running on port %d\n", port);

    while (1) {
        client_sock = accept(server_sock,
            (struct sockaddr*) & client_name,
            &client_name_len);
        /*
        作用: 接收一个套接字中已建立的连接
        函数原型: int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
        参数1: sockfd 套接字描述符
        参数2: addr 通讯层所知的连接实体地址
        参数3: addrlen 指向存有addr地址长度的整型数
        return: 成功返回非负整数，该整数是接收到套接字的描述符；失败返回－1.
        */
        if (client_sock == -1) 
            error_die("accept");
        /* accept_request(client_sock); */
        if (pthread_create(&newthread, NULL, accept_request, (void*)&client_sock) != 0)
            perror("pthread_create");
        /*
        函数原型:
        int pthread_create(
                 pthread_t *restrict tidp,   //新创建的线程ID指向的内存单元。
                 const pthread_attr_t *restrict attr,  //线程属性，默认为NULL
                 void *(*start_rtn)(void *), //新创建的线程从start_rtn函数的地址开始运行
                 void *restrict arg //默认为NULL。若上述函数需要参数，将参数放入结构中并将地址作为arg传入。
                  );
        */
    }

    close(server_sock); 

    return(0);
}

/*
阅读顺序: main -> startup -> accept_request -> execute_cgi

*/
