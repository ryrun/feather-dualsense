import SceneKit
import SwiftUI

struct ControllerSceneView: NSViewRepresentable {
    var report: StatusReport
    var modelURL: URL?
    var backgroundColor: NSColor
    var highlightColor: NSColor

    func makeCoordinator() -> Coordinator {
        Coordinator()
    }

    func makeNSView(context: Context) -> SCNView {
        let view = SCNView()
        view.scene = context.coordinator.scene
        view.backgroundColor = backgroundColor
        view.allowsCameraControl = false
        view.autoenablesDefaultLighting = false
        view.isPlaying = true
        view.rendersContinuously = true
        view.preferredFramesPerSecond = 60
        context.coordinator.setModelURL(modelURL)
        return view
    }

    func updateNSView(_ nsView: SCNView, context: Context) {
        nsView.backgroundColor = backgroundColor
        context.coordinator.setModelURL(modelURL)
        context.coordinator.update(
            report: report,
            backgroundColor: backgroundColor,
            highlightColor: highlightColor
        )
    }

    final class Coordinator {
        let scene = SCNScene()
        private let poseNode = SCNNode()
        private let modelRoot = SCNNode()
        private var loaded = false
        private var modelURL: URL?
        private var lastSequence: UInt16?
        private var lastUpdate = CFAbsoluteTimeGetCurrent()
        private var yaw: CGFloat = 0
        private var pitch: CGFloat = 0
        private var roll: CGFloat = 0
        private var returnToResetFade: CGFloat = 0
        private var buttonNodes: [ButtonNode] = []
        private var nodeBaseColors: [SCNNode: NSColor] = [:]
        private var stickRings: [String: SCNNode] = [:]
        private var stickPivots: [String: StickPivot] = [:]
        private var touchPointNodes: [SCNNode] = []
        private var touchTrailNodes: [[SCNNode]] = [[], []]
        private var touchTrailSamples: [[TouchTrailSample]] = [[], []]
        private var lastTouchPositions: [SCNVector3?] = [nil, nil]
        private var touchPadBounds: TouchBounds?

        init() {
            poseNode.addChildNode(modelRoot)
            scene.rootNode.addChildNode(poseNode)
            setupCamera()
            setupLights()
        }

        func setModelURL(_ url: URL?) {
            guard modelURL != url else {
                return
            }
            modelURL = url
            loaded = false
            modelRoot.childNodes.forEach { $0.removeFromParentNode() }
            buttonNodes.removeAll()
            nodeBaseColors.removeAll()
            stickRings.removeAll()
            stickPivots.removeAll()
            touchPointNodes.removeAll()
            touchTrailNodes = [[], []]
            touchTrailSamples = [[], []]
            lastTouchPositions = [nil, nil]
            touchPadBounds = nil
            loadModelIfNeeded()
        }

        func update(report: StatusReport, backgroundColor: NSColor, highlightColor: NSColor) {
            scene.background.contents = backgroundColor
            updateButtons(report: report, highlightColor: highlightColor)
            updateStickRings(report: report, highlightColor: highlightColor)
            updateTouchVisuals(report: report, highlightColor: highlightColor)
            updatePose(report: report)
        }

        private func loadModelIfNeeded() {
            guard !loaded else {
                return
            }
            loaded = true

            guard let url = modelURL,
                FileManager.default.fileExists(atPath: url.path),
                let container = try? ObjModelLoader.load(url: url)
            else {
                addMissingModelPlaceholder()
                return
            }

            prepareModel(container)
            centerAndScale(container)
            modelRoot.addChildNode(container)
        }

