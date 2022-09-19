#include <linux/module.h>   /* Needed by all modules */
#include <linux/kernel.h>   /* Needed for KERN_INFO */
#include <linux/fs.h>   /* Needed for KERN_INFO */
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/smp.h>
#include <linux/major.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cpu.h>
#include <linux/notifier.h>
#include <linux/uaccess.h>
#include <linux/gfp.h>
#include <asm/processor.h>
#include <asm/msr.h>

#define MAKE_MINOR(cpu, reg) (cpu<<8 | reg)
#define GET_MINOR_REG(minor) (minor & 0xff)
#define GET_MINOR_CPU(minor) (minor >> 8)
#define XCR_MINOR_BASE  0x80 

static int major_n = 0;
static struct class *ctrlreg_class;


struct ctrlreg_info
{
    unsigned int reg;
    unsigned long value;
    unsigned long error;
};

static void ctrlreg_smp_do_read(void* p)
{
    struct ctrlreg_info* info = p;
    info->error = 0;

    printk(KERN_INFO "ctrlreg: do read of reg%u\n", info->reg);

    switch (info->reg)
    {
        case 0: info->value = read_cr0(); break;
        case 2: info->value = read_cr2(); break;
        case 3: info->value = native_read_cr3_pa(); break;
        case 4: info->value = native_read_cr4(); break;


        default:
            info->error =  -EINVAL;
    }   
}

static void ctrlreg_smp_do_write(void* p)
{
    struct ctrlreg_info* info = p;
    info->error = 0;

    switch (info->reg)
    {
        case 0: write_cr0(info->value); break;
        case 2: write_cr2(info->value); break;
        case 3: write_cr3(info->value); break;
        #ifdef CONFIG_LKTDM
        case 4: native_write_cr4(info->value); break;
        #endif

        default:
            info->error =  -EINVAL;
    }   
}


static ssize_t ctrlreg_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    unsigned int minor = iminor(file_inode(file));
    unsigned int cpu = GET_MINOR_CPU(minor);
    unsigned int reg = GET_MINOR_REG(minor);
    struct ctrlreg_info info = {.reg = reg};
    unsigned long err;

    printk(KERN_INFO "ctrlreg: read for cpu%u reg%u\n", cpu, reg);
    printk(KERN_INFO "ctrlreg: read of %zu bytes\n", count);

    if (count < sizeof(unsigned long))
        return -EINVAL;

    printk(KERN_INFO "ctrlreg: scheduling read\n");

    err = smp_call_function_single(cpu, ctrlreg_smp_do_read, &info, 1);
    if (IS_ERR_VALUE(err))
        return err;

    printk(KERN_INFO "ctrlreg: read success: %x\n", info.error);

    if (IS_ERR_VALUE(info.error))
        return err;

    err = copy_to_user(buf, &info.value, sizeof(unsigned long));

    printk(KERN_INFO "ctrlreg: read copy result: %x ( %lu )\n", err, sizeof(unsigned long));

    if (IS_ERR_VALUE(err))
        return err;

    printk(KERN_INFO "ctrlreg: read done\n");

    return sizeof(unsigned long);
}


static ssize_t ctrlreg_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    unsigned int minor = iminor(file_inode(file));
    unsigned int cpu = GET_MINOR_CPU(minor);
    unsigned int reg = GET_MINOR_REG(minor);
    struct ctrlreg_info info = {.reg = reg};
    unsigned long err;

    printk(KERN_INFO "ctrlreg: write for cpu%u reg%u\n", cpu, reg);
    printk(KERN_INFO "ctrlreg: write of %zu bytes\n", count);

    if (count < sizeof(unsigned long))
        return -EINVAL;

    printk(KERN_INFO "ctrlreg: scheduling write\n");

    err = copy_from_user((void*)buf, &info.value, sizeof(unsigned long));

    printk(KERN_INFO "ctrlreg: write copy data: %x ( %lu )\n", err, sizeof(unsigned long));

    if (IS_ERR_VALUE(err))
        return err;

    err = smp_call_function_single(cpu, ctrlreg_smp_do_write, &info, 1);
    if (IS_ERR_VALUE(err))
        return err;

    printk(KERN_INFO "ctrlreg: write success: %x\n", info.error);

    if (IS_ERR_VALUE(info.error))
        return err;

    printk(KERN_INFO "ctrlreg: write done\n");

    return sizeof(unsigned long);
}

static void ctrlreg_can_open(void *p)
{
    unsigned int* reg = p;
    unsigned int reg_num = *reg;
    unsigned int ebx, edx, eax, ecx;
    unsigned int support_xgetbv, support_ia32e;

    *reg = 0;   //Success

    printk(KERN_INFO "ctrlreg: can open reg %u\n", reg_num);

    if (reg_num <= 4 && reg_num != 1)
        return;

#ifdef CONFIG_X86_64
    if (reg_num == 8)
        return;
#endif  



    cpuid_count(0x0d, 1, &eax, &ebx, &ecx, &edx);

    support_xgetbv = cpuid_ecx(1) & 0x04000000;
    support_ia32e = cpuid_edx(0x80000001) & 0x20000000;

    printk(KERN_INFO "ctrlreg: xgetbv = %d\n", support_xgetbv);
    printk(KERN_INFO "ctrlreg: ia32e = %d\n", support_ia32e);

    if (support_xgetbv && support_ia32e)
        return;

    printk(KERN_INFO "ctrlreg: open denied");  

    *reg = -EIO;
}

