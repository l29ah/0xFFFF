/*
 *  0xFFFF - Open Free Fiasco Firmware Flasher
 *  Copyright (C) 2007-2009  pancake <pancake@youterm.com>
 *  Copyright (C) 2012  Pali Rohár <pali.rohar@gmail.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <usb.h>

#include "nolo.h"
#include "image.h"
#include "global.h"
#include "printf-utils.h"

/* Request type */
#define NOLO_WRITE		64
#define NOLO_QUERY		192

/* Request */
#define NOLO_STATUS		1
#define NOLO_GET_NOLO_VERSION	3
#define NOLO_IDENTIFY		4
#define NOLO_SET		16
#define NOLO_GET		17
#define NOLO_STRING		18
#define NOLO_SET_STRING		19
#define NOLO_GET_STRING		20
#define NOLO_BOOT		130
#define NOLO_REBOOT		131

/* Index */
#define NOLO_RD_MODE		0
#define NOLO_ROOT_DEVICE	1
#define NOLO_USB_HOST_MODE	2
#define NOLO_ADD_RD_FLAGS	3
#define NOLO_DEL_RD_FLAGS	4

/* Values - R&D flags */
#define NOLO_RD_FLAG_NO_OMAP_WD		0x002
#define NOLO_RD_FLAG_NO_EXT_WD		0x004
#define NOLO_RD_FLAG_NO_LIFEGUARD	0x008
#define NOLO_RD_FLAG_SERIAL_CONSOLE	0x010
#define NOLO_RD_FLAG_NO_USB_TIMEOUT	0x020
#define NOLO_RD_FLAG_STI_CONSOLE	0x040
#define NOLO_RD_FLAG_NO_CHARGING	0x080
#define NOLO_RD_FLAG_FORCE_POWER_KEY	0x100

/* Values - Boot mode */
#define NOLO_BOOT_MODE_NORMAL		0
#define NOLO_BOOT_MODE_UPDATE		1

static int nolo_identify_string(struct usb_device_info * dev, const char * str, char * out, size_t size) {

	char buf[512];
	char * ptr;
	int ret;

	memset(buf, 0, sizeof(buf));

	ret = usb_control_msg(dev->udev, NOLO_QUERY, NOLO_IDENTIFY, 0, 0, (char *)buf, sizeof(buf), 2000);
	if ( ret < 0 )
		ERROR_RETURN("NOLO_IDENTIFY failed", -1);

	ptr = memmem(buf, ret, str, strlen(str));
	if ( ! ptr )
		ERROR_RETURN("Substring was not found", -1);

	ptr += strlen(str) + 1;

	while ( ptr-buf < ret && *ptr < 32 )
		++ptr;

	if ( ! *ptr )
		return -1;

	strncpy(out, ptr, size-1);
	out[size-1] = 0;
	return strlen(out);

}

static int nolo_set_string(struct usb_device_info * dev, char * str, char * arg) {

	if ( usb_control_msg(dev->udev, NOLO_WRITE, NOLO_STRING, 0, 0, str, strlen(str), 2000) < 0 )
		ERROR_RETURN("NOLO_REQUEST_STRING failed", -1);

	if ( usb_control_msg(dev->udev, NOLO_WRITE, NOLO_SET_STRING, 0, 0, arg, strlen(arg), 2000) < 0 )
		ERROR_RETURN("NOLO_REQUEST_STRING_ARG failed", -1);

	return 0;

}

static int nolo_get_string(struct usb_device_info * dev, char * str, char * out, size_t size) {

	if ( usb_control_msg(dev->udev, NOLO_WRITE, NOLO_STRING, 0, 0, str, strlen(str), 2000) < 0 )
		ERROR_RETURN("NOLO_REQUEST_STRING failed", -1);

	if ( usb_control_msg(dev->udev, NOLO_QUERY, NOLO_GET_STRING, 0, 0, out, size, 2000) < 0 )
		ERROR_RETURN("NOLO_RESPONCE_STRING failed", -1);

	out[size-1] = 0;
	return strlen(out);

}