        private func prepareModel(_ root: SCNNode) {
            for objectNode in root.childNodes {
                let objectName = Self.elementName(for: objectNode)
                objectNode.enumerateHierarchy { node, _ in
                    guard let geometry = node.geometry else {
                        return
                    }
                    let elementName = Self.elementName(for: node, fallback: objectName)
                    let baseColor = Self.elementColor(for: elementName)
                    nodeBaseColors[node] = baseColor
                    if elementName == "TouchPad" {
                        Self.applyTouchPadMaterial(to: geometry)
                    } else {
                        Self.applyColor(baseColor, to: geometry)
                    }
                    applyPresentationOffset(node, elementName: elementName)
                    if let button = Self.button(forModelName: elementName) {
                        buttonNodes.append(ButtonNode(button: button, node: node))
                    }
                    if elementName == "Left_Stick" || elementName == "Right_Stick" {
                        let id = elementName == "Left_Stick" ? "left" : "right"
                        createStickRing(for: node, id: id)
                        createStickPivot(for: node, id: id)
                    }
                    if elementName == "TouchPad" {
                        createTouchVisuals(for: node)
                    }
                }
            }
            createMissingFallbackButtons(in: root)
        }

        private func applyColor(_ color: NSColor, to node: SCNNode) {
            node.enumerateHierarchy { child, _ in
                guard let geometry = child.geometry else {
                    return
                }
                Self.applyColor(color, to: geometry)
            }
        }

        private func createMissingFallbackButtons(in root: SCNNode) {
            let l1 = findNode(named: "L1", in: root)
            let r1 = findNode(named: "R1", in: root)
            let leftStick = findNode(named: "Left_Stick", in: root)
            let rightStick = findNode(named: "Right_Stick", in: root)

            if findNode(named: "Fn1", in: root) == nil {
                createFallbackFnButton(
                    in: root,
                    name: "Fn1",
                    source: l1,
                    stick: leftStick,
                    rotatePositive: true
                )
            }
            if findNode(named: "Fn2", in: root) == nil {
                createFallbackFnButton(
                    in: root,
                    name: "Fn2",
                    source: r1,
                    stick: rightStick,
                    rotatePositive: false
                )
            }
            if findNode(named: "Left_Paddle", in: root) == nil {
                createFallbackPaddleButton(
                    in: root,
                    name: "Left_Paddle",
                    source: l1,
                    stick: leftStick,
                    side: 1
                )
            }
            if findNode(named: "Right_Paddle", in: root) == nil {
                createFallbackPaddleButton(
                    in: root,
                    name: "Right_Paddle",
                    source: r1,
                    stick: rightStick,
                    side: -1
                )
            }
        }

        @discardableResult
        private func createFallbackFnButton(
            in root: SCNNode,
            name: String,
            source: SCNNode?,
            stick: SCNNode?,
            rotatePositive: Bool
        ) -> SCNNode? {
            guard let source,
                  let stick,
                  let geometry = source.geometry?.copy() as? SCNGeometry,
                  let sourceCenter = localMeshCenter(source),
                  let stickCenter = meshCenter(stick, in: root) else {
                return nil
            }

            let color = Self.elementColor(for: name)
            Self.assignIndependentMaterials(color, to: geometry)

            let node = SCNNode(geometry: geometry)
            node.name = name
            node.pivot = SCNMatrix4MakeTranslation(sourceCenter.x, sourceCenter.y, sourceCenter.z)
            node.position = SCNVector3(stickCenter.x, stickCenter.y - 0.043, stickCenter.z + 0.034)
            node.eulerAngles = source.eulerAngles
            node.eulerAngles.z += rotatePositive ? .pi : -.pi
            node.scale = source.scale
            node.scale.x *= 0.52
            node.scale.y *= 0.52 * 1.18
            node.scale.z *= 0.52
            root.addChildNode(node)
            registerFallbackButton(node, name: name, color: color)
            return node
        }

        @discardableResult
        private func createFallbackPaddleButton(
            in root: SCNNode,
            name: String,
            source: SCNNode?,
            stick: SCNNode?,
            side: CGFloat
        ) -> SCNNode? {
            guard let source,
                  let stick,
                  let geometry = source.geometry?.copy() as? SCNGeometry,
                  let sourceCenter = localMeshCenter(source),
                  let stickCenter = meshCenter(stick, in: root) else {
                return nil
            }

            let color = Self.elementColor(for: name)
            Self.assignIndependentMaterials(color, to: geometry)

            let node = SCNNode(geometry: geometry)
            node.name = name
            node.pivot = SCNMatrix4MakeTranslation(sourceCenter.x, sourceCenter.y, sourceCenter.z)
            node.position = SCNVector3(
                stickCenter.x + side * 0.045,
                stickCenter.y - 0.055,
                stickCenter.z + 0.092
            )
            node.eulerAngles = source.eulerAngles
            node.eulerAngles.z += (side * .pi) / 1.7
            node.scale = source.scale
            node.scale.x *= 0.72 * 2.55
            node.scale.y *= 0.72
            node.scale.z *= 0.72
            root.addChildNode(node)
            registerFallbackButton(node, name: name, color: color)
            return node
        }

