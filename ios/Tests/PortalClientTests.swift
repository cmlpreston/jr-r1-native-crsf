import XCTest
@testable import R1ConfigCore

final class MockProtocol: URLProtocol {
    static var respond: ((URLRequest) throws -> (Int, Data))!
    override class func canInit(with request: URLRequest) -> Bool { true }
    override class func canonicalRequest(for request: URLRequest) -> URLRequest { request }
    override func startLoading() {
        do {
            let (status, data) = try Self.respond(request)
            client?.urlProtocol(self, didReceive: HTTPURLResponse(url: request.url!, statusCode: status, httpVersion: nil, headerFields: nil)!, cacheStoragePolicy: .notAllowed)
            client?.urlProtocol(self, didLoad: data)
            client?.urlProtocolDidFinishLoading(self)
        } catch { client?.urlProtocol(self, didFailWithError: error) }
    }
    override func stopLoading() {}
}

final class PortalClientTests: XCTestCase {
    func fixture(allowed: Bool = true, name: String = "Buggy") -> HandsetState {
        HandsetState(version: 1, controlAllowed: allowed, token: "fresh-token", model: name, slot: 1,
                     hostname: "r1evo", ip: "192.168.4.1", channels: (1...16).map {
            Channel(channel: $0, negativeEndpoint: 100, positiveEndpoint: 100,
                    negativeRate: 100, positiveRate: 100, negativeExpo: 0, positiveExpo: 0,
                    negativeRamp: 0, positiveRamp: 0, profileSubtrim: 0, reversed: false, trim: 0, subtrim: 0)
        })
    }
    func client() throws -> PortalClient {
        let config = URLSessionConfiguration.ephemeral
        config.protocolClasses = [MockProtocol.self]
        return try PortalClient(address: "192.168.4.1", password: "secret", configuration: config)
    }
    func testRejectsInvalidAddresses() {
        for address in ["https://r1evo.local", "http://r1:secret@host", "host/api", "host?token=x", "host:8080"] {
            XCTAssertThrowsError(try PortalClient(address: address, password: "test"))
        }
    }
    func testFormEncodingPreservesReservedCharacters() {
        XCTAssertEqual(String(data: PortalClient.form(["name": "A & B+="]), encoding: .utf8), "name=A%20%26%20B%2B%3D")
    }
    func testCUTRequiredBeforeAnyPost() async throws {
        var methods: [String] = []
        MockProtocol.respond = { request in
            methods.append(request.httpMethod!)
            return (200, try JSONEncoder().encode(self.fixture(allowed: false)))
        }
        let api = try client(), model = fixture()
        do { _ = try await api.save(channel: model.channels[0], original: model.channels[0], model: model); XCTFail("Must block") }
        catch { XCTAssertTrue(error.localizedDescription.contains("CUT")) }
        XCTAssertEqual(methods, ["GET"])
    }
    func testModelChangeBlocksWrite() async throws {
        MockProtocol.respond = { _ in (200, try JSONEncoder().encode(self.fixture(name: "Other"))) }
        let api = try client()
        do { _ = try await api.rename("New", model: fixture()); XCTFail("Must block") }
        catch { XCTAssertTrue(error.localizedDescription.contains("active model changed")) }
    }
    func testChannelChangeBlocksWrite() async throws {
        var changed = fixture().channels
        changed[0].positiveRate = 50
        let state = HandsetState(version: 1, controlAllowed: true, token: "t", model: "Buggy", slot: 1, hostname: "r1evo", ip: "", channels: changed)
        MockProtocol.respond = { _ in (200, try JSONEncoder().encode(state)) }
        let api = try client(), original = fixture().channels[0]
        do { _ = try await api.save(channel: original, original: original, model: fixture()); XCTFail("Must block") }
        catch { XCTAssertTrue(error.localizedDescription.contains("channel changed")) }
    }
    func testSaveUsesFreshTokenAndVerifiesReadBack() async throws {
        let initial = fixture()
        var draft = initial.channels[0]; draft.reversed = true
        var calls = 0
        MockProtocol.respond = { request in
            calls += 1
            XCTAssertEqual(request.value(forHTTPHeaderField: "Authorization"), "Basic cjE6c2VjcmV0")
            if calls == 2 {
                XCTAssertEqual(request.httpMethod, "POST")
                XCTAssertEqual(request.url?.path, "/api/channel")
                XCTAssertEqual(request.value(forHTTPHeaderField: "X-R1-Token"), "fresh-token")
                return (200, Data("Channel saved".utf8))
            }
            var channels = initial.channels
            if calls == 3 { channels[0] = draft }
            return (200, try JSONEncoder().encode(HandsetState(version: 1, controlAllowed: true, token: "fresh-token", model: initial.model, slot: 1, hostname: "r1evo", ip: "", channels: channels)))
        }
        let result = try await client().save(channel: draft, original: initial.channels[0], model: initial)
        XCTAssertEqual(result.channels[0], draft)
        XCTAssertEqual(calls, 3)
    }
    func testLostSaveResponseIsNotRetried() async throws {
        var calls = 0
        MockProtocol.respond = { _ in
            calls += 1
            if calls == 2 { throw URLError(.timedOut) }
            return (200, try JSONEncoder().encode(self.fixture()))
        }
        do { _ = try await client().rename("New", model: fixture()); XCTFail("Must fail") }
        catch { XCTAssertTrue(error.localizedDescription.contains("Save was not verified")) }
        XCTAssertEqual(calls, 2)
    }
    func testReadbackMismatchIsNotSuccess() async throws {
        MockProtocol.respond = { request in
            request.httpMethod == "POST" ? (200, Data()) : (200, try JSONEncoder().encode(self.fixture()))
        }
        do { _ = try await client().rename("New", model: fixture()); XCTFail("Must fail") }
        catch { XCTAssertTrue(error.localizedDescription.contains("Read-back did not match")) }
    }
    func testAuthenticationAndSessionErrorsAreVisible() async throws {
        for (status, expected) in [(401, "Sign-in failed"), (403, "session expired"), (423, "CUT")] {
            MockProtocol.respond = { _ in (status, Data()) }
            do { _ = try await client().state(); XCTFail("Must reject HTTP error") }
            catch { XCTAssertTrue(error.localizedDescription.contains(expected)) }
        }
    }
    func testMalformedStateIsRejected() async throws {
        MockProtocol.respond = { _ in
            (200, Data("{\"version\":1,\"controlAllowed\":true,\"token\":\"\",\"model\":\"Buggy\",\"slot\":1,\"hostname\":\"r1evo\",\"ip\":\"\",\"channels\":[]}".utf8))
        }
        do { _ = try await client().state(); XCTFail("Must reject incomplete state") }
        catch { XCTAssertTrue(error.localizedDescription.contains("Unsupported")) }
    }
    func testInvalidDraftDoesNotReachNetwork() async throws {
        var calls = 0
        MockProtocol.respond = { _ in calls += 1; return (500, Data()) }
        var draft = fixture().channels[0]; draft.negativeEndpoint = 49
        do { _ = try await client().save(channel: draft, original: fixture().channels[0], model: fixture()); XCTFail("Must reject invalid draft") }
        catch { XCTAssertTrue(error.localizedDescription.contains("negativeEndpoint")) }
        XCTAssertEqual(calls, 0)
    }

}