static int nolo_get_version_string(struct usb_device_info * dev, const char * str, char * out, size_t size) {

	int ret;
	char buf[512];

	if ( strlen(str) > 500 )
		return -1;

	sprintf(buf, "version:%s", str);

	ret = nolo_get_string(dev, buf, out, size);
	if ( ret < 0 )
		return ret;

	if ( ! out[0] )
		return -1;

	return ret;

}

int nolo_init(struct usb_device_info * dev) {

	uint32_t val = 1;

	printf("Initializing NOLO...\n");
	while ( val != 0 )
		if ( usb_control_msg(dev->udev, NOLO_QUERY, NOLO_STATUS, 0, 0, (char *)&val, 4, 2000) == -1 )
			ERROR_RETURN("NOLO_STATUS failed", -1);

	return 0;
}

enum device nolo_get_device(struct usb_device_info * dev) {

	char buf[20];
	if ( nolo_identify_string(dev, "prod_code", buf, sizeof(buf)) < 0 )
		return DEVICE_UNKNOWN;
	else if ( buf[0] )
		return device_from_string(buf);
	else
		return DEVICE_UNKNOWN;

}

int nolo_load_image(struct usb_device_info * dev, struct image * image) {

	printf("nolo_load_image is not implemented yet\n");
	return -1;

}

int nolo_flash_image(struct usb_device_info * dev, struct image * image) {

	printf("nolo_flash_image is not implemented yet\n");
	return -1;

}

int nolo_boot_device(struct usb_device_info * dev, char * cmdline) {

	int size = 0;
	int mode = NOLO_BOOT_MODE_NORMAL;

	if ( cmdline && strncmp(cmdline, "update", strlen("update")) == 0 && cmdline[strlen("update")] <= 32 ) {
		mode = NOLO_BOOT_MODE_UPDATE;
		cmdline += strlen("update");
		if ( *cmdline ) ++cmdline;
		while ( *cmdline && *cmdline <= 32 )
			++cmdline;
		if ( *cmdline ) {
			printf("Booting kernel to update mode with cmdline: %s...\n", cmdline);
			size = strlen(cmdline);
		} else {
			printf("Booting kernel to update mode with default cmdline...\n");
			cmdline = NULL;
		}
	} else if ( cmdline && cmdline[0] ) {
		printf("Booting kernel with cmdline: '%s'...\n", cmdline);
		size = strlen(cmdline)+1;
	} else {
		printf("Booting kernel with default cmdline...\n");
		cmdline = NULL;
	}

	if ( usb_control_msg(dev->udev, NOLO_WRITE, NOLO_BOOT, mode, 0, cmdline, size, 2000) < 0 )
		ERROR_RETURN("Booting failed", -1);

	return 0;

}

int nolo_reboot_device(struct usb_device_info * dev) {

	printf("Rebooting device...\n");
	if ( usb_control_msg(dev->udev, NOLO_WRITE, NOLO_REBOOT, 0, 0, NULL, 0, 2000) < 0 )
		ERROR_RETURN("NOLO_REBOOT failed", -1);
	return 0;

}

int nolo_get_root_device(struct usb_device_info * dev) {

	uint8_t device;
	if ( usb_control_msg(dev->udev, NOLO_QUERY, NOLO_GET, 0, NOLO_ROOT_DEVICE, (char *)&device, 1, 2000) < 0 )
		ERROR_RETURN("Cannot get root device", -1);
	return device;

}

int nolo_set_root_device(struct usb_device_info * dev, int device) {

	printf("Setting root device to %d...\n", device);
	if ( simulate )
		return 0;
	if ( usb_control_msg(dev->udev, NOLO_WRITE, NOLO_SET, device, NOLO_ROOT_DEVICE, NULL, 0, 2000) < 0 )
		ERROR_RETURN("Cannot set root device", -1);
	return 0;

}

