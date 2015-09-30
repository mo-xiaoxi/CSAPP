/*
 * tsh - A tiny shell program with job control
 *
 * < 201308060111 momomoxiaoxi >
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

/* Misc manifest constants */
#define MAXLINE	1024   /* max line size */
#define MAXARGS	 128   /* max args on a command line */
#define MAXJOBS	  16   /* max jobs at any point in time 同一时刻最大工作量*/
#define MAXJID	1<<16   /* max job ID */

/* Job states */
#define UNDEF 0 /* undefined 未定义的*/
#define FG 1	/* running in foreground 在前台运行*/
#define BG 2	/* running in background 在后台运行*/
#define ST 3	/* stopped 停止*/

/*
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *	 FG -> ST  : ctrl-z
 *	 ST -> FG  : fg command
 *	 ST -> BG  : bg command
 *	 BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */

/* addional global variables */
int pause_flag = 0;

/* Global variables */
extern char **environ;	  /* defined in libc */
char prompt[] = "tsh> ";	/* command line prompt (DO NOT CHANGE) promt代表命令行*/
int verbose = 0;	        /* if true, print additional output */
int nextjid = 1;	        /* next job ID to allocate */
char sbuf[MAXLINE];	     /* for composing sprintf messages */

struct job_t {	          /* The job struct */
	pid_t pid;              /* job PID 这里理解成进程*/
	int jid;                /* job ID [1, 2, ...] ，这里我后来理解成线程JIDs should be denoted on the command line by the prefix ’%’.*/
	int state;              /* UNDEF, BG, FG, or ST */
	char cmdline[MAXLINE];  /* command line */
};
struct job_t jobs[MAXJOBS]; /* The job list */
/* End global variables */
/*这里讲一下线程和进程的区别，进程可以由多个线程组成。线程是指进程内的一个执行单元,也是进程内的可调度实体.
进程有独立的地址空间，一个进程崩溃后，在保护模式下不会对其它进程产生影响，而线程只是一个进程中的不同执行路
径。线程有自己的堆栈和局部变量，但线程之间没有单独的地址空间，一个线程死掉就等于整个进程死掉，所以多进程
的程序要比多线程的程序健壮，但在进程切换时，耗费资源较大，效率要差一些。但对于一些要求同时进行并且又要共
享某些变量的并发操作，只能用线程，不能用进程。
*/

/* Function prototypes 函数原型*/



/* Here are the functions that you will implement 下面几个函数是要我们自己写得*/
void eval(char *cmdline);//参照书上写的并修改了下-对命令行求值，首要任务是调用parseline函数，
//这个函数解析了以空格分割的命令行参数，并构造最终会传递给execve的argv参数
int builtin_cmd(char **argv);//参照书修改--这个函数主要检查第一个命令是不是内置的外壳命令。
//这里所谓的内置外壳命令是quit，jobs，bg，fg。如果第一个不是内置命令，则返回0
void waitfg(pid_t pid);//等待处理PID
void sigchld_handler(int sig);//处理SIGCHLD信号--这个信号处理程序回收子进程--如果子进程是僵尸进程
//或者收到stop之类的信号时，调用
void sigtstp_handler(int sig);//处理SIGTSTP信号---停止直到下一个SIGCONT---键盘输入CTRL+z会触发这个信号
void sigint_handler(int sig);//处理SIGINT信号--在键盘上输入CTRL+C会导致一个SIGINT信号被发送到外壳。
//外壳捕获该信号，然后发送SIGINT信号到这个前台进程组中的每个进程



/* addional helper routines */
pid_t Fork(void);//fork,you know it!具体实现的时候调用了系统fork（）函数，并稍微做了点错误处理
int Setpgid(pid_t pid, pid_t pgid);
/*调用系统函数，setpgid。该函数作用是设置进程组识别码，将参数pid指定进程所属的组识别码设为
参数pgid指定进程所属的组识别码。如果参数pid 为0，则会用来设置目前进程的组识别码，如果参数
pgid为0，则由pid指定的进程ID将用作进程组ID。执行成功返回组识别码，否则返回-1*/

/* Here are helper routines that we've provided for you */
int parseline(const char *cmdline, char **argv);
/*解析命令，并构建argv数组，括号中的单引号字符被视为一个单独的参数。如果用户请求后台工作返回true，
请求前台工作，返回false。即最后一个参数是“&”，表示后台运行，否则前台
 */
