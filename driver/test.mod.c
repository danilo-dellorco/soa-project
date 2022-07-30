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
	{ 0x6bc3fbc0, "__unregister_chrdev" },
	{ 0x87a21cb3, "__ubsan_handle_out_of_bounds" },
	{ 0x4fd3ca9d, "__register_chrdev" },
	{ 0xcefb0c9f, "__mutex_init" },
	{ 0x13c49cc2, "_copy_from_user" },
	{ 0xeb233a45, "__kmalloc" },
	{ 0x72c26d96, "kmem_cache_alloc_trace" },
	{ 0x9dc7c3ab, "kmalloc_caches" },
	{ 0x6b10bee1, "_copy_to_user" },
	{ 0x88db9f48, "__check_object_size" },
	{ 0x37a0cba, "kfree" },
	{ 0x3213f038, "mutex_unlock" },
	{ 0x754d539c, "strlen" },
	{ 0x4dfa8d4b, "mutex_lock" },
	{ 0xd8cef6e1, "clear_user" },
	{ 0x92997ed8, "_printk" },
	{ 0xbdfb6dbb, "__fentry__" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "EF043126078D0DB88036010");
