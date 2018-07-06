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

> 本文章所对应项目长期维护与更新，因为在我自己的几台测试机上用得还挺顺手的。本项目作为作者本人的一个学习项目将会长期更新以修复当前可能存在的Bug以及跟进以后Android NDK可能出现的主流汇编模式。

## 前言

在目前的安卓APP测试中对于Native Hook的需求越来越大，越来越多的APP开始逐渐使用NDK来开发核心或者敏感代码逻辑。个人认为原因如下：
1. 安全的考虑。各大APP越来越注重安全性，NDK所编译出来的so库逆向难度明显高于java代码产生的dex文件。越是敏感的加密算法与数据就越是需要用NDK进行开发。
2. 性能的追求。NDK对于一些高性能的功能需求是java层无法比拟的。
3. 手游的兴起。虚幻4，Unity等引擎开发的手游中都有大量包含游戏逻辑的so库。

因此，本人调查了一下Android Native Hook工具目前的现状。尽管Java层的Hook工具多种多样，但是Native Hook的工具却非常少并且在安卓5.0以上的适配工具更是寥寥无几。（文末说明1）而目前Native Hook主要有两大技术路线：
1. PLT Hook
2. Inline Hook

这两种技术路线本人都实践了一下，关于它们的对比，我在《Android Native Hook》中有介绍，所以这里就不多说了。最终，我用了Inline Hook来做这个项目。


## 目标效果

根据本人自身的使用需求提出了如下几点目标：

1. 工具运行原理中不能涉及调试目标APP，否则本工具在遇到反调试措施的APP时会失效。尽管可以先去逆向调试patch掉反调试功能，但是对于大多数情况下只是想看看参数和返回值的Hook需求而言，这样的前期处理实在过于麻烦。
2. 依靠现有的各大Java Hook工具就能运行本工具，换句话说就是最好能用类似这些工具的插件的形式加载起本工具从而获得Native Hook的能力。由于Java Hook工具如Xposed、YAHFA等对于各个版本的Android都做了不错的适配，因此利用这些已有的工具即可向目标APP的Native层中注入我们的Hook功能将会方便很多小伙伴的使用。
3. 既然要能够让各种Java Hook工具都能用本工具得到Native Hook的能力，那就这个工具就要有被加载起来以后自动执行自身功能逻辑的能力！而不是针对各个Java Hook工具找调用起来的方式。
4. 要适配Android NDK下的armv7和thumb-2指令集。由于现在默认编译为thumb-2模式，所以对于thumb16和thumb32的Native Hook支持是重中之重。
5. 修复Inline Hook后的原本指令。
6. Hook目标的最小单位至少是函数，最好可以是某行汇编代码。

## 最终方案

最后完成项目的方案是：本工具是一个so库。用Java Hook工具在APP运行一开始的onCreate方法处Hook，然后
加载本so后，自动开始执行Hook逻辑。
为了方便叙述，接下来的Java Hook工具我就使用目前这类工具里最流行的Xposed，本项目的生成文件名为libautohook.so。


## 自动执行

我们只是用Xposed加载了这个libautohook.so，那其中的函数该怎么自动执行呢？
目前想到两个方法：

1. 利用JniOnload来自动执行。该函数是NDK中用户可以选择性自定义实现的函数。如果用户不实现，则系统默认使用NDK的版本为1.1。但是如果用户有定义这个函数，那Android VM就会在System.loadLibrary()加载so库时自动先执行这个函数来获得其返回的版本号。尽管该函数最终要返回的是NDK的版本号，但是其函数可以加入任意其它逻辑的代码，从而实现加载so时的自动执行。这样就能优先于所有其它被APP NDK调用的功能函数被调用，从而进行Hook。目前许多APP加固工具和APP初始化工作都会用此方法。

2. 本文采用的是第二种方法。该方法网络资料中使用较少。它是利用了`__attribute__((constructor))`属性。使用这个constructor属性编译的普通ELF文件被加载入内存后，最先执行的不是main函数，而是具有该属性的函数。同样，本项目中利用此属性编译出来的so文件被加载后，尽管so里没有main函数，但是依然能优先执行，且其执行甚至在JniOnload之前。于是逆向分析了一下编译出来的so库文件。发现具有`constructor`属性的函数会被登记在.init_array中。（相对应的`destructor`属性会在ELF卸载时被自动调用，这些函数会被登记入.fini_array）

