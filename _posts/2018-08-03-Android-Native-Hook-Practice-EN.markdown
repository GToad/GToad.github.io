---
layout:     post
title:      "Android Native Hook Practice EN"
subtitle:   ""
date:       2018-08-03 10:00:00
author:     "GToad"
header-img: "img/post-bg-2018.jpg"
catalog: true
tags:
    - Android
    - NDK
    - Hook
    - inline hook
    - native hook
    - LTS
---

> This is just some part of the article [Android Native Hook工具实践](https://gtoad.github.io/2018/07/06/Android-Native-Hook-Practice/). I translate it in English because there are several e-mails in English sent to me with questions I've written in the Chinese article.

> I wish I can finish this in the future, I will explain some important part of the project.

## How to fix the opcode?

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
