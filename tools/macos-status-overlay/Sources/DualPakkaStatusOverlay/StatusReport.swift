import Foundation

struct TouchPointStatus: Equatable {
    var active: Bool = false
    var id: UInt8 = 0
    var x: UInt16 = 0
    var y: UInt16 = 0
}

struct StatusReport: Equatable {
    static let reportId: UInt8 = 0x7e
    static let payloadLength = 63

    var version: UInt8 = 0
    var controllerType: UInt8 = 0
    var sequence: UInt16 = 0
    var buttons: UInt64 = 0
    var leftX: UInt8 = 128
    var leftY: UInt8 = 128
    var rightX: UInt8 = 128
    var rightY: UInt8 = 128
    var l2: UInt8 = 0
    var r2: UInt8 = 0
    var flags: UInt16 = 0
    var touch0 = TouchPointStatus()
    var touch1 = TouchPointStatus()
    var gyroX: Int16 = 0
    var gyroY: Int16 = 0
    var gyroZ: Int16 = 0
    var accelX: Int16 = 0
    var accelY: Int16 = 0
    var accelZ: Int16 = 0

    var connected: Bool { flags & (1 << 0) != 0 }
    var gyroMouseActive: Bool { flags & (1 << 3) != 0 }
    var gyroStickActive: Bool { flags & (1 << 4) != 0 }

    init() {}

    init?(payload: Data) {
        guard payload.count >= Self.payloadLength else {
            return nil
        }

        version = payload.u8(0)
        controllerType = payload.u8(1)
        sequence = payload.u16(2)
        buttons = payload.u64(4)
        leftX = payload.u8(12)
        leftY = payload.u8(13)
        rightX = payload.u8(14)
        rightY = payload.u8(15)
        l2 = payload.u8(16)
        r2 = payload.u8(17)
        flags = payload.u16(18)
        touch0 = TouchPointStatus(
            active: flags & (1 << 1) != 0,
            id: payload.u8(20),
            x: payload.u16(22),
            y: payload.u16(24)
        )
        touch1 = TouchPointStatus(
            active: flags & (1 << 2) != 0,
            id: payload.u8(21),
            x: payload.u16(26),
            y: payload.u16(28)
        )
        gyroX = payload.i16(30)
        gyroY = payload.i16(32)
        gyroZ = payload.i16(34)
        accelX = payload.i16(36)
        accelY = payload.i16(38)
        accelZ = payload.i16(40)
    }
}

private extension Data {
    func u8(_ offset: Int) -> UInt8 {
        self[startIndex + offset]
    }

    func u16(_ offset: Int) -> UInt16 {
        UInt16(u8(offset)) | (UInt16(u8(offset + 1)) << 8)
    }

    func u64(_ offset: Int) -> UInt64 {
        var value: UInt64 = 0
        for index in 0..<8 {
            value |= UInt64(u8(offset + index)) << UInt64(index * 8)
        }
        return value
    }

    func i16(_ offset: Int) -> Int16 {
        Int16(bitPattern: u16(offset))
    }
}