int nolo_get_usb_host_mode(struct usb_device_info * dev) {

	uint32_t enabled;
	if ( usb_control_msg(dev->udev, NOLO_QUERY, NOLO_GET, 0, NOLO_USB_HOST_MODE, (void *)&enabled, 4, 2000) < 0 )
		ERROR_RETURN("Cannot get USB host mode status", -1);
	return enabled ? 1 : 0;

}

int nolo_set_usb_host_mode(struct usb_device_info * dev, int enable) {

	printf("%s USB host mode...\n", enable ? "Enabling" : "Disabling");
	if ( simulate )
		return 0;
	if ( usb_control_msg(dev->udev, NOLO_WRITE, NOLO_SET, enable, NOLO_USB_HOST_MODE, NULL, 0, 2000) < 0 )
		ERROR_RETURN("Cannot change USB host mode status", -1);
	return 0;

}

int nolo_get_rd_mode(struct usb_device_info * dev) {

	uint8_t enabled;
	if ( usb_control_msg(dev->udev, NOLO_QUERY, NOLO_GET, 0, NOLO_RD_MODE, (char *)&enabled, 1, 2000) < 0 )
		ERROR_RETURN("Cannot get R&D mode status", -1);
	return enabled ? 1 : 0;

}

int nolo_set_rd_mode(struct usb_device_info * dev, int enable) {

	printf("%s R&D mode...\n", enable ? "Enabling" : "Disabling");
	if ( simulate )
		return 0;
	if ( usb_control_msg(dev->udev, NOLO_WRITE, NOLO_SET, enable, NOLO_RD_MODE, NULL, 0, 2000) < 0 )
		ERROR_RETURN("Cannot change R&D mode status", -1);
	return 0;

}

#define APPEND_STRING(ptr, buf, size, string) do { if ( (int)size > ptr-buf ) ptr += snprintf(ptr, size-(ptr-buf), "%s,", string); } while (0)

int nolo_get_rd_flags(struct usb_device_info * dev, char * flags, size_t size) {

	uint16_t add_flags;
	char * ptr = flags;

	if ( usb_control_msg(dev->udev, NOLO_QUERY, NOLO_GET, 0, NOLO_ADD_RD_FLAGS, (char *)&add_flags, 2, 2000) < 0 )
		ERROR_RETURN("Cannot get R&D flags", -1);

	if ( add_flags & NOLO_RD_FLAG_NO_OMAP_WD )
		APPEND_STRING(ptr, flags, size, "no-omap-wd");
	if ( add_flags & NOLO_RD_FLAG_NO_EXT_WD )
		APPEND_STRING(ptr, flags, size, "no-ext-wd");
	if ( add_flags & NOLO_RD_FLAG_NO_LIFEGUARD )
		APPEND_STRING(ptr, flags, size, "no-lifeguard-reset");
	if ( add_flags & NOLO_RD_FLAG_SERIAL_CONSOLE )
		APPEND_STRING(ptr, flags, size, "serial-console");
	if ( add_flags & NOLO_RD_FLAG_NO_USB_TIMEOUT )
		APPEND_STRING(ptr, flags, size, "no-usb-timeout");
	if ( add_flags & NOLO_RD_FLAG_STI_CONSOLE )
		APPEND_STRING(ptr, flags, size, "sti-console");
	if ( add_flags & NOLO_RD_FLAG_NO_CHARGING )
		APPEND_STRING(ptr, flags, size, "no-charging");
	if ( add_flags & NOLO_RD_FLAG_FORCE_POWER_KEY )
		APPEND_STRING(ptr, flags, size, "force-power-key");

	if ( ptr != flags && *(ptr-1) == ',' )
		*(--ptr) = 0;

	return ptr-flags;

}

#undef APPEND_STRING