void sigquit_handler(int sig);
//驱动程序可以优雅地终止,通过发送一个SIGQUIT信号子shell。

void clearjob(struct job_t *job);//you know it!
void initjobs(struct job_t *jobs);//init,you know it!
int maxjid(struct job_t *jobs);//返回最大分配的job id 
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline);//add job！
int deletejob(struct job_t *jobs, pid_t pid);//....
pid_t fgpid(struct job_t *jobs);//返回现在在前台工作的任务的pid，如果没有返回0
struct job_t *getjobpid(struct job_t *jobs, pid_t pid);//在工作组里，寻找指定pid的进程。如果没有返回NULL
struct job_t *getjobjid(struct job_t *jobs, int jid);//和上面比较相似
int pid2jid(pid_t pid);//得到线程的进程编号
void listjobs(struct job_t *jobs);//字面意思

void usage(void);//使用说明
void unix_error(char *msg);//unix方面的错误处理，这里偏系统层
void app_error(char *msg);//应用方面的错误处理，这里偏应用层
typedef void handler_t(int);// 定义了一个类型———handler_t，表示返回值为void型
//（参数为int型）的函数（的）指针。 它可以用来声明一个或多个函数指针。
handler_t *Signal(int signum, handler_t *handler);//信号包装处理

/*
 * main - The shell's main routine。
 */
int main(int argc, char **argv)
{
	char c;
	char cmdline[MAXLINE];
	int emit_prompt = 1; /* emit prompt (default) */

	/* Redirect stderr to stdout (so that driver will get all output
	 * on the pipe connected to stdout) 重定向*/
	dup2(1, 2);//复制文件句柄 1->2
 
	/* Parse the command line */
	while ((c = getopt(argc, argv, "hvp")) != EOF) {//函数说明 getopt()用来分析命令行参数。参数argc和argv分别代表参数个数和内容,hvp说明是处理这三个
	    switch (c) {//不知道怎么调试到这一段程序
	    case 'h':             /* print help message */
	        usage();
		break;
	    case 'v':             /* emit additional diagnostic info */
	        verbose = 1;
		break;
	    case 'p':             /* don't print a prompt */
	        emit_prompt = 0;  /* handy for automatic testing */
		break;
	default:
	        usage();
	}
	}

	/* Install the signal handlers */

	/* These are the ones you will need to implement 处理信号*/
	Signal(SIGINT,  sigint_handler);   /* ctrl-c */
	Signal(SIGTSTP, sigtstp_handler);  /* ctrl-z */
	Signal(SIGCHLD, sigchld_handler);  /* Terminated or stopped child */

	/* This one provides a clean way to kill the shell */
	Signal(SIGQUIT, sigquit_handler);

	/* Initialize the job list 初始化工作列表*/
	initjobs(jobs);

	/* Execute the shell's read/eval loop */
	while (1) {

	/* Read command line */
	if (emit_prompt) {
		printf("%s", prompt);
		fflush(stdout);//清除读写缓冲区，如果成功刷新,fflush返回0。指定的流没有缓冲区或者只读打开时也返回0值。返回EOF指出一个错
	}
	if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
		app_error("fgets error");
	if (feof(stdin)) { /* End of file (ctrl-d) */
		fflush(stdout);
		exit(0);
	}

	/* Evaluate the command line */
	eval(cmdline);
	fflush(stdout);
	fflush(stdout);
	}

	exit(0); /* control never reaches here 一般不会到达这里*/
}

