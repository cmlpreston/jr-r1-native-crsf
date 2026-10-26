// swift-tools-version: 5.9
import PackageDescription
let package = Package(name: "R1ConfigCore", platforms: [.macOS(.v13)], products: [
    .library(name: "R1ConfigCore", targets: ["R1ConfigCore"])
], targets: [
    .target(name: "R1ConfigCore", path: "R1Config/Core"),
    .testTarget(name: "R1ConfigCoreTests", dependencies: ["R1ConfigCore"], path: "Tests")
])
