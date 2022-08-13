#include "kooh.h"

static asmlinkage long (*real_sys_clone)(struct pt_regs *regs);

static asmlinkage long fh_sys_clone(struct pt_regs *regs)
{
	long ret;

	pr_info("bpf() before\n");

	ret = real_sys_clone(regs);

	pr_info("bpf() after: %ld\n", ret);

	return ret;
}

static struct ftrace_hook test_hooks[] = {
  { "__x64_sys_bpf", fh_sys_clone, &real_sys_clone },
};


static int fh_init(void)
{
	int err;

	err = kh_install_hooks(test_hooks, ARRAY_SIZE(test_hooks));
	if (err)
		return err;

	pr_info("module loaded\n");

	return 0;
}
module_init(fh_init);

static void fh_exit(void)
{
	kh_remove_hooks(test_hooks, ARRAY_SIZE(test_hooks));

	pr_info("module unloaded\n");
}
module_exit(fh_exit);