        private func registerFallbackButton(_ node: SCNNode, name: String, color: NSColor) {
            nodeBaseColors[node] = color
            if let button = Self.button(forModelName: name) {
                buttonNodes.append(ButtonNode(button: button, node: node))
            }
        }

        private func findNode(named name: String, in root: SCNNode) -> SCNNode? {
            var result: SCNNode?
            root.enumerateHierarchy { node, stop in
                if Self.elementName(for: node) == name {
                    result = node
                    stop.pointee = true
                }
            }
            return result
        }

        private func meshCenter(_ node: SCNNode, in root: SCNNode) -> SCNVector3? {
            guard node.geometry != nil else {
                return nil
            }
            guard let center = localMeshCenter(node) else {
                return nil
            }
            return node.convertPosition(center, to: root)
        }

        private func localMeshCenter(_ node: SCNNode) -> SCNVector3? {
            guard node.geometry != nil else {
                return nil
            }
            let bounds = node.boundingBox
            return SCNVector3(
                (bounds.min.x + bounds.max.x) * 0.5,
                (bounds.min.y + bounds.max.y) * 0.5,
                (bounds.min.z + bounds.max.z) * 0.5
            )
        }

        private func updateButtons(report: StatusReport, highlightColor: NSColor) {
            for entry in buttonNodes {
                let pressed = (report.buttons & (UInt64(1) << UInt64(entry.button.bit))) != 0
                let base = nodeBaseColors[entry.node] ?? Self.modelBodyColor
                let color: NSColor
                if entry.button.model == "L2" {
                    color = Self.mix(
                        base, highlightColor, amount: triggerMix(report.l2, pressed: pressed))
                } else if entry.button.model == "R2" {
                    color = Self.mix(
                        base, highlightColor, amount: triggerMix(report.r2, pressed: pressed))
                } else {
                    color = pressed ? highlightColor : base
                }
                applyColor(color, to: entry.node)
            }
        }

        private func triggerMix(_ value: UInt8, pressed: Bool) -> CGFloat {
            let strength = min(1, max(0, CGFloat(value) / 255))
            guard pressed || strength > 0 else {
                return 0
            }
            return 0.35 + strength * 0.65
        }

        private func updateStickRings(report: StatusReport, highlightColor: NSColor) {
            updateStickRing(
                id: "left",
                x: Self.normalizedAxis(report.leftX),
                y: -Self.normalizedAxis(report.leftY),
                highlightColor: highlightColor
            )
            updateStickRing(
                id: "right",
                x: Self.normalizedAxis(report.rightX),
                y: -Self.normalizedAxis(report.rightY),
                highlightColor: highlightColor
            )
        }

        private func updateStickRing(id: String, x: CGFloat, y: CGFloat, highlightColor: NSColor) {
            guard let ring = stickRings[id] else {
                return
            }
            let magnitude = min(1, hypot(x, y))
            let wasActive = ring.isHidden == false
            let active = wasActive ? magnitude > 0.05 : magnitude > 0.08
            ring.isHidden = !active
            ring.enumerateHierarchy { node, _ in
                node.geometry?.materials.forEach {
                    let baseColor = Self.modelControlColor
                    let color = Self.mix(baseColor, highlightColor, amount: magnitude)
                    $0.diffuse.contents = color
                    $0.emission.contents = Self.mix(NSColor.black, color, amount: 0.2 + magnitude * 0.35)
                    if node.name?.contains("glow") == true {
                        $0.transparency = active ? 0.45 + magnitude * 0.25 : 0
                    } else {
                        $0.transparency = active ? 1 : 0
                    }
                }
            }
            updateStickTilt(id: id, x: x, y: y)
        }

