#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;
BUILD_LTO_INFO;

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
	{ 0x7b0aa9f9, "module_layout" },
	{ 0xdfd71ed2, "param_ops_ulong" },
	{ 0x724b8374, "param_array_ops" },
	{ 0xc5b6f236, "queue_work_on" },
	{ 0x2d3385d3, "system_wq" },
	{ 0xf9a482f9, "msleep" },
	{ 0x13c49cc2, "_copy_from_user" },
	{ 0xeb233a45, "__kmalloc" },
	{ 0x6bc3fbc0, "__unregister_chrdev" },
	{ 0x4fd3ca9d, "__register_chrdev" },
	{ 0xd9a5ea54, "__init_waitqueue_head" },
	{ 0xcefb0c9f, "__mutex_init" },
	{ 0x6b10bee1, "_copy_to_user" },
	{ 0x88db9f48, "__check_object_size" },
	{ 0x754d539c, "strlen" },
	{ 0x3eeb2322, "__wake_up" },
	{ 0x3213f038, "mutex_unlock" },
	{ 0xd8cef6e1, "clear_user" },
	{ 0x89940875, "mutex_lock_interruptible" },
	{ 0x87a21cb3, "__ubsan_handle_out_of_bounds" },
	{ 0x72c26d96, "kmem_cache_alloc_trace" },
	{ 0x9dc7c3ab, "kmalloc_caches" },
	{ 0xd0da656b, "__stack_chk_fail" },
	{ 0x92540fbf, "finish_wait" },
	{ 0x8ddd8aad, "schedule_timeout" },
	{ 0x8c26d495, "prepare_to_wait_event" },
	{ 0xfe487975, "init_wait_entry" },
	{ 0xbb9ed3bf, "mutex_trylock" },
	{ 0xe2c17b5d, "__SCT__might_resched" },
	{ 0x7f02188f, "__msecs_to_jiffies" },
	{ 0x8304585a, "current_task" },
	{ 0x37a0cba, "kfree" },
	{ 0x92997ed8, "_printk" },
	{ 0xbdfb6dbb, "__fentry__" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "9FB6CEE0AA1767F9C70815C");
