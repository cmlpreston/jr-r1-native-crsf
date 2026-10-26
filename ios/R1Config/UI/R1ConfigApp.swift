import SwiftUI

@main
struct R1ConfigApp: App {
    var body: some Scene { WindowGroup { RootView().tint(.teal) } }
}

@MainActor
final class HandsetStore: ObservableObject {
    @Published var state: HandsetState?
    @Published var busy = false
    @Published var message: String?
    @Published var connected = false
    var client: PortalClient?

    func perform(_ action: () async throws -> HandsetState) async {
        guard !busy else { return }
        busy = true
        defer { busy = false }
        do {
            state = try await action()
            connected = true
        } catch {
            connected = false
            message = error.localizedDescription
        }
    }

    func refresh() async {
        guard let client else { return }
        await perform { try await client.state() }
    }

    func disconnect() {
        client = nil
        state = nil
        connected = false
    }
}

struct RootView: View {
    @StateObject private var store = HandsetStore()
    @Environment(\.scenePhase) private var phase
    @State private var address = "r1evo.local"
    @State private var password = ""
    @State private var modelEditor = false

    var body: some View {
        NavigationStack {
            Group {
                if let state = store.state {
                    List {
                        Section {
                            Label(store.connected ? (state.controlAllowed ? "Configuration permitted" : "Viewing only") : "Connection needs refresh",
                                  systemImage: store.connected && state.controlAllowed ? "checkmark.shield" : "lock.shield")
                                .foregroundStyle(store.connected && state.controlAllowed ? .teal : .orange)
                            Text("Changes require CUT. The handset checks permission again for every save.")
                                .font(.footnote).foregroundStyle(.secondary)
                        }
                        Section("Active model · slot \(state.slot)") {
                            HStack { Text(state.model).font(.title2.bold()); Spacer()
                                Button("Rename") { modelEditor = true }
                                    .disabled(!canEdit)
                            }
                            LabeledContent("Handset", value: state.hostname)
                            LabeledContent("IP address", value: state.ip)
                        }
                        Section("Channels") {
                            ForEach(state.channels.sorted { $0.id < $1.id }) { channel in
                                NavigationLink {
                                    ChannelEditor(original: channel, model: state, store: store)
                                } label: {
                                    HStack {
                                        Text(String(format: "%02d", channel.channel)).font(.title3.monospaced().bold()).foregroundStyle(.teal)
                                        VStack(alignment: .leading) {
                                            Text("Channel \(channel.channel)")
                                            Text("Endpoints \(channel.negativeEndpoint) / \(channel.positiveEndpoint)% · \(channel.reversed ? "Reversed" : "Normal")")
                                                .font(.caption).foregroundStyle(.secondary)
                                        }
                                    }.padding(.vertical, 3)
                                }
                            }
                        }
                        Section("Advanced setup") {
                            if let url = store.client?.baseURL {
                                Link(destination: url) { Label("Open handset web portal", systemImage: "safari") }
                            }
                            Text("Control assignments, mixing, maps and live screen are available in the portal. Sign in there with the same setup password.")
                                .font(.footnote).foregroundStyle(.secondary)
                            Button("Disconnect", role: .destructive) { store.disconnect() }.disabled(store.busy)
                        }
                    }
                    .refreshable { await store.refresh() }
                } else {
                    Form {
                        Section {
                            Label("R-1 EVO", systemImage: "antenna.radiowaves.left.and.right").font(.largeTitle.bold()).padding(.vertical)
                            Text("Configure your XIAO transmitter over Wi-Fi.").foregroundStyle(.secondary)
                        }
                        Section("Connect to your handset") {
                            TextField("Hostname or IP", text: $address)
                                .keyboardType(.URL).textInputAutocapitalization(.never).autocorrectionDisabled()
                            SecureField("Wi-Fi setup password", text: $password)
                            Button {
                                let secret = password
                                password = ""
                                Task {
                                    await store.perform {
                                        let client = try PortalClient(address: address, password: secret)
                                        let state = try await client.state()
                                        store.client = client
                                        return state
                                    }
                                }
                            } label: { Label("Connect", systemImage: "wifi") }
                                .disabled(store.busy || password.isEmpty)
                        }
                        Section("On the transmitter") {
                            Text("Open Setup → Handset → Wi-Fi / web setup. Connect both devices to the same home Wi-Fi, or join the R-1-EVO hotspot and use 192.168.4.1.")
                            Text("Use the setup password shown on the LCD. The boot PIN is separate. Username is r1.")
                            Text("The handset uses local HTTP. Connect only over a trusted network. Your password is kept in memory for this connection and is not saved to disk.")
                                .font(.footnote).foregroundStyle(.secondary)
                        }
                    }
                }
            }
            .navigationTitle(store.state == nil ? "Transmitter" : "R-1 EVO")
            .toolbar {
                if store.state != nil { Button { Task { await store.refresh() } } label: { Image(systemName: "arrow.clockwise") }.disabled(store.busy) }
                if store.busy { ProgressView() }
            }
            .sheet(isPresented: $modelEditor) {
                if let state = store.state { RenameView(model: state, store: store) }
            }
            .alert("Handset", isPresented: Binding(get: { store.message != nil }, set: { if !$0 { store.message = nil } })) {
                Button("OK") { store.message = nil }
            } message: { Text(store.message ?? "") }
            .task(id: phase) {
                guard phase == .active else { store.connected = false; return }
                while !Task.isCancelled {
                    if store.client != nil { await store.refresh() }
                    do { try await Task.sleep(for: .seconds(5)) } catch { return }
                }
            }
        }
    }
    private var canEdit: Bool { phase == .active && store.connected && !store.busy && store.state?.controlAllowed == true }
}

