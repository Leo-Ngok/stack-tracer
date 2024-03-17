# A novel C stack tracer
## Preresquisites
- x86-64 (AMD64) system
- Linux
- GCC compiler
## Usage
``` c
foo() {
    // ...
    // call_depth is # of functions called from 
    // main + 2 (_start and libc_start_main)
    int call_depth = 100;
    print_stackframe(call_depth);
    // ...
}
```

## Compilation and Assembling
Do the following for object file
```
gcc -O2 -Wall stack_tracer.c -c stack_tracer.o
```

link it to your project.
```
$(YOUR_LINKER) $(OBJS) stack_tracer.o
```
## Execution
```
Be warned that do not use this fn in production environment, or else stack contents would be leaked and security would be compromised.
+-----------------------------
| Entry: 0x55b514181750
| Address to return: 0x55b51418176e
| Stack pointer: 0x7fff76b93d70
| Base pointer: 0x7fff76b93d78
| Description: ./trace(+0x176e) [0x55b51418176e]
+-----------------------------
+-----------------------------
| Entry: 0x55b514181790
| Address to return: 0x55b5141817ab
| Stack pointer: 0x7fff76b93d80
| Base pointer: 0x7fff76b93d88
| Description: ./trace(+0x17ab) [0x55b5141817ab]
+-----------------------------
+-----------------------------
| Entry: 0x55b514181180
| Address to return: 0x55b5141811ae
| Stack pointer: 0x7fff76b93d90
| Base pointer: 0x7fff76b93d98
| Description: ./trace(+0x11ae) [0x55b5141811ae]
+-----------------------------
+-----------------------------
| Entry: ***
| Address to return: *** (return address of main, in libc-start_main)
| Stack pointer: 0x7fff76b93da0
| Base pointer: 0x7fff76b93e68
| Description: /lib/x86_64-linux-gnu/libc.so.6(__libc_start_main+0x***) [***]
+-----------------------------
+-----------------------------
| Entry: 0x55b5141811e0
| Address to return: 0x55b51418120e
| Stack pointer: 0x7fff76b93e70
| Base pointer: (nil)
| Description: ./trace(+0x120e) [0x55b51418120e]
+-----------------------------
```
NOTE: Addresses of libc is masked, for security purposes. These are not masked when you use it.
## How it works.

实现原理： 利用 GCC 提供的 `execinfo` 获取一系列的函数返回地址。

与此同时，在函数内部获取栈顶指针，遍历调用栈，通过与返回地址比对，确定栈帧的边界，直到获取 `_start` 函数的栈帧为止。

接下来利用返回地址，对途径的代码向后遍历，直至扫描到 `endbr64` 指令。该指令是 Intel 为了减缓返回定向攻击(return oriented programming)、控制流定向攻击 (control oriented programming) 而设计的，是 CET 技术的一部分，在函数入口设置，用以核对`call` 指令。

当前实现依赖 `execinfo` 库实现，其原理是利用 `DWARF`规范定义下的 `.eh_frame` 和 `.eh_frame_hdr` 段，获取 CFI (call frame information), 特别是 CIE (Common Information Entry) 和 FDE(Frame Description Entry)，可以追踪栈帧的变化。

利用 GCC 工具编译(注意不用汇编)，观察其汇编文件，出现 `.cfi` 开首的标记，即为这些项目的标注，汇编器可以对此生成 CFI 表项。

由于其实现原理比较复杂，因此被列为扩展部分，暂不实现。

## Credit

https://lesenechal.fr/en/linux/unwinding-the-stack-the-hard-way
https://zhuanlan.zhihu.com/p/658352901
https://codebrowser.dev/glibc/glibc/debug/backtrace.c.html
https://zhuanlan.zhihu.com/p/507138494
https://www.cnblogs.com/mickole/p/3246702.html

## TODOS.
- FFI for rust and other compiled languages.
- PDB support for msvc projects.
- Native support to DWARF.
- Support to other architectures.