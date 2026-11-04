import Foundation
import SceneKit
import SwiftUI

struct DialogState: Identifiable {
    let id: Int32
    let style: Int32
    let title: String
    let message: String
    let button1: String
    let button2: String
}

struct ChatLine: Identifiable {
    let id = UUID()
    let name: String
    let text: String
    let colour: Color
    let isServer: Bool
}

/// Owns the SceneKit world and pumps the C++ client once per frame.
final class GameEngine: ObservableObject {
    let scene = SCNScene()

    @Published var loggedIn = false
    @Published var status = "не подключено"
    @Published var players = 0
    @Published var coords = ""
    @Published var chat: [ChatLine] = []
    @Published var sprint = false
    @Published var dialog: DialogState?
    @Published var logs: [String] = []
    @Published var logURL = ""

    private var timer: Timer?
    private var playerNode: SCNNode!
    private var cameraNode: SCNNode!
    private var remoteNodes: [Int32: SCNNode] = [:]
    private var lastState: Int32 = -1

    private var inputLR: Float = 0
    private var inputUD: Float = 0
    private var jumpHeld = false
    var cameraYaw: Float = 0

    init() {
        samp_init()
        buildScene()
        drainQueues()  // surface the previous session tail on the login screen
    }

    // MARK: - Scene

    private func buildScene() {
        scene.background.contents = UIColor(red: 0.42, green: 0.52, blue: 0.62, alpha: 1)

        let floor = SCNFloor()
        floor.reflectivity = 0
        floor.firstMaterial?.diffuse.contents = gridTexture()
        floor.firstMaterial?.diffuse.wrapS = .repeat
        floor.firstMaterial?.diffuse.wrapT = .repeat
        floor.firstMaterial?.diffuse.contentsTransform = SCNMatrix4MakeScale(40, 40, 1)
        floor.firstMaterial?.isDoubleSided = true
        let floorNode = SCNNode(geometry: floor)
        scene.rootNode.addChildNode(floorNode)

        let sun = SCNLight()
        sun.type = .directional
        sun.intensity = 1100
        sun.castsShadow = true
        let sunNode = SCNNode()
        sunNode.light = sun
        sunNode.eulerAngles = SCNVector3(-Float.pi / 3, Float.pi / 5, 0)
        scene.rootNode.addChildNode(sunNode)

        let ambient = SCNLight()
        ambient.type = .ambient
        ambient.intensity = 420
        ambient.color = UIColor(white: 0.86, alpha: 1)
        let ambientNode = SCNNode()
        ambientNode.light = ambient
        scene.rootNode.addChildNode(ambientNode)

        playerNode = SCNNode()
        playerNode.addChildNode(capsule(colour: UIColor(red: 0.16, green: 0.62, blue: 0.92, alpha: 1)))
        let nose = SCNCone(topRadius: 0, bottomRadius: 0.16, height: 0.36)
        nose.firstMaterial?.diffuse.contents = UIColor.white
        let noseNode = SCNNode(geometry: nose)
        noseNode.eulerAngles = SCNVector3(Float.pi / 2, 0, 0)
        noseNode.position = SCNVector3(0, 0.55, -0.42)
        playerNode.addChildNode(noseNode)
        scene.rootNode.addChildNode(playerNode)

        cameraNode = SCNNode()
        let camera = SCNCamera()
        camera.fieldOfView = 62
        camera.zNear = 0.15
        camera.zFar = 900
        cameraNode.camera = camera
        scene.rootNode.addChildNode(cameraNode)
    }

    private func gridTexture() -> UIImage {
        let size = CGSize(width: 128, height: 128)
        let renderer = UIGraphicsImageRenderer(size: size)
        return renderer.image { ctx in
            UIColor(red: 0.30, green: 0.36, blue: 0.31, alpha: 1).setFill()
            ctx.fill(CGRect(origin: .zero, size: size))
            UIColor(white: 1, alpha: 0.10).setStroke()
            let path = UIBezierPath()
            path.move(to: .zero)
            path.addLine(to: CGPoint(x: size.width, y: 0))
            path.move(to: .zero)
            path.addLine(to: CGPoint(x: 0, y: size.height))
            path.lineWidth = 3
            path.stroke()
        }
    }

