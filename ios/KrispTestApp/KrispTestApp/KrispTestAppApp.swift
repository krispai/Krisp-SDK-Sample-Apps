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

    @State private var showAlert = false
    @State private var alertMessage = ""
    
    init() {
        let isLoaded = Bool(KrispAudioSDK.load())
        if !isLoaded {
            appState.showAlert = true
            appState.alertMessage = "Failed to load Krisp Audio SDK. App will not work."
        }
    }
    var body: some Scene {
        WindowGroup {
            NavigationView {
                WelcomeView()
            }
            .alert(isPresented: $showAlert) {
                Alert(title: Text("Loading Error"),
                    message: Text(alertMessage),
                    dismissButton: .default(Text("OK")))
            }
        }
    }
}

class AppState: ObservableObject {
    @Published var showAlert: Bool = false
    @Published var alertMessage: String = ""
}
