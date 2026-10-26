import Foundation

// Redirects are rejected so a handset cannot forward credentials to another host.
final class NoRedirect: NSObject, URLSessionTaskDelegate {
    func urlSession(_ session: URLSession, task: URLSessionTask,
                    willPerformHTTPRedirection response: HTTPURLResponse,
                    newRequest request: URLRequest,
                    completionHandler: @escaping (URLRequest?) -> Void) {
        completionHandler(nil)
    }
}

final class PortalClient {
    let baseURL: URL
    private let authorization: String
    private let session: URLSession

    init(address: String, password: String, configuration: URLSessionConfiguration = .ephemeral) throws {
        let text = address.trimmingCharacters(in: .whitespacesAndNewlines)
        guard let parts = URLComponents(string: text.contains("://") ? text : "http://" + text),
              parts.scheme == "http", let host = parts.host, !host.isEmpty,
              parts.user == nil, parts.password == nil, parts.query == nil, parts.fragment == nil,
              parts.path.isEmpty || parts.path == "/", parts.port == nil || parts.port == 80,
              let url = parts.url else {
            throw PortalError.message("Enter a handset hostname or IP, such as r1evo.local or 192.168.4.1.")
        }
        baseURL = url
        authorization = "Basic " + Data(("r1:" + password).utf8).base64EncodedString()
        configuration.urlCache = nil
        configuration.httpCookieStorage = nil
        configuration.urlCredentialStorage = nil
        configuration.timeoutIntervalForRequest = 10
        configuration.timeoutIntervalForResource = 15
        session = URLSession(configuration: configuration, delegate: NoRedirect(), delegateQueue: nil)
    }

    deinit { session.invalidateAndCancel() }

    static func form(_ fields: [String: String]) -> Data {
        var allowed = CharacterSet.alphanumerics
        allowed.insert(charactersIn: "-._~")
        return Data(fields.sorted { $0.key < $1.key }.map {
            $0.key.addingPercentEncoding(withAllowedCharacters: allowed)! + "=" +
            $0.value.addingPercentEncoding(withAllowedCharacters: allowed)!
        }.joined(separator: "&").utf8)
    }

    private func request(path: String, fields: [String: String]? = nil, token: String? = nil) async throws -> Data {
        var request = URLRequest(url: baseURL.appendingPathComponent(path))
        request.cachePolicy = .reloadIgnoringLocalCacheData
        request.setValue(authorization, forHTTPHeaderField: "Authorization")
        if let fields {
            request.httpMethod = "POST"
            request.httpBody = Self.form(fields)
            request.setValue("application/x-www-form-urlencoded", forHTTPHeaderField: "Content-Type")
            request.setValue(token, forHTTPHeaderField: "X-R1-Token")
        }
        let (data, response) = try await session.data(for: request)
        guard let response = response as? HTTPURLResponse else { throw PortalError.message("Invalid handset response.") }
        switch response.statusCode {
        case 200...299: return data
        case 401: throw PortalError.message("Sign-in failed. Use the Wi-Fi setup password shown on the handset, with username r1.")
        case 403: throw PortalError.message("Setup session expired. Refresh and review your settings before saving again.")
        case 423: throw PortalError.message("Changes blocked. Put the transmitter in CUT and finish any startup warnings.")
        default:
            let detail = String(data: data.prefix(300), encoding: .utf8) ?? "Invalid response"
            throw PortalError.message("Handset error \(response.statusCode): \(detail)")
        }
    }

    func state() async throws -> HandsetState {
        let state = try JSONDecoder().decode(HandsetState.self, from: await request(path: "api/state"))
        guard state.version == 1, !state.token.isEmpty, (1...10).contains(state.slot),
              state.channels.count == 16, Set(state.channels.map(\.channel)) == Set(1...16) else {
            throw PortalError.message("Unsupported handset configuration response.")
        }
        for channel in state.channels { try channel.validate() }
        return state
    }

    // Each save uses a fresh token and checks that the edited model/channel has not changed.
    // There is no firmware compare-and-swap API; another editor can still race this check.
    func save(channel: Channel, original: Channel, model: HandsetState) async throws -> HandsetState {
        try channel.validate()
        guard channel.channel == original.channel else { throw PortalError.message("Channel changed; reopen the editor.") }
        let fresh = try await writableState(model: model)
        guard fresh.channels.first(where: { $0.id == original.id }) == original else {
            throw PortalError.message("This channel changed on the handset. Reload it before saving.")
        }
        return try await writeAndVerify(path: "api/channel", fields: channel.fields, token: fresh.token) { result in
            result.slot == model.slot && result.model == model.model && result.channels.first(where: { $0.id == channel.id }) == channel
        }
    }

    func rename(_ name: String, model: HandsetState) async throws -> HandsetState {
        guard (1...12).contains(name.utf8.count), name.utf8.allSatisfy({ (32...126).contains($0) }) else {
            throw PortalError.message("Use 1–12 printable ASCII characters for the model name.")
        }
        let fresh = try await writableState(model: model)
        return try await writeAndVerify(path: "api/model", fields: ["name": name], token: fresh.token) {
            $0.slot == model.slot && $0.model == name
        }
    }

    private func writableState(model: HandsetState) async throws -> HandsetState {
        let fresh = try await state()
        guard fresh.controlAllowed else { throw PortalError.message("Changes blocked. Put the transmitter in CUT first.") }
        guard fresh.slot == model.slot, fresh.model == model.model else {
            throw PortalError.message("The active model changed. Refresh before editing.")
        }
        return fresh
    }

    private func writeAndVerify(path: String, fields: [String: String], token: String,
                                verify: (HandsetState) -> Bool) async throws -> HandsetState {
        do {
            _ = try await request(path: path, fields: fields, token: token)
            let result = try await state()
            guard verify(result) else { throw PortalError.message("Read-back did not match the requested settings.") }
            return result
        } catch {
            throw PortalError.message("Save was not verified. Refresh and inspect the handset before retrying. \(error.localizedDescription)")
        }
    }
}
