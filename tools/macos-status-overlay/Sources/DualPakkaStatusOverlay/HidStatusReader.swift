import Foundation
import IOKit.hid

@MainActor
final class HidStatusReader: ObservableObject {
    private static let stadiaVendorId = 0x18d1
    private static let stadiaProductId = 0x9400
    private static let ds4VendorId = 0x054c
    private static let ds4ProductId = 0x09cc
    private static let statusUsagePage = 0xff42
    private static let statusUsage = 0x01
    private static let inputReportLength = 64

    @Published private(set) var statusText = "Not connected"
    @Published private(set) var report = StatusReport()

    private var manager: IOHIDManager?
    private var device: IOHIDDevice?
    private var inputBuffer = [UInt8](repeating: 0, count: inputReportLength)

    func start() {
        stop()

        let manager = IOHIDManagerCreate(kCFAllocatorDefault, IOOptionBits(kIOHIDOptionsTypeNone))
        let matches: [[String: Any]] = [
            matchDictionary(vendorId: Self.stadiaVendorId, productId: Self.stadiaProductId),
            matchDictionary(vendorId: Self.ds4VendorId, productId: Self.ds4ProductId),
        ]
        IOHIDManagerSetDeviceMatchingMultiple(manager, matches as CFArray)

        let context = UnsafeMutableRawPointer(Unmanaged.passUnretained(self).toOpaque())
        IOHIDManagerRegisterDeviceMatchingCallback(manager, deviceMatchedCallback, context)
        IOHIDManagerRegisterDeviceRemovalCallback(manager, deviceRemovedCallback, context)
        IOHIDManagerScheduleWithRunLoop(manager, CFRunLoopGetMain(), CFRunLoopMode.defaultMode.rawValue)

        let result = IOHIDManagerOpen(manager, IOOptionBits(kIOHIDOptionsTypeNone))
        self.manager = manager
        statusText = result == kIOReturnSuccess ? "Waiting for Status HID" : "HID open failed: \(result)"
    }

    func stop() {
        if let device {
            IOHIDDeviceUnscheduleFromRunLoop(device, CFRunLoopGetMain(), CFRunLoopMode.defaultMode.rawValue)
        }
        device = nil
        if let manager {
            IOHIDManagerUnscheduleFromRunLoop(manager, CFRunLoopGetMain(), CFRunLoopMode.defaultMode.rawValue)
            IOHIDManagerClose(manager, IOOptionBits(kIOHIDOptionsTypeNone))
        }
        manager = nil
        statusText = "Stopped"
    }

    private func matchDictionary(vendorId: Int, productId: Int) -> [String: Any] {
        [
            kIOHIDVendorIDKey: vendorId,
            kIOHIDProductIDKey: productId,
            kIOHIDPrimaryUsagePageKey: Self.statusUsagePage,
            kIOHIDPrimaryUsageKey: Self.statusUsage,
        ]
    }

    fileprivate func attach(device: IOHIDDevice) {
        if let currentDevice = self.device, currentDevice !== device {
            IOHIDDeviceUnscheduleFromRunLoop(currentDevice, CFRunLoopGetMain(), CFRunLoopMode.defaultMode.rawValue)
        }

        self.device = device
        inputBuffer = [UInt8](repeating: 0, count: Self.inputReportLength)

        let context = UnsafeMutableRawPointer(Unmanaged.passUnretained(self).toOpaque())
        inputBuffer.withUnsafeMutableBufferPointer { pointer in
            guard let baseAddress = pointer.baseAddress else {
                return
            }
            IOHIDDeviceRegisterInputReportCallback(
                device,
                baseAddress,
                pointer.count,
                inputReportCallback,
                context
            )
        }
        IOHIDDeviceScheduleWithRunLoop(device, CFRunLoopGetMain(), CFRunLoopMode.defaultMode.rawValue)
        setStatusText("Connected")
    }

    fileprivate func detach(device: IOHIDDevice) {
        if self.device === device {
            IOHIDDeviceUnscheduleFromRunLoop(device, CFRunLoopGetMain(), CFRunLoopMode.defaultMode.rawValue)
            self.device = nil
            report = StatusReport()
            setStatusText("Disconnected")
        }
    }

    fileprivate func handleInputReport(reportId: UInt32, data: UnsafeMutablePointer<UInt8>, length: CFIndex) {
        guard reportId == StatusReport.reportId else {
            return
        }

        let payload: Data
        if length == StatusReport.payloadLength {
            payload = Data(bytes: data, count: StatusReport.payloadLength)
        } else if length >= Self.inputReportLength && data.pointee == StatusReport.reportId {
            payload = Data(bytes: data.advanced(by: 1), count: StatusReport.payloadLength)
        } else {
            setStatusText("Unexpected report length: \(length)")
            return
        }

        guard let parsed = StatusReport(payload: payload) else {
            setStatusText("Failed to parse report")
            return
        }

        report = parsed
        setStatusText("Connected")
    }

    private func setStatusText(_ text: String) {
        if statusText != text {
            statusText = text
        }
    }
}

private let deviceMatchedCallback: IOHIDDeviceCallback = { context, _, _, device in
    guard let context else {
        return
    }
    let reader = Unmanaged<HidStatusReader>.fromOpaque(context).takeUnretainedValue()
    Task { @MainActor in
        reader.attach(device: device)
    }
}

private let deviceRemovedCallback: IOHIDDeviceCallback = { context, _, _, device in
    guard let context else {
        return
    }
    let reader = Unmanaged<HidStatusReader>.fromOpaque(context).takeUnretainedValue()
    Task { @MainActor in
        reader.detach(device: device)
    }
}

private let inputReportCallback: IOHIDReportCallback = {
    context,
    result,
    _,
    _,
    reportId,
    report,
    reportLength in
    guard let context, result == kIOReturnSuccess else {
        return
    }
    let reader = Unmanaged<HidStatusReader>.fromOpaque(context).takeUnretainedValue()
    Task { @MainActor in
        reader.handleInputReport(reportId: reportId, data: report, length: reportLength)
    }
}