/*
 * eval - Evaluate the command line that the user has just typed in
 *
 * If the user has requested a built-in command (quit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.
*/
void eval(char *cmdline)
{
	char *argv[MAXARGS]; 	/* Argument list execve() */
	char buf[MAXLINE]; 	/* Holds modified command line */
	int bg; 		/* Should the job run in bg or fg? */
	pid_t pid; 		/* Process id */
	int jid;
	sigset_t mask;
	strcpy(buf, cmdline);
	bg = parseline(buf, argv);
	if (NULL == argv[0]) {
		return; 	/* Ignore empty lines */
	}

	if (!builtin_cmd(argv)) {
		sigemptyset(&mask);//sigemptyset()用来将参数set(mask)信号集初始化并清空。
		sigaddset(&mask, SIGCHLD);//sigaddset()用来将参数signum(sigchld) 代表的信号加入至参数set(mask) 信号集里。
		sigprocmask(SIG_BLOCK, &mask, NULL); /* Block SIGCHLD 用于改变进程的当前阻塞信号集,也可以用来检测当前进程的信号掩码。*/

		if ((pid = Fork()) == 0) { 	/* child runs user job */
			Setpgid(0, 0); 		/* set child group pid */
			sigprocmask(SIG_UNBLOCK, &mask, NULL); /* Unblock SIGCHLD */
			/*if (execve(argv[0], argv, environ) < 0) {
				printf("%s: Command not found.\n", argv[0]);
				exit(0);
			}原文档代码，测试得时候无法调用系统linux命令，下面是修改后代码*/
			
			if (execvp(argv[0], argv) >= 0) {
					printf("This is a Linux command.\n");
			}
			else
			{
				//for ( i = 0; environ[i] != NULL; i++) 
			      //  printf("[%d] : %s\n", i, environ[i]); 
				printf("%s: Command not found.\n", argv[0]);
				exit(0);
			}
			
		}

		/* Parent waits for foreground job to terminate */
		if (!bg) {
			int status;
			addjob(jobs, pid, FG, cmdline);
			sigprocmask(SIG_UNBLOCK, &mask, NULL); /* Unblock SIGCHLD */
			waitfg(pid);
		} else { 	/* child runs in backround */
			addjob(jobs, pid, BG, cmdline);
			jid = pid2jid(pid);
			printf("[%d] (%d) %s", jid, pid ,cmdline);
			sigprocmask(SIG_UNBLOCK, &mask, NULL); /* Unblock SIGCHLD */
		}
	}

 	return;
}

/*
 * parseline - Parse the command line and build the argv array.
 *
 * Characters enclosed in single quotes are treated as a single
 * argument.  Return true if the user has requested a BG job, false if
 * the user has requested a FG job.
 */
int parseline(const char *cmdline, char **argv)
{
	static char array[MAXLINE]; /* holds local copy of command line */
	char *buf = array;          /* ptr that traverses command line */
	char *delim;                /* points to first space delimiter定界符、分界符 */
	int argc;                   /* number of args */
	int bg;                     /* background job? */

	strcpy(buf, cmdline);
	buf[strlen(buf)-1] = ' ';  /* replace trailing '\n' with space */
	while (*buf && (*buf == ' '))
	/* ignore leading spaces 忽略前导空格，
	这里的与说明buf指向的地方确实存在东西，且为‘ ’。但是为什么不直接*buf==' ',还不是很明白*/
	buf++;

	/* Build the argv list */
	argc = 0;
	if (*buf == '\'') {
	buf++;
	delim = strchr(buf, '\'');//查找字符串buf中首次出现字符'的位置返回首次出现c的位置的指针，
	//返回的地址是被查找字符串指针开始的第一个与'相同字符的指针，如果BUF中不存在'则返回NULL。
	}
	else {
	delim = strchr(buf, ' ');
	}

	while (delim) {
	argv[argc++] = buf;
	*delim = '\0';
	buf = delim + 1;
	while (*buf && (*buf == ' ')) /* ignore spaces */
		   buf++;

	if (*buf == '\'') {
		buf++;
		delim = strchr(buf, '\'');
	}
	else {
		delim = strchr(buf, ' ');
	}
	}
	argv[argc] = NULL;

	if (argc == 0)  /* ignore blank line */
	return 1;

	/* should the job run in the background? */
	if ((bg = (*argv[argc-1] == '&')) != 0) {//这里判断是否在后台运行
	argv[--argc] = NULL;
	}
	return bg;
}

/*
 * builtin_cmd - If the user has typed a built-in command then execute
 *	it immediately.
 */
int builtin_cmd(char **argv)
{
	char *quit = "quit";
	char *jobs_str = "jobs";
	char *bg_str = "bg";
	char *fg_str = "fg";
	if (0 == strcmp(argv[0], quit)) {
		exit(0);
	} else if (0 == strcmp(argv[0], jobs_str)) {
		listjobs(jobs);
		return 1;
	} else if (0 == strcmp(argv[0], bg_str) || 0 == strcmp(argv[0], fg_str)) {
		do_bgfg(argv);
		return 1;
	}
   	 return 0;	 /* not a builtin command */
}

/*
 * do_bgfg - Execute the builtin bg and fg commands
 */
