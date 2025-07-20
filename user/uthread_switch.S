# 声明 .text 段，表示这是可执行代码
.text

# 声明 thread_switch 为全局符号，这样C代码才能链接和调用它
.globl thread_switch

# thread_switch(old_context, new_context)
# 函数的入口点。按照RISC-V调用约定：
# a0 寄存器（第一个参数）: 指向旧线程 context 结构体的指针
# a1 寄存器（第二个参数）: 指向新线程 context 结构体的指针
thread_switch:
    # --- 保存旧线程的上下文 ---
    # 将 callee-saved 寄存器保存到 a0 指向的 old_context 中。
    # sd (store doubleword) 指令将一个64位寄存器的值存入内存。
    # 偏移量(0, 8, 16, ...)严格对应于我们在 C 代码中定义的 struct context 成员的顺序。
    
    sd ra, 0(a0)      # 保存返回地址 (return address)
    sd sp, 8(a0)      # 保存栈指针 (stack pointer)
    sd s0, 16(a0)     # 保存 callee-saved 寄存器 s0
    sd s1, 24(a0)     # 保存 s1
    sd s2, 32(a0)     # 保存 s2
    sd s3, 40(a0)     # 保存 s3
    sd s4, 48(a0)     # 保存 s4
    sd s5, 56(a0)     # 保存 s5
    sd s6, 64(a0)     # 保存 s6
    sd s7, 72(a0)     # 保存 s7
    sd s8, 80(a0)     # 保存 s8
    sd s9, 88(a0)     # 保存 s9
    sd s10, 96(a0)    # 保存 s10
    sd s11, 104(a0)   # 保存 s11

    # --- 恢复新线程的上下文 ---
    # 从 a1 指向的 new_context 中加载 callee-saved 寄存器。
    # ld (load doubleword) 指令从内存中读取一个64位的值到寄存器。
    
    ld ra, 0(a1)      # 恢复返回地址
    ld sp, 8(a1)      # 恢复栈指针
    ld s0, 16(a1)     # 恢复 s0
    ld s1, 24(a1)     # 恢复 s1
    ld s2, 32(a1)     # 恢复 s2
    ld s3, 40(a1)     # 恢复 s3
    ld s4, 48(a1)     # 恢复 s4
    ld s5, 56(a1)     # 恢复 s5
    ld s6, 64(a1)     # 恢复 s6
    ld s7, 72(a1)     # 恢复 s7
    ld s8, 80(a1)     # 恢复 s8
    ld s9, 88(a1)     # 恢复 s9
    ld s10, 96(a1)    # 恢复 s10
    ld s11, 104(a1)   # 恢复 s11

    # 执行返回指令。
    # 这条指令会跳转到 ra 寄存器中存储的地址。
    # 因为 ra 刚刚从新线程的上下文中恢复，所以程序会跳转到新线程
    # 上次被中断的地方，或者对于一个新创建的线程，跳转到它的启动函数。
    ret
