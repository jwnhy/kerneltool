#include "kooh.h"
#include <linux/bpf.h>

MODULE_DESCRIPTION("Kernel Module Hooking via ftrace");
MODULE_AUTHOR("jwnhy <jwnhy0@gmail.com");
MODULE_LICENSE("GPL");

static long (*real_bpf_prog_run)(u64 *regs, const struct bpf_insn *insn);

static long kh_bpf_prog_run(u64 *regs, const struct bpf_insn *insn)
{
	long ret;

	pr_info("bpf() before\n");

	ret = real_bpf_prog_run(regs, insn);

	pr_info("bpf() after: %ld\n", ret);

	return ret;
}

static struct ftrace_hook test_hooks[] = {
  { "___bpf_prog_run", kh_bpf_prog_run, &real_bpf_prog_run},
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
