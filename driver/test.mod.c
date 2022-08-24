#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/export-internal.h>
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
	{ 0xbdfb6dbb, "__fentry__" },
	{ 0x92997ed8, "_printk" },
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0x37a0cba, "kfree" },
	{ 0x72ed1b3b, "current_task" },
	{ 0x7f02188f, "__msecs_to_jiffies" },
	{ 0xe2c17b5d, "__SCT__might_resched" },
	{ 0xbb9ed3bf, "mutex_trylock" },
	{ 0xfe487975, "init_wait_entry" },
	{ 0x8c26d495, "prepare_to_wait_event" },
	{ 0x8ddd8aad, "schedule_timeout" },
	{ 0x92540fbf, "finish_wait" },
	{ 0xd0da656b, "__stack_chk_fail" },
	{ 0x84822bf, "kmalloc_caches" },
	{ 0xaf467b62, "kmem_cache_alloc_trace" },
	{ 0x87a21cb3, "__ubsan_handle_out_of_bounds" },
	{ 0x4dfa8d4b, "mutex_lock" },
	{ 0xd8cef6e1, "clear_user" },
	{ 0x3213f038, "mutex_unlock" },
	{ 0x3eeb2322, "__wake_up" },
	{ 0x754d539c, "strlen" },
	{ 0x88db9f48, "__check_object_size" },
	{ 0x6b10bee1, "_copy_to_user" },
	{ 0xcefb0c9f, "__mutex_init" },
	{ 0xd9a5ea54, "__init_waitqueue_head" },
	{ 0xaa0848d, "__register_chrdev" },
	{ 0x6bc3fbc0, "__unregister_chrdev" },
	{ 0xeb233a45, "__kmalloc" },
	{ 0x13c49cc2, "_copy_from_user" },
	{ 0xf9a482f9, "msleep" },
	{ 0x2d3385d3, "system_wq" },
	{ 0xc5b6f236, "queue_work_on" },
	{ 0xcf3ace6c, "param_array_ops" },
	{ 0x62a653ca, "param_ops_ulong" },
	{ 0x6b3adab9, "module_layout" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "436829495B05449CB65EA41");
