---
layout:     post
title:      "Android反调试技术整理与实践(未完成)"
subtitle:   ""
date:       2017-06-25 12:00:00
author:     "GToad"
header-img: "img/android-three.jpg"
catalog: true
tags:
    - Android
    - 反调试
    - LTS
---

> 本文长期维护与更新。作为一个安卓反调试的笔记吧。  

> 本文为作者本人原创，转载请注明出处：[GToad Blog](https://gtoad.github.io/2018/07/05/Android-Anti-Debug/)

## 前言

安卓APP的反调试技术网上有许多，是保护APP运行逻辑被破解的重要技术方向之一。个人对这项技术的推崇在于：它不仅仅能对动态分析造成麻烦，其对于一些基于调试的hook工具（如frida工具）也能有妨碍作用。

## 基于时间的检测

通过在代码不同的地方获取时间值，从而可以求出这个过程中的执行时间。如果两个时间值相差过大，则说明中间的代码流程被调试了。因为调试者停下来一步步观察了这一段代码的执行情况，因此这部分代码的执行时间远远超出了普通状态的执行时间。

这个方法一个不好的特点是需要一定的代码跨度，因此可能需要暴露部分代码逻辑。因为如果两个时间的取值点非常非常近，那很可能调试者在两者之间没有断点从而迅速跳过。

```c
extern "C"
JNIEXPORT jstring

JNICALL
Java_com_sec_gtoad_antidebug_MainActivity_stringFromTime(
        JNIEnv *env,
        jobject /* this */) {
    long start,end;
    std::string hello = "Hello from time";
    start = clock();
    ...
    （部分代码逻辑）
    ...
    end = clock();
    if(end-start>10000){
        hello = "Debug from time";
    }
    return env->NewStringUTF(hello.c_str());
}
```

## 基于文件的检测

通过查看系统中的指定文件，也可以得到调试的信息。主要有如下6个文件，把它们两两一组。相同组内的两个文件内容是完全一样的。

1. /proc/pid/status 和 /proc/pid/task/pid/status：普通状态下，TracerPid这项应该为0；调试状态下为调试进程的PID。
![](/img/in-post/post-android-anti-debug/file1.png)
2. /proc/pid/stat 和 /proc/pid/task/pid/stat：调试状态下，括号后面的第一个字母应该为t。
![](/img/in-post/post-android-anti-debug/file2.png)
3. /proc/pid/wchan 和 /proc/pid/task/pid/wchan：调试状态下，里面内容为ptrace_stop。
![](/img/in-post/post-android-anti-debug/file3.png)

```c
extern "C"
JNIEXPORT jstring

JNICALL
Java_com_sec_gtoad_antidebug_MainActivity_stringFromFile(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello;
    std::stringstream stream;
    int pid = getpid();
    int fd;
    stream << pid;
    stream >> hello;
    hello = "/proc/" + hello + "/status";
    //LOGI(hello);
    char* pathname = new char[30];
    strcpy(pathname,hello.c_str());
    char* buf = new char[500];
    int flag = O_RDONLY;
    fd = open(pathname, flag);
    read(fd, buf, 500);
    char* c;
    char* tra = "TracerPid";
    c = strstr(buf, tra);
    char* d;
    d = strstr(c,"\n");
    int length = d-c;
    strncpy(buf,c+11,length-11);
    buf[length-11]='\0';
    hello = buf;
    if (strcmp(buf,"0")){
        hello = "Debug from file";
    }
    else{
        hello = "Hello from file";
    }
    close(fd);

    return env->NewStringUTF(hello.c_str());
}
```


## 基于套路的检测

这部分方法是基于检测常见调试工具是否存在，从而进行反调试的。一般来说Android应用调试的时候，系统里可能会运行android_server、gdb、gdbserver等进程。在Android 6.0前，可以直接获得系统进程表，从而查看是否有这几个关键字的进程名称。

也可以通过监视常用的调试端口——23946来判断是否处于调试环境下。信息来源可以是/proc/net/tcp。

本方法套路多又深，没有代表性的代码。

## Dalvik虚拟机内部相关字段

Android Java虚拟机结构中也保存了是否被调试的相关数据。Android 5.0以前的系统版本中可以通过调用java.lang.Object->dalvik.system.VMDebug->Dalvik_dalvik_system_VMDebug_isDebuggerConnected()来获得结果。之后的版本中改成了android.os.Debug类下的isDebuggerConnected()方法。
由于是Java层的系统调用，所以相比于Native层，本方法会更容易被发现，且被Hook篡改返回值也会更简单。

```java
if(android.os.Debug.isDebuggerConnected()){
                    Toast.makeText(context, "Debug from vm",Toast.LENGTH_LONG).show();
                }
                else{
                    Toast.makeText(context, "Hello from vm",Toast.LENGTH_LONG).show();
                }
```

## ptrace

由于Linux下每个进程同一时刻最多只能被一个进程调试，因此APP可以通过自己ptrace自己的方式来抢先占坑。

```
extern "C"
JNIEXPORT jstring

JNICALL
Java_com_sec_gtoad_antidebug_MainActivity_stringFromPtrace(
        JNIEnv *env,
        jobject /* this */) {
    int check = ptrace(PTRACE_TRACEME,0 ,0 ,0);
    LOGI("ret of ptrace : %d",check);
    ...
    部分逻辑代码
    ...
    std::string hello = "Hello from ptrace";
    if(check != 0){
        hello = "Debug from ptrace";
    }
    return env->NewStringUTF(hello.c_str());
}
```

## 断点扫描

IDA等调试器在调试时候的原理是向断点地址插入breakpoint汇编指令，而把原来的指令暂时备份到别处。因此，本方法通过扫描自身so的代码部分中是否存在breakpoint指令即可。

一般来说Android App有arm模式和thumb模式，因此需要都检查一下(一下数据为小数端表示)：

1. Arm：0x01，0x00，0x9f，0xef
2. Thumb16：0x01，0xde
3. Thumb32：0xf0，0xf7，0x00，0xa0

关键的寻找breakpoint代码如下：

```c
bool checkBreakPoint ()
{
    __android_log_print(ANDROID_LOG_INFO,"JNI","13838438");
    int i, j;
    unsigned int base, offset, pheader;
    Elf32_Ehdr *elfhdr;
    Elf32_Phdr *ph_t;

    base = getLibAddr ("libnative-lib.so");

    if (base == 0) {
        LOGI ("getLibAddr failed");
        return false;
    }
    __android_log_print(ANDROID_LOG_INFO,"JNI","13838439");

    elfhdr = (Elf32_Ehdr *) base;
    pheader = base + elfhdr->e_phoff;

    for (i = 0; i < elfhdr->e_phnum; i++) {
        ph_t = (Elf32_Phdr*)(pheader + i * sizeof(Elf32_Phdr)); // traverse program header

        if ( !(ph_t->p_flags & 1) ) continue;
        offset = base + ph_t->p_vaddr;
        offset += sizeof(Elf32_Ehdr) + sizeof(Elf32_Phdr) * elfhdr->e_phnum;

        char *p = (char*)offset;
        for (j = 0; j < ph_t->p_memsz; j++) {
            if(*p == 0x01 && *(p+1) == 0xde) {
                LOGI ("Find thumb bpt %p", p);
                return true;
            } else if (*p == 0xf0 && *(p+1) == 0xf7 && *(p+2) == 0x00 && *(p+3) == 0xa0) {
                LOGI ("Find thumb2 bpt %p", p);
                return true;
            } else if (*p == 0x01 && *(p+1) == 0x00 && *(p+2) == 0x9f && *(p+3) == 0xef) {
                LOGI ("Find arm bpt %p", p);
                return true;
            }
            p++;
        }
    }
    return false;
}
```

## 信号处理

上面提到了调试器会在断点处使用breakpoint命令，而这条指令也使被调试进程发出信号SIGTRAP。通常调试器会截获Linux系统内给被调试进程的各种信号，由调试者可选地传递给被调试进程。但是SIGTRAP是个例外，因为通常的目标程序中不会出现breakpoint，因为这会使得程序自己奔溃。因此，当调试器遇到SIGTRAP信号时会认为是自己下的断点发出的。这样一来当调试器给这个breakpoint命令插入断点breakpoint后，备份的命令也是breakpoint，这样当继续执行时，调试器将备份指令恢复并执行，结果误以为备份后这个位置发出的SIGTRAP又是自己下的断点造成的，这样一来就会使得调试器的处理逻辑出现错误，不同的调试器会导致各种不同的问题。在IDA pro 6.8下，会在下面两张图中来回切换，但是调试器却一步都进行不下去：

![](/img/in-post/post-android-anti-debug/bkpt1.png)

![](/img/in-post/post-android-anti-debug/bkpt2.png)

同样的情况还有Android Studio 3.0的调试功能也被卡死：

![](/img/in-post/post-android-anti-debug/bkpt3.png)

关键代码如下：
```c
// 跳转到bkpt陷阱处：

    __asm__("push {r5}\n\t"
            "push {r0-r4,lr}\n\t"
            "mov r0,pc\n\t"  
            "add r0,r0,#6\n\t"
            "mov lr,r0\n\t"
            "mov pc,%0\n\t"
            "pop {r0-r5}\n\t"
            "mov lr,r5\n\t" 
            "pop {r5}\n\t"
    :
    :"r"(g_addr)
    :);

// bkpt陷阱处：

    push {r0-r4}
    breakpoint——thumb16{0x01,0xde} thumb32{0xf0,0xf7,0x00,0xa0} arm32{0x01,0x00,0x9f,0xef}
    pop {r0-r4}
    mov pc, lr

// signal handler处：

void my_sigtrap(int sig){
    LOGI("my_sigtrap\n");

    char change_bkp[] = {0x00,0x46}; //mov r0,r0
    memcpy(g_addr+2,change_bkp,2);
    __builtin___clear_cache(g_addr,(g_addr+8)); // need to clear cache
    LOGI("chang bpk to nop\n");

}
```

上面这个例子可以看出，原本这个APP遇到SIGTRAP信号时，会使用signal handler抢先处理，把bkpt的地方覆盖成NOP，这样便能正常运行。而调试器却卡死了。那么是不是把手动把这个bkpt修改成nop就可以了呢？仅仅是这个简单的例子可以，但是这个signal handler函数中完全可以做更多的操作，如修改别的部分的代码，进行别的逻辑处理等。此时NOP了bkpt也会有许多别的错误发生。由于调试signal handler的难度较高，故在这里写入复杂逻辑很难被动态分析出。

## 调试器的错误理解

之前提到过，Android App尽管可能支持各种平台如MIPS,X86架构。但是一般在现在的主流手机上依然是运行在Arm架构的CPU上的。而Arm架构的CPU却不仅仅只是运行Arm指令集，还会运行Thumb指令集，并且目前Android Studio已经将Thumb-2模式设定为默认NDK编译指令集，比Arm指令集还要优先。这是为什么？因为Thumb-2模式是Thumb16和Thumb32指令的混合执行，有更高的效率和代码密度，对于APP的运行效率和空间占用都有着更好的表现。

但是这对于调试器来说并不是好事。Thumb16和Thumb32在opcode上没有冲突，只要一条条按照顺序去反汇编，就可以得到正确的Thumb指令。但是Arm指令集和Thumb指令集是会有冲突的，一条Thumb32指令是可以被理解为作用意义完全不同的另一条Arm指令的，甚至2条Thumb16指令可以被调试器误解为一条合法的Arm指令。

而这两个模式的切换涉及跳转时候的地址数值，这个值可能是动态产生的，因此对编译器来说难以判断跳转后的代码是该理解为Thumb还是Arm，下图就是上面信号处理反调试方法中，IDA pro 6.8将Thumb指令误以为Arm指令，从而导致调试出错：

![](/img/in-post/post-android-anti-debug/code1632confusion.png)

![](/img/in-post/post-android-anti-debug/code1632confusion.png)

## 多进程/线程

本方法需要结合上述的一些方法才行，本身并不是一个具体的反调试技术，而是一种编程策略。其思想在于启动一个守护进程/线程来检测主逻辑进程/线程是否被调试，如果被调试就杀死主逻辑进程/线程；或者两个线程/进程互相检测是否被调试或存活等。

个人认为并不是上述所有方法都能对多线程/进程有效，子进程/线程对主进程/线程的保护仅如下方法有效：

1. 基于文件的检测
2. 断点扫描
3. 基于套路的检测
4. 基于时间的检测（极少）
5. Dalvik虚拟机内部相关字段

其它方法可能依然更适合主线程/进程自己实现。本方法只是一个策略，实现可以千奇百怪，无典型代码。

这里使用乌云以前某文章的样例代码和说明图：

![](/img/in-post/post-android-anti-debug/wooyun-anti-debug-fork.png)

```c
int pipefd[2];
int childpid;

void *anti3_thread(void *){

    int statue=-1,alive=1,count=0;

    close(pipefd[1]);

    while(read(pipefd[0],&statue,4)>0)
        break;
    sleep(1);

    //这里改为非阻塞
    fcntl(pipefd[0], F_SETFL, O_NONBLOCK); //enable fd的O_NONBLOCK

    LOGI("pip-->read = %d", statue);

    while(true) {

        LOGI("pip--> statue = %d", statue);
        read(pipefd[0], &statue, 4);
        sleep(1);

        LOGI("pip--> statue2 = %d", statue);
        if (statue != 0) {
            kill(childpid,SIGKILL);
            kill(getpid(), SIGKILL);
            return NULL;
        }
        statue = -1;
    }
}

void anti3(){
    int pid,p;
    FILE *fd;
    char filename[MAX];
    char line[MAX];

    pid = getpid();
    sprintf(filename,"/proc/%d/status",pid);// 读取proc/pid/status中的TracerPid
    p = fork();
    if(p==0) //child
    {
        LOGI("Child");
        close(pipefd[0]); //关闭子进程的读管道
        int pt,alive=0;
        pt = ptrace(PTRACE_TRACEME, 0, 0, 0); //子进程反调试
        while(true)
        {
            fd = fopen(filename,"r");
            while(fgets(line,MAX,fd))
            {
                if(strstr(line,"TracerPid") != NULL)
                {
                    LOGI("line %s",line);
                    int statue = atoi(&line[10]);
                    LOGI("########## tracer pid:%d", statue);
                    write(pipefd[1],&statue,4);//子进程向父进程写 statue值

                    fclose(fd);

                    if(statue != 0)
                    {
                        LOGI("########## tracer pid:%d", statue);
                        return ;
                    }

                    break;
                }
            }
            sleep(1);

        }
    }else{
        LOGI("Father");
        childpid = p;
    }
}

extern "C"
JNIEXPORT jstring

JNICALL
Java_com_sec_gtoad_antidebug_MainActivity_stringFromFork(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "Hello from fork";
    pthread_t id_0;
    id_0 = pthread_self();
    pipe(pipefd);
    pthread_create(&id_0,NULL,anti3_thread,(void*)NULL);
    LOGI("Start");
    anti3();
    return env->NewStringUTF(hello.c_str());
}
```

## 样例APK

为方便读者学习和理解，这边本人提供一个没有开android:debugable="false"的[apk文件](/img/in-post/post-android-anti-debug/anti-debug.apk)。其集成了所有上述的反调试方法最简单的样例。

每个方法都有一个按钮，对应一个JNI函数，读者可以通过点击不同的按钮来尝试调试不同的样例。当前`基于套路的检测`这个方法对应的`Trck`按钮还没想到比较好的样例展示方法，其它按钮效果如下：

1. `TIME`按钮：基于时间的检测，未发现调试返回"Hello from time",发现调试返回"Debug from time"
2. `FILE`按钮：基于文件的检测，未发现调试返回"Hello from file",发现调试返回"Debug from file"
3. `VM`按钮：Dalvik虚拟机内部相关字段，未发现调试返回"Hello from vm",发现调试返回"Debug from vm"
4. `PTRACE`按钮：基于ptrace的检测，未发现调试返回"Hello from ptrace",发现调试返回"Debug from ptrace"
5. `BKPT`按钮：断点扫描，未发现调试返回"Hello from bkpt",发现调试返回"Debug from bkpt"。
6. `FORK`按钮：多进程/线程，开启检测后会立刻返回"Hello from fork",发现调试后直接杀死本APP。本按钮按下后效果持续，建议最后尝试。
7. `SIGNAL`按钮：调试器的错误理解+信号处理，未发现调试返回"Hello from signal",发现调试后前者会让调试崩溃，后者会让调试卡死。

![](/img/in-post/post-android-anti-debug/example-apk.png)