static int ctrlreg_open(struct inode *inode, struct file *file)
{
    unsigned int cpu;
    unsigned int reg;
    unsigned int minor;
    unsigned long err;



    minor = iminor(file_inode(file));
    cpu = GET_MINOR_CPU(minor);
    reg = GET_MINOR_REG(minor);

    printk(KERN_INFO "ctrlreg: open device for cpu%u reg%u\n", cpu, reg);

    if (cpu >= nr_cpu_ids || !cpu_online(cpu))
        return -ENXIO;  /* No such CPU */

    err  = smp_call_function_single(cpu, ctrlreg_can_open, &reg, 1);
    if (IS_ERR_VALUE(err))
        return err;

    return reg;
}


static const struct file_operations ctrlreg_fops = 
{
    .owner = THIS_MODULE,
    .read = ctrlreg_read,
    .write = ctrlreg_write,
    .open = ctrlreg_open
};


static int ctrlreg_device_create(int cpu)
{
    struct device *dev = NULL;
    int i;

    printk(KERN_INFO "ctrlreg: device create for cpu %d\n", cpu);




    //CR0, 2-4, 8
    for (i = 0; i <= 8; i++)
    {
        if ((i>4 && i<8) || i == 1)
            continue;       //Skip non existent regs

        printk(KERN_INFO "ctrlreg: device cpu%dcr%d\n", cpu, i);
        dev = device_create(ctrlreg_class, NULL, MKDEV(major_n, MAKE_MINOR(cpu, i)), NULL, "cpu%dcr%d", cpu, i);
        if (IS_ERR(dev))
          return PTR_ERR(dev);
    }   

    //XCR0
    for (i = 0; i <= 0; i++)
    {
        printk(KERN_INFO "ctrlreg: device cpu%dxcr%d\n", cpu, i);
        dev = device_create(ctrlreg_class, NULL, MKDEV(major_n, MAKE_MINOR(cpu, (XCR_MINOR_BASE+i))), NULL, "cpu%dxcr%d", cpu, i);
        if (IS_ERR(dev))
          return PTR_ERR(dev);
    }

    return 0;
}

static void ctrlreg_device_destroy(int cpu)
{
    int i;

    //CR0, 2-4, 8
    for (i = 0; i <= 8; i++)
    {
        if ((i>4 && i<8) || i == 1)
            continue;       //Skip non existent regs

        device_destroy(ctrlreg_class, MKDEV(major_n, MAKE_MINOR(cpu, i)));
    }

    //XCR0
    for (i = 0; i <= 0; i++)
        device_destroy(ctrlreg_class, MKDEV(major_n, MAKE_MINOR(cpu, (XCR_MINOR_BASE+i))));
}

static char* ctrlreg_devnode(struct device *dev, umode_t *mode)
{
    unsigned int minor = MINOR(dev->devt), cpu = GET_MINOR_CPU(minor), reg = GET_MINOR_REG(minor);

    if (reg < XCR_MINOR_BASE)
        return kasprintf(GFP_KERNEL, "crs/cpu%u/cr%u", cpu, reg);
    else
        return kasprintf(GFP_KERNEL, "crs/cpu%u/xcr%u", cpu, reg-XCR_MINOR_BASE);
}


int __init ctrlreg_init(void)
{
    unsigned long err = 0, i = 0;

    printk(KERN_INFO "ctrlreg: init\n");

    if ((major_n = __register_chrdev(0, 0, NR_CPUS, "crs", &ctrlreg_fops)) < 0)
        return major_n;

    printk(KERN_INFO "ctrlreg: major number is %u\n", major_n);



    ctrlreg_class = class_create(THIS_MODULE, "ctrlreg\n");
    if (IS_ERR(ctrlreg_class)) 
    {
        err = PTR_ERR(ctrlreg_class);
        goto out_chrdev;
    }

    printk(KERN_INFO "ctrlreg: class created\n");

    ctrlreg_class->devnode = ctrlreg_devnode;

    for_each_online_cpu(i) 
    {
        err = ctrlreg_device_create(i);
        if (IS_ERR_VALUE(err))
            goto out_class;
    }


    printk(KERN_INFO "ctrlreg: init success\n");

    err = 0;
    goto out;

out_class:
    i = 0;
    for_each_online_cpu(i) 
    {
        ctrlreg_device_destroy(i);
    }
    class_destroy(ctrlreg_class);

out_chrdev:
    __unregister_chrdev(CPUID_MAJOR, 0, NR_CPUS, "ctrlreg");
out:
    return err;
}


static void __exit ctrlreg_exit(void)
{
    int cpu = 0;

    for_each_online_cpu(cpu)
        ctrlreg_device_destroy(cpu);
    class_destroy(ctrlreg_class);
    __unregister_chrdev(CPUID_MAJOR, 0, NR_CPUS, "ctrlreg");
}

module_init(ctrlreg_init);
module_exit(ctrlreg_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Kee Nemesis 241");
MODULE_DESCRIPTION("Read and write Control Registers");