void do_bgfg(char **argv)
{
	char *bg_str = "bg";
	int jid;
	pid_t pid;
	struct job_t *job;

	/* error handing */
	if (NULL == argv[1]) {
		printf("%s command requires PID or %%jobid argument\n", argv[0]);
		return;
	}
	if ('%' != argv[1][0] && (argv[1][0] > '9' || argv[1][0] < '0')) {
		printf("%s: argument must be a pid or %%jobid\n", argv[0]);
		return;
	}

	if ('%' == argv[1][0]) {
		jid = atoi(argv[1]+1);
		job = getjobjid(jobs, jid);
		if (NULL == job) {
			printf("%s: No such job\n", argv[1]);
			return;
		}
		pid = jobs[jid-1].pid;
	} else {
		char *pid_str;
		int i = 0;
		pid_str = (char *) malloc (strlen(argv[1]) * sizeof(char));
		pid_str[0] = '\0';
		while ('\0' != argv[1][i] && (argv[1][i] >= '0' && argv[1][i] <= '9')) {
			pid_str[i] = argv[1][i];
			++i;
		}
		pid_str[i] = '\0';
		pid = atoi(pid_str);
		jid = pid2jid(pid);
		job = getjobjid(jobs, jid);
		if (NULL == job) {
			printf("(%s): No such process\n", pid_str);
			return;
		}
	}

	/* bg */
	if (0 == strcmp(argv[0], bg_str)) {
		if (kill(-pid, SIGCONT) < 0) {
			fprintf(stdout, "kill error");
			return;
		}
		jobs[jid-1].state = BG;
		printf("[%d] (%d) %s", jid, pid, jobs[jid-1].cmdline);
	} else { 	/* fg */
		if (kill(-pid, SIGCONT) < 0) {
			fprintf(stdout, "kill error");
			return;
		}
		jobs[jid-1].state = FG;
		waitfg(pid);
	}
	return;
}

/*
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t pid)
{
	/*sleep(10);*/
	pause_flag = 0;
	pause();//make jincheng sleep
	if (!pause_flag) {
		pause();
	}
	/*sleep(1);*/
	return;
}

/*****************
 * Signal handlers
 *****************/

/*
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *	 a child job terminates (becomes a zombie), or stops because it
 *	 received a SIGSTOP or SIGTSTP signal. The handler reaps all
 *	 available zombie children, but doesn't wait for any other
 *	 currently running children to terminate.
 */
void sigchld_handler(int sig)//处理子进程
{
	pid_t pid;
	int jid;
	pause_flag = 0;
	while ((pid = waitpid(-1, NULL, WNOHANG)) > 0) {
		jid = pid2jid(pid);
		if (FG == jobs[jid-1].state) {
			pause_flag = 1;
		}
		deletejob(jobs, pid);
	}
	return;
}

/*
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *	user types ctrl-c at the keyboard.  Catch it and send it along
 *	to the foreground job.
 */
void sigint_handler(int sig)//处理int信号
{
	pid_t pid;
	int jid;
	pid = fgpid(jobs);
	jid = pid2jid(pid);
	pause_flag = 1;
	if (kill(-pid, SIGINT) < 0) {
		fprintf(stdout, "kill error");
		return;
	}
	else
	printf("JID [%d] (%d) terminated by signal %d\n", jid, pid, SIGINT);

   	return;
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *	 the user types ctrl-z at the keyboard. Catch it and suspend the
 *	 foreground job by sending it a SIGTSTP.
 */
void sigtstp_handler(int sig)
{
	pid_t pid;
	int jid;
	pid = fgpid(jobs);
	if (kill(-pid, SIGTSTP) < 0) {
		fprintf(stdout, "kill error");
		return;
	}
	jid = pid2jid(pid);
	jobs[jid-1].state = ST;
	printf("JID [%d] (%d) stopped by signal %d\n", jid, pid, SIGTSTP);
	pause_flag = 1;

	return;
}

/*********************
 * End signal handlers
 *********************/

/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/

/* clearjob - Clear the entries in a job struct */
void clearjob(struct job_t *job) {//清空工作列表
	job->pid = 0;
	job->jid = 0;
	job->state = UNDEF;
	job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void initjobs(struct job_t *jobs) {//初始化
	int i;

	for (i = 0; i < MAXJOBS; i++)
	clearjob(&jobs[i]);
}

/* maxjid - Returns largest allocated job ID */
int maxjid(struct job_t *jobs)//返回最大的已分配的工作ID
{
	int i, max=0;

	for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].jid > max)
		max = jobs[i].jid;
	return max;
}