![](/img/in-post/post-android-native-hook-practice/init_array.png)

值得一提的是，`constructor`属性的函数是可以有多个的，对其执行顺序有要求的同学可以通过在代码中对这些函数声明进行排序从而改变其在.init_array中的顺序，二者是按顺序对应的。而执行时，会从.init_array中自上而下地执行这些函数。

## 方案设计

先说一下使用的工具：

1. 使用keystone查找指定架构下汇编指令的机器码
2. 使用MS VISIO制作了下面的设计图
3. 调试工具用的是IDA

#### Arm32方案

现在我们的代码可以在一开始就执行了，那该如何设计这套Inline Hook方案呢？目标是thumb-2和arm指令集下是两套相似的方案。我参考了腾讯游戏安全实验室的一篇教程，其中给出了一个初步的armv7指令集下的Native Hook方案，整理后如下图（高清无码大图请点我）：

![](/img/in-post/post-android-native-hook-practice/armhook.png)

第一步，根据/proc/self/map中目标so库的内存加载地址与目标Hook地址的偏移计算出实际需要Hook的内存地址。将目标地址处的2条ARM32汇编代码（8 Bytes）进行备份，然后用一条LDR PC指令和一个地址（共计8 Bytes）替换它们。这样就能（以arm模式）将PC指向图中第二部分stub代码所在的位置。由于使用的是LDR而不是BLX，所以lr寄存器不受影响。关键代码如下：

```c
//LDR PC, [PC, #-4]对应的机器码为：0xE51FF004

BYTE szLdrPCOpcodes[8] = {0x04, 0xF0, 0x1F, 0xE5};
//将目的地址拷贝到跳转指令下方的4 Bytes中

memcpy(szLdrPCOpcodes + 4, &pJumpAddress, 4);
```

第二步，构造stub代码。构造思路是先保存当前全部的寄存器状态到栈中。然后用BLX命令（以arm模式）跳转去执行用户自定义的Hook后的函数。执行完成后，从栈恢复所有的寄存器状态。最后（以arm模式）跳转至第三部分备份代码处。关键代码如下：

```
_shellcode_start_s:
    push    {r0, r1, r2, r3}
    mrs     r0, cpsr
    str     r0, [sp, #0xC]
    str     r14, [sp, #8]   
    add     r14, sp, #0x10
    str     r14, [sp, #4]    
    pop     {r0}               
    push    {r0-r12}           
    mov     r0, sp
    ldr     r3, _hookstub_function_addr_s
    blx     r3
    ldr     r0, [sp, #0x3C]
    msr     cpsr, r0
    ldmfd   sp!, {r0-r12}       
    ldr     r14, [sp, #4]
    ldr     sp, [r13]
    ldr     pc, _old_function_addr_s
```

第三步，构造备份代码。构造思路是先执行之前备份的2条arm32代码（共计8 Btyes），然后用LDR指令跳转回Hook地址+8bytes的地址处继续执行。此处先不考虑PC修复，下文会说明。构造出来的汇编代码如下：

```
备份代码1
备份代码2
LDR PC, [PC, #-4]
HOOK_ADDR+8
```

#### Thumb-2方案

以上是本工具在arm指令集上的Native Hook基本方案。那么在thumb-2指令集上该怎么办呢？我决定使用多模式切换来实现(文末解释2)，整理后如下图（高清无码大图请点我）：

![](/img/in-post/post-android-native-hook-practice/thumbhook.png)

`虽然这部分内容与arm32很相似，但由于细节坑较多，所以我认为下文重新梳理详细思路是必要的。`

第一步，根据/proc/self/map中目标so库的内存加载地址与目标Hook地址的偏移计算出实际需要Hook的内存地址。将目标地址处的X Bytes的thumb汇编代码进行备份。然后用一条LDR.W PC指令和一个地址（共计8 Bytes）替换它们。这样就能（以arm模式）将PC指向图中第二部分stub代码所在的位置。由于使用的是LDR.W而不是BLX，所以lr寄存器不受影响。