        private func updateStickTilt(id: String, x: CGFloat, y: CGFloat) {
            guard let stick = stickPivots[id] else {
                return
            }
            let maxTilt: CGFloat = 0.28
            stick.pivot.eulerAngles.x = stick.baseRotation.x + y * maxTilt
            stick.pivot.eulerAngles.y = stick.baseRotation.y + x * maxTilt
        }

        private func updateTouchVisuals(report: StatusReport, highlightColor: NSColor) {
            let now = CFAbsoluteTimeGetCurrent()
            let touches = [report.touch0, report.touch1]

            for index in 0..<min(touches.count, touchPointNodes.count) {
                let touch = touches[index]
                let point = touchPointNodes[index]
                guard touch.active, let position = touchPosition(touch) else {
                    point.isHidden = true
                    lastTouchPositions[index] = nil
                    continue
                }

                point.isHidden = false
                point.position = position
                point.geometry?.materials.forEach {
                    $0.diffuse.contents = highlightColor
                    $0.emission.contents = Self.mix(NSColor.black, highlightColor, amount: 0.35)
                }
                addTouchTrailSegment(index: index, position: position, time: now)
            }

            updateTouchTrails(now: now, highlightColor: highlightColor)
        }

        private func touchPosition(_ touch: TouchPointStatus) -> SCNVector3? {
            guard let bounds = touchPadBounds else {
                return nil
            }
            let width = bounds.max.x - bounds.min.x
            let height = bounds.max.y - bounds.min.y
            let paddingX = width * 0.08
            let paddingTop = height * 0.26
            let minX = bounds.min.x + paddingX
            let maxX = bounds.max.x - paddingX
            let minY = bounds.min.y
            let maxY = bounds.max.y - paddingTop
            let normalizedX = Swift.min(CGFloat(1), Swift.max(CGFloat(0), CGFloat(touch.x) / 1919))
            let normalizedY = Swift.min(CGFloat(1), Swift.max(CGFloat(0), CGFloat(touch.y) / 1079))
            let x = maxX - normalizedX * (maxX - minX)
            let y = maxY - normalizedY * (maxY - minY)
            return SCNVector3(x, y, bounds.min.z - 0.006)
        }

        private func addTouchTrailSegment(index: Int, position: SCNVector3, time: CFTimeInterval) {
            guard index < lastTouchPositions.count else {
                return
            }
            guard let previous = lastTouchPositions[index] else {
                lastTouchPositions[index] = position
                return
            }
            let dx = position.x - previous.x
            let dy = position.y - previous.y
            let distance = sqrt(dx * dx + dy * dy)
            guard distance >= 0.008 else {
                return
            }

            touchTrailSamples[index].insert(TouchTrailSample(from: previous, to: position, time: time), at: 0)
            if touchTrailSamples[index].count > Self.touchTrailLength {
                touchTrailSamples[index].removeLast()
            }
            lastTouchPositions[index] = position
        }

        private func updateTouchTrails(now: CFTimeInterval, highlightColor: NSColor) {
            for touchIndex in 0..<touchTrailNodes.count {
                touchTrailSamples[touchIndex].removeAll {
                    now - $0.time > Self.touchTrailLifetime
                }

                for nodeIndex in 0..<touchTrailNodes[touchIndex].count {
                    let node = touchTrailNodes[touchIndex][nodeIndex]
                    guard nodeIndex < touchTrailSamples[touchIndex].count else {
                        node.isHidden = true
                        continue
                    }

                    let sample = touchTrailSamples[touchIndex][nodeIndex]
                    let ageFade = max(0, 1 - CGFloat(now - sample.time) / CGFloat(Self.touchTrailLifetime))
                    let countFade = 1 - (CGFloat(nodeIndex) / CGFloat(Self.touchTrailLength)) * 0.45
                    let dx = sample.to.x - sample.from.x
                    let dy = sample.to.y - sample.from.y
                    let length = max(0.001, sqrt(dx * dx + dy * dy))

                    node.position = SCNVector3(sample.from.x + dx * 0.5, sample.from.y + dy * 0.5, sample.from.z)
                    node.eulerAngles.z = atan2(dy, dx)
                    node.scale.x = length
                    node.geometry?.materials.forEach {
                        $0.diffuse.contents = highlightColor
                        $0.emission.contents = Self.mix(NSColor.black, highlightColor, amount: 0.25)
                        $0.transparency = max(0, min(1, 0.46 * ageFade * countFade))
                    }
                    node.isHidden = false
                }
            }
        }