int nolo_set_rd_flags(struct usb_device_info * dev, const char * flags) {

	const char * ptr = flags;
	const char * endptr = ptr;

	int add_flags = 0;
	int del_flags = 0;

	if ( flags && flags[0] )
		printf("Setting R&D flags to: %s...\n", flags);
	else
		printf("Clearing all R&D flags...\n");

	while ( ptr && *ptr ) {

		while ( *ptr <= 32 )
			++ptr;

		if ( ! *ptr )
			break;

		endptr = strchr(ptr, ',');
		if ( endptr )
			endptr -= 1;
		else
			endptr = ptr+strlen(ptr);

		if ( strncmp("no-omap-wd", ptr, endptr-ptr) == 0 )
			add_flags |= NOLO_RD_FLAG_NO_OMAP_WD;
		if ( strncmp("no-ext-wd", ptr, endptr-ptr) == 0 )
			add_flags |= NOLO_RD_FLAG_NO_EXT_WD;
		if ( strncmp("no-lifeguard-reset", ptr, endptr-ptr) == 0 )
			add_flags |= NOLO_RD_FLAG_NO_LIFEGUARD;
		if ( strncmp("serial-console", ptr, endptr-ptr) == 0 )
			add_flags |= NOLO_RD_FLAG_SERIAL_CONSOLE;
		if ( strncmp("no-usb-timeout", ptr, endptr-ptr) == 0 )
			add_flags |= NOLO_RD_FLAG_NO_USB_TIMEOUT;
		if ( strncmp("sti-console", ptr, endptr-ptr) == 0 )
			add_flags |= NOLO_RD_FLAG_STI_CONSOLE;
		if ( strncmp("no-charging", ptr, endptr-ptr) == 0 )
			add_flags |= NOLO_RD_FLAG_NO_CHARGING;
		if ( strncmp("force-power-key", ptr, endptr-ptr) == 0 )
			add_flags |= NOLO_RD_FLAG_FORCE_POWER_KEY;

		if ( *(endptr+1) )
			ptr = endptr+2;
		else
			break;

	}

	if ( ! ( add_flags & NOLO_RD_FLAG_NO_OMAP_WD ) )
		del_flags |= NOLO_RD_FLAG_NO_OMAP_WD;
	if ( ! ( add_flags & NOLO_RD_FLAG_NO_EXT_WD ) )
		del_flags |= NOLO_RD_FLAG_NO_EXT_WD;
	if ( ! ( add_flags & NOLO_RD_FLAG_NO_LIFEGUARD ) )
		del_flags |= NOLO_RD_FLAG_NO_LIFEGUARD;
	if ( ! ( add_flags & NOLO_RD_FLAG_SERIAL_CONSOLE ) )
		del_flags |= NOLO_RD_FLAG_SERIAL_CONSOLE;
	if ( ! ( add_flags & NOLO_RD_FLAG_NO_USB_TIMEOUT ) )
		del_flags |= NOLO_RD_FLAG_NO_USB_TIMEOUT;
	if ( ! ( add_flags & NOLO_RD_FLAG_STI_CONSOLE ) )
		del_flags |= NOLO_RD_FLAG_STI_CONSOLE;
	if ( ! ( add_flags & NOLO_RD_FLAG_NO_CHARGING ) )
		del_flags |= NOLO_RD_FLAG_NO_CHARGING;
	if ( ! ( add_flags & NOLO_RD_FLAG_FORCE_POWER_KEY ) )
		del_flags |= NOLO_RD_FLAG_FORCE_POWER_KEY;

	if ( simulate )
		return 0;

	if ( usb_control_msg(dev->udev, NOLO_WRITE, NOLO_SET, add_flags, NOLO_ADD_RD_FLAGS, NULL, 0, 2000) < 0 )
		ERROR_RETURN("Cannot add R&D flags", -1);

	if ( usb_control_msg(dev->udev, NOLO_WRITE, NOLO_SET, del_flags, NOLO_DEL_RD_FLAGS, NULL, 0, 2000) < 0 )
		ERROR_RETURN("Cannot del R&D flags", -1);

	return 0;

}

int nolo_get_hwrev(struct usb_device_info * dev, char * hwrev, size_t size) {

	return nolo_identify_string(dev, "hw_rev", hwrev, size);

}

