import AppKit
import SwiftUI

@main
struct DualPakkaStatusOverlayApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) private var appDelegate
    @StateObject private var reader = HidStatusReader()
    private let modelURL = CommandLineModelURL.resolve()

    var body: some Scene {
        WindowGroup {
            OverlayView(reader: reader, modelURL: modelURL)
                .onAppear {
                    reader.start()
                }
                .onDisappear {
                    reader.stop()
                }
        }
    }
}

private enum CommandLineModelURL {
    static func resolve() -> URL? {
        let arguments = CommandLine.arguments.dropFirst()
        guard let path = arguments.first, !path.isEmpty else {
            return nil
        }

        let expandedPath: String
        if path.hasPrefix("~/") {
            expandedPath = NSString(string: path).expandingTildeInPath
        } else {
            expandedPath = path
        }

        let url = URL(fileURLWithPath: expandedPath)
        if url.path.hasPrefix("/") {
            return url
        }
        return URL(fileURLWithPath: FileManager.default.currentDirectoryPath)
            .appendingPathComponent(expandedPath)
    }
}

final class AppDelegate: NSObject, NSApplicationDelegate {
    private var activity: NSObjectProtocol?

    func applicationDidFinishLaunching(_ notification: Notification) {
        NSApp.setActivationPolicy(.regular)
        NSApp.activate(ignoringOtherApps: true)
        activity = ProcessInfo.processInfo.beginActivity(
            options: [.userInitiated, .latencyCritical],
            reason: "Keep DualPakka status overlay rendering smoothly"
        )
    }

    func applicationWillTerminate(_ notification: Notification) {
        if let activity {
            ProcessInfo.processInfo.endActivity(activity)
        }
    }

    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool {
        true
    }
}