        private func updatePose(report: StatusReport) {
            if lastSequence == report.sequence {
                return
            }
            lastSequence = report.sequence

            let now = CFAbsoluteTimeGetCurrent()
            let delta = min(0.05, max(1.0 / 120.0, now - lastUpdate))
            lastUpdate = now

            let gx = abs(report.gyroX) < 8 ? 0 : CGFloat(report.gyroX)
            let gy = abs(report.gyroY) < 8 ? 0 : CGFloat(report.gyroY)
            let gz = abs(report.gyroZ) < 8 ? 0 : CGFloat(report.gyroZ)
            let gyroMagnitude = sqrt(gx * gx + gy * gy + gz * gz)
            let scale: CGFloat = 0.00088

            yaw += gz * delta * scale
            pitch += gx * delta * scale
            roll += gy * delta * scale
            applyStillReturnToReset(gyroMagnitude: gyroMagnitude, delta: delta)

            poseNode.eulerAngles = SCNVector3(pitch, yaw, roll)
        }

        private func applyStillReturnToReset(gyroMagnitude: CGFloat, delta: CGFloat) {
            let motion = min(1, max(0, (gyroMagnitude - 26) / 220))
            let targetFade = 1 - motion * motion
            let fadeBlend = 1 - exp(-8 * delta)
            returnToResetFade += (targetFade - returnToResetFade) * fadeBlend
            let blend = (1 - exp(-2.6 * delta)) * returnToResetFade
            yaw += -yaw * blend
            pitch += -pitch * blend
            roll += -roll * blend
        }

        private func setupCamera() {
            let camera = SCNCamera()
            camera.zNear = 0.01
            camera.zFar = 100

            let cameraNode = SCNNode()
            cameraNode.camera = camera
            cameraNode.position = SCNVector3(0, 0.4, 4.0)
            cameraNode.look(at: SCNVector3(0, 0, 0))
            scene.rootNode.addChildNode(cameraNode)
        }

        private func setupLights() {
            let ambient = SCNLight()
            ambient.type = .ambient
            ambient.intensity = 430
            let ambientNode = SCNNode()
            ambientNode.light = ambient
            scene.rootNode.addChildNode(ambientNode)

            let key = SCNLight()
            key.type = .directional
            key.intensity = 980
            let keyNode = SCNNode()
            keyNode.light = key
            keyNode.eulerAngles = SCNVector3(-0.8, 0.5, 0.2)
            scene.rootNode.addChildNode(keyNode)
        }

        private func centerAndScale(_ node: SCNNode) {
            let bounds = node.boundingBox
            let min = bounds.min
            let maxPoint = bounds.max
            let center = SCNVector3(
                (min.x + maxPoint.x) * 0.5,
                (min.y + maxPoint.y) * 0.5,
                (min.z + maxPoint.z) * 0.5
            )
            let size = SCNVector3(maxPoint.x - min.x, maxPoint.y - min.y, maxPoint.z - min.z)
            let maxDimension = Swift.max(size.x, Swift.max(size.y, size.z))
            if maxDimension > 0 {
                let scale = 3.2 / maxDimension
                node.scale = SCNVector3(scale, scale, scale)
            }
            node.position = SCNVector3(
                -center.x * node.scale.x, -center.y * node.scale.y, -center.z * node.scale.z)
            node.eulerAngles.x = 0
            node.eulerAngles.y = .pi
            node.eulerAngles.z = 0
        }

        private func applyPresentationOffset(_ node: SCNNode, elementName: String) {
            if elementName == "L1" || elementName == "R1" {
                node.position.y += 0.007
            } else if elementName == "L2" || elementName == "R2" {
                let bounds = node.boundingBox
                let sizeY = bounds.max.y - bounds.min.y
                let stretchY: CGFloat = 1.65
                node.scale.y *= stretchY
                let offset = sizeY * (stretchY - 1) * 0.5 - 0.06
                node.position.y += offset
            }
        }

