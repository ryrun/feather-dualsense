package main

/*
#cgo darwin CFLAGS: -x objective-c
#cgo darwin LDFLAGS: -framework IOKit -framework CoreFoundation
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDManager.h>
#include <stdlib.h>

typedef void (*report_cb_t)(uint8_t *data, int length);

static CFMutableDictionaryRef make_match(uint16_t vendor, uint16_t product, uint32_t usage_page, uint32_t usage) {
	CFMutableDictionaryRef dict = CFDictionaryCreateMutable(
		kCFAllocatorDefault,
		0,
		&kCFTypeDictionaryKeyCallBacks,
		&kCFTypeDictionaryValueCallBacks
	);

	int value;
	CFNumberRef number;

	value = vendor;
	number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &value);
	CFDictionarySetValue(dict, CFSTR(kIOHIDVendorIDKey), number);
	CFRelease(number);

	value = product;
	number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &value);
	CFDictionarySetValue(dict, CFSTR(kIOHIDProductIDKey), number);
	CFRelease(number);

	value = usage_page;
	number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &value);
	CFDictionarySetValue(dict, CFSTR(kIOHIDPrimaryUsagePageKey), number);
	CFRelease(number);

	value = usage;
	number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &value);
	CFDictionarySetValue(dict, CFSTR(kIOHIDPrimaryUsageKey), number);
	CFRelease(number);

	return dict;
}

extern void goReportCallback(uint8_t *data, int length);

static void input_report_callback(
	void *context,
	IOReturn result,
	void *sender,
	IOHIDReportType type,
	uint32_t report_id,
	uint8_t *report,
	CFIndex report_length
) {
	if (result != kIOReturnSuccess) {
		return;
	}
	goReportCallback(report, (int)report_length);
}

static void device_matching_callback(void *context, IOReturn result, void *sender, IOHIDDeviceRef device) {
	uint8_t *buffer = (uint8_t *)context;
	IOHIDDeviceOpen(device, kIOHIDOptionsTypeNone);
	IOHIDDeviceRegisterInputReportCallback(device, buffer, 64, input_report_callback, NULL);
	IOHIDDeviceScheduleWithRunLoop(device, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
}

static int run_hid(uint8_t *buffer) {
	IOHIDManagerRef manager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
	if (manager == NULL) {
		return -1;
	}

	CFMutableDictionaryRef stadia = make_match(0x18d1, 0x9400, 0xff42, 0x01);
	CFMutableDictionaryRef ds4 = make_match(0x054c, 0x09cc, 0xff42, 0x01);
	const void *matches_values[] = { stadia, ds4 };
	CFArrayRef matches = CFArrayCreate(kCFAllocatorDefault, matches_values, 2, &kCFTypeArrayCallBacks);

	IOHIDManagerSetDeviceMatchingMultiple(manager, matches);
	IOHIDManagerRegisterDeviceMatchingCallback(manager, device_matching_callback, buffer);
	IOHIDManagerScheduleWithRunLoop(manager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);

	IOReturn result = IOHIDManagerOpen(manager, kIOHIDOptionsTypeNone);

	CFRelease(matches);
	CFRelease(stadia);
	CFRelease(ds4);

	if (result != kIOReturnSuccess) {
		CFRelease(manager);
		return (int)result;
	}

	CFRunLoopRun();
	CFRelease(manager);
	return 0;
}
*/
import "C"

import (
	"fmt"
	"unsafe"
)

var darwinReportSink chan<- []byte

func readStatusPackets(sink chan<- []byte) error {
	darwinReportSink = sink
	buffer := (*C.uint8_t)(C.malloc(C.size_t(packetLength)))
	if buffer == nil {
		return fmt.Errorf("failed to allocate HID input buffer")
	}
	defer C.free(unsafe.Pointer(buffer))

	result := C.run_hid(buffer)
	if result != 0 {
		return fmt.Errorf("IOHIDManager failed: %d", int(result))
	}
	return nil
}

//export goReportCallback
func goReportCallback(data *C.uint8_t, length C.int) {
	if darwinReportSink == nil || length <= 0 {
		return
	}
	packet := C.GoBytes(unsafe.Pointer(data), length)
	select {
	case darwinReportSink <- packet:
	default:
	}
}