`细节1`：为什么说是X Bytes？参考了网上不少的资料，发现大部分代码中都简单地将arm模式设置为8 bytes的备份，thumb模式12 bytes的备份。对arm32来说很合理，因为2条arm32指令足矣，上文处理arm32时也是这么做的。而thumb-2模式则不一样，thumb-2模式是thumb16（2 bytes）与thumb32（4 bytes）指令混合使用。本人在实际测试中出现过2+2+2+2+2+4>12的情形，这种情况下，最后一条thumb32指令会被截断，从而在备份代码中执行了一条只有前半段的thumb32，而在4->1的返回后还要执行一个只有后半段的thumb32。因此，本项目中在第一步备份代码前会检查最后第11和12byte是不是前半条thumb32，如果不是，则备份12 byte。如果是的话，就备份10 byte。

`细节2`：为什么Plan B是10 byte？我们需要插入的跳转是8 byte，但是thumb32中如果指令涉及修改PC的话，那么这条指令所在的地址一定要能整除4，否则程序会崩溃。我们的指令地址肯定都是能被2整除的，但是能被4整除是真的说不准。因此，当出现地址不能被4整除时，我们需要先补一个thumb16的NOP指令（2 bytes）。这样一来就需要2+8=10 Bytes了。尽管这时候选择14 Bytes也差不多，我也没有内存空间节省强迫症，但是选择这10 Bytes主要还是为了提醒一下大家这边补NOP的细节问题。
关键代码如下：

```c
bool InitThumbHookInfo(INLINE_HOOK_INFO* pstInlineHook)
{
    ......

    uint16_t *p11 = pstInlineHook->pHookAddr-1+11;
    //判断最后由(pHookAddr-1)[10:11]组成的thumb命令是不是thumb32，

    //如果是的话就需要备份14byte或者10byte才能使得汇编指令不被截断。由于跳转指令在补nop的情况下也只需要10byte，

    //所以就取pstInlineHook->backUpLength为10

    if(isThumb32(*p11))
    {
        LOGI("The last ins is thumb32. Length will be 10.");
        pstInlineHook->backUpLength = 10;
        memcpy(pstInlineHook->szbyBackupOpcodes, pstInlineHook->pHookAddr-1, pstInlineHook->backUpLength);
    }
    else{
        LOGI("The last ins is not thumb32. Length will be 12.");
        pstInlineHook->backUpLength = 12;
        //修正：否则szbyBackupOpcodes会向后偏差1 byte

        memcpy(pstInlineHook->szbyBackupOpcodes, pstInlineHook->pHookAddr-1, pstInlineHook->backUpLength); 
    }
    
    ......
}

bool BuildThumbJumpCode(void *pCurAddress , void *pJumpAddress)
{
    ......

        //LDR PC, [PC, #0]对应的thumb机器码为：0xf000f8df, NOP为BF00

        if (CLEAR_BIT0((uint32_t)pCurAddress) % 4 != 0) {
            BYTE szLdrPCOpcodes[12] = {0x00, 0xBF, 0xdF, 0xF8, 0x00, 0xF0};
            memcpy(szLdrPCOpcodes + 6, &pJumpAddress, 4);
            memcpy(pCurAddress, szLdrPCOpcodes, 10);
            cacheflush(*((uint32_t*)pCurAddress), 10, 0);
        }
        else{
            BYTE szLdrPCOpcodes[8] = {0xdF, 0xF8, 0x00, 0xF0};
            //将目的地址拷贝到跳转指令缓存位置

            memcpy(szLdrPCOpcodes + 4, &pJumpAddress, 4);
            memcpy(pCurAddress, szLdrPCOpcodes, 8);
            cacheflush(*((uint32_t*)pCurAddress), 8, 0);
        }

    ......
}


```

第二步，构造stub代码。构造思路是先保存当前全部的寄存器状态到栈中。然后用BLX命令（以arm模式）跳转去执行用户自定义的Hook后的函数。执行完成后，从栈恢复所有的寄存器状态。最后（以thumb模式）跳转至第三部分备份代码处。

`细节1`：为什么跳转到第三部分要用thumb模式？因为第三部分中是含有备份的thumb代码的，而同一个顺序执行且没有内部跳转的代码段是无法改变执行模式的。因此，整个第三部分的汇编指令都需要跟着备份代码用thumb指令来编写。

