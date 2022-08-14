#ifndef __KOOH_H__
#define __KOOH_H__

#include <linux/ftrace.h>
#include <linux/kernel.h>
#include <linux/linkage.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/kprobes.h>
#include <linux/kallsyms.h>


/*
 * struct ftrace_hook - describe a hook.
 *
 * public:
 * @name: name of the function to hook
 * @function: pointer to the new function
 * @original: pointer to the original function
 *
 * private:
 * @address: kernel address of the function entry
 * @ops: ftrace_ops state of this hook
 *
 * Only @name, @hook, @original fields are needed from user.
 */

struct ftrace_hook {
  const char *name;
  void *function;
  void *original;

  unsigned long address;
  struct ftrace_ops ops;
};

int kh_install_hook(struct ftrace_hook* hook);
int kh_install_hooks(struct ftrace_hook* hook, size_t count);
void kh_remove_hook(struct ftrace_hook* hook);
void kh_remove_hooks(struct ftrace_hook* hook, size_t count);

unsigned long lookup_name(const char *name);

#ifndef CONFIG_X86_64
#error Only x86_64 is supported
#endif

/*
 * - USE_FENTRY_OFFSET = 0 detect recursion using function return address
 * - USE_FENTRY_OFFSET = 1 jumping over the frace call
 */
#define USE_FENTRY_OFFSET 0

#if !USE_FENTRY_OFFSET
#pragma GCC optimize("-fno-optimize-sibling-calls")
#endif

#endif