        private func createStickRing(for node: SCNNode, id: String) {
            let bounds = node.boundingBox
            let sizeX = bounds.max.x - bounds.min.x
            let sizeY = bounds.max.y - bounds.min.y
            let radius = CGFloat(max(sizeX, sizeY)) * 0.34
            guard radius > 0 else {
                return
            }
            let tube = max(radius * 0.02, 0.002)

            let group = SCNNode()
            group.name = "\(id)_stick_ring"
            group.position = SCNVector3(
                (bounds.min.x + bounds.max.x) * 0.5,
                (bounds.min.y + bounds.max.y) * 0.5,
                bounds.min.z - tube * 0.9
            )

            let core = SCNNode(geometry: SCNTorus(ringRadius: radius, pipeRadius: tube))
            core.name = "\(id)_stick_ring_core"
            core.eulerAngles.x = .pi / 2
            core.geometry?.materials = [Self.transparentMaterial()]

            let glow = SCNNode(
                geometry: SCNTorus(ringRadius: radius * 1.02, pipeRadius: tube * 1.9))
            glow.name = "\(id)_stick_ring_glow"
            glow.eulerAngles.x = .pi / 2
            glow.geometry?.materials = [Self.transparentMaterial()]

            group.addChildNode(glow)
            group.addChildNode(core)
            group.isHidden = true
            node.addChildNode(group)
            stickRings[id] = group
        }

        private func createStickPivot(for node: SCNNode, id: String) {
            guard let parent = node.parent else {
                return
            }

            let bounds = node.boundingBox
            let pivotLocal = SCNVector3(
                (bounds.min.x + bounds.max.x) * 0.5,
                (bounds.min.y + bounds.max.y) * 0.5,
                bounds.max.z + (bounds.max.z - bounds.min.z) * 0.12
            )
            let pivotWorld = node.convertPosition(pivotLocal, to: parent)
            let oldWorldTransform = node.worldTransform

            let pivot = SCNNode()
            pivot.name = "\(id)_stick_pivot"
            pivot.position = pivotWorld
            parent.addChildNode(pivot)

            node.removeFromParentNode()
            pivot.addChildNode(node)
            node.transform = pivot.convertTransform(oldWorldTransform, from: nil)

            stickPivots[id] = StickPivot(
                pivot: pivot,
                baseRotation: pivot.eulerAngles
            )
        }

        private func createTouchVisuals(for node: SCNNode) {
            guard let parent = node.parent else {
                return
            }
            let bounds = node.boundingBox
            touchPadBounds = TouchBounds(min: bounds.min, max: bounds.max)

            let width = bounds.max.x - bounds.min.x
            let height = bounds.max.y - bounds.min.y
            let radius = max(min(width, height) * 0.14, 0.006)
            let start = SCNVector3(
                (bounds.min.x + bounds.max.x) * 0.5,
                (bounds.min.y + bounds.max.y) * 0.5,
                bounds.min.z - 0.006
            )

            touchPointNodes = (0..<2).map { index in
                let plane = SCNPlane(width: radius * 2, height: radius * 2)
                plane.cornerRadius = radius
                plane.materials = [Self.touchMaterial(opacity: 1)]
                let point = SCNNode(geometry: plane)
                point.name = "TouchPoint_\(index)"
                point.position = start
                point.renderingOrder = 6
                point.isHidden = true
                parent.addChildNode(point)
                return point
            }

            touchTrailNodes = (0..<2).map { touchIndex in
                (0..<Self.touchTrailLength).map { segmentIndex in
                    let plane = SCNPlane(width: 1, height: radius * 0.52)
                    plane.materials = [Self.touchMaterial(opacity: 0)]
                    let segment = SCNNode(geometry: plane)
                    segment.name = "TouchTrail_\(touchIndex)_\(segmentIndex)"
                    segment.position = start
                    segment.renderingOrder = 5
                    segment.isHidden = true
                    parent.addChildNode(segment)
                    return segment
                }
            }
        }

