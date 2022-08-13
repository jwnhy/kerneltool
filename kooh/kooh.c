#include "kooh.h"


/*
 * In Linux 5.7 and after, the kallsyms_lookup_name is no longer exported.
 * We use kprobe to get its address then use it as usual.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 7, 0)
unsigned long lookup_name(const char *name) {
  return kallsyms_lookup_name(name);
}
#else
struct kprobe kp = {.symbol_name = "kallsyms_lookup_name"};
typedef unsigned long (*kallsyms_lookup_name_t)(const char *name);
kallsyms_lookup_name_t kallsyms_lookup_name_addr = NULL;
unsigned long lookup_name(const char *name) {
  if (!kallsyms_lookup_name_addr) {
    register_kprobe(&kp);
    kallsyms_lookup_name_addr = (kallsyms_lookup_name_t)kp.addr;
    unregister_kprobe(&kp);
  } 
  return kallsyms_lookup_name_addr(name);
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 11, 0)
#define FTRACE_OPS_FL_RECURSION FTRACE_OPS_FL_RECURSION_SAFE

#define frace_regs pt_regs
__always_inline struct pt_regs *
ftrace_get_regs(struct frace_regs *fregs) {
  return fregs;
}
#endif

int kh_resolve_hook_address(struct ftrace_hook *hook) {
  hook->address = lookup_name(hook->name);

  if (!hook->address) {
    pr_debug("unresolved symbol: %s\n", hook->name);
    return -ENOENT;
  }
#if USE_FENTRY_OFFSET
  *((unsigned long *)hook->original) = hook->address + MCOUNT_INSN_SIZE;
#else
  *((unsigned long *)hook->original) = hook->address;
#endif
  return 0;
}

void notrace kh_ftrace_thunk(unsigned long ip, unsigned long parent_ip,
                                    struct ftrace_ops *ops,
                                    struct ftrace_regs *fregs) {
  struct pt_regs *regs = ftrace_get_regs(fregs);
  struct ftrace_hook *hook = container_of(ops, struct ftrace_hook, ops);
#if USE_FENTRY_OFFSET
  regs->ip = (unsigned long)hook->function;
#else
  if (!within_module(parent_ip, THIS_MODULE))
    regs->ip = (unsigned long)hook->function;
#endif
}

/*
 * kh_install_hook() - register and enable a hoook
 * @hook: the hook to install
 *
 * return:
 * zero on success
 * negative error code otherwise
 */

int kh_install_hook(struct ftrace_hook *hook) {
  int err;
  err = kh_resolve_hook_address(hook);
  if (err)
    return err;

  /*
   * Changing %rip: IPMODIFY | SAVE_REGS
   * Our own anti-recursion: RECURSION
   */
  hook->ops.func = kh_ftrace_thunk;
  hook->ops.flags = FTRACE_OPS_FL_SAVE_REGS | FTRACE_OPS_FL_IPMODIFY |
                    FTRACE_OPS_FL_RECURSION;

  err = ftrace_set_filter_ip(&hook->ops, hook->address, 0, 0);
  if (err) {
    pr_debug("ftrace_set_filter_ip() failed: %d\n", err);
    return err;
  }

  err = register_ftrace_function(&hook->ops);
  if (err) {
    pr_debug("register_ftrace_function() failed: %d\n", err);
    ftrace_set_filter_ip(&hook->ops, hook->address, 1, 0);
    return err;
  }
  return 0;
}

/*
 * kh_remove_hook() disable and unregister a hook
 * @hook: the hook to remove
 */

void kh_remove_hook(struct ftrace_hook *hook) {
  int err;
  err = unregister_ftrace_function(&hook->ops);
  if (err) {
    pr_debug("unregister_ftrace_function() failed: %d\n", err);
  }

  err = ftrace_set_filter_ip(&hook->ops, hook->address, 1, 0);
  if (err) {
    pr_debug("ftrace_set_filter_ip() failed: %d\n", err);
  }
}

/*
 * kh_install_hooks() - register and enable multiple hooks
 * @hooks: array of hooks to install
 * @count: #hooks to install
 * return:
 * zero on success
 * negative error code otherwise
 */
int kh_install_hooks(struct ftrace_hook *hooks, size_t count) {
  int err;
  size_t i;

  for (i = 0; i < count; i++) {
    err = kh_install_hook(&hooks[i]);
    if (err)
      goto error;
  }
  return 0;

error:
  while (i) {
    kh_remove_hook(&hooks[--i]);
  }
  return err;
}

/**
 * fh_remove_hooks() - disable and unregister multiple hooks
 * @hooks: array of hooks to remove
 * @count: number of hooks to remove
 */
void kh_remove_hooks(struct ftrace_hook *hooks, size_t count) {
  size_t i;

  for (i = 0; i < count; i++)
    kh_remove_hook(&hooks[i]);
}