int nolo_set_hwrev(struct usb_device_info * dev, const char * hwrev) {

	printf("nolo_set_hwrev is not implemented yet\n");
	return -1;

}

int nolo_get_kernel_ver(struct usb_device_info * dev, char * ver, size_t size) {

	return nolo_get_version_string(dev, "kernel", ver, size);

}

int nolo_set_kernel_ver(struct usb_device_info * dev, const char * ver) {

	printf("nolo_set_kernel_ver is not implemented yet\n");
	return -1;

}

int nolo_get_nolo_ver(struct usb_device_info * dev, char * ver, size_t size) {

	uint32_t version;

	if ( usb_control_msg(dev->udev, NOLO_QUERY, NOLO_GET_NOLO_VERSION, 0, 0, (char *)&version, 4, 2000) < 0 )
		ERROR_RETURN("Cannot get NOLO version", -1);

	if ( (version & 255) > 1 )
		ERROR_RETURN("Invalid NOLO version", -1);

	return snprintf(ver, size, "%d.%d.%d", version >> 20 & 15, version >> 16 & 15, version >> 8 & 255);

}

int nolo_set_nolo_ver(struct usb_device_info * dev, const char * ver) {

	printf("nolo_set_nolo_ver is not implemented yet\n");
	return -1;

}

int nolo_get_sw_ver(struct usb_device_info * dev, char * ver, size_t size) {

	return nolo_get_version_string(dev, "sw-release", ver, size);

}

int nolo_set_sw_ver(struct usb_device_info * dev, const char * ver) {

	printf("nolo_set_sw_ver is not implemented yet\n");
	return -1;

}

int nolo_get_content_ver(struct usb_device_info * dev, char * ver, size_t size) {

	return nolo_get_version_string(dev, "content", ver, size);

}

int nolo_set_content_ver(struct usb_device_info * dev, const char * ver) {

	printf("nolo_set_content_ver is not implemented yet\n");
	return -1;

}


/**** OLD CODE ****/


//#define CMD_WRITE 64
//#define CMD_QUERY 192

//struct usb_dev_handle *dev;

//static void query_error_message()
//{
	/* query error message */
/*	int len = 0;
	char nolomsg[2048];
	memset(nolomsg, '\0', 2048);
	usb_control_msg(dev, 192, 5, 0, 0, nolomsg, 2048, 2000);
	nolomsg[2047] = '\0';
	printf("\nNOLO says:\n");
	if (nolomsg[0] == '\0') {
		printf(" (.. silence ..)\n");
	} else {
//		dump_bytes((unsigned char *)nolomsg, 128);
		do {
			printf(" - %s\n", nolomsg+len);
			len+=strlen(nolomsg+len)+1;
		} while(nolomsg[len]!='\0');
	}
}*/

//void flash_image(struct image * image)
//{
//	FILE *fd;
//	int vlen = 0;
//	int request;
	/*/ n800 flash queries have a variable size */
//	unsigned char fquery[256]; /* flash query */
/*	unsigned long long size, off;
	unsigned char bsize[4], tmp;
	unsigned char nolofiller[128];
	const char * piece = image_type_to_string(image->type);
	const char * version = image->version;
	ushort hash = image->hash;

//	if (piece == NULL) {
		//exit(1);
//		piece = fpid_file(filename);
		if (piece == NULL) {
			printf("Unknown piece type\n");
			return;
		}
//		printf("Piece type: %s\n", piece);
//	}*/

/*	if (piece != NULL) {
		if (!strcmp(piece, "fiasco")) {
			fiasco_flash(filename);
			return;
		}
	}*/

/*	if (version)
		vlen = strlen(version)+1;

//	fd = fopen(filename, "rb");
//	if (fd == NULL) {
//		printf("Cannot open file\n");
//		exit(1);
//	}*/
	/* cook flash query */
