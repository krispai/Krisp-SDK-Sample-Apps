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
                }
            }
        }
    }

    class AppState: ObservableObject {
        @Published var showAlert: Bool = false
        @Published var alertMessage: String = ""
        @Published var versionText: String = ""
    }
