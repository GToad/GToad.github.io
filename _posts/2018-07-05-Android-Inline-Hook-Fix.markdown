---
layout:     post
title:      "Android Inline Hook中的指令修复详解"
subtitle:   ""
date:       2018-07-13 12:00:00
author:     "GToad"
header-img: "img/post-bg-2017.jpg"
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

> 本文是另一篇[《Android Native Hook工具实践》](/2018/07/06/Android-Native-Hook-Practice/)的一份补充文章，本人建议各位学习者先去看该篇主文以了解本文的需求背景。本文将会对该文中`指令修复`部分的技术细节进行阐述。本文的部分源码来自于ele7enxxh大神的项目，由于大神的技术博客里解释省略了较多细节，所以出于学习的目的，本人也会一点一点慢慢啃。同时由于该库并不完善，因此本文将补充该库没考虑到的一些修复指令方案，并且对许多特殊状况和Bug也尽力解决。

## 前言

目前需要进行指令修复的ARM32指令主要可以分为如下几类：

1. BLX_ARM, BL_ARM, B_ARM（不包含条件跳转）, BX_ARM
2. 条件跳转BEQ_ARM BNE_ARM BCS_ARM BCC_ARM BMI_ARM BPL_ARM BVS_ARM BVC_ARM BHI_ARM BLS_ARM BGE_ARM BLT_ARM BGT_ARM BLE_ARM
3. ADD_ARM
4. ADR_ARM, LDR_ARM, MOV_ARM
5. 其它指令 OTHER_ARM

Thumb16指令主要可以分为如下几类：

1. B1_THUMB16（不包含条件跳转）, B2_THUMB16, BX_THUMB16
2. 条件跳转B1_BEQ_THUMB16, B1_BNE_THUMB16, B1_BCS_THUMB16, B1_BCC_THUMB16, B1_BMI_THUMB16, B1_BPL_THUMB16, B1_BVS_THUMB16, B1_BVC_THUMB16, B1_BHI_THUMB16, B1_BLS_THUMB16, B1_BGE_THUMB16, B1_BLT_THUMB16, B1_BGT_THUMB16, B1_BLE_THUMB16
3. ADD_THUMB16
4. MOV_THUMB16, ADR_THUMB16, LDR_THUMB16
5. 其它指令 OTHER_THUMB16

Thumb32指令主要可以分为如下几类：

1. B1_THUMB32（不包含条件跳转）, B2_THUMB32, BX_THUMB32, BLX_THUMB32, BL_THUMB32
2. 条件跳转B1_BEQ_THUMB32, B1_BNE_THUMB32, B1_BCS_THUMB32, B1_BCC_THUMB32, B1_BMI_THUMB32, B1_BPL_THUMB32, B1_BVS_THUMB32, B1_BVC_THUMB32, B1_BHI_THUMB32, B1_BLS_THUMB32, B1_BGE_THUMB32, B1_BLT_THUMB32, B1_BGT_THUMB32, B1_BLE_THUMB32
3. TBB_THUMB32, TBH_THUMB32
4. ADR1_THUMB32, ADR2_THUMB32, LDR_THUMB32
5. 其它指令 OTHER_THUMB32

下面我来一一分析它们具体的修复方案。其中条件跳转指令先单独拿出来作为一个部分进行讲解。

## 条件跳转




## ARM32

#### BLX, BL, B, BX

修复代码如下：
```c
	if (type == BLX_ARM || type == BL_ARM || type == B_ARM || type == BX_ARM) {
		uint32_t x;
		int top_bit;
		uint32_t imm32;
		uint32_t value;

		if (type == BLX_ARM || type == BL_ARM) {
			trampoline_instructions[trampoline_pos++] = 0xE28FE004;	// ADD LR, PC, #4
		}
		trampoline_instructions[trampoline_pos++] = 0xE51FF004;  	// LDR PC, [PC, #-4]
		if (type == BLX_ARM) {
			x = ((instruction & 0xFFFFFF) << 2) | ((instruction & 0x1000000) >> 23); //BLX_ARM
		}
		else if (type == BL_ARM || type == B_ARM) {
			x = (instruction & 0xFFFFFF) << 2;                                       //BL_ARM B_ARM
		}
		else {
			x = 0;                                                                   //BX_ARM
		}
		
		top_bit = x >> 25;
		imm32 = top_bit ? (x | (0xFFFFFFFF << 26)) : x;
		if (type == BLX_ARM) {
			value = pc + imm32 + 1;
		}
		else {
			value = pc + imm32;
		}
		trampoline_instructions[trampoline_pos++] = value;
		
	}
```

