/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2018 Kalray Inc.
 */

#ifndef _ASM_KVX_SYSCALL_H
#define _ASM_KVX_SYSCALL_H

#include <linux/err.h>
#include <linux/audit.h>

#include <asm/ptrace.h>

/* The array of function pointers for syscalls. */
extern void *sys_call_table[];

void scall_machine_exit(unsigned char value);

/**
 * syscall_get_nr - find what system call a task is executing
 * @task:	task of interest, must be blocked
 * @regs:	task_pt_regs() of @task
 *
 * If @task is executing a system call or is at system call
 * tracing about to attempt one, returns the system call number.
 * If @task is not executing a system call, i.e. it's blocked
 * inside the kernel for a fault or signal, returns -1.
 *
 * Note this returns int even on 64-bit machines.  Only 32 bits of
 * system call number can be meaningful.  If the actual arch value
 * is 64 bits, this truncates to 32 bits so 0xffffffff means -1.
 *
 * It's only valid to call this when @task is known to be blocked.
 */
static inline int syscall_get_nr(struct task_struct *task, struct pt_regs *regs)
{
	if (!in_syscall(regs))
		return -1;

	return es_sysno(regs);
}

static inline long syscall_get_error(struct task_struct *task,
				     struct pt_regs *regs)
{
	/* 0 if syscall succeeded, otherwise -Errorcode */
	return IS_ERR_VALUE(regs->r0) ? regs->r0 : 0;
}

static inline long syscall_get_return_value(struct task_struct *task,
					    struct pt_regs *regs)
{
	return regs->r0;
}

static inline int syscall_get_arch(struct task_struct *task)
{
	return AUDIT_ARCH_KVX;
}

static inline void syscall_get_arguments(struct task_struct *task,
					 struct pt_regs *regs,
					 unsigned long *args)
{
	args[0] = regs->orig_r0;
	args++;
	memcpy(args, &regs->r1, 5 * sizeof(args[0]));
}

int __init setup_syscall_sigreturn_page(void *sigpage_addr);

#endif