    private func capsule(colour: UIColor) -> SCNNode {
        let body = SCNCapsule(capRadius: 0.34, height: 1.5)
        body.firstMaterial?.diffuse.contents = colour
        body.firstMaterial?.lightingModel = .physicallyBased
        let node = SCNNode(geometry: body)
        node.position = SCNVector3(0, 0.75, 0)
        node.castsShadow = true
        return node
    }

    private func remoteNode(for id: Int32, name: String) -> SCNNode {
        let root = SCNNode()
        root.addChildNode(capsule(colour: UIColor(red: 0.92, green: 0.45, blue: 0.28, alpha: 1)))

        let label = SCNText(string: name, extrusionDepth: 0)
        label.font = UIFont.systemFont(ofSize: 12, weight: .semibold)
        label.firstMaterial?.diffuse.contents = UIColor.white
        label.firstMaterial?.isDoubleSided = true
        let labelNode = SCNNode(geometry: label)
        labelNode.scale = SCNVector3(0.012, 0.012, 0.012)
        labelNode.position = SCNVector3(-0.25, 1.85, 0)
        let billboard = SCNBillboardConstraint()
        billboard.freeAxes = SCNBillboardAxis.Y
        labelNode.constraints = [billboard]
        root.addChildNode(labelNode)

        remoteNodes[id] = root
        scene.rootNode.addChildNode(root)
        return root
    }

    // MARK: - Session

    func connect(host: String, port: Int, nick: String) {
        guard samp_connect(host, Int32(port), nick) != 0 else {
            status = "не удалось открыть сокет"
            return
        }
        loggedIn = true
        chat.removeAll()

        let port = samp_log_port()
        if port > 0, let ip = Self.localIPv4() {
            logURL = "http://\(ip):\(port)/log"
        } else {
            logURL = "лог-сервер недоступен"
        }

        startLoop()
    }

    func disconnect() {
        stopLoop()
        samp_disconnect()
        loggedIn = false
        status = "не подключено"
    }

    func sendChat(_ text: String) {
        guard !text.isEmpty else { return }
        samp_chat_send(text)
    }

    func setInput(lr: Float, ud: Float) {
        inputLR = lr
        inputUD = ud
    }

    func setJump(_ held: Bool) {
        jumpHeld = held
    }

    func orbitCamera(byDX dx: CGFloat) {
        cameraYaw -= Float(dx) * 0.35
        while cameraYaw < 0 { cameraYaw += 360 }
        while cameraYaw >= 360 { cameraYaw -= 360 }
        samp_set_camera_yaw(cameraYaw)
    }

    private func startLoop() {
        stopLoop()
        samp_set_camera_yaw(cameraYaw)
        let timer = Timer(timeInterval: 1.0 / 60.0, repeats: true) { [weak self] _ in
            self?.tick()
        }
        RunLoop.main.add(timer, forMode: .common)
        self.timer = timer
    }

    private func stopLoop() {
        timer?.invalidate()
        timer = nil
    }

    // MARK: - Frame

    private func tick() {
        samp_set_input(inputLR, inputUD, sprint ? 1 : 0, jumpHeld ? 1 : 0)
        samp_tick()

        var snapshot = SampSnapshot()
        samp_snapshot(&snapshot)

        if snapshot.state != lastState {
            lastState = snapshot.state
            status = statusText(snapshot.state)
        }
        players = Int(snapshot.players)
        coords = String(format: "%.1f  %.1f  %.1f", snapshot.x, snapshot.y, snapshot.z)

        // GTA (x, y, z) -> SceneKit (x, z, -y)
        let playerPos = SCNVector3(snapshot.x, snapshot.z + 0.05, -snapshot.y)
        playerNode.position = playerPos
        playerNode.eulerAngles.y = snapshot.heading * .pi / 180

        updateCamera(around: playerPos)
        updateRemotes()
        drainQueues()
    }

    private func updateCamera(around target: SCNVector3) {
        let yaw = cameraYaw * .pi / 180
        let back = SCNVector3(sin(yaw) * 6.0, 3.0, cos(yaw) * 6.0)
        cameraNode.position = target.adding(back)
        cameraNode.look(at: target.adding(SCNVector3(0, 1.0, 0)),
                        up: SCNVector3(0, 1, 0), localFront: SCNVector3(0, 0, -1))
    }

