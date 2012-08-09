/*
    0xFFFF - Open Free Fiasco Firmware Flasher
    Copyright (C) 2012  Pali Rohár <pali.rohar@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <usb.h>

#include "global.h"
#include "device.h"
#include "usb-device.h"

static int prev = 0;
#define PRINTF_BACK() do { if ( prev ) { printf("\r%-*s\r", prev, ""); prev = 0; } } while (0)
#define PRINTF_ADD(format, ...) do { prev += printf(format, ##__VA_ARGS__); } while (0)
#define PRINTF_LINE(format, ...) do { PRINTF_BACK(); PRINTF_ADD(format, ##__VA_ARGS__); fflush(stdout); } while (0)
#define PRINTF_END() do { if ( prev ) { printf("\n"); prev = 0; } } while (0)
#define PRINTF_ERROR(errno, format, ...) do { PRINTF_END(); ERROR(errno, format, ##__VA_ARGS__); } while (0)

static struct usb_flash_device usb_devices[] = {
	{ 0x0421, 0x0105,  2,  1, -1, FLASH_NOLO, { DEVICE_SU_18, DEVICE_RX_44, DEVICE_RX_48, DEVICE_RX_51, 0 } },
	{ 0x0421, 0x0106,  0, -1, -1, FLASH_COLD, { DEVICE_RX_51, 0 } },
	{ 0x0421, 0x01c7,  0, -1, -1, FLASH_DISK, { DEVICE_RX_51, 0 } },
	{ 0x0421, 0x01c8, -1, -1, -1, FLASH_MKII, { DEVICE_RX_51, 0 } },
	{ 0x0421, 0x0431,  0, -1, -1, FLASH_DISK, { DEVICE_SU_18, DEVICE_RX_34, 0 } },
	{ 0x0421, 0x3f00,  2,  1, -1, FLASH_NOLO, { DEVICE_RX_34, 0 } },
	{ 0, 0, -1, -1, -1, 0, {} }
};

static const char * usb_flash_protocols[] = {
	[FLASH_NOLO] = "NOLO",
	[FLASH_COLD] = "Cold flashing",
	[FLASH_MKII] = "Mkii protcol",
	[FLASH_DISK] = "RAW disk",
};

const char * usb_flash_protocol_to_string(enum usb_flash_protocol protocol) {

	if ( protocol > sizeof(usb_flash_protocols) )
		return NULL;

	return usb_flash_protocols[protocol];

}

static void usb_device_info_print(const struct usb_flash_device * dev) {

	int i;

	PRINTF_ADD("USB device %s", device_to_string(dev->devices[0]));

	for ( i = 1; dev->devices[i]; ++i )
		PRINTF_ADD("/%s", device_to_string(dev->devices[i]));

	PRINTF_ADD(" (%#04x:%#04x) in %s mode", dev->vendor, dev->product, usb_flash_protocol_to_string(dev->protocol));

}

static struct usb_device_info * usb_device_is_valid(struct usb_device * dev) {

	int i;
	struct usb_device_info * ret = NULL;

	for ( i = 0; usb_devices[i].vendor; ++i ) {

		if ( dev->descriptor.idVendor == usb_devices[i].vendor && dev->descriptor.idProduct == usb_devices[i].product ) {

			PRINTF_END();
			PRINTF_ADD("Found ");
			usb_device_info_print(&usb_devices[i]);
			PRINTF_END();

			PRINTF_LINE("Opening USB...");
			usb_dev_handle * udev = usb_open(dev);
			if ( ! udev ) {
				PRINTF_ERROR(errno, "usb_open failed");
				return NULL;
			}

#if defined(LIBUSB_HAS_GET_DRIVER_NP) && defined(LIBUSB_HAS_DETACH_KERNEL_DRIVER_NP)
			PRINTF_LINE("Detaching kernel from USB interface...");
			usb_detach_kernel_driver_np(udev, usb_devices[i].interface);
#endif

			PRINTF_LINE("Claiming USB interface...");
			if ( usb_claim_interface(udev, usb_devices[i].interface) < 0 ) {
				PRINTF_ERROR(errno, "usb_claim_interface failed");
				usb_close(udev);
				return NULL;
			}

			if ( usb_devices[i].alternate >= 0 ) {
				PRINTF_LINE("Setting alternate USB interface...");
				if ( usb_set_altinterface(udev, usb_devices[i].alternate) < 0 ) {
					PRINTF_ERROR(errno, "usb_claim_interface failed");
					usb_close(udev);
					return NULL;
				}
			}

			if ( usb_devices[i].configuration >= 0 ) {
				PRINTF_LINE("Setting USB configuration...");
				if ( usb_set_configuration(udev, usb_devices[i].configuration) < 0 ) {
					PRINTF_ERROR(errno, "usb_set_configuration failed");
					usb_close(udev);
					return NULL;
				}
			}

			ret = malloc(sizeof(struct usb_device_info));
			if ( ! ret ) {
				ALLOC_ERROR();
				usb_close(udev);
				return NULL;
			}

			ret->dev = udev;
			ret->info = &usb_devices[i];
			break;
		}
	}

	return ret;

}

static struct usb_device_info * usb_search_device(struct usb_device * dev, int level) {

	int i;
	struct usb_device_info * ret = NULL;

	if ( ! dev )
		return NULL;

	ret = usb_device_is_valid(dev);
	if ( ret )
		return ret;

	for ( i = 0; i < dev->num_children; i++ ) {
		ret = usb_search_device(dev->children[i], level + 1);
		if ( ret )
			break;
	}

	return ret;

}

struct usb_device_info * usb_open_and_wait_for_device(void) {

	struct usb_bus * bus;
	struct usb_device_info * ret = NULL;
	int i = 0;
	static char progress[] = {'/','-','\\', '|'};

	usb_init();
	usb_find_busses();

	while ( 1 ) {

		PRINTF_LINE("Waiting for USB device... %c", progress[++i%sizeof(progress)]);

		usb_find_devices();

		for ( bus = usb_get_busses(); bus; bus = bus->next ) {

			if ( bus->root_dev )
				ret = usb_search_device(bus->root_dev, 0);
			else {
				struct usb_device *dev;
				for ( dev = bus->devices; dev; dev = dev->next ) {
					ret = usb_search_device(dev, 0);
					if ( ret )
						break;
				}
			}

			if ( ret )
				break;

		}

		if ( ret )
			break;

		usleep(0xc350); // 0.5s

	}

	PRINTF_BACK();

	if ( ! ret )
		return NULL;

	return ret;

}

void usb_close_device(struct usb_device_info * dev) {

	free(dev->dev);
	free(dev);

}
