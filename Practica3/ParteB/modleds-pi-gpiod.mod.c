#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x3868185, "module_layout" },
	{ 0x310024a9, "device_destroy" },
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0x6b104956, "kobject_put" },
	{ 0xeb9398df, "cdev_del" },
	{ 0x729486cf, "class_destroy" },
	{ 0xfd935d05, "gpiod_put" },
	{ 0x56eefe6b, "device_create" },
	{ 0x94176683, "__class_create" },
	{ 0x24fc62bc, "cdev_add" },
	{ 0x61e59031, "cdev_init" },
	{ 0x835aa563, "cdev_alloc" },
	{ 0xe3ec2f2b, "alloc_chrdev_region" },
	{ 0xd99ae477, "gpiod_direction_output" },
	{ 0xc5e1beab, "gpio_to_desc" },
	{ 0x47229b5c, "gpio_request" },
	{ 0x3c3ff9fd, "sprintf" },
	{ 0xe000dd36, "gpiod_set_value" },
	{ 0xc5850110, "printk" },
	{ 0x86332725, "__stack_chk_fail" },
	{ 0xbcab6ee6, "sscanf" },
	{ 0xae353d77, "arm_copy_from_user" },
	{ 0x8f678b07, "__stack_chk_guard" },
	{ 0x57dba99f, "try_module_get" },
	{ 0x54b73ef6, "module_put" },
	{ 0xb1ad28e0, "__gnu_mcount_nc" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "6002AA15AE111DD3903109B");
