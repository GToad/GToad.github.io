---
layout:     post
title:      "Android Native Hook技术路线概述"
subtitle:   ""
date:       2017-06-24 12:00:00
author:     "GToad"
header-img: "img/post-bg-2015.jpg"
catalog: true
tags:
    - Android
    - NDK
    - Hook
    - inline hook
    - plt hook
    - native hook
    - LTS
---

> 本文长期维护与更新。作为一个安卓Native Hook的笔记吧。  

##前言

在目前的安卓APP测试中对于Native Hook的需求越来越大，越来越多的APP开始逐渐使用NDK来开发核心或者敏感代码逻辑。
个人认为原因如下：
1. 安全的考虑。各大APP越来越注重安全性，NDK所编译出来的so库逆向难度明显高于java代码产生的dex文件。越是敏感的加密算法与数据就越是需要用NDK进行开发。
2. 性能的追求。NDK对于一些高性能的功能需求是java层无法比拟的。
3. 手游的兴起。虚幻4，Unity等引擎开发的手游中都有大量包含游戏逻辑的so库。

因此，本人调查了一下Android Native Hook工具目前的现状:有点惨不忍睹......
尽管Java层的Hook工具多种多样。
主要有两大路线：
1. PLT Hook
2. Inline Hook

这两种技术路线本人都实践了一下，下面来对比总结。


##PLT Hook

本技术路线的典型代表是爱奇艺开源的xHook工具。


##Inline Hook

本技术路线的基本原理是在代码段中插入跳转，从而达到Hook的效果。详情如下图所示：



##总结

从上面的分析中不难看出，这两种技术各有特点。PLT Hook技术就好比自行车，容易得到，操作简便，但是功能极为有限;Inline Hook技术就像汽车，造价昂贵，操作复杂，但是几乎可以应对各种需求。
因此对于正在寻找Native Hook工具的同学们需要仔细预估一下自己的Native Hook需求，如果只对于系统调用有参数或者性能上的监控需求，那可以考虑PLT Hook，而如果是希望应对各种各样APP自己独有的NDK函数或者代码段的话，目前只能选择Inline Hook。