struct RenameView: View {
    let model: HandsetState
    @ObservedObject var store: HandsetStore
    @Environment(\.dismiss) private var dismiss
    @State private var name: String
    init(model: HandsetState, store: HandsetStore) {
        self.model = model; self.store = store; _name = State(initialValue: model.model)
    }
    var body: some View {
        NavigationStack {
            Form {
                TextField("Model name", text: $name).autocorrectionDisabled()
                Text("1–12 printable ASCII characters. Saves to the active model on the handset.").font(.footnote)
            }.navigationTitle("Rename model").toolbar {
                ToolbarItem(placement: .cancellationAction) { Button("Cancel") { dismiss() }.disabled(store.busy) }
                ToolbarItem(placement: .confirmationAction) {
                    Button("Save") {
                        guard let client = store.client else { return }
                        Task {
                            await store.perform { try await client.rename(name, model: model) }
                            if store.connected { dismiss() }
                        }
                    }.disabled(store.busy || !store.connected || store.state?.controlAllowed != true || name == model.model)
                }
            }
        }.interactiveDismissDisabled(store.busy)
    }
}

struct ChannelEditor: View {
    let original: Channel
    let model: HandsetState
    @ObservedObject var store: HandsetStore
    @Environment(\.dismiss) private var dismiss
    @State private var draft: Channel
    @State private var confirm = false
    init(original: Channel, model: HandsetState, store: HandsetStore) {
        self.original = original; self.model = model; self.store = store
        _draft = State(initialValue: original)
    }
    var body: some View {
        Form {
            Section {
                Text("Edit the two input directions independently. Changes stay on this phone until you save.")
                    .font(.footnote).foregroundStyle(.secondary)
                Toggle("Reversed", isOn: $draft.reversed)
            }
            Section("Endpoints") {
                value("Negative", $draft.negativeEndpoint, 50...100, "%")
                value("Positive", $draft.positiveEndpoint, 50...100, "%")
            }
            Section("Rates") {
                value("Negative", $draft.negativeRate, 0...100, "%")
                value("Positive", $draft.positiveRate, 0...100, "%")
            }
            Section("Expo") {
                value("Negative", $draft.negativeExpo, -100...100, "%")
                value("Positive", $draft.positiveExpo, -100...100, "%")
            }
            Section("Ramp") {
                value("Negative", $draft.negativeRamp, 0...2000, " ms", step: 10)
                value("Positive", $draft.positiveRamp, 0...2000, " ms", step: 10)
            }
            Section("Trim") {
                value("Profile sub-trim", $draft.profileSubtrim, -20...20, "%")
                value("Trim", $draft.trim, -200...200, "‰")
                value("Sub-trim", $draft.subtrim, -200...200, "‰")
            }
            Section {
                Button("Save to handset") { confirm = true }
                    .disabled(draft == original || store.busy || !store.connected || store.state?.controlAllowed != true)
                if !store.connected || store.state?.controlAllowed != true {
                    Text("Refresh the connection and put the transmitter in CUT to save.").font(.footnote).foregroundStyle(.orange)
                }
            }
        }
        .disabled(store.busy)
        .navigationTitle("Channel \(draft.channel)")
        .confirmationDialog("Save channel \(draft.channel) to \(model.model)?", isPresented: $confirm, titleVisibility: .visible) {
            Button("Save channel") {
                guard let client = store.client else { return }
                Task {
                    await store.perform { try await client.save(channel: draft, original: original, model: model) }
                    if store.connected { dismiss() }
                }
            }
        } message: { Text("The handset must permit configuration. Settings will be read back after saving.") }
    }
    private func value(_ label: String, _ binding: Binding<Int>, _ range: ClosedRange<Int>, _ unit: String, step: Int = 1) -> some View {
        Stepper(value: binding, in: range, step: step) {
            LabeledContent(label, value: "\(binding.wrappedValue)\(unit)")
        }
    }
}