        private func addMissingModelPlaceholder() {
            let text = SCNText(string: "Pass an OBJ path", extrusionDepth: 0.02)
            text.font = NSFont.systemFont(ofSize: 0.18, weight: .medium)
            text.firstMaterial?.diffuse.contents = NSColor.white
            let node = SCNNode(geometry: text)
            node.position = SCNVector3(-0.95, 0, 0)
            modelRoot.addChildNode(node)
        }

        private static func material(_ color: NSColor) -> SCNMaterial {
            let material = SCNMaterial()
            material.diffuse.contents = color
            material.roughness.contents = 0.84
            material.metalness.contents = 0.05
            material.isDoubleSided = true
            return material
        }

        private static func transparentMaterial() -> SCNMaterial {
            let material = SCNMaterial()
            material.diffuse.contents = NSColor.white
            material.emission.contents = NSColor.black
            material.transparency = 0
            material.blendMode = .alpha
            material.isDoubleSided = true
            material.lightingModel = .constant
            material.readsFromDepthBuffer = true
            material.writesToDepthBuffer = false
            return material
        }

        private static func touchMaterial(opacity: CGFloat) -> SCNMaterial {
            let material = SCNMaterial()
            material.diffuse.contents = NSColor.white
            material.emission.contents = NSColor.white
            material.transparency = opacity
            material.blendMode = .alpha
            material.isDoubleSided = true
            material.lightingModel = .constant
            material.readsFromDepthBuffer = false
            material.writesToDepthBuffer = false
            return material
        }

        private static func applyTouchPadMaterial(to geometry: SCNGeometry) {
            let material = SCNMaterial()
            material.diffuse.contents = touchPadTexture()
            material.roughness.contents = 0.98
            material.metalness.contents = 0.02
            material.isDoubleSided = true
            geometry.materials = [material]
        }

        private static func touchPadTexture() -> NSImage {
            let size = NSSize(width: 96, height: 96)
            let image = NSImage(size: size)
            image.lockFocus()

            modelBodyColor.setFill()
            NSRect(origin: .zero, size: size).fill()

            let path = NSBezierPath()
            path.lineWidth = 1
            NSColor(calibratedWhite: 0.63, alpha: 0.34).setStroke()
            stride(from: -96, through: 192, by: 12).forEach { y in
                path.move(to: NSPoint(x: -8, y: y))
                path.line(to: NSPoint(x: 104, y: y + 112))
            }
            path.stroke()

            image.unlockFocus()
            return image
        }

        private static func applyColor(_ color: NSColor, to geometry: SCNGeometry) {
            if geometry.materials.isEmpty {
                geometry.materials = [material(color)]
                return
            }

            for material in geometry.materials {
                material.lightingModel = .physicallyBased
                material.diffuse.contents = color
                material.roughness.contents = 0.84
                material.metalness.contents = 0.05
                material.isDoubleSided = true
            }
            geometry.firstMaterial?.diffuse.contents = color
        }

        private static func assignIndependentMaterials(_ color: NSColor, to geometry: SCNGeometry) {
            if geometry.materials.isEmpty {
                geometry.materials = [material(color)]
                return
            }
            geometry.materials = geometry.materials.map { _ in material(color) }
        }

        private static func button(forModelName name: String) -> SceneButton? {
            buttons.first { $0.model == name }
        }

        private static func elementName(for node: SCNNode, fallback: String = "") -> String {
            if let name = node.name, !name.isEmpty {
                return name
            }
            if let name = node.geometry?.name, !name.isEmpty {
                return name
            }
            return fallback
        }

        private static func elementColor(for name: String) -> NSColor {
            elementColors[name] ?? modelBodyColor
        }

        private static func normalizedAxis(_ value: UInt8) -> CGFloat {
            min(1, max(-1, (CGFloat(value) - 128) / 127))
        }

