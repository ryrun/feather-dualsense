import SwiftUI

struct OverlayView: View {
    @ObservedObject var reader: HidStatusReader
    var modelURL: URL?
    @State private var sceneBackgroundColor = AppColors.obsBackground
    @State private var sceneHighlightColor = AppColors.sceneHighlight
    @State private var sceneOnly = false
    @State private var window: NSWindow?
    @State private var didApplyInitialWindowMode = false

    var body: some View {
        Group {
            if sceneOnly {
                sceneOnlyView
            } else {
                VStack(alignment: .leading, spacing: 14) {
                    header

                    HStack(alignment: .top, spacing: 14) {
                        scenePanel
                            .frame(minWidth: 430, maxWidth: .infinity)
                        VStack(spacing: 14) {
                            controllerPanel
                            motionPanel
                        }
                        .frame(width: 310)
                    }

                    HStack(alignment: .top, spacing: 14) {
                        sticksPanel
                        touchPanel
                        flagsPanel
                    }
                    .frame(maxHeight: .infinity, alignment: .top)
                }
                .padding(18)
                .frame(
                    minWidth: WindowMetrics.normalContentSize.width,
                    minHeight: WindowMetrics.normalContentSize.height
                )
                .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
                .background(AppColors.background)
                .foregroundStyle(AppColors.primary)
            }
        }
        .background(
            WindowAccessor { resolvedWindow in
                if let resolvedWindow {
                    window = resolvedWindow
                    if !didApplyInitialWindowMode {
                        didApplyInitialWindowMode = true
                        applyWindowMode(resize: true)
                    }
                }
            }
            .frame(width: 0, height: 0)
        )
        .onChange(of: sceneOnly) { _ in
            applyWindowMode(resize: true)
        }
    }

    private var header: some View {
        HStack(spacing: 12) {
            VStack(alignment: .leading, spacing: 3) {
                Text("Feather DualSense HID Remapper")
                    .font(.system(size: 18, weight: .semibold))
                Text(reader.statusText)
                    .font(.system(size: 12))
                    .foregroundStyle(AppColors.secondary)
            }

            Spacer()

            statusPill("Status HID", active: reader.report.connected)
            statusPill("Gyro Mouse", active: reader.report.gyroMouseActive)
            statusPill("Gyro Stick", active: reader.report.gyroStickActive)
        }
    }

    private var controllerPanel: some View {
        panel("Controller") {
            infoGrid {
                row("Controller", controllerName(reader.report.controllerType))
                row("Report Version", "\(reader.report.version)")
                row("Sequence", "\(reader.report.sequence)")
                row("Buttons", "0x\(String(reader.report.buttons, radix: 16))")
                row("L2 / R2", "\(reader.report.l2) / \(reader.report.r2)")
            }
        }
    }

    private var scenePanel: some View {
        panel("3D") {
            sceneControls

            sceneView
                .frame(height: 300)
                .clipShape(RoundedRectangle(cornerRadius: 6))
        }
    }

    private var sceneOnlyView: some View {
        sceneView
            .frame(maxWidth: .infinity, maxHeight: .infinity)
            .background(sceneBackgroundColor)
            .ignoresSafeArea()
            .focusable()
            .onExitCommand {
                sceneOnly = false
            }
    }

    private var sceneView: some View {
        ControllerSceneView(
            report: reader.report,
            backgroundColor: NSColor(sceneBackgroundColor),
            highlightColor: NSColor(sceneHighlightColor)
        )
        .modelURL(modelURL)
    }

    private var sceneControls: some View {
        HStack(spacing: 12) {
            ColorPicker("3D BG", selection: $sceneBackgroundColor)
                .labelsHidden()
            Text("3D BG")
                .font(.system(size: 11))
                .foregroundStyle(AppColors.secondary)
            ColorPicker("3D Highlight", selection: $sceneHighlightColor)
                .labelsHidden()
            Text("3D Highlight")
                .font(.system(size: 11))
                .foregroundStyle(AppColors.secondary)
            Spacer()
            Button("3D Only") {
                sceneOnly = true
            }
        }
    }

    private func applyWindowMode(resize: Bool) {
        guard let window else {
            return
        }

        if sceneOnly {
            window.styleMask = [.borderless]
            window.minSize = WindowMetrics.sceneOnlyContentSize
            window.maxSize = WindowMetrics.sceneOnlyContentSize
            if resize {
                resizeWindowKeepingTopLeft(window, contentSize: WindowMetrics.sceneOnlyContentSize)
            }
        } else {
            window.minSize = WindowMetrics.normalContentSize
            window.maxSize = WindowMetrics.unrestrictedMaxSize
            window.styleMask = [.titled, .closable, .miniaturizable, .resizable]
            window.titleVisibility = .visible
            window.titlebarAppearsTransparent = false
            window.contentMinSize = WindowMetrics.normalContentSize
            if resize {
                resizeWindowKeepingTopLeft(window, contentSize: WindowMetrics.normalContentSize)
            }
        }
    }

    private func resizeWindowKeepingTopLeft(_ window: NSWindow, contentSize: NSSize) {
        let currentFrame = window.frame
        let newFrame = window.frameRect(
            forContentRect: NSRect(origin: currentFrame.origin, size: contentSize))
        let adjustedOrigin = NSPoint(
            x: currentFrame.minX,
            y: currentFrame.maxY - newFrame.height
        )
        window.setFrame(NSRect(origin: adjustedOrigin, size: newFrame.size), display: true)
    }