    private func updateRemotes() {
        var buffer = [SampRemote](repeating: SampRemote(), count: 64)
        let count = Int(samp_players(&buffer, Int32(buffer.count)))

        var seen = Set<Int32>()
        for i in 0..<count {
            let remote = buffer[i]
            seen.insert(remote.id)
            let node = remoteNodes[remote.id] ?? remoteNode(for: remote.id, name: nameString(remote))
            node.position = SCNVector3(remote.x, remote.z + 0.05, -remote.y)
            node.eulerAngles.y = remote.heading * .pi / 180
        }
        for (id, node) in remoteNodes where !seen.contains(id) {
            node.removeFromParentNode()
            remoteNodes.removeValue(forKey: id)
        }
    }

    private func nameString(_ remote: SampRemote) -> String {
        var value = remote.name
        return withUnsafeBytes(of: &value) { raw -> String in
            let bytes = raw.prefix { $0 != 0 }
            return String(decoding: bytes, as: UTF8.self)
        }
    }

    private func drainQueues() {
        var name = [CChar](repeating: 0, count: 64)
        var text = [CChar](repeating: 0, count: 256)
        var kind: Int32 = 0
        var colour: UInt32 = 0
        while samp_poll_chat(&name, Int32(name.count), &text, Int32(text.count), &kind, &colour) != 0 {
            let line = ChatLine(name: String(cString: name),
                                text: String(cString: text),
                                colour: Self.color(from: colour),
                                isServer: kind == 1)
            chat.append(line)
            if chat.count > 40 { chat.removeFirst(chat.count - 40) }
        }

        var log = [CChar](repeating: 0, count: 256)
        var newLogs: [String] = []
        while samp_poll_log(&log, Int32(log.count)) != 0 {
            newLogs.append(String(cString: log))
        }
        if !newLogs.isEmpty {
            logs.append(contentsOf: newLogs)
            if logs.count > 60 { logs.removeFirst(logs.count - 60) }
            newLogs.forEach { print("[samp] \($0)") }
        }

        var box = SampDialog()
        if samp_dialog(&box) != 0 {
            dialog = DialogState(id: box.id,
                                 style: box.style,
                                 title: Self.cstr(&box.title),
                                 message: Self.cstr(&box.message),
                                 button1: Self.cstr(&box.button1),
                                 button2: Self.cstr(&box.button2))
        } else {
            dialog = nil
        }
    }

    func respond(button: Int32, input: String) {
        samp_dialog_response(button, input, -1)
        dialog = nil
    }

    /// IPv4 address of the Wi-Fi interface (en0), used for the log URL.
    static func localIPv4() -> String? {
        var result: String?
        var ifaddr: UnsafeMutablePointer<ifaddrs>?
        guard getifaddrs(&ifaddr) == 0, let first = ifaddr else { return nil }
        defer { freeifaddrs(ifaddr) }

        for pointer in sequence(first: first, next: { $0.pointee.ifa_next }) {
            let interface = pointer.pointee
            guard let addr = interface.ifa_addr, addr.pointee.sa_family == UInt8(AF_INET) else { continue }
            guard String(cString: interface.ifa_name) == "en0" else { continue }
            var host = [CChar](repeating: 0, count: Int(NI_MAXHOST))
            if getnameinfo(addr, socklen_t(addr.pointee.sa_len), &host,
                           socklen_t(host.count), nil, 0, NI_NUMERICHOST) == 0 {
                result = String(cString: host)
            }
        }
        return result
    }

    /// Reads a fixed-size C char array (imported as a Swift tuple) as UTF-8.
    static func cstr<T>(_ value: inout T) -> String {
        withUnsafeBytes(of: &value) { raw in
            String(decoding: raw.prefix { $0 != 0 }, as: UTF8.self)
        }
    }

    private static func color(from argb: UInt32) -> Color {
        let a = Double((argb >> 24) & 0xFF) / 255.0
        let r = Double((argb >> 16) & 0xFF) / 255.0
        let g = Double((argb >> 8) & 0xFF) / 255.0
        let b = Double(argb & 0xFF) / 255.0
        return Color(red: r, green: g, blue: b, opacity: a == 0 ? 1 : a)
    }

    private func statusText(_ state: Int32) -> String {
        switch state {
        case 0: return "не подключено"
        case 1: return "подключение…"
        case 2: return "вход на сервер…"
        case 3: return "спавн…"
        case 4: return "в игре"
        default: return "ошибка"
        }
    }
}

private extension SCNVector3 {
    func adding(_ other: SCNVector3) -> SCNVector3 {
        SCNVector3(x + other.x, y + other.y, z + other.z)
    }
}