        private static func mix(_ a: NSColor, _ b: NSColor, amount: CGFloat) -> NSColor {
            let amount = min(1, max(0, amount))
            guard let ca = a.usingColorSpace(.deviceRGB),
                let cb = b.usingColorSpace(.deviceRGB)
            else {
                return amount >= 0.5 ? b : a
            }
            return NSColor(
                deviceRed: ca.redComponent + (cb.redComponent - ca.redComponent) * amount,
                green: ca.greenComponent + (cb.greenComponent - ca.greenComponent) * amount,
                blue: ca.blueComponent + (cb.blueComponent - ca.blueComponent) * amount,
                alpha: 1
            )
        }

        private static let modelBodyColor = NSColor(hex: 0xf7f7f2)
        private static let modelControlColor = NSColor(hex: 0x3a3a3a)
        private static let modelShoulderColor = NSColor(hex: 0x868686)
        private static let modelInnerColor = NSColor.black
        private static let touchTrailLength = 18
        private static let touchTrailLifetime: CFTimeInterval = 0.42

        private static let elementColors: [String: NSColor] = [
            "defaultMaterial.001": modelInnerColor,
            "Cross": modelBodyColor,
            "Circle": modelBodyColor,
            "Square": modelBodyColor,
            "Triangle": modelBodyColor,
            "Dpad_Up": modelBodyColor,
            "Dpad_Right": modelBodyColor,
            "Dpad_Down": modelBodyColor,
            "Dpad_Left": modelBodyColor,
            "Create": modelBodyColor,
            "Options": modelBodyColor,
            "TouchPad": modelBodyColor,
            "L1": modelShoulderColor,
            "L2": modelControlColor,
            "R1": modelShoulderColor,
            "R2": modelControlColor,
            "PS": modelControlColor,
            "Mute": modelControlColor,
            "Left_Stick": modelControlColor,
            "Right_Stick": modelControlColor,
            "BottomCover": modelControlColor,
            "InnerCover": modelInnerColor,
            "Fn1": modelShoulderColor,
            "Fn2": modelShoulderColor,
            "Left_Paddle": modelShoulderColor,
            "Right_Paddle": modelShoulderColor,
        ]

        private static let buttons: [SceneButton] = [
            .init(bit: 0, model: "Square"),
            .init(bit: 1, model: "Cross"),
            .init(bit: 2, model: "Circle"),
            .init(bit: 3, model: "Triangle"),
            .init(bit: 4, model: "L1"),
            .init(bit: 5, model: "R1"),
            .init(bit: 6, model: "L2"),
            .init(bit: 7, model: "R2"),
            .init(bit: 8, model: "Create"),
            .init(bit: 9, model: "Options"),
            .init(bit: 10, model: "Left_Stick"),
            .init(bit: 11, model: "Right_Stick"),
            .init(bit: 12, model: "TouchPad"),
            .init(bit: 13, model: "PS"),
            .init(bit: 14, model: "Mute"),
            .init(bit: 15, model: "Left_Paddle"),
            .init(bit: 16, model: "Right_Paddle"),
            .init(bit: 17, model: "Dpad_Up"),
            .init(bit: 18, model: "Dpad_Right"),
            .init(bit: 19, model: "Dpad_Down"),
            .init(bit: 20, model: "Dpad_Left"),
            .init(bit: 35, model: "Fn1"),
            .init(bit: 36, model: "Fn2"),
        ]
    }
}

extension ControllerSceneView {
    func modelURL(_ url: URL?) -> ControllerSceneView {
        var copy = self
        copy.modelURL = url
        return copy
    }
}

private struct SceneButton {
    let bit: Int
    let model: String
}

private struct ButtonNode {
    let button: SceneButton
    let node: SCNNode
}

private struct StickPivot {
    let pivot: SCNNode
    let baseRotation: SCNVector3
}

private struct TouchTrailSample {
    let from: SCNVector3
    let to: SCNVector3
    let time: CFTimeInterval
}

private struct TouchBounds {
    let min: SCNVector3
    let max: SCNVector3
}

extension NSColor {
    fileprivate convenience init(hex: UInt32) {
        self.init(
            deviceRed: CGFloat((hex >> 16) & 0xff) / 255,
            green: CGFloat((hex >> 8) & 0xff) / 255,
            blue: CGFloat(hex & 0xff) / 255,
            alpha: 1
        )
    }
}
