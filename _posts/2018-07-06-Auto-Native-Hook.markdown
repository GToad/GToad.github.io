---
layout:     post
title:      "Android Native Hook工具实践"
subtitle:   ""
date:       2018-07-06 12:00:00
author:     "GToad"
header-img: "img/post-bg-2015.jpg"
catalog: true
tags:
    - Android
    - NDK
    - Hook
    - inline hook
    - native hook
    - LTS
---

> 本项目长期维护与更新，因为在我自己的几台测试机上用得还挺顺手的。  

##前言

在目前的安卓APP测试中对于Native Hook的需求越来越大，越来越多的APP开始逐渐使用NDK来开发核心或者敏感代码逻辑。
个人认为原因如下：
1. 安全的考虑。各大APP越来越注重安全性，NDK所编译出来的so库逆向难度明显高于java代码产生的dex文件。越是敏感的加密算法与数据就越是需要用NDK进行开发。
2. 性能的追求。NDK对于一些高性能的功能需求是java层无法比拟的。
3. 手游的兴起。虚幻4，Unity等引擎开发的手游中都有大量包含游戏逻辑的so库。

因此，本人调查了一下Android Native Hook工具目前的现状。尽管Java层的Hook工具多种多样，但是Native Hook的工具缺寥寥无几。（文末解释1）
而目前Native Hook主要有两大技术路线：
1. PLT Hook
2. Inline Hook

这两种技术路线本人都实践了一下，关于它们的对比，我在《Android Native Hook》中有介绍，所以这里就不多说了。

最终，我用了Inline Hook来做这个项目。

（关于目前公开的Android Native Hook工具寥寥无几这一点我补充解释一下：唯一一个公开且接近于Java Hook的Xposed那样好用的工具可能就只是Cydia Substrate了。但是该项目已经好几年没更新，并且只支持到安卓5.0以前。还有一个不错的Native Hook工具是Frida，但是它的运行原理涉及调试，因此遇到反调试会相当棘手。）


##目标效果

根据本人自身的使用需求提出了如下几点目标：

1. 工具运行原理中不能涉及调试目标APP，否则本工具在遇到反调试措施的APP时会失效。尽管可以先去逆向调试patch掉反调试功能，但是对于大多数情况下只是想看看参数和返回值的Hook需求而言，这样的前期处理实在过于麻烦。
2. 依靠现有的各大Java Hook工具就能运行本工具，换句话说就是最好能用类似这些工具的插件的形式加载起本工具从而获得Native Hook的能力。由于Java Hook工具如Xposed、YAHFA等对于各个版本的Android都做了不错的适配，因此利用这些已有的工具即可向目标APP的Native层中注入我们的Hook功能将会方便很多小伙伴的使用。
3. 既然要能够让各种Java Hook工具都能用本工具得到Native Hook的能力，那就这个工具就要有被加载起来以后自动执行自身功能逻辑的能力！而不是针对各个Java Hook工具找调用起来的方式。
4. 要适配Android NDK下的armv7和thumb-2指令集。由于现在默认编译为thumb-2模式，所以对于thumb16和thumb32的Native Hook支持是重中之重。
5. 修复Inline Hook后的原本指令。
6. Hook目标的最小单位至少是函数，最好可以是某行汇编代码。

##最终方案

最后完成项目的方案是：本工具是一个so库。用Java Hook工具在APP运行一开始的onCreate方法处Hook，然后
加载本so后，自动开始执行Hook逻辑。
为了方便叙述，接下来的Java Hook工具我就使用目前这类工具里最流行的Xposed，本项目的生成文件名为libautohook.so。


##自动执行

我们只是用Xposed加载了这个libautohook.so，那其中的函数该怎么自动执行呢？
目前想到两个方法：

1. 利用JniOnload来自动执行。该函数是NDK中用户可以选择性自定义实现的函数。如果用户不实现，则系统默认使用NDK的版本为1.1。但是如果用户有定义这个函数，那就会在onloadlib加载so库时自动先执行这个函数。尽管该函数最终要返回的

2. 

##方案设计

现在我们的代码可以在一开始就执行了，那该如何设计这套Inline Hook方案呢？目标是thumb-2和arm指令集下是两套相似的方案。我参考了腾讯游戏安全实验室的一篇教程，其中给出了一个初步的armv7指令集下的Native Hook方案，整理后如下图：

于是这就是本工具在arm指令集上的Native Hook方案。那么在thumb-2指令集上该怎么办呢？我决定使用多模式切换来实现：


##


##Inline Hook

本技术路线的基本原理是在代码段中插入跳转，从而达到Hook的效果。详情如下图所示：



##总结

从上面的分析中不难看出，这两种技术各有特点。PLT Hook技术就好比自行车，容易得到，操作简便，但是功能极为有限;Inline Hook技术就像汽车，造价昂贵，操作复杂，但是几乎可以应对各种需求。
因此对于正在寻找Native Hook工具的同学们需要仔细预估一下自己的Native Hook需求，如果只对于系统调用有参数或者性能上的监控需求，那可以考虑PLT Hook，而如果是希望应对各种各样APP自己独有的NDK函数或者代码段的话，目前只能选择Inline Hook。


