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

> 本文为作者本人原创，转载请注明出处：[GToad Blog](https://gtoad.github.io/2018/07/05/Android-Native-Hook/)

## 前言

安卓APP的反调试技术网上有许多，是保护APP运行逻辑被破解的重要技术方向之一。个人对这项技术的推崇在于：它不仅仅能对动态分析造成麻烦，其对于一些基于调试的hook工具（如frida工具）也能有妨碍作用。

## 基于时间的检测

通过在代码不同的地方获取时间值，从而可以求出这个过程中的执行时间。如果两个时间值相差过大，则说明中间的代码流程被调试了。因为调试者停下来一步步观察了这一段代码的执行情况，因此这部分代码的执行时间远远超出了普通状态的执行时间。

## 基于文件的检测

通过查看系统中的指定文件，也可以得到调试的信息。主要有如下6个文件，把它们两两一组。相同组内的两个文件内容是完全一样的。

1. /proc/pid/status 和 /proc/pid/task/pid/status：普通状态下，TracerPid这项应该为0；调试状态下为调试进程的PID。
（图片）
2. /proc/pid/stat 和 /proc/pid/task/pid/stat：调试状态下，括号后面的第一个字母应该为t。
（图片）
3. /proc/pid/wchan 和 /proc/pid/task/pid/wchan：调试状态下，里面内容为ptrace_stop。
（图片）


## 基于套路的检测

这部分方法是基于检测常见调试工具是否存在，从而进行反调试的。一般来说Android应用调试的时候，系统里可能会运行android_server、gdb、gdbserver等进程。在Android 6.0前，可以直接获得系统进程表，从而查看是否有这几个关键字的进程名称。
也可以通过监视常用的调试端口——23946来判断是否处于调试环境下。信息来源可以是/proc/net/tcp。

## Dalvik虚拟机内部相关字段

Android Java虚拟机结构中也保存了是否被调试的相关数据。Android 5.0以前的系统版本中可以通过调用java.lang.Object->dalvik.system.VMDebug->Dalvik_dalvik_system_VMDebug_isDebuggerConnected()来获得结果。之后的版本中改成了android.os.Debug类下的isDebuggerConnected()方法。
由于是Java层的系统调用，所以相比于Native层，本方法会更容易被发现，且被Hook篡改返回值也会更简单。

## ptrace

由于Linux下每个进程同一时刻最多只能被一个进程调试，因此APP可以通过自己ptrace自己的方式来抢先占坑。

## 断点扫描

IDA等调试器在调试时候的原理是向断点地址插入breakpoint汇编指令，而把原来的指令暂时备份到别处。因此，本方法通过扫描自身so的代码部分中是否存在breakpoint指令即可。

一般来说Android App有arm模式和thumb模式，因此需要都检查一下(一下数据为小数端表示)：

1. Arm：0x01，0x00，0x9f，0xef
2. Thumb16：0x01，0xde
3. Thumb32：0xf0，0xf7，0x00，0xa0

## 信号处理

上面提到了调试器会在断点处使用breakpoint命令，而这条指令也使被调试进程发出信号SIGTRAP。通常调试器会截获Linux系统内给被调试进程的各种信号，由调试者可选地传递给被调试进程。但是SIGTRAP是个例外，因为通常的目标程序中不会出现breakpoint，因为这会使得程序自己奔溃。因此，当调试器遇到SIGTRAP信号时会认为是自己下的断点发出的。这样一来当调试器给这个breakpoint命令插入断点breakpoint后，备份的命令也是breakpoint，这样当继续执行时，调试器将备份指令恢复并执行，结果误以为备份后这个位置发出的SIGTRAP又是自己下的断点造成的，这样一来就会使得调试器的处理逻辑出现错误，不同的调试器会导致各种不同的问题。

## 调试器的错误理解

之前提到过，Android App尽管可能支持各种平台如MIPS,X86架构。但是一般在现在的主流手机上依然是运行在Arm架构的CPU上的。而Arm架构的CPU却不仅仅只是运行Arm指令集，还会运行Thumb指令集，并且目前Android Studio已经将Thumb-2模式设定为默认NDK编译指令集，比Arm指令集还要优先。这是为什么？因为Thumb-2模式是Thumb16和Thumb32指令的混合执行，有更高的效率和代码密度，对于APP的运行效率和空间占用都有着更好的表现。

但是这对于调试器来说并不是好事。Thumb16和Thumb32在opcode上没有冲突，只要一条条按照顺序去反汇编，就可以得到正确的Thumb指令。但是Arm指令集和Thumb指令集是会有冲突的，一条Thumb32指令是可以被理解为作用意义完全不同的另一条Arm指令的，甚至2条Thumb16指令可以被调试器误解为一条合法的Arm指令。

而这两个模式的切换涉及跳转时候的地址表示。

## 多进程


