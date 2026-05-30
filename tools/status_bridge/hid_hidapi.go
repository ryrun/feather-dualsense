//go:build !darwin

package main

import (
	"log"
	"sync"
	"time"

	"github.com/karalabe/hid"
)

var statusInterfaces = map[int]struct{}{
	1: {},
	2: {},
	3: {},
}

var backendIDs = []struct {
	vendorID  uint16
	productID uint16
}{
	{vendorID: 0x18d1, productID: 0x9400},
	{vendorID: 0x054c, productID: 0x09cc},
}

func readStatusPackets(sink chan<- []byte) error {
	openPaths := make(map[string]struct{})
	var openMu sync.Mutex

	for {
		for _, backend := range backendIDs {
			for _, info := range hid.Enumerate(backend.vendorID, backend.productID) {
				if !isStatusCandidate(info) {
					continue
				}
				openMu.Lock()
				if _, exists := openPaths[info.Path]; exists {
					openMu.Unlock()
					continue
				}
				openPaths[info.Path] = struct{}{}
				openMu.Unlock()

				go readHidapiDevice(info, sink, func() {
					openMu.Lock()
					delete(openPaths, info.Path)
					openMu.Unlock()
				})
			}
		}
		time.Sleep(2 * time.Second)
	}
}

func isStatusCandidate(info hid.DeviceInfo) bool {
	if info.UsagePage != 0 || info.Usage != 0 {
		return info.UsagePage == statusUsagePage && info.Usage == statusUsage
	}
	_, ok := statusInterfaces[info.Interface]
	return ok
}

func readHidapiDevice(info hid.DeviceInfo, sink chan<- []byte, done func()) {
	defer done()

	device, err := info.Open()
	if err != nil {
		log.Printf("Failed to open HID path %s interface %d: %v", info.Path, info.Interface, err)
		return
	}
	defer device.Close()

	log.Printf(
		"Opened HID path %s (VID 0x%04x, PID 0x%04x, interface %d, usage 0x%04x:0x%04x)",
		info.Path,
		info.VendorID,
		info.ProductID,
		info.Interface,
		info.UsagePage,
		info.Usage,
	)

	buffer := make([]byte, packetLength)
	for {
		n, err := device.Read(buffer)
		if err != nil {
			log.Printf("Read failed on HID path %s: %v", info.Path, err)
			return
		}
		if _, ok := statusPayload(buffer[:n]); !ok {
			continue
		}
		packet := make([]byte, n)
		copy(packet, buffer[:n])
		sink <- packet
	}
}