    private var motionPanel: some View {
        panel("Motion") {
            infoGrid {
                row("Gyro X", "\(reader.report.gyroX)")
                row("Gyro Y", "\(reader.report.gyroY)")
                row("Gyro Z", "\(reader.report.gyroZ)")
                row("Accel X", "\(reader.report.accelX)")
                row("Accel Y", "\(reader.report.accelY)")
                row("Accel Z", "\(reader.report.accelZ)")
            }
        }
    }

    private var sticksPanel: some View {
        panel("Sticks") {
            HStack(spacing: 18) {
                stickView(
                    title: "Left",
                    x: normalizedAxis(reader.report.leftX),
                    y: normalizedAxis(reader.report.leftY)
                )
                stickView(
                    title: "Right",
                    x: normalizedAxis(reader.report.rightX),
                    y: normalizedAxis(reader.report.rightY)
                )
            }
            .frame(height: 124, alignment: .top)
        }
    }

    private var touchPanel: some View {
        panel("Touch") {
            VStack(alignment: .leading, spacing: 10) {
                touchRow("Touch 0", reader.report.touch0)
                touchRow("Touch 1", reader.report.touch1)
            }
        }
    }

    private var flagsPanel: some View {
        panel("Flags") {
            VStack(alignment: .leading, spacing: 8) {
                statusPill("Connected", active: reader.report.connected)
                statusPill("Touch 0", active: reader.report.touch0.active)
                statusPill("Touch 1", active: reader.report.touch1.active)
                statusPill("Gyro Mouse", active: reader.report.gyroMouseActive)
                statusPill("Gyro Stick", active: reader.report.gyroStickActive)
            }
        }
    }

    private func panel<Content: View>(
        _ title: String,
        @ViewBuilder content: () -> Content
    ) -> some View {
        VStack(alignment: .leading, spacing: 12) {
            Text(title)
                .font(.system(size: 13, weight: .semibold))
                .foregroundStyle(AppColors.primary)
            content()
        }
        .padding(14)
        .frame(maxWidth: .infinity, alignment: .topLeading)
        .background(AppColors.panel)
        .overlay(
            RoundedRectangle(cornerRadius: 8)
                .stroke(AppColors.line, lineWidth: 1)
        )
        .clipShape(RoundedRectangle(cornerRadius: 8))
    }

    private func infoGrid<Content: View>(
        @ViewBuilder content: () -> Content
    ) -> some View {
        Grid(alignment: .leading, horizontalSpacing: 18, verticalSpacing: 7) {
            content()
        }
        .font(.system(size: 12))
    }

    private func row(_ label: String, _ value: String) -> some View {
        GridRow {
            Text(label)
                .foregroundStyle(AppColors.secondary)
            Text(value)
                .fontDesign(.monospaced)
                .lineLimit(1)
        }
    }

    private func touchRow(_ label: String, _ touch: TouchPointStatus) -> some View {
        HStack {
            statusPill(label, active: touch.active)
            Text(touch.active ? "id \(touch.id), x \(touch.x), y \(touch.y)" : "inactive")
                .font(.system(size: 12, design: .monospaced))
                .foregroundStyle(AppColors.secondary)
        }
    }

    private func statusPill(_ label: String, active: Bool) -> some View {
        Text(label)
            .font(.system(size: 11, weight: .medium))
            .padding(.horizontal, 9)
            .padding(.vertical, 5)
            .foregroundStyle(active ? Color.black : AppColors.secondary)
            .background(active ? AppColors.highlight : AppColors.control)
            .clipShape(Capsule())
    }

    private func stickView(title: String, x: Double, y: Double) -> some View {
        VStack(spacing: 8) {
            ZStack {
                Circle()
                    .fill(AppColors.control)
                Circle()
                    .stroke(AppColors.line, lineWidth: 1)
                Circle()
                    .fill(AppColors.highlight)
                    .frame(width: 12, height: 12)
                    .offset(x: x * 38, y: y * 38)
            }
            .frame(width: 96, height: 96)

            Text("\(title) \(x, specifier: "%.2f") / \(y, specifier: "%.2f")")
                .font(.system(size: 11, design: .monospaced))
                .foregroundStyle(AppColors.secondary)
                .lineLimit(1)
                .minimumScaleFactor(0.82)
                .frame(width: 112, height: 14)
        }
        .frame(width: 112, height: 118, alignment: .top)
    }

    private func controllerName(_ value: UInt8) -> String {
        switch value {
        case 1:
            "DualSense"
        case 2:
            "DualSense Edge"
        default:
            "Unknown (\(value))"
        }
    }

    private func normalizedAxis(_ value: UInt8) -> Double {
        min(1, max(-1, (Double(value) - 128) / 127))
    }
}

private enum AppColors {
    static let background = Color(red: 0.06, green: 0.07, blue: 0.08)
    static let panel = Color(red: 0.09, green: 0.11, blue: 0.13)
    static let control = Color(red: 0.13, green: 0.16, blue: 0.19)
    static let line = Color(red: 0.18, green: 0.22, blue: 0.27)
    static let primary = Color(red: 0.95, green: 0.97, blue: 0.99)
    static let secondary = Color(red: 0.62, green: 0.68, blue: 0.75)
    static let highlight = Color(red: 0.58, green: 0.94, blue: 0.74)
    static let obsBackground = Color(red: 1.0, green: 0.0, blue: 0.86)
    static let sceneHighlight = Color(red: 1.0, green: 0.02, blue: 0.0)
}

private enum WindowMetrics {
    static let normalContentSize = NSSize(width: 1280, height: 740)
    static let sceneOnlyContentSize = NSSize(width: 800, height: 600)
    static let unrestrictedMaxSize = NSSize(
        width: CGFloat.greatestFiniteMagnitude,
        height: CGFloat.greatestFiniteMagnitude
    )
}
