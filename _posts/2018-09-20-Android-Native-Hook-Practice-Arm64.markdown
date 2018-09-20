---
layout:     post
title:      "ARM64下的Android Native Hook工具实践（未完成）"
subtitle:   ""
date:       2018-09-20 10:30:00
author:     "GToad"
header-img: "img/android-five.png"
catalog: true
tags:
    - Android
    - NDK
    - Hook
    - arm64
    - inline hook
    - native hook
    - LTS
---

> 本文讲的是[Android Native Hook工具实践](https://gtoad.github.io/2018/07/06/Android-Native-Hook-Practice/)一文的后续。为了使得安全测试人员可以在ARM64的手机上进行Android Native Hook而继续做的一些工作。

> 本文对测试机环境配置的要求较高，文中使用的设备是Pixel。

## 背景

ARM64的手机芯片是向下兼容ARM32和Thumb-2指令集的，这也导致许多App其实并没有管用户手机是32位的还是64位的，依然只在apk中打包32位的.so而并没有arm64-v8a的so文件。并且有的App的32位so还不是armeabi-v7a，例如微信依然在使用ARMv5，这个架构老得连Thumb-2都不支持。由此可见市面上相当多的App其实并没有急着去使用ARM64。

第一部64位的谷歌亲儿子手机是2015年的nexus 6p，它搭载了高通的第一个ARM64架构的芯片——骁龙810。而最后一部32位的谷歌亲儿子手机是2014年的nexus 5。也就是说nexus 5已经发布4年了，我们且不讨论一部手机能不能正常工作4年，但它的性能是肯定已经开始跟不上当前的许多用户需求了。我的nexus 5测试机在仅仅安装了微信，QQ等5个常用App后的不久便开始卡了起来。

2018年初的时候Android 4.4及以前的手机比例已经低于30%了，Nexus 5也是最后一部能刷Android 4.4的谷歌亲儿子，多巧啊。当初做这个Native Hook工具的初衷就是认为Android 4.4及之前的手机在市面上的比例在未来的几年会进一步缩小，测试人员迟早需要面对在5.0以上的测试机环境中进行安全测试的情况，因为2015年后发布的手机几乎出厂设置版本都高于Android 5.0，再没有4.4的手机给我们用了。

也许这些Neuxs 5同时代的手机可以用模拟器模拟，但是目前反模拟器技术也是一大热门，在测试前先过掉各个App的反模拟器检测会耽误不少功夫。

因此，过不了几年，等32位手机急剧下降到一定比例的时候，不少App会为了追求更高的性能而选择ARM64的。因此有必要对ARM64指令集进行一定的知识储备和工具开发。

## 环境准备

想要开发这个工具，那我至少需要一个ARM64的运行环境，有如下三种选择：

1. 虚拟机——太慢了，而且在x64的电脑上模拟ARM64可能会出现真机上不会出现的坑。
2. 骁龙810+的品牌手机——不少品牌如华为、oppo连解锁都困难，那ROOT手机就更麻烦了，刷机也比亲儿子麻烦。
3. 骁龙810+的谷歌亲儿子——最坏的情况下也能用AOSP来自己编译系统刷机。`采用`

那么选哪款谷歌亲儿子好呢？

1. Nexus 6p——骁龙810，二手450元左右，外形不是很好看。
2. Pixel——骁龙821，二手无锁版750元左右，黑版外形可接受。`采用`
3. Pixel 2——骁龙835，二手2000+，太贵了。

刷什么系统？

1. CyanogenMod——俗称CM，在用Nexus 5做测试的时候很喜欢刷这个，自带ROOT和全局调试，但是官方已经倒闭了，生前给Pixel没留啥资源。
2. LineageOS——CM的残部组建的，这个系统刷了以后发现ROOT要单独刷，要配合TWRP，但是最后发现兼容性也有问题，ROOT包并没有起作用。Pixel与Android 7.1的“双系统”策略也留了好多没填补成功的坑。
3. Google官方——业界标杆，估计出不了太离谱的坑。`采用`

如何获取ROOT权限并搭建基本调试环境？

1. Kingroot等root工具都失败了
2. twrp+Magisk——可以获得ROOT权限，但Magisk与正常的Xposed框架存在冲突，只能使用Magisk定制版的Xposed，但那个不稳定，老是使得手机在开机画面中无线循环。
3. twrp+SuperSU——可以获得ROOT权限，但发现在企图把IDA pro的调试服务端android-server64放入/system/bin下时会报错：read-only file system。这个错误按理来说用`adb root``adb remount`就能解决了，但这里并没有效果，直接使用`mount`命令也无果，最后使用需要执行`adb disable verity`而这条命令在官方编译好的user版本系统中并没有执行权限。
4. AOSP——由于众所周知的原因国内对AOSP的编译工作会比国外麻烦，下载代码的过程也不能按照官方教程来进行。最后编译上的小错误也很多，每次编译时间长，磁盘占用超过了150GB。但是现在我也只能靠它了。`采用`

AOSP编译哪个版本？

1. user——给普通用户用的，我们通常买到的手机中都是user版的系统，用户权限非常小，没有ROOT和其它权限。
2. userdebug——给应用开发者用的，自带ROOT权限和全局调试，适合对运行在安卓系统之上的App进行调试与测试。`采用`
3. eng——给系统研究者用的，自带ROOT权限和其它一些Google额外提供的调试工具，但是实际编译下来发现缺乏对硬件的支持，主要是给模拟器用的。适合那些学习安卓源码的人使用。

系统编译好刷好机以后，开始打算按照老套路，装个Xposed来加载我们的so文件从而开展Native Hook的工作。但是产生了一系列的报错，经过测试和推测，最终的原因应该是Android 7.0之后对于非公开API的调用限制。这下就得先解决这个问题，否则我们的so根本就加载不起来。有如下几条路让我选择：

1. 360的非虫大佬9月分在自己的微信公众号上发了一篇《动手打造Android7.0以上的注入工具》——难度较高，暂且跟不上T.T
2. 后悔没有买450元的Nexus 6p——虽然Nexus6p可以刷6.0的系统从而避开这个问题，但是这就和我们在安卓Dalvik转ART后把测试工作环境限制在4.4之前版本一样，都是一时的退让，过几年年，大家都用Android 7.0以上的手机了，那就又不行了。
3. AOSP中/bionic/linker/linker.cpp下存在一个灰名单，把我们的so文件加入这个灰名单就能加载。——每次测试一个新的App就重新变异一次AOSP？然后其它环境都重装？太麻烦了。
4. 官方文档中有提及这个机制也有白名单/vendor/etc/public.libraries.txt和/system/etc/public.libraries.txt——修改它们可能会引起别的App的崩溃，但是目前它是最方便的了 `采用`
5. AOSP里把这个机制给去掉——个人认为最好的方案，之后研究，核心代码可能在 /bionic/linker/linker.cpp中的static bool load_library()函数中。







`那为什么要做ARM64上的Android Native Hook？`

原因

Some e-mails was sent to me about the `B..` problems. They find I change BLS to BHI, BNE to BEQ and they think I make a mistake. In fact, I changed them all :

1. BEQ --> BNE
2. BNE --> BEQ
3. BCS --> BCC
4. BCC --> BCS
5. BMI --> BPL
6. BPL --> BMI
7. BVS --> BVC
8. BVC --> BVS
9. BHI --> BLS
10. BLS --> BHI
11. BGE --> BLT
12. BLT --> BGE
13. BGT --> BLE
14. BLE --> BGT

But I do this on purpose. Let's see the picture below, it's beautiful right? Every code has a piece of fix codes, because there isn't any `B..` code.

![](https://gtoad.github.io/img/in-post/post-android-native-hook-practice/b_condition_fix_design_2.png)

If there is an B.. code in it, the fix can be the picture below. OMG, I feel sick! The fix code is in two part! And the second part is at the end of the entire fix code! And the X in `BLS X` is hard to know... , but I use `pstInlineHook->backUpFixLengthList` to predict and get value of X.

![](https://gtoad.github.io/img/in-post/post-android-native-hook-practice/b_condition_fix_design_1.png)

Even worse when there are two `B..` code, it's more complicated, and I wanna vomit:

![](https://gtoad.github.io/img/in-post/post-android-native-hook-practice/b_condition_fix_design_3.png)

So how to make the two part in one? I try this, and the fix code is beautiful again~:

![](https://gtoad.github.io/img/in-post/post-android-native-hook-practice/b_condition_fix_old_design_1.png)

But the 12 bytes with 3 jump? I think maybe it can be more beautiful... More pithy and more short, an great idea come to my mind ---- `opposite logic`! This can be 4 bytes shorter in arm32 and thumb32 without adding NOP. And the jump logic is less. It's like this:

![](https://gtoad.github.io/img/in-post/post-android-native-hook-practice/b_condition_fix_new_design_1.png)

Now the fix code with `B..` is beautiful and short. Hahahahaha........

That's why you can see the `BNE` is changed to `BEQ`in fix code. 

If you still have some questions please create issue in the [repo](https://github.com/GToad/Android_Inline_Hook) so the other people can see and help us. I will try my best to answer them in English.
