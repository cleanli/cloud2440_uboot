/*
 * (C) Copyright 2008
 * Stuart Wood, Lab X Technologies <stuart.wood@labxtechnologies.com>
 *
 * (C) Copyright 2004
 * Jian Zhang, Texas Instruments, jzhang@ti.com.

 * (C) Copyright 2000-2006
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * (C) Copyright 2001 Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Andreas Heppel <aheppel@sysgo.de>

 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

/* #define DEBUG */

#include <common.h>
#include <command.h>
#include <environment.h>
#include <linux/stddef.h>
#include <malloc.h>
#include <nand.h>

#if defined(CONFIG_CMD_SAVEENV) && defined(CONFIG_CMD_NAND)
#define CMD_SAVEENV
#elif defined(CONFIG_ENV_OFFSET_REDUND)
#error Cannot use CONFIG_ENV_OFFSET_REDUND without CONFIG_CMD_SAVEENV & CONFIG_CMD_NAND
#endif

#if defined(CONFIG_ENV_SIZE_REDUND) && (CONFIG_ENV_SIZE_REDUND != CONFIG_ENV_SIZE)
#error CONFIG_ENV_SIZE_REDUND should be the same as CONFIG_ENV_SIZE
#endif

#ifdef CONFIG_INFERNO
#error CONFIG_INFERNO not supported yet
#endif

#ifndef CONFIG_ENV_RANGE
#define CONFIG_ENV_RANGE	CONFIG_ENV_SIZE
#endif

/* references to names in env_common.c */
extern uchar default_environment[];

char * env_name_spec = "NAND";

static u_char tmpchar[CONFIG_ENV_SIZE] ;
static u_char next_write_page1, next_write_page2;

#if defined(ENV_IS_EMBEDDED)
extern uchar environment[];
env_t *env_ptr = (env_t *)(&environment[0]);
#elif defined(CONFIG_NAND_ENV_DST)
env_t *env_ptr = (env_t *)CONFIG_NAND_ENV_DST;
#else /* ! ENV_IS_EMBEDDED */
env_t *env_ptr = 0;
#endif /* ENV_IS_EMBEDDED */


/* local functions */
#if !defined(ENV_IS_EMBEDDED)
static void use_default(void);
#endif

DECLARE_GLOBAL_DATA_PTR;

uchar env_get_char_spec (int index)
{
	return ( *((uchar *)(gd->env_addr + index)) );
}


/* this is called before nand_init()
 * so we can't read Nand to validate env data.
 * Mark it OK for now. env_relocate() in env_common.c
 * will call our relocate function which does the real
 * validation.
 *
 * When using a NAND boot image (like sequoia_nand), the environment
 * can be embedded or attached to the U-Boot image in NAND flash. This way
 * the SPL loads not only the U-Boot image from NAND but also the
 * environment.
 */
int env_init(void)
{
#if defined(ENV_IS_EMBEDDED) || defined(CONFIG_NAND_ENV_DST)
	int crc1_ok = 0, crc2_ok = 0;
	env_t *tmp_env1;

#ifdef CONFIG_ENV_OFFSET_REDUND
	env_t *tmp_env2;

	tmp_env2 = (env_t *)((ulong)env_ptr + CONFIG_ENV_SIZE);
	crc2_ok = (crc32(0, tmp_env2->data, ENV_SIZE) == tmp_env2->crc);
#endif

	tmp_env1 = env_ptr;

	crc1_ok = (crc32(0, tmp_env1->data, ENV_SIZE) == tmp_env1->crc);

	if (!crc1_ok && !crc2_ok) {
		gd->env_addr  = 0;
		gd->env_valid = 0;

		return 0;
	} else if (crc1_ok && !crc2_ok) {
		gd->env_valid = 1;
	}
#ifdef CONFIG_ENV_OFFSET_REDUND
	else if (!crc1_ok && crc2_ok) {
		gd->env_valid = 2;
	} else {
		/* both ok - check serial */
		if(tmp_env1->flags == 255 && tmp_env2->flags == 0)
			gd->env_valid = 2;
		else if(tmp_env2->flags == 255 && tmp_env1->flags == 0)
			gd->env_valid = 1;
		else if(tmp_env1->flags > tmp_env2->flags)
			gd->env_valid = 1;
		else if(tmp_env2->flags > tmp_env1->flags)
			gd->env_valid = 2;
		else /* flags are equal - almost impossible */
			gd->env_valid = 1;
	}

	if (gd->env_valid == 2)
		env_ptr = tmp_env2;
	else
#endif
	if (gd->env_valid == 1)
		env_ptr = tmp_env1;

	gd->env_addr = (ulong)env_ptr->data;

#else /* ENV_IS_EMBEDDED || CONFIG_NAND_ENV_DST */
	gd->env_addr  = (ulong)&default_environment[0];
	gd->env_valid = 1;
#endif /* ENV_IS_EMBEDDED || CONFIG_NAND_ENV_DST */

	return (0);
}

#ifdef CMD_SAVEENV
/*
 * The legacy NAND code saved the environment in the first NAND device i.e.,
 * nand_dev_desc + 0. This is also the behaviour using the new NAND code.
 */
