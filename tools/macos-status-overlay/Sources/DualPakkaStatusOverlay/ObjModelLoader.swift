import Foundation
import SceneKit

enum ObjModelLoader {
    static func load(url: URL) throws -> SCNNode {
        let text = try String(contentsOf: url, encoding: .utf8)
        var positions: [SCNVector3] = []
        var normals: [SCNVector3] = []
        var objects: [ObjMeshBuilder] = []
        var current = ObjMeshBuilder(name: "default")

        func finishCurrentObject() {
            if !current.vertices.isEmpty {
                objects.append(current)
            }
        }

        for rawLine in text.split(separator: "\n", omittingEmptySubsequences: true) {
            let line = rawLine.trimmingCharacters(in: .whitespacesAndNewlines)
            if line.isEmpty || line.hasPrefix("#") || line.hasPrefix("mtllib ") || line.hasPrefix("usemtl ") {
                continue
            }

            if line.hasPrefix("o ") || line.hasPrefix("g ") {
                finishCurrentObject()
                let name = String(line.dropFirst(2)).trimmingCharacters(in: .whitespaces)
                current = ObjMeshBuilder(name: name.isEmpty ? "unnamed" : name)
                continue
            }

            if line.hasPrefix("v ") {
                let parts = line.split(separator: " ")
                guard parts.count >= 4,
                      let x = Float(parts[1]),
                      let y = Float(parts[2]),
                      let z = Float(parts[3]) else {
                    continue
                }
                positions.append(SCNVector3(x, y, z))
                continue
            }

            if line.hasPrefix("vn ") {
                let parts = line.split(separator: " ")
                guard parts.count >= 4,
                      let x = Float(parts[1]),
                      let y = Float(parts[2]),
                      let z = Float(parts[3]) else {
                    continue
                }
                normals.append(SCNVector3(x, y, z))
                continue
            }

            if line.hasPrefix("f ") {
                let tokens = line.split(separator: " ").dropFirst()
                let faceIndices = tokens.compactMap { token -> Int32? in
                    let parts = token.split(separator: "/", omittingEmptySubsequences: false)
                    guard let positionIndex = resolveObjIndex(parts.first, count: positions.count) else {
                        return nil
                    }
                    let normalIndex = parts.count >= 3
                        ? resolveObjIndex(parts[2], count: normals.count)
                        : nil
                    return current.append(
                        position: positions[positionIndex],
                        normal: normalIndex.map { normals[$0] }
                    )
                }

                guard faceIndices.count >= 3 else {
                    continue
                }
                for index in 1..<(faceIndices.count - 1) {
                    current.indices.append(faceIndices[0])
                    current.indices.append(faceIndices[index])
                    current.indices.append(faceIndices[index + 1])
                }
            }
        }
        finishCurrentObject()

        let root = SCNNode()
        for object in objects {
            guard let node = object.makeNode() else {
                continue
            }
            root.addChildNode(node)
        }
        return root
    }

    private static func resolveObjIndex(_ value: Substring?, count: Int) -> Int? {
        guard let value, let raw = Int(value), raw != 0 else {
            return nil
        }
        let index = raw > 0 ? raw - 1 : count + raw
        guard index >= 0 && index < count else {
            return nil
        }
        return index
    }
}

private struct ObjMeshBuilder {
    var name: String
    var vertices: [SCNVector3] = []
    var normals: [SCNVector3] = []
    var indices: [Int32] = []

    mutating func append(position: SCNVector3, normal: SCNVector3?) -> Int32 {
        vertices.append(position)
        normals.append(normal ?? SCNVector3(0, 0, 1))
        return Int32(vertices.count - 1)
    }

    func makeNode() -> SCNNode? {
        guard !vertices.isEmpty, !indices.isEmpty else {
            return nil
        }

        let geometry = SCNGeometry(
            sources: [
                SCNGeometrySource(vertices: vertices),
                SCNGeometrySource(normals: normals),
            ],
            elements: [
                SCNGeometryElement(indices: indices, primitiveType: .triangles),
            ]
        )
        geometry.name = name
        geometry.materials = [SCNMaterial()]

        let node = SCNNode(geometry: geometry)
        node.name = name
        return node
    }
}
