// swift-tools-version: 5.9

import PackageDescription

let package = Package(
    name: "DualPakkaStatusOverlay",
    platforms: [
        .macOS(.v13)
    ],
    products: [
        .executable(
            name: "DualPakkaStatusOverlay",
            targets: ["DualPakkaStatusOverlay"]
        )
    ],
    targets: [
        .executableTarget(
            name: "DualPakkaStatusOverlay"
        )
    ]
)