`细节2`：第二部分是arm模式，但是第三部分却是thumb模式，如何切换？我在`第一步的细节2`中提到过，无论是arm还是thumb模式，每条汇编指令的地址肯定都能整除2，因为最小的thumb16指令也需要2 Bytes。那么这时候Arm架构就规定了，当跳转地址是单数时，就代表要切换到thumb模式来执行；当跳转地址是偶数时，就代表用Arm模式来执行。这个模式不是切换的概念，换句话说与跳转前的执行模式无关。无论跳转前是arm还是thumb，只要跳转的目标地址是单数就代表接下来要用thumb模式执行，反之arm模式亦然。这真的是个很不错的设定，因为我们只需要考虑接下来的执行模式就行了。这里，本人就是通过将第三部分的起始地址+1来使得跳转后程序以thumb模式执行。

`细节3`：下方的关键代码中`ldr  r3, _old_function_addr_s_thumb`到`str  r3, _old_function_addr_s_thumb`就是用来给目标地址+1的。这部分代码不能按照逻辑紧贴着最后的`ldr  pc, _old_function_addr_s_thumb`来写，而是一定要写在恢复全部寄存器状态的前面，否则这里用到的r3会错过恢复从而引起不稳定。

`细节4`：那条bic指令是用来清除`_old_function_addr_s_thumb`变量的最低位的。因为如果该Hook目标会被多次调用，那每次这个`_old_function_addr_s_thumb`都会被+1。第一次没有问题，成功变成了thumb模式，而第二次会以arm模式下偏2 bytes跳转，之后偏差越来越大，模式交叉出现。因此，本人使用bic指令来清除每次Hook调用后的地址+1效果。

关键代码如下：

```
_shellcode_start_s_thumb:
    push    {r0, r1, r2, r3}
    mrs     r0, cpsr
    str     r0, [sp, #0xC]
    str     r14, [sp, #8]   
    add     r14, sp, #0x10
    str     r14, [sp, #4]    
    pop     {r0}               
    push    {r0-r12}           
    mov     r0, sp
    ldr     r3, _hookstub_function_addr_s_thumb
    blx     r3
    ldr     r3, _old_function_addr_s_thumb
    bic     r3, r3, #1
    add     r3, r3, #0x1
    str     r3, _old_function_addr_s_thumb
    ldr     r3, [sp, #-0x34]
    ldr     r0, [sp, #0x3C]
    msr     cpsr, r0
    ldmfd   sp!, {r0-r12}       
    ldr     r14, [sp, #4]
    ldr     sp, [r13]
    ldr     pc, _old_function_addr_s_thumb
```

第三步，构造备份代码。构造思路是先执行之前备份的X Bytes的thumb-2代码，然后用LDR.W指令来跳转回Hook地址+Xbytes的地址处继续执行。此处先不考虑PC修复，下文会说明。

`细节1`：LDR是arm32的指令，LDR.W是thumb32的指令，作用是相同的。这里想说的是：为什么整个过程中都一直在用LDR和LDR.W，只有在第二步中有使用过BLX指令来进行跳转？原因很简单，为了保存状态。从第一步跳转到stub开始，如果跳转使用了BLX，那就会影响到lr等寄存器，而如果使用LDR/LDR.W则只会改变PC来实现跳转而已。stub中唯一的那次BLX是由于当时需要跳转到用户自己写的Hook功能函数中，这是个正规的函数，它最后需要凭借BLX设置的lr寄存器来跳转回BLX指令的下一条指令。并且这个唯一的BLX处于保存全部寄存器的下面，恢复全部寄存器的上面，这部分的代码就是所谓的“安全地带”。因此，这其中改变的lr寄存器将在之后被恢复成最初始的状态。`第二步的细节3`中提及的r3寄存器的操作要放在这个“安全区”里也是这个原因。而在stub之外，我们的跳转只能影响到PC，不可以去改变lr寄存器，所以必须使用LDR/LDR.W。

`细节2`：下面的抽象图中可以发现与arm中的不同，arm中最后是`LDR PC, [PC, #-4]`,这是由于CPU三级流水的关系，执行某条汇编指令时，PC的值在arm下是当前地址+8，在thumb-2下是当前地址+4。而我们要跳转的地址在本条指令后的4 Bytes处，因此，arm下需要PC-4，thumb下就是PC指向的地址。

构造出来的汇编代码抽象形式如下：

