---
layout:     post
title:      "Android APP逆向分析基础"
subtitle:   ""
date:       2019-10-29 9:00:00
author:     "GToad"
header-img: "img/android-four.jpg"
catalog: true
tags:
    - Android
    - Reverse Engineering
    - APP analyse
---

> 最近在看雪CTF上看到一个比较综合的安卓逆向题目，非常适合用来当入门案例，因此写一下这篇基础教程。

## 前言

本文主要通过一个Android应用逆向案例来讲解Android APP逆向分析的大致流程。对于Android APP的一些网络功能测试和加壳脱壳相关内容则并不会涉及。主要是捋一下安卓在IDA中调试的一个基础过程。

## 分析对象

常见Android APP中主要是包含两种编程语言：`Java`和`JNI`，对于Java代码的逆向主要步骤就是把`.apk`文件中的`DEX`文件进行逆向反编译，常用的工具有jd-gui、jadx-gui等，通常我用[jadx-gui](https://github.com/skylot/jadx)。而对于JNI代码则保存在.so文件中，这类文件就是通常Linux中的ELF文件，内含机器码使用IDA对其进行逆向分析。

本文中讲解的案例题目可以点击这里[下载](https://github.com/GToad/GToad.github.io/releases/download/AndroidAPPBase/Transformers.apk)，使用adb shell命令将其安装到一部测试机上运行后如下图所示：
![](/img/in-post/post-android-app-base/test.png)


界面上就下方那个输入框可以输入字符，上面的那个“13909876543”无法改变编辑。随意输入了“12345678”后点击登陆，界面上返回的是“error”消息。很明显，这又是个输入指定字符串的典型crackme题目。

## Java层分析

在了解了基本功能后，便先使用jadx-gui对目标应用进行Java代码的反编译。反编译后当然是直接去看看MainActivity里的代码情况，主要代码如下图所示：
![](/img/in-post/post-android-app-base/mainactivity.png)

观察这个代码发现的确是一个验证我们输入的逻辑，但是非常奇怪，代码中对于输入错误后的错误提示与我们之前实际运行情况并不相符，实际运行中的反馈为“error”，而这段代码中则是“登陆失败”，这肯定不会是Android系统自动帮我们翻译的，那么这里的代码就存在了一定的问题。通过仔细观察发现了端倪，这个MainActivity的父类并不是通常的`AppCompatActivity`，而是`AppCompiatActivity`，而这就是本次案例的第一个陷阱所在，我们根据路径去查看这个`AppCompiatActivity`，发现其代码如下：
![](/img/in-post/post-android-app-base/compiat.png)

主要逻辑都在onStart中，该函数中的逻辑也是将我们输入的密码进行判断，如果验证成功就打印flag，验证失败就打印“error”。因此基本可以确定这里的代码逻辑才是我们真正需要去分析的。那么，对于这部分代码我们进行进一步的分析，发现它把我们输入的password送入了一个叫做eq()的函数中进行校验。而该eq函数是一个native属性的函数，说明它是由JNI代码提供。于是便查看该类用System.loadLibrary()方法加载了哪些JNI库，如下图所示：
![](/img/in-post/post-android-app-base/loadlibrary.png)

从上图可知，此处加载的是`liboo000oo.so`，于是使用`apktool`工具的d参数对目标apk进行拆包，从拆包后的目录的lib中找到了ARMv7架构的liboo000oo.so文件，将该文件拖入IDA中进行分析。

## JNI层分析

将这个.so文件拖入IDA后，我们自然就是想要找到eq函数的位置，但是通过IDA分析后，并没有发现函数名中带有“eq”的函数，这时候，我们就自然想到了一个常见的套路：JNI动态注册。这个套路就是使用RegisterNatives方法对函数进行动态的注册，从而使得函数名称可以被隐藏。而通常，这个套路是在JNI_Onload函数中进行注册，因为这个函数是每次.so被加载时都会自动执行的一个函数，在该函数内将eq函数的函数代码与“eq”的名称动态注册后，接下来的Java代码层便可以使用eq函数了。

因此，我们在IDA中跳转到了JNI_Onload函数处进行查看，F5插件反编译后的代码如下如所示。
![](/img/in-post/post-android-app-base/jnionload.png)

此处我们没有发现调用RegisterNtives的代码，其实并不是这样，我们发现其中部分变量总是会加上一个很大的offset进行使用，这些变量的类型其实并不是IDA反编译出来的int类型，而是JNIEnv*类型，因此，我们可以通过选中这些变量然后按Y键来改变变量解析的类型，如下图所示：
![](/img/in-post/post-android-app-base/jnienv.png)

修改类型后，IDA会自动重新解析这些offset的具体含义，这时候我们便可以从反编译的代码中发现RegisterNatives方法了。如下图所示：
![](/img/in-post/post-android-app-base/jnionloadenv.png)

接下来，我们需要查看这个RegisterNatives函数的具体输入参数才能够找出eq函数真正的函数代码。该函数的参数中有一个叫做`gMethods[]`的数据参数，该参数的第一个元素是需要注册的函数名，第二个是类型，第三个是实际函数的地址。于是我们代码中可以查看代表这个gMethods[]的`“off_4014”`参数，发现在静态情况下该参数的内存数据还未到位，因此我们需要使用动态调试的方法来看看该参数的具体内容。

## JNI调试

想要对一个Android APP的.so库代码进行IDA调试的话首先先要把该IDA安装目录下的dbgsrv中的android_server发送到测试的手机上。这里我们将该文件存放到手机的`/system/bin`目录下，并给予执行权限。然后在adb shell下运行android_server，该程序和其它dbgsrv一样会占用23946端口。如下图所示：
![](/img/in-post/post-android-app-base/androidserver.png)

接着我们在一个新的终端下使用命令`adb forward tcp:23946 tcp:23946`，该命令可以使当前测试手机上的23946调试端口映射到我们电脑的23946端口上。此时我们在IDA中便可以通过配置`IP为127.0.0.1，端口为23946`来使得调试器对当前测试手机可以进行远程调试。如下图所示：
![](/img/in-post/post-android-app-base/adbforward.png)

接下来我们在终端中继续执行命令`adb shell am start -D -n com.zhuotong.crackme/com.zhuotong.crackme.MainActivity`，此处会打开目标APP的MainActivity并等待调试接入。如下图所示：
![](/img/in-post/post-android-app-base/waitdebug.png)

最后，我们用IDA的`Attach to process`功能对目标进程进行连接，此时如下图所示，可以看到目标安卓测试手机上的所有进程，通过`Ctrl+F`快捷键可以查找到当前我们需要的进程。
![](/img/in-post/post-android-app-base/attach.png)

Attach后，我们打开Android DDMS，然后在终端中运行命令`jdb -connect com.sun.jdi.SocketAttach:hostname=127.0.0.1,port=8700`。此时IDA便可以进行远程调试了，通过JNI_Onload方法中的断点，我们可以查看RegisterNatives的参数情况，如下图所示：
![](/img/in-post/post-android-app-base/ondebug.png)

我们可以看到，第一个参数是字符串`“eq”`，第二个参数是字符串`“(Ljava/lang/String;)Z”`，第三个参数是一个地址，指向了sub_B4C8D784函数。于是该函数就是eq函数真正的代码位置。
![](/img/in-post/post-android-app-base/gmethod.png)

对该函数进行反编译后下断点，在我们输入`12345678`并点击登陆按钮后，程序流程成功执行到该断点。至此我们就确定了本题中的关键校验函数代码逻辑，可点击[这里](https://github.com/GToad/GToad.github.io/releases/download/AndroidAPPBase/eq.c)下载C文件。

## RC4加密算法

通过逆向分析可以发现，我们输入的字符串在进行一系列简单的过滤检查后，将会执行一个加密算法，主要有如下特征：

![](/img/in-post/post-android-app-base/base1.png)

1. 将一个字符串重复延长至256Byte。如上图所示。
2. 在内存中初始化了一块256Byte的内存，其中包含0x00-0xFF的不重复数据，如下图所示。
3. 运算的核心算法为`v35 = (unsigned __int8)v48[(unsigned __int8)(v34 + v48[v26])] ^ (unsigned __int8)s[v27];`


![](/img/in-post/post-android-app-base/base2.png)

由此我们可以判断出当前的加密算法应该是一个RC4加密算法，它的key为`“36f36b3c-a03e-4996-8759-8408e626c215”`。它有一个如上图中自定义的初始化BOX，因此，这个RC4算法需要我们额外进行解密脚本的编写，它与网上别的RC4代码中的BOX内容不同。

## BASE64加密算法

通过进一步的逆向，我们发现，流式加密算法RC4产生的密文内容每凑到3个BYTE了以后就会被传入下一个加密算法中进行计算，该加密算法也呈现出一些特征：

1. 输入为3个Byte，输出为4个Byte。
2. 内存中出现了一个65个可见字符组成的表，如下图所示。
3. 大量使用移位运算，如>>2，>>4，>>6。

![](/img/in-post/post-android-app-base/base3.png)

因此，我们可以很容易联想到这是一个Base64算法，但是其可见字符表不再是"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/="，而是"!:#$%&()+-*/`~_[]{}?<>,.@^abcdefghijklmnopqrstuvwxyz0123456789\\';"。

## 解密思路

在Base64加密算法阶段的末尾找到了一些异或运算。因此，我们可以概括出本题对input的加密流程如下图所示：
![](/img/in-post/post-android-app-base/base4.png)

那么，解密的思路就是反过来：
![](/img/in-post/post-android-app-base/base5.png)

## 解密脚本

根据上文中的解密思路与关键参数，我们便可以写出解密脚本，我用了python进行编写：

```javascript
import string

def b64dec(s):
    ts = ""
    for i in xrange(len(s)):
        if s[i] == ";":
            ts += ";"
        elif i % 4 == 0:
            ts += chr(ord(s[i]) ^ 7)
        elif i % 4 == 2:
            ts += chr(ord(s[i]) ^ 0xf)
        else:
            ts += s[i]
    print(ts)
 
    tab = string.maketrans("!:#$%&()+-*/`~_[]{}?<>,.@^abcdefghijklmnopqrstuvwxyz0123456789\\';", "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=")
    tmp = ts.translate(tab)
    print(tmp)
    return tmp.decode("base64")

step1 = b64dec("207B392A3867612A6C21546E3F4023666A276A245C673B3B".decode("hex"))

def rc4(key, txt):
    box = bytearray("D7DF02D4FE6F533C256C999706568FDE40116407361570CA18177D6ADB1330372960E123288A508CAC2F8820270F7C52A2ABFCA1CC21141FC2B28B2CB03A66463DBB42A50C7522D8C3761E8374F0F61C26D14F0BFF4C4DC187035AEEA45D9EF4C80D62633E447BA368321BAA2D05F3F7166194E0D0D3986978E90A65918E35857A5186103F7F82DDB51A95E743FD9B2445EF925CE496A99C55899AEAF9905FB80484CF679300A639A84E59316BAD5E5B77B154DC3841B6479F73BAF8AEC4BE34014B2A8DBDC5C6E8AFC9F5CBFBCD79CE1271D2FA09D5BC581980DA491DE62EE37EB73BB3A0B9E5576ED908EBC7ED81F1F2BFC0A74AD62BB4729D0E6DEC48E233".decode("hex"))
 
    key_len = len(key)
    j = 0
    for i in range(256):
        j = (j + box[i] + ord(key[i %key_len])) %256
        box[i], box[j] = box[j], box[i]
 
    result = ""
    i = j = 0
    for e in txt:
        i = (i+1) %256
        j = (j + box[i]) %256
        box[i], box[j] = box[j], box[i]
        result += chr(ord(e) ^ box[(box[i] + box[j]) %256])
 
    return result
 
print(rc4("36f36b3c-a03e-4996-8759-8408e626c215", step1))

```
最终运行结果为：
![](/img/in-post/post-android-app-base/base6.png)

把这串字符串输入测试手机，验证正确：
![](/img/in-post/post-android-app-base/base7.png)







