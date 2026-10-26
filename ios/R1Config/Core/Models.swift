import Foundation

struct HandsetState: Codable {
    let version: Int
    let controlAllowed: Bool
    let token: String
    let model: String
    let slot: Int
    let hostname: String
    let ip: String
    let channels: [Channel]
}

struct Channel: Codable, Identifiable, Equatable {
    var id: Int { channel }
    var channel: Int
    var negativeEndpoint: Int
    var positiveEndpoint: Int
    var negativeRate: Int
    var positiveRate: Int
    var negativeExpo: Int
    var positiveExpo: Int
    var negativeRamp: Int
    var positiveRamp: Int
    var profileSubtrim: Int
    var reversed: Bool
    var trim: Int
    var subtrim: Int

    var fields: [String: String] {
        ["channel": "\(channel)", "negativeEndpoint": "\(negativeEndpoint)",
         "positiveEndpoint": "\(positiveEndpoint)", "negativeRate": "\(negativeRate)",
         "positiveRate": "\(positiveRate)", "negativeExpo": "\(negativeExpo)",
         "positiveExpo": "\(positiveExpo)", "negativeRamp": "\(negativeRamp)",
         "positiveRamp": "\(positiveRamp)", "profileSubtrim": "\(profileSubtrim)",
         "reversed": reversed ? "1" : "0", "trim": "\(trim)", "subtrim": "\(subtrim)"]
    }

    func validate() throws {
        let ranges: [String: ClosedRange<Int>] = ["channel": 1...16,
            "negativeEndpoint": 50...100, "positiveEndpoint": 50...100,
            "negativeRate": 0...100, "positiveRate": 0...100,
            "negativeExpo": -100...100, "positiveExpo": -100...100,
            "negativeRamp": 0...2000, "positiveRamp": 0...2000,
            "profileSubtrim": -20...20, "trim": -200...200, "subtrim": -200...200]
        for (key, range) in ranges {
            guard let value = fields[key].flatMap(Int.init), range.contains(value) else {
                throw PortalError.message("Invalid \(key). Reload the channel before editing.")
            }
        }
    }
}

enum PortalError: LocalizedError {
    case message(String)
    var errorDescription: String? {
        switch self { case .message(let text): return text }
    }
}
