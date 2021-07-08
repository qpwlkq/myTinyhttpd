# myTinyhttpd
超轻量级http服务器Tinyhttpd的学习&amp;改造

1999年写的代码, 20年过去了, 依然热度不减, 麻雀虽小, 五脏俱全.  

### 0 error && 0 warning 运行
##### 1. bug更改
源码放在今天有几个bug:
```
1、声明函数修改如下
//void accept_request(int);
void *accept_request(void *);
2、定义函数修改如下：
//void accept_request(int client)
void *accept_request(void *client1)
并将函数内return;均改为return NULL;
3、startup函数中
//int namelen = sizeof(name);
socklen_t namelen = sizeof(name);
4、main函数中
//int client_name_len = sizeof(client_name);
socklen_t client_name_len = sizeof(client_name);
5、main函数中
//if (pthread_create(&newthread , NULL, accept_request, client_sock) != 0)
if (pthread_create(&newthread , NULL, accept_request, (void *)&client_sock) != 0)
```
##### 2. pthread库安装
```
sudo apt-get install glibc-doc
sudo apt-get install manpages-posix-dev
find / -name libpthread.so
```
在codeblocks上使用:
编译 -> Compiler -> Linker setting -> Add
然后将find找到的位置复制粘贴一下, 确认即可.

### 现有功能说明
1. 仅支持GET和POST
2. 仅支持Http1.0和Http1.x

### 改进目标
0. 理清逻辑
1. 修改逻辑bug
2. 支持更多请求方法
3. 增加Http 更多版本的支持