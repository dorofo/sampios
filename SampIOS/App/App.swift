import SceneKit
import SwiftUI

@main
struct SampApp: App {
    @StateObject private var engine = GameEngine()

    var body: some Scene {
        WindowGroup {
            RootView(engine: engine)
                .statusBarHidden(true)
                .preferredColorScheme(.dark)
        }
    }
}

struct RootView: View {
    @ObservedObject var engine: GameEngine

    var body: some View {
        if engine.loggedIn {
            GameView(engine: engine)
        } else {
            LoginView(engine: engine)
        }
    }
}

// MARK: - Login

struct LoginView: View {
    @ObservedObject var engine: GameEngine

    @State private var host = "192.168.1.1"
    @State private var port = "7777"
    @State private var nick = "iplayer"

    var body: some View {
        ZStack {
            LinearGradient(colors: [Color(red: 0.09, green: 0.13, blue: 0.17),
                                    Color(red: 0.16, green: 0.22, blue: 0.26)],
                           startPoint: .topLeading, endPoint: .bottomTrailing)
                .ignoresSafeArea()

            VStack(alignment: .leading, spacing: 18) {
                Text("SAMP iOS")
                    .font(.system(size: 34, weight: .heavy, design: .rounded))
                    .foregroundStyle(.white)
                Text("сервер, ник — и в игру")
                    .font(.system(size: 14, weight: .medium, design: .rounded))
                    .foregroundStyle(.white.opacity(0.55))

                field("Сервер", text: $host, keyboard: .URL)
                field("Порт", text: $port, keyboard: .numberPad)
                field("Ник", text: $nick, keyboard: .default)

                Button {
                    engine.connect(host: host, port: Int(port) ?? 7777, nick: nick)
                } label: {
                    Text("Войти")
                        .font(.system(size: 18, weight: .bold, design: .rounded))
                        .frame(maxWidth: .infinity)
                        .padding(.vertical, 14)
                        .background(Color(red: 0.16, green: 0.62, blue: 0.92))
                        .foregroundStyle(.white)
                        .clipShape(RoundedRectangle(cornerRadius: 14, style: .continuous))
                }

                Text(engine.status)
                    .font(.system(size: 13, design: .monospaced))
                    .foregroundStyle(.white.opacity(0.6))
            }
            .padding(28)
            .frame(maxWidth: 420)
        }
    }

    private func field(_ title: String, text: Binding<String>, keyboard: UIKeyboardType) -> some View {
        VStack(alignment: .leading, spacing: 6) {
            Text(title.uppercased())
                .font(.system(size: 11, weight: .bold, design: .rounded))
                .foregroundStyle(.white.opacity(0.45))
            TextField("", text: text)
                .keyboardType(keyboard)
                .autocapitalization(.none)
                .disableAutocorrection(true)
                .padding(12)
                .background(Color.white.opacity(0.08))
                .clipShape(RoundedRectangle(cornerRadius: 10, style: .continuous))
                .foregroundStyle(.white)
        }
    }
}

// MARK: - Game

struct GameView: View {
    @ObservedObject var engine: GameEngine
    @State private var draft = ""
    @State private var showChat = false
    @State private var joystick = CGPoint.zero
    @State private var lookStart: CGFloat?

    var body: some View {
        ZStack {
            SceneKitView(scene: engine.scene)
                .ignoresSafeArea()

            // look-around: drag on the right half of the screen
            Color.clear
                .contentShape(Rectangle())
                .gesture(
                    DragGesture(minimumDistance: 4)
                        .onChanged { value in
                            if lookStart == nil { lookStart = value.location.x }
                            engine.orbitCamera(byDX: value.translation.width - (lookStart == nil ? 0 : prevDX))
                            prevDX = value.translation.width
                        }
                        .onEnded { _ in
                            lookStart = nil
                            prevDX = 0
                        }
                )
                .allowsHitTesting(!showChat)

            VStack {
                HStack(alignment: .top) {
                    VStack(alignment: .leading, spacing: 2) {
                        Text(engine.status)
                            .font(.system(size: 13, weight: .bold, design: .rounded))
                        Text("\(engine.players) игроков · \(engine.coords)")
                            .font(.system(size: 11, design: .monospaced))
                            .opacity(0.7)
                    }
                    .foregroundStyle(.white)
                    .padding(8)
                    .background(Color.black.opacity(0.35))
                    .clipShape(RoundedRectangle(cornerRadius: 10, style: .continuous))

                    Spacer()

                    Button { engine.disconnect() } label: {
                        Text("Выйти")
                            .font(.system(size: 12, weight: .bold, design: .rounded))
                            .padding(.horizontal, 12)
                            .padding(.vertical, 8)
                            .background(Color.black.opacity(0.35))
                            .clipShape(Capsule())
                            .foregroundStyle(.white)
                    }
                }
                Spacer()
                chatPanel
                controls
            }
            .padding(14)
        }
    }

    @State private var prevDX: CGFloat = 0

    private var chatPanel: some View {
        VStack(alignment: .leading, spacing: 6) {
            ForEach(engine.chat.suffix(6)) { line in
                Text(line.isServer ? line.text : (line.name.isEmpty ? line.text : "\(line.name): \(line.text)"))
                    .font(.system(size: 12, weight: .medium, design: .rounded))
                    .foregroundStyle(line.isServer ? Color(red: 1, green: 0.85, blue: 0.45) : line.colour)
                    .shadow(radius: 1)
            }
            HStack(spacing: 8) {
                TextField("сообщение…", text: $draft)
                    .textFieldStyle(.plain)
                    .font(.system(size: 13, design: .rounded))
                    .padding(8)
                    .background(Color.black.opacity(0.45))
                    .clipShape(RoundedRectangle(cornerRadius: 8, style: .continuous))
                    .foregroundStyle(.white)
                    .onTapGesture { showChat = true }
                Button {
                    engine.sendChat(draft)
                    draft = ""
                    showChat = false
                } label: {
                    Image(systemName: "paperplane.fill")
                        .padding(9)
                        .background(Color(red: 0.16, green: 0.62, blue: 0.92))
                        .clipShape(Circle())
                        .foregroundStyle(.white)
                }
            }
        }
        .frame(maxWidth: 460, alignment: .leading)
        .padding(.bottom, 6)
    }

    private var controls: some View {
        HStack(alignment: .bottom) {
            JoystickView { point in
                engine.setInput(lr: Float(point.x), ud: Float(-point.y))
            }
            .frame(width: 132, height: 132)

            Spacer()

            VStack(spacing: 12) {
                Button {
                    engine.sprint.toggle()
                } label: {
                    Text("БЕГ")
                        .font(.system(size: 13, weight: .heavy, design: .rounded))
                        .frame(width: 64, height: 64)
                        .background(engine.sprint ? Color(red: 0.95, green: 0.55, blue: 0.15) : Color.black.opacity(0.4))
                        .clipShape(Circle())
                        .foregroundStyle(.white)
                }
                Button {
                    engine.setJump(true)
                    DispatchQueue.main.asyncAfter(deadline: .now() + 0.18) { engine.setJump(false) }
                } label: {
                    Text("ПРЫЖ")
                        .font(.system(size: 13, weight: .heavy, design: .rounded))
                        .frame(width: 64, height: 64)
                        .background(Color.black.opacity(0.4))
                        .clipShape(Circle())
                        .foregroundStyle(.white)
                }
            }
        }
    }
}

struct SceneKitView: UIViewRepresentable {
    let scene: SCNScene

    func makeUIView(context: Context) -> SCNView {
        let view = SCNView()
        view.scene = scene
        view.backgroundColor = .black
        view.antialiasingMode = .multisampling2X
        view.isPlaying = true
        view.rendersContinuously = true
        view.pointOfView = scene.rootNode.childNodes.first { $0.camera != nil }
        return view
    }

    func updateUIView(_ uiView: SCNView, context: Context) {}
}

struct JoystickView: View {
    var onChange: (CGPoint) -> Void
    @State private var offset = CGSize.zero

    var body: some View {
        ZStack {
            Circle()
                .fill(Color.black.opacity(0.30))
                .overlay(Circle().stroke(Color.white.opacity(0.25), lineWidth: 1.5))
            Circle()
                .fill(Color.white.opacity(0.75))
                .frame(width: 54, height: 54)
                .offset(offset)
        }
        .contentShape(Circle())
        .gesture(
            DragGesture(minimumDistance: 0)
                .onChanged { value in
                    let radius = 44.0
                    var dx = value.translation.width
                    var dy = value.translation.height
                    let len = max(1, sqrt(dx * dx + dy * dy))
                    if len > radius {
                        dx = dx / len * radius
                        dy = dy / len * radius
                    }
                    offset = CGSize(width: dx, height: dy)
                    onChange(CGPoint(x: dx / radius, y: dy / radius))
                }
                .onEnded { _ in
                    offset = .zero
                    onChange(.zero)
                }
        )
    }
}
