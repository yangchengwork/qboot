#include "bios.h"
#include "e820.h"
#include "stdio.h"
#include "ioport.h"
#include "string.h"
#include "fw_cfg.h"
#include "linuxboot.h"
#include "multiboot.h"

struct fw_cfg_file {
	uint32_t size;
	uint16_t select;
	char name[57];
};

static int filecnt;
static struct fw_cfg_file *files;

void fw_cfg_setup(void)
{
	int i, n;

	fw_cfg_select(FW_CFG_FILE_DIR);
	n = fw_cfg_readl_be();
	filecnt = n;
	files = malloc_fseg(sizeof(files[0]) * n);

	for (i = 0; i < n; i++) {
		files[i].size = fw_cfg_readl_be();
		files[i].select = fw_cfg_readw_be();
		fw_cfg_readw_be();
		fw_cfg_read(files[i].name, sizeof(files[i].name) - 1);
	}
}

int fw_cfg_file_id(char *name)
{
	int i;

	for (i = 0; i < filecnt; i++)
		if (!strcmp(name, files[i].name))
			return i;

	return -1;
}

uint32_t fw_cfg_file_size(int id)
{
	if (id == -1)
		return 0;
	return files[id].size;
}

void fw_cfg_file_select(int id)
{
	fw_cfg_select(files[id].select);
}

/* Multiboot trampoline.  QEMU does the ELF parsing.  */

static void boot_multiboot_from_fw_cfg(void)
{
	void *kernel_addr, *kernel_entry;
	struct mb_info *mb;
	struct mb_mmap_entry *mbmem;
	int i;
	uint32_t sz;

	fw_cfg_select(FW_CFG_KERNEL_SIZE);
	sz = fw_cfg_readl_le();
	if (!sz)
		panic();

	fw_cfg_select(FW_CFG_KERNEL_ADDR);
	kernel_addr = (void *) fw_cfg_readl_le();
	fw_cfg_select(FW_CFG_KERNEL_DATA);
	fw_cfg_read(kernel_addr, sz);

	fw_cfg_select(FW_CFG_INITRD_SIZE);
	sz = fw_cfg_readl_le();
	if (!sz)
		panic();

	fw_cfg_select(FW_CFG_INITRD_ADDR);
	mb = (struct mb_info *) fw_cfg_readl_le();
	fw_cfg_select(FW_CFG_INITRD_DATA);
	fw_cfg_read(mb, sz);

	mb->mem_lower = 639;
	mb->mem_upper = (lowmem - 1048576) >> 10;

	mb->mmap_length = 0;
	for (i = 0; i < e820->nr_map; i++) {
		mbmem = (struct mb_mmap_entry *) (mb->mmap_addr + mb->mmap_length);
		mbmem->size = sizeof(e820->map[i]);
		mbmem->base_addr = e820->map[i].addr;
		mbmem->length = e820->map[i].size;
		mbmem->type = e820->map[i].type;
		mb->mmap_length += sizeof(*mbmem);
	}

#ifdef BENCHMARK_HACK
	/* Exit just before getting to vmlinuz, so that it is easy
	 * to time/profile the firmware.
	 */
	outb(0xf4, 1);
#endif

	fw_cfg_select(FW_CFG_KERNEL_ENTRY);
	kernel_entry = (void *) fw_cfg_readl_le();
	asm volatile("jmp *%2" : : "a" (0x2badb002), "b"(mb), "c"(kernel_entry));
	panic();
}

void boot_from_fwcfg(void)
{
	struct linuxboot_args args;
	uint32_t kernel_size;

	fw_cfg_select(FW_CFG_CMDLINE_SIZE);
	args.cmdline_size = fw_cfg_readl_le();
	fw_cfg_select(FW_CFG_INITRD_SIZE);
	args.initrd_size = fw_cfg_readl_le();

	/* QEMU has already split the real mode and protected mode
	 * parts.  Recombine them in args.vmlinuz_size.
	 */
	fw_cfg_select(FW_CFG_KERNEL_SIZE);
	kernel_size = fw_cfg_readl_le();
	fw_cfg_select(FW_CFG_SETUP_SIZE);
	args.vmlinuz_size = kernel_size + fw_cfg_readl_le();

	if (!args.vmlinuz_size)
		return;

	fw_cfg_select(FW_CFG_SETUP_DATA);
	fw_cfg_read(args.header, sizeof(args.header));

	if (!parse_bzimage(&args))
		boot_multiboot_from_fw_cfg();

	/* SETUP_DATA already selected */
	if (args.setup_size > sizeof(args.header))
		fw_cfg_read(args.setup_addr + sizeof(args.header),
			    args.setup_size - sizeof(args.header));

	fw_cfg_select(FW_CFG_KERNEL_DATA);
	fw_cfg_read(args.kernel_addr, kernel_size);

	fw_cfg_select(FW_CFG_CMDLINE_DATA);
	fw_cfg_read(args.cmdline_addr, args.cmdline_size);

	if (args.initrd_size) {
		fw_cfg_select(FW_CFG_INITRD_DATA);
		fw_cfg_read(args.initrd_addr, args.initrd_size);
	}

	boot_bzimage(&args);
}