1. 假设原程序实际要跳转到的地址为`addr1`。
2. 由于BLX,BL指令会在跳转前将下一条指令地址保存到LR寄存器中，因此先用`ADD LR, PC, #4`把LR寄存器的值保存下来，这个值是本条指令地址+12 bytes处，即存储`addr1`数据的下一个地址。
3. 用`LDR PC, [PC, #-4]`将PC指向原本要跳转过去的地址`addr1`。
4. 将`addr1`的具体值保存在接下来的4 bytes中。

这个`addr1`的具体值是计算出来的，由于B系列指令使用的是相对地址，因此，需要加上PC的值。

代码中的x的值就是偏移值，它处于指令的后3 bytes，用`instruction & 0xFFFFFF`即可得到。当它为0时，代表当前PC所指向的地址（ARM32下为本条命令+8 bytes）。每当它增加或减少1，就代表地址增加或减少4 bytes，因此代码中使用`<<2`对x的值进行*4。

所以这个x也可能是个负数，因此通过取top_bit来判断当前这个偏移的正负，用imm32代表综合考虑了x、正负、thumb标志的偏移值。

BLX指令需要额外关心跳转后thumb-2模式下的地址，由于thumb模式下地址的最小单位是2 bytes，而不是ARM32下的4 bytes，因此需要通过`instruction & 0x1000000`获得指令第25 bit的标志位，如果标志位为1，地址就需要+2 bytes。

最后，将pc加上imm32就是所需跳转的地址的绝对地址`addr1`。由于BLX需要切换thumb模式，因此我们用`LDR PC, [PC, #-4]`来跳转的时候需要`addr1`额外再+1来切换CPU模式。

#### ADD

修复代码如下：
```c
    else if (type == ADD_ARM) {
		int rd;
		int rm;
		int r;
		
		rd = (instruction & 0xF000) >> 12;
		rm = instruction & 0xF;
		
		for (r = 12; ; --r) { //找一个既不是rm,也不是rd的寄存器
			if (r != rd && r != rm) {
				break;
			}
		}
		
		trampoline_instructions[trampoline_pos++] = 0xE52D0004 | (r << 12);	// PUSH {Rr}
		trampoline_instructions[trampoline_pos++] = 0xE59F0008 | (r << 12);	// LDR Rr, [PC, #8]
		trampoline_instructions[trampoline_pos++] = (instruction & 0xFFF0FFFF) | (r << 16);
		trampoline_instructions[trampoline_pos++] = 0xE49D0004 | (r << 12);	// POP {Rr}
		trampoline_instructions[trampoline_pos++] = 0xE28FF000;	// ADD PC, PC MFK!这明明是ADD PC, PC, #0好么！
		trampoline_instructions[trampoline_pos++] = pc;
	}
```
ADD指令可能会涉及到PC，PC就是R15寄存器，用到它的时候它会位于该指令的[16:19]bit上，所以要找个别的寄存器存着原来程序PC的值代替这里的R15即可。

本指令会用到位于指令[12:15]和[0:3]bit上的两个寄存器rd和rm，这两个在修复过程中要避免改变。

1. 从前12个寄存器中找一个既不是rd也不是rm的寄存器Rr就行，把它pop到栈上保存当前的值。
2. 用`LDR Rr, [PC, #8]`把修复代码尾部保存的PC的值赋给Rr。
3. 用`(instruction & 0xFFF0FFFF) | (r << 16);`把Rr代替掉原来[16:19]bit上的PC，这样这条ADD执行后就是原本程序中的结果。
4. Rr的作用已经用完了，那就把它保存在栈上原来的值pop出来恢复它。
5. 用`ADD PC, PC, #0`来跳过下面尾部的pc数据，去继续执行pc值下方的代码。
6. 用4 bytes保存了原本程序中的pc值。

#### ADR, LDR, MOV

修复代码如下：
```c
    else if (type == ADR1_ARM || type == ADR2_ARM || type == LDR_ARM || type == MOV_ARM) {
		int r;
		uint32_t value;
		
		r = (instruction & 0xF000) >> 12;
		
		if (type == ADR1_ARM || type == ADR2_ARM || type == LDR_ARM) {
			uint32_t imm32;
			
			imm32 = instruction & 0xFFF;
			if (type == ADR1_ARM) {
				value = pc + imm32;
			}
			else if (type == ADR2_ARM) {
				value = pc - imm32;
			}
			else if (type == LDR_ARM) {
				int is_add;
				
				is_add = (instruction & 0x800000) >> 23;
				if (is_add) {
					value = ((uint32_t *) (pc + imm32))[0];
				}
				else {
					value = ((uint32_t *) (pc - imm32))[0];
				}
			}
		}
		else {
			value = pc;
		}
			
		trampoline_instructions[trampoline_pos++] = 0xE51F0000 | (r << 12);	// LDR Rr, [PC]
		trampoline_instructions[trampoline_pos++] = 0xE28FF000;	// ADD PC, PC
		trampoline_instructions[trampoline_pos++] = value;
	}
```

