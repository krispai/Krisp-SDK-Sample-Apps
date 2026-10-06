    //
    //  KrispTestAppApp.swift
    //  KrispTestApp
    //
    //  Created by Aram Tatalyan on 07.01.25.
    //

    import SwiftUI
    import Foundation
    import UniformTypeIdentifiers


    struct FileTypes {
        static let model: UTType = {
            return UTType(filenameExtension: "kef") ?? UTType.data
        }()
    }

    @main
    struct KrispTestAppApp: App {
        
        @StateObject private var appState = AppState()

       // @State private var showAlert = false
        //@State private var alertMessage = ""
        //@State private var versionText = ""

        var body: some Scene {
            WindowGroup {
                NavigationView {
                    WelcomeView(appState: appState)
                }
                .alert(isPresented: $appState.showAlert) {
                    Alert(title: Text("Loading Error"),
                        message: Text(appState.alertMessage),
                        dismissButton: .default(Text("OK")))
                }
                .onAppear {
                    let isLoaded = KrispAudioSDK.load()
                    if !isLoaded {
                        appState.showAlert = true
                        appState.alertMessage = "Failed to load Krisp Audio SDK. App will not work."
                    }
                    appState.versionText = KrispAudioSDK.getVersion()
                    // Headless repro: KRISP_REPRO_MODEL=<path to .kef> [KRISP_REPRO_RATE] [KRISP_REPRO_ITERATIONS]
                    let env = ProcessInfo.processInfo.environment
                    if isLoaded, let modelPath = env["KRISP_REPRO_MODEL"], let data = FileManager.default.contents(atPath: modelPath) {
                        let rate = UInt32(env["KRISP_REPRO_RATE"] ?? "") ?? 48000
                        let iterations = Int32(env["KRISP_REPRO_ITERATIONS"] ?? "") ?? 10
                        DispatchQueue.global(qos: .userInitiated).async {
                            let result = KrispLeakRepro.run(withModelData: data, sampleRate: rate, iterations: iterations, processFrames: 0)
                            print("KRISP_REPRO version \(KrispAudioSDK.getVersion())\n\(result ?? "")\nKRISP_REPRO_DONE")
                        }
                    }
                }
            }
        }
    }

    class AppState: ObservableObject {
        @Published var showAlert: Bool = false
        @Published var alertMessage: String = ""
        @Published var versionText: String = ""
    }