int writeenv(size_t offset, u_char *buf, size_t save_size)
{
	size_t end = offset + save_size;
	size_t amount_saved = 0;
	size_t blocksize, len;

	u_char *char_ptr;

	blocksize = nand_info[0].erasesize;
	len = min(blocksize, save_size);

	while (amount_saved < save_size && offset < end) {
		if (nand_block_isbad(&nand_info[0], offset)) {
			offset += blocksize;
		} else {
			char_ptr = &buf[amount_saved];
			if (nand_write(&nand_info[0], offset, &len,
					char_ptr))
				return 1;
			offset += blocksize;
			amount_saved += len;
		}
	}
	if (amount_saved != save_size)
		return 1;

	return 0;
}

static size_t get_page(size_t n)
{
	return (n+511)/512;
}

static size_t find_env_len(env_t * envp)
{
	u_char * up = (u_char*)envp + CONFIG_ENV_SIZE - 1;
	size_t i = 0;
	while(i < CONFIG_ENV_SIZE){
		if(*up)
			return CONFIG_ENV_SIZE - i;
		up--;
		i++;
	}
	return 0;
}	

void memprint(char*s, int len)
{
	int i = 0;
	while(len--){
		if((i++ & 0x0f) == 0)
			printf("\n");
		printf("%02x ", *s++);

	}
	printf("\n");
}

#ifdef CONFIG_ENV_OFFSET_REDUND
int saveenv(void)
{
	int ret = 0;
	nand_erase_options_t nand_erase_options;
	size_t total;

	env_ptr->flags++;
	total = find_env_len(env_ptr);
	//memprint(env_ptr, total+0x100);
	memset(tmpchar, 0xff, CONFIG_ENV_SIZE);
	memcpy(tmpchar, (u_char*)env_ptr, total);

	nand_erase_options.length = CONFIG_ENV_RANGE;
	nand_erase_options.quiet = 0;
	nand_erase_options.jffs2 = 0;
	nand_erase_options.scrub = 0;

	if (CONFIG_ENV_RANGE < CONFIG_ENV_SIZE)
		return 1;
	if ((total + 1) > CONFIG_ENV_SIZE)
		return 1;
	if(gd->env_valid == 1) {
		if(total + 1 > CONFIG_ENV_SIZE - next_write_page2 * 512){
			puts ("Erasing redundant Nand...\n");
			nand_erase_options.offset = CONFIG_ENV_OFFSET_REDUND;
			if (nand_erase_opts(&nand_info[0], &nand_erase_options))
				return 1;
			next_write_page2 = 0;
		}
		tmpchar[total] =  next_write_page2;

		puts ("Writing to redundant Nand... ");
		printf("nand offset %x size %x\n", CONFIG_ENV_OFFSET_REDUND + next_write_page2 * 512,512 * get_page(total + 1));
		ret = writeenv(CONFIG_ENV_OFFSET_REDUND + next_write_page2 * 512, tmpchar, 512 * get_page(total + 1));
		next_write_page2 += get_page(total+1);
	} else {
		if(total + 1 > CONFIG_ENV_SIZE - next_write_page1 * 512){
			puts ("Erasing Nand...\n");
			nand_erase_options.offset = CONFIG_ENV_OFFSET;
			if (nand_erase_opts(&nand_info[0], &nand_erase_options))
				return 1;
			next_write_page1 = 0;
		}
		tmpchar[total] =  next_write_page1;

		puts ("Writing to Nand... ");
		printf("nand offset %x size %x\n", CONFIG_ENV_OFFSET + next_write_page1 * 512,512 * get_page(total + 1));
		ret = writeenv(CONFIG_ENV_OFFSET+ next_write_page1 * 512, tmpchar, 512 * get_page(total + 1));
		next_write_page1 += get_page(total+1);
	}
	if (ret) {
		puts("FAILED!\n");
		return 1;
	}

	puts ("done\n");
	gd->env_valid = (gd->env_valid == 2 ? 1 : 2);
	return ret;
}
#else /* ! CONFIG_ENV_OFFSET_REDUND */
int saveenv(void)
{
	int ret = 0;
	nand_erase_options_t nand_erase_options;

	nand_erase_options.length = CONFIG_ENV_RANGE;
	nand_erase_options.quiet = 0;
	nand_erase_options.jffs2 = 0;
	nand_erase_options.scrub = 0;
	nand_erase_options.offset = CONFIG_ENV_OFFSET;

	if (CONFIG_ENV_RANGE < CONFIG_ENV_SIZE)
		return 1;
	puts ("Erasing Nand...\n");
	if (nand_erase_opts(&nand_info[0], &nand_erase_options))
		return 1;

	puts ("Writing to Nand... ");
	if (writeenv(CONFIG_ENV_OFFSET, (u_char *) env_ptr)) {
		puts("FAILED!\n");
		return 1;
	}

	puts ("done\n");
	return ret;
}
#endif /* CONFIG_ENV_OFFSET_REDUND */
#endif /* CMD_SAVEENV */

