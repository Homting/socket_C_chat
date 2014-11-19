/************************************************************************
	> File Name: chat_server.c
	> Author: LiJin
	> Mail: 594268218@qq.com homtingli@gmail.com
	> Created Time: 2014-11-19 23:12
 ************************************************************************/
#include <stdio.h>   
#include <stdlib.h>
#include <sys/types.h>  //数据类型定义
#include <sys/stat.h>
#include <netinet/in.h>  //定义数据结构sockaddr_in
#include <sys/socket.h>  //提供socket函数及数据结构
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ipc.h>	//key_t
#include <errno.h>
#include <sys/shm.h>
#include <time.h>
#include <string.h>
#include <arpa/inet.h> //inet ntoa 乱码
#define PERM S_IRUSR|S_IWUSR  //share memory
#define MYPORT 6666  //宏定义定义通信端口
#define MAXCLIENT 10 //宏定义，定义服务程序可以连接的最大客户数量

#define WELCOME "|~~~~~~~~~~~~~~chat only!~~~~~~~~~~~~~~|" 
//将int类型转换成char *类型
//输出当前系统时间为string
void itoa(int value,char * string,int radix)
{
	int power,j;
   	j=value;
   	for(power=1;j>=10;j/=10)
    	power*=10;
   	for(;power>0;power/=10)
   	{
    	*string++='0'+value/power;
    	value%=power;
   	}
   	*string='\0';
}

//得到当前系统时间
void get_cur_time(char * time_str)//time_str 记录时间
{
	time_t timep;
   	struct tm *p_curtime;
   	char *time_tmp;//记录临时时间变量
   	time_tmp=(char *)malloc(2);
   	memset(time_tmp,0,2);

   	memset(time_str,0,20);
   	time(&timep);
   	p_curtime = localtime(&timep);
   	strcat(time_str," (");
   	//get hour
   	itoa(p_curtime->tm_hour,time_tmp,10);
   	strcat(time_str,time_tmp);
   	strcat(time_str,":");
   	//get min
   	itoa(p_curtime->tm_min,time_tmp,10);
   	strcat(time_str,time_tmp);
   	strcat(time_str,":");
   	//get second
   	itoa(p_curtime->tm_sec,time_tmp,10);
   	strcat(time_str,time_tmp);
   	strcat(time_str,")");
   	free(time_tmp);
}
//创建共享存储区
//系统建立IPC通讯 （消息队列、信号量和共享内存） 
//时必须指定一个ID值。通常情况下，该id值通过ftok函数得到。
key_t shm_create()
{
   	key_t shmid;
   	//shmid = shmget(IPC_PRIVATE,1024,PERM);
   	//shmget(得到一个共享内存标识符或创建一个共享内存对象)
   	/*
	IPC_PRIVATE  isn't  a flag field but a key_t type. 
	If this special value is used for key, the system call ignores everything but the
    least significant 9 bits of shmflg and creates a new shared memory segment (on success).
   	*/
   	if((shmid = shmget(IPC_PRIVATE,1024,PERM)) == -1)
   	{
    	fprintf(stderr,"Create Share Memory Error:%s\n\a",strerror(errno));
     	exit(1);
   	}
   	return shmid;
}
//端口绑定函数,创建套接字，并绑定到指定端口
int bindPort(unsigned short int port)
{  
   	int sockfd;
   	struct sockaddr_in my_addr;
   	sockfd = socket(AF_INET,SOCK_STREAM,0);//创建基于流套接字
   	my_addr.sin_family = AF_INET;//IPv4协议族
   	my_addr.sin_port = htons(port);//端口转换
   	my_addr.sin_addr.s_addr = INADDR_ANY;
   	bzero(&(my_addr.sin_zero),0);

   	if(bind(sockfd,(struct sockaddr*)&my_addr,sizeof(struct sockaddr)) == -1)
   	{
    	perror("bind");
     	exit(1);
   	}
   	printf("bing success!\n");
   	return sockfd;
}
int main(int argc, char *argv[])
{
	int sockfd,clientfd,sin_size,recvbytes; //定义监听套接字、客户套接字
   	pid_t pid,ppid;  //定义父子线程标记变量
   	char *buf, *r_addr, *w_addr, *temp, *time_str;//定义临时存储区
   	struct sockaddr_in client_addr;  //定义地址结构
   	key_t shmid;

   	shmid = shm_create(); //创建共享存储区

   	temp = (char *)malloc(255);
   	time_str=(char *)malloc(20);
   	sockfd = bindPort(MYPORT);//绑定端口
   	while(1)
   	{     
    	if(listen(sockfd,MAXCLIENT) == -1)//在指定端口上监听
     	{
       		perror("listen");
       		exit(1);
     	}
     	printf("listening......\n");
     	if((clientfd = accept(sockfd,(struct sockaddr*)&client_addr,&sin_size)) == -1)//接收客户端连接
     	{
       		perror("accept");
       		exit(1);
     	}
     	printf("accept from:%s\n",inet_ntoa(client_addr.sin_addr));
     	//printf("accept from:%d\n",client_addr.sin_addr);
     	//将一个IP转换成一个互联网标准点分格式的字符串。
     	send(clientfd,WELCOME,strlen(WELCOME),0);//发送问候信息
     	buf = (char *)malloc(255);
    
    	ppid = fork();//创建子进程
     	if(ppid == 0)
     	{
       		//printf("ppid=0\n");
       		pid = fork();  //创建子进程   
       		while(1)
       		{
         		if(pid > 0)
         		{
             		//父进程用于接收信息
           			memset(buf,0,255);
           			//printf("recv\n");
           			//sleep(1);
           			if((recvbytes = recv(clientfd,buf,255,0)) <= 0)
           			{
             			perror("recv client leave");
                  //return Success
             			close(clientfd);
             			raise(SIGKILL);//向正在执行的程序发送一个信号
             			//kill函数发送SIGKILL杀死子进程。
             			exit(1);
           			}
           			//write buf's data to share memory
           			w_addr = shmat(shmid, 0, 0);
           			//shmat(把共享内存区对象映射到调用进程的地址空间
                	memset(w_addr, '\0', 1024);
           			strncpy(w_addr, buf, 1024);
           			get_cur_time(time_str);
           			strcat(buf,time_str);
           			printf(" %s\n",buf);
         		}
         		else if(pid == 0)
         		{
             	//进程用于发送信息
           		//scanf("%s",buf);
           			sleep(1);
           			r_addr = shmat(shmid, 0, 0);
           			//printf("---%s\n",r_addr);
          			//printf("cmp:%d\n",strcmp(temp,r_addr));
           			if(strcmp(temp,r_addr) != 0)
           			{
             			strcpy(temp,r_addr);
             			get_cur_time(time_str);            
             			strcat(r_addr,time_str);
            			//printf("discriptor:%d\n",clientfd);
             			//if(send(clientfd,buf,strlen(buf),0) == -1)
             			if(send(clientfd,r_addr,strlen(r_addr),0) == -1)
             			{
               				perror("send");
             			}
             			memset(r_addr, '\0', 1024);
             			strcpy(r_addr,temp);
           			}
         		}
         		else
           			perror("fork");
       		}
     	}
   	}
   	printf("------------------------------\n");
   	free(buf);
   	close(sockfd);
   	close(clientfd);
   	return 0;
}