1. 用`(instruction & 0xF000) >> 12`来获取命令中需要用到的寄存器Rr。
2. 由于这三个指令用的也是相对地址，所以用imm32获取指令[11:0]bit的值后，直接和pc进行加减。由于ADD和LDR涉及偏移的正负性，所以额外寻找各自的标志位来确认是用原pc值加还是减。
3. 用`LDR Rr, [PC]`将上一步计算得到的value的值赋给Rr。
4. 用`ADD PC, PC, #0`命令来跳过下方存储的value数据，从value后面的指令开始继续执行。

#### 其它指令

ARM32下的其它指令无需修复，直接在备份代码中使用即可。

## Thumb32

#### B, BX, BLX, BL

#### TBB, TBH

#### ADR, LDR

#### 其它指令

Thumb32下的其它指令无需修复，直接在备份代码中使用即可。

## 局限性思考

本项目虽然尽可能多地考虑了各种指令与情况的修复方案，但是依然如下情况下可能对使用者造成问题。但是本人认为这些问题都是可以通过正确的使用方法来规避的。因此这部分可能更像是个使用指导。以下内容可能比较钻牛角尖，考虑的都是一些脑补的极端情况：

1. `备份之外的程序其它指令需要跳转到备份指令范围内`
2. `Hook在函数最最最末尾`

#### 局限1分析

这个情况理论上存在但`非常少见`且`可以避免`。因为备份指令的范围其实非常小，Arm下只有2条指令8 Bytes的大小，Thumb-2下也只有10/12 Bytes，对于有成千上万条指令的原程序来说概率非常小！并且这些备份的空间不是都会出问题。

###### 概率小

`对于Arm来说`：第一个4 Bytes上覆盖的是LDR跳转指令，第二个4 Bytes是地址，因此跳转到第一个LDR指令上是完全没有问题的，就按照Hook流程运行而已，只有正好跳转到第二个地址处才会出现将数据误以为是指令的错误。

`对于Thumb-2来说`：跳转到Hook地址开头可能的Nop或LDR指令也是完全没有问题的，并且12/10Bytes下跳转到最后的4/2 Bytes中也是没有问题的，因为那里并没有被覆盖指令。依然仅仅是直接跳转到那个4 Bytes的地址才出问题。当然Thumb-2模式下可能出现最后的2 Bytes是被截断的Thumb32位的后16位的可能性（它的前16位被跳转地址的后16位覆盖了）。

###### 可以避免

在使用Hook工具前肯定是会逆向分析的。要避免这种情况最简单的方法就是不要让自己的备份代码“跨代码块”。什么意思？看下图即可理解：

（贴一张有loc小标签的图，别太密集，有一个就行，否则读者会觉得这样的情景很多）

上图中是我们使用IDA等逆向分析工具时的界面，可以看到其中有一个个小标签，这些小标签就是可能被跳转的目标。因此当我们的备份范围没有跨越两个或多个小标签的时候就不会出现其它指令跳转到我们插桩代码中间的跳转地址处的情况了。

#### 使用指导1

不要“跨代码块”进行Hook。

#### 局限2分析

这个情况与其说是局限，倒更应当是个使用指导上的说明：有使用者可能会去用本工具Hook某函数的末尾从而希望获取返回值。于是将目标地址对准了那个函数最后的retn指令上。我个人不建议这么做，而是建议Hook在它的上层调用函数中，也就是调用这个函数后的语句上来获取返回值。

如果真的遇到了不得不Hook在函数末尾附近的情况，个人认为也是很好解决的：

`对于Arm来说`：可以Hook在retn前的一条指令A上，这样就是备份这条指令A和retn，然后用本工具输出全部的寄存器状态看看。使用者完全可以脑算出这一条指令A后retn到底会是什么状态。

`对于Thumb-2来说`：同理，可以Hook在retn前的几条指令上，然后根据几条指令前的寄存器状态推算一下之后retn的状态。

###### 使用指导2

想得到函数的返回值应该去调用它的函数里Hook，而不是在目标函数末尾。实在有特殊情况的话根据本工具的Hook信息依然可以很轻松推算出返回值。