```
备份代码1
备份代码2
备份代码3
......
LDR.W PC, [PC, #0]
HOOK_ADDR + 8
```


## PC修复

在上文的处理中，我们很好地保存并恢复了寄存器原本的状态。那么，原本目标程序的汇编指令真的是在它原有的状态下执行的吗？依然不是。虽然寄存器的确一模一样，但是那几条被备份的指令是被移动到了另一个地址上。这样当执行它们的时候PC寄存器的值就改变了。因此，如果这条指令的操作如果涉及到PC的值，那这条指令的执行效果就很可能和原来不一样。所以，我们需要对备份的指令进行修复。
这部分的修复操作参考了`ele7enxxh`大神的博客和项目，里面修复了几乎全部可能出现的PC相关指令的情况！！！真的是太牛逼了！！！！！！具体修复细节本人之后会些分析文章。本人在此中只说一下本项目中是如何处理这个环节的：

1. 遍历备份的指令，arm32自然是一个个4 bytes的指令取走去处理就好，thumb-2则需要判断指令是thumb16还是thumb32，把它们一条条取出来处理。
2. 对每条指令进行PC修复，根据Hook目标地址和该指令在备份代码里的偏移以及CPU的三级流水作用来计算出这条指令当时原本PC的值。从而用这个计算出来的值来代替这个指令中对当前PC的计算。
3. 将每条备份代码修复后的代码按顺序拼接（不需要修复的就用原来的指令去拼接），并在末尾拼接上原本的LDR/LDR.W跳转指令。

于是上文第三步构造出来的汇编代码抽象形式如下：

```
备份代码1
备份代码2
涉及PC的备份代码3的修复代码1
涉及PC的备份代码3的修复代码2
涉及PC的备份代码3的修复代码3
涉及PC的备份代码3的修复代码4
涉及PC的备份代码3的修复代码5
备份代码4
涉及PC的备份代码5的修复代码1
涉及PC的备份代码5的修复代码2
LDR/LDR.W PC, [PC, #-4]
HOOK_ADDR + X
```

## 使用说明（以Xposed为例）

使用者先找到想要Hook的目标，然后在本项目中写自己需要的Hook功能，然后在项目根目录使用`ndk-build`进行编译，需要注意的是本项目中需要严格控制arm和thumb模式，所以`/jni/InlineHook/`和`/jni/Interface/`目录下的Android.mk中`LOCAL_ARM_MODE := arm`不要修改，因为现在默认是编译成thumb模式，这样一来第二步和自定义的Hook函数就不再是设计图中的ARM模式了。自己写的Hook功能写在InlineHook.cpp下，注意`constructor`属性，示例代码如下：
```c
void ModifyIBored() __attribute__((constructor));

/**


 * 针对IBored应用，通过inline hook改变游戏逻辑的测试函数


 */
void ModifyIBored()
{
    LOGI("In IHook's ModifyIBored.");
    int target_offset = 0x43b8; //想Hook的目标在目标so中的偏移

    bool is_target_thumb = true; //目标是否是thumb模式？

    void* pModuleBaseAddr = GetModuleBaseAddr(-1, "libnative-lib.so"); //目标so的名称

    if(pModuleBaseAddr == 0)
    {
        LOGI("get module base error.");
        return;
    }
    
    uint32_t uiHookAddr = (uint32_t)pModuleBaseAddr + target_offset; //真实Hook的内存地址

    //之所以人来判断那是因为Native Hook之前肯定是要逆向分析一下的，那时候就能知道是哪种模式。而且自动识别arm和thumb比较麻烦。

    if(is_target_thumb){ 
        uiHookAddr++;
        LOGI("uiHookAddr is %X in thumb mode", uiHookAddr);
    }
    else{
        LOGI("uiHookAddr is %X in arm mode", uiHookAddr);
    }
    
    InlineHook((void*)(uiHookAddr), EvilHookStubFunctionForIBored);
}
```

本项目在有Xposed框架的测试机上运行时，可以使用一个插件在APP的起始环节就加载本项目的so。本人使用这个插件加载so就很方便啦，不用重启手机，它会自动去系统路径下寻找文件名符合的so然后加载到目标APP中。这个插件的关键代码如下：