/* addjob - Add a job to the job list */
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline)
//添加一个任务（工作）
{
	int i;

	if (pid < 1)//错误处理，一般PID都大于0
	return 0;

	for (i = 0; i < MAXJOBS; i++) {//找到一个空的jobs组
	if (jobs[i].pid == 0) {
		jobs[i].pid = pid;
		jobs[i].state = state;
		jobs[i].jid = nextjid++;
		if (nextjid > MAXJOBS)
		nextjid = 1;
		strcpy(jobs[i].cmdline, cmdline);
  		if(verbose){
		    printf("Added job [%d] %d %s\n", jobs[i].jid, jobs[i].pid, jobs[i].cmdline);
	        }
	        return 1;
	}
	}
	printf("Tried to create too many jobs\n");
	return 0;
}

/* deletejob - Delete a job whose PID=pid from the job list */
int deletejob(struct job_t *jobs, pid_t pid)
{
	int i;

	if (pid < 1)
	return 0;

	for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid == pid) {
		clearjob(&jobs[i]);
		nextjid = maxjid(jobs)+1;
		return 1;
	}
	}
	return 0;
}

/* fgpid - Return PID of current foreground job, 0 if no such job */
pid_t fgpid(struct job_t *jobs) {
	int i;

	for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].state == FG)
		return jobs[i].pid;
	return 0;
}

/* getjobpid  - Find a job (by PID) on the job list */
struct job_t *getjobpid(struct job_t *jobs, pid_t pid) {
	int i;

	if (pid < 1)
	return NULL;
	for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].pid == pid)
		return &jobs[i];
	return NULL;
}

/* getjobjid  - Find a job (by JID) on the job list */
struct job_t *getjobjid(struct job_t *jobs, int jid)
{
	int i;

	if (jid < 1)
	return NULL;
	for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].jid == jid)
		return &jobs[i];
	return NULL;
}

/* pid2jid - Map process ID to job ID */
int pid2jid(pid_t pid)
{
	int i;

	if (pid < 1)
	return 0;
	for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].pid == pid) {
	        return jobs[i].jid;
	    }
	return 0;
}

/* listjobs - Print the job list */
void listjobs(struct job_t *jobs)
{
	int i;

	for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid != 0) {
		printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid);
		switch (jobs[i].state) {
		case BG:
			printf("Running ");
			break;
		case FG:
			printf("Foreground ");
			break;
		case ST:
			printf("Stopped ");
			break;
		default:
			printf("listjobs: Internal error: job[%d].state=%d ",
			   i, jobs[i].state);
		}
		printf("%s", jobs[i].cmdline);
	}
	}
}
/******************************
 * end job list helper routines
 ******************************/


/***********************
 * Other helper routines
 ***********************/

/*
 * usage - print a help message
 */
void usage(void)
{
	printf("Usage: shell [-hvp]\n");
	printf("   -h   print this message\n");
	printf("   -v   print additional diagnostic information\n");
	printf("   -p   do not emit a command prompt\n");
	exit(1);
}

/*
 * unix_error - unix-style error routine
 */
void unix_error(char *msg)
{
	fprintf(stdout, "%s: %s\n", msg, strerror(errno));
	exit(1);
}

/*
 * app_error - application-style error routine
 */
void app_error(char *msg)
{
	fprintf(stdout, "%s\n", msg);
	exit(1);
}

/*
 * Signal - wrapper for the sigaction function
 */
handler_t *Signal(int signum, handler_t *handler)
{
	struct sigaction action, old_action;

	action.sa_handler = handler;
	sigemptyset(&action.sa_mask); /* block sigs of type being handled */
	action.sa_flags = SA_RESTART; /* restart syscalls if possible */

	if (sigaction(signum, &action, &old_action) < 0)
	unix_error("Signal error");
	return (old_action.sa_handler);
}

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *	child shell by sending it a SIGQUIT signal.
 */
void sigquit_handler(int sig)
{
	printf("Terminating after receipt of SIGQUIT signal\n");
	exit(1);
}

/* fork error handing wrapper */
pid_t Fork(void) {
	pid_t pid;
	if ((pid = fork()) < 0) {
		unix_error("Fork error");
	}
	return pid;
}

/* setpgid error handing wrapper */
int Setpgid(pid_t pid, pid_t pgid)
{
	if (setpgid(pid, pgid) < 0) {
		fprintf(stdout, "setpgid error");
		exit(0);
	}
	return 0;
}