/*	memset(fquery, '\x00', 27);              // reset buffer
	memcpy(fquery, "\x2e\x19\x01\x01", 4);   // header
	//memcpy(fquery+5, "\xbf\x6b", 2); // some magic (modified crc16???)
	memcpy(fquery+5, &hash, 2);
	tmp = fquery[5]; fquery[5] = fquery[6]; fquery[6] = tmp;
	memcpy(fquery+7, piece, strlen(piece));  // XXX ??!??

	printf("| hash: 0x%hhx%hhx ", fquery[5], fquery[6]);
//	size = get_file_size(filename);
	size = image->size;
	bsize[0] = (size & 0xff000000) >> 24;
	bsize[1] = (size & 0x00ff0000) >> 16;
	bsize[2] = (size & 0x0000ff00) >> 8;
	bsize[3] = (size & 0x000000ff);
	printf("size: %lld (%02x %02x %02x %02x)\n",
		size, bsize[0], bsize[1], bsize[2], bsize[3]);
	memcpy(fquery+0x13, &bsize, 4);
	if (vlen) memcpy(fquery+27, version, vlen);
	
	if (!strcmp(piece, "rootfs"))
		request = 85;
	else    request = 68;

	//dump_bytes(fquery, 27+vlen);
	if (usb_control_msg(dev, CMD_WRITE, request, 0, 0, (char *)fquery, 27+vlen, 2000) <0) {
		query_error_message();
		perror("flash_image.header");
		exit(1);
	}*/

	/*/ cook and bulk nollo filler */
/*	memset(&nolofiller, '\xff', 128);
	memcpy(nolofiller+0x00, "NOLO filler", 11);
	memcpy(nolofiller+0x40, "NOLO filler", 11);
	usb_bulk_write(dev, 2, (char *)nolofiller, 128, 5000);
	usb_bulk_write(dev, 2, (char *)nolofiller, 0,   5000);*/

	/*/ bulk write image here */
/*	printf("[=] Bulkwriting the %s piece...\n", piece);
	fflush(stdout);

	#define BSIZE 0x20000

	for(off = 0; off<size; off += BSIZE) {
		char buf[BSIZE];
		int bread, bsize = size-off;
		if (bsize>BSIZE) bsize = BSIZE;
//		bread = fread(buf, bsize, 1, fd);
		bread = image_read(image, buf, bsize);
		if (bread != 1)
			printf("WARNING: Oops wrong read %d vs %d \n", bread, bsize);
		bread = usb_bulk_write(dev, 2, buf, bsize, 5000);
		if (bread == 64) {
			query_error_message();
//			fclose(fd);
			return;
		}
		printf_progressbar(off, size);
		if (bread<0) {
			printf("\n");
			perror(" -ee- ");
//			fclose(fd);
			return;
		}
		fflush(stdout);
	}
//	fclose(fd);*/
	/*/ EOF */
/*	usb_bulk_write(dev, 2, (char *)nolofiller, 0, 1000);
	printf_progressbar(1, 1);
	printf("\n");

	// index = 4????
	if (!strcmp(piece, "rootfs")) {
		if (usb_control_msg(dev, CMD_WRITE, 82, 0, 0, (char *)fquery, 0, 30000)<0) {
			fprintf(stderr, "Oops. Invalid checksum?\n");
			exit(1);
		}
	} else {
		int t = 0;
		if (!strcmp(piece, "secondary"))
			t = 1;
		else
		if (!strcmp(piece, "kernel"))
			t = 3;
		else
		if (!strcmp(piece, "initfs"))
			t = 4;
		if (!strcmp(piece, "xloader"))
			printf("xloader flashed not commiting until secondary arrives...\n");
		else
		if (usb_control_msg(dev, CMD_WRITE, 80, 0, t, (char *)fquery, 0, 10000)<0) {
			fprintf(stderr, "Oops. Invalid checksum?\n");
			exit(1);
		}
	}

	// unknown query !! :""
	if (usb_control_msg(dev, CMD_WRITE, 67, 0, 0, (char *)fquery, 0, 2000)<0) {
		fprintf(stderr, "Oops, the flash was denied or so :/\n");
		exit(1);
	}
	printf("Flash done succesfully.\n");
}*/