```java
public class HookToast implements IXposedHookLoadPackage{
    @Override
    public void handleLoadPackage(XC_LoadPackage.LoadPackageParam lpp) throws Throwable {
        String packageName="";
        String activityName="";
        String soName="";
        try{
            packageName = "com.sec.gtoad.inline_hook_test3"; //目标app
            activityName = "com.sec.gtoad.inline_hook_test3.MainActivity"; //目标app的启动activity
            soName = "InlineHook";                          //我们so的名称（libInlineHook.so）
        } catch (Exception e){
            XposedBridge.log("parse result " + e.getMessage());
            Log.w("GToad", "parse result " + e.getMessage());
        }

        if(!lpp.packageName.equals(packageName)) return;
        XposedBridge.log("load package: " + lpp.packageName);
        Log.w("GToad","load package: " + lpp.packageName);

        hookActivityOnCreate(lpp,activityName,soName,packageName); //当启动Activity开始创建时，就加载我们的so库

    }

    public static boolean loadArbitrarySo(XC_LoadPackage.LoadPackageParam lpp, String soname, String pkg) {
        if (lpp.packageName.equals(pkg)) {
            XposedBridge.log("trying to load so file: " + soname + " for " + pkg);
            Log.w("GToad","trying to load so file: " + soname + " for " + pkg);
            try {
                Log.w("GToad","loading1");

                // /vendor/lib:/system/lib 只要把我们的so放到这些目录之一插件就能找到
                Log.w("GToad",System.getProperty("java.library.path")); 
                System.loadLibrary(soname);
                Log.w("GToad","loading2");
            } catch (Exception e) {
                XposedBridge.log("failed to load so");
                Log.w("GToad","failed to load so");
                return false;
            }
            XposedBridge.log("" + soname + " loaded");
            Log.w("GToad","" + soname + " loaded");
            return true;
        }
        XposedBridge.log("" + pkg + " not found");
        Log.w("GToad","" + pkg + " not found");
        return false;
    }

        private void hookActivityOnCreate(final XC_LoadPackage.LoadPackageParam lpp, final String activityName, final String soName, final String packageName){
        try {
            XposedHelpers.findAndHookMethod(activityName, lpp.classLoader, "onCreate", Bundle.class, new XC_MethodHook() {
                @Override
                protected void beforeHookedMethod(MethodHookParam mhp) throws Throwable {
                    XposedBridge.log("before " + activityName + ".onCreate");
                    Log.w("GToad","before " + activityName + ".onCreate");
                    super.beforeHookedMethod(mhp);
                }

                @Override
                protected void afterHookedMethod(MethodHookParam mhp) throws Throwable {
                    XposedBridge.log("after " + activityName + ".onCreate");
                    Log.w("GToad","after " + activityName + ".onCreate");
                    loadArbitrarySo(lpp,soName,packageName);
                    super.afterHookedMethod(mhp);
                }
            });
        }  catch (Throwable e) {
            XposedBridge.log("" + activityName + ".onCreate " + e.getMessage());
        }
    }
}
```

## 总结

本项目最终形式为一个so库，它可以与任何一个能加载它的工具进行配合，达到Native Hook的效果。并且Hook的最小粒度单位是任意一条汇编指令，这在日常测试中作用很大。

真的非常感谢腾讯游戏安全实验室和ele7enxxh大牛的开源项目为本项目提供的参考。


##文末说明

由于本项目的初衷是为了满足作者自身测试需求才做的，所以关于文中的一些解释与需求可能与别的同学的理解有偏差，这很正常。此处补充解释一下：

1. 关于目前公开的Android Native Hook工具寥寥无几这一点我补充解释一下：唯一一个公开且接近于Java Hook的Xposed那样好用的工具可能就只是Cydia Substrate了。但是该项目已经好几年没更新，并且只支持到安卓5.0以前。还有一个不错的Native Hook工具是Frida，但是它的运行原理涉及调试，因此遇到反调试会相当棘手。由于本人反调试遇到的情况较多，所以Frida不怎么用。

2. 为啥不在thumb-2模式设计时都使用thumb？因为第二部分写汇编的时候用arm写起来容易，而且文中解释过无论跳转前是arm还是thumb模式，跳转后想要用thumb模式都需要给地址+1，所以当然能用arm的地方就用arm，这样方便。并且如果有多个不同模式的Hook目标，这时用户自定义的Hook函数只能统一编译成同一个模式，所以选择ARM模式。


