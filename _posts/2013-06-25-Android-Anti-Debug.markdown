---
layout:     post
title:      "Android反调试技术整理与实践(未完成)"
subtitle:   ""
date:       2017-06-25 12:00:00
author:     "GToad"
header-img: "img/post-bg-2015.jpg"
catalog: true
tags:
    - Android
    - 反调试
    - LTS
---

> 本文长期维护与更新。作为一个安卓反调试的笔记吧。  
> 本文为作者本人原创，转载请注明出处：[GToad Blog](https://gtoad.github.io/2018/07/05/Android-Native-Hook/)

##前言

安卓APP的反调试技术网上有许多，是保护APP运行逻辑被破解的重要技术方向之一。个人对这项技术的推崇在于：它不仅仅能对动态分析造成麻烦，其对于一些基于调试的hook工具（如frida工具）也能有妨碍作用。