int readenv (size_t offset, u_char * buf)
{
	size_t end = offset + CONFIG_ENV_RANGE;
	size_t amount_loaded = 0;
	size_t blocksize, len;

	u_char *char_ptr;

	blocksize = nand_info[0].erasesize;
	len = min(blocksize, CONFIG_ENV_SIZE);

	while (amount_loaded < CONFIG_ENV_SIZE && offset < end) {
		if (nand_block_isbad(&nand_info[0], offset)) {
			offset += blocksize;
		} else {
			char_ptr = &buf[amount_loaded];
			if (nand_read(&nand_info[0], offset, &len, char_ptr))
				return 1;
			offset += blocksize;
			amount_loaded += len;
		}
	}
	if (amount_loaded != CONFIG_ENV_SIZE)
		return 1;

	return 0;
}

u_char find_env(u_char*tmpchar, env_t *tmpenv)
{
	u_char * p = tmpchar + CONFIG_ENV_SIZE;
	unsigned int offset, len;
	
	memset(tmpenv, 0, CONFIG_ENV_SIZE);
	while((unsigned int)(--p) > (unsigned int)tmpchar){
		if(*p != 0xff)
			break;	
	}
	offset = 512 * (*p);
	printf("find effective env at offset %x ", offset);
	//this indicate the block not init, need erase
	if(tmpchar + offset >= p)
		goto err;
	len = (unsigned int)(p - tmpchar) - offset;
	printf("len %x\n", len);
	memcpy(tmpenv, tmpchar+offset, len);
	return offset/512 + get_page(len+1);
err:
	printf("error in env\n");
	return CONFIG_ENV_SIZE/512;
}

#ifdef CONFIG_ENV_OFFSET_REDUND
void env_relocate_spec (void)
{
#if !defined(ENV_IS_EMBEDDED)
	int crc1_ok = 0, crc2_ok = 0;
	env_t *tmp_env1, *tmp_env2;

	tmp_env1 = (env_t *) malloc(CONFIG_ENV_SIZE);
	tmp_env2 = (env_t *) malloc(CONFIG_ENV_SIZE);

	if ((tmp_env1 == NULL) || (tmp_env2 == NULL)){ 
		puts("Can't allocate buffers for environment\n");
		free (tmp_env1);
		free (tmp_env2);
		return use_default();
	}

	printf("Check %x: ", CONFIG_ENV_OFFSET);
	if (readenv(CONFIG_ENV_OFFSET, tmpchar))
		puts("No Valid Environment Area Found\n");
	next_write_page1 = find_env(tmpchar, tmp_env1);

	printf("Check %x: ", CONFIG_ENV_OFFSET_REDUND);
	if (readenv(CONFIG_ENV_OFFSET_REDUND, tmpchar))
		puts("No Valid Reundant Environment Area Found\n");
	next_write_page2 = find_env(tmpchar, tmp_env2);

	crc1_ok = (crc32(0, tmp_env1->data, ENV_SIZE) == tmp_env1->crc);
	crc2_ok = (crc32(0, tmp_env2->data, ENV_SIZE) == tmp_env2->crc);

	if(!crc1_ok && !crc2_ok) {
		free(tmp_env1);
		free(tmp_env2);
		return use_default();
	} else if(crc1_ok && !crc2_ok)
		gd->env_valid = 1;
	else if(!crc1_ok && crc2_ok)
		gd->env_valid = 2;
	else {
		/* both ok - check serial */
		if(tmp_env1->flags == 255 && tmp_env2->flags == 0)
			gd->env_valid = 2;
		else if(tmp_env2->flags == 255 && tmp_env1->flags == 0)
			gd->env_valid = 1;
		else if(tmp_env1->flags > tmp_env2->flags)
			gd->env_valid = 1;
		else if(tmp_env2->flags > tmp_env1->flags)
			gd->env_valid = 2;
		else /* flags are equal - almost impossible */
			gd->env_valid = 1;

	}

	free(env_ptr);
	if(gd->env_valid == 1) {
		env_ptr = tmp_env1;
		free(tmp_env2);
	} else {
		env_ptr = tmp_env2;
		free(tmp_env1);
	}

#endif /* ! ENV_IS_EMBEDDED */
}
#else /* ! CONFIG_ENV_OFFSET_REDUND */
/*
 * The legacy NAND code saved the environment in the first NAND device i.e.,
 * nand_dev_desc + 0. This is also the behaviour using the new NAND code.
 */
void env_relocate_spec (void)
{
#if !defined(ENV_IS_EMBEDDED)
	int ret;

	ret = readenv(CONFIG_ENV_OFFSET, (u_char *) env_ptr);
	if (ret)
		return use_default();

	if (crc32(0, env_ptr->data, ENV_SIZE) != env_ptr->crc)
		return use_default();
#endif /* ! ENV_IS_EMBEDDED */
}
#endif /* CONFIG_ENV_OFFSET_REDUND */

#if !defined(ENV_IS_EMBEDDED)
static void use_default()
{
	puts ("*** Warning - bad CRC or NAND, using default environment\n\n");
	set_default_env();
}
#endif
