//
//  WelcomeView.swift
//  KrispTestApp
//
//  Created by Aram Tatalyan on 13.01.25.
//

import SwiftUI

struct WelcomeView: View {
    @ObservedObject var appState: AppState
    
    var body: some View {
        VStack(spacing: 40) {
            Spacer()
            HStack {
                Spacer()
                Image("KrispIcon")
                    .resizable()
                    .aspectRatio(contentMode: .fit)
                    .padding(.top, 40)
                    .frame(minWidth: 100, maxWidth: 115)
                Text("Krisp Test\nApplication")
                    .multilineTextAlignment(TextAlignment.leading)
                    .font(.largeTitle)
                    .fontWeight(.bold)
                    //.foregroundColor(Color.primary)
                    .lineLimit(2, reservesSpace: true)
                    .minimumScaleFactor(0.5)
                    //.layoutPriority(1)
                Spacer()
            }
            Text("Krisp SDK Version: \(appState.versionText)")
                .multilineTextAlignment(TextAlignment.leading)
                .font(.title)
                .fontWeight(.bold)
                .minimumScaleFactor(0.5)
            Text("Select Testing Mode")
                .multilineTextAlignment(TextAlignment.leading)
                .font(.title)
                .fontWeight(.bold)
                .minimumScaleFactor(0.5)
            NavigationLink(destination: FileProcessingView()) {
                Text("File Processing")
                    .font(.headline)
                    .frame(maxWidth: .infinity)
                    .padding()
                    .background(Color.blue)
                    .cornerRadius(8)
                    .foregroundColor(.white)
                    .padding(.horizontal, 20)
            }
            NavigationLink(destination: LeakReproView()) {
                Text("Session Leak Repro")
                    .font(.headline)
                    .frame(maxWidth: .infinity)
                    .padding()
                    .background(Color.orange.cornerRadius(8))
                    .foregroundColor(.white)
                    .padding(.horizontal, 20)
            }
            NavigationLink(destination: AudioCaptureView()) {
                Text("Audio Capture")
                    .font(.headline)
                    .frame(maxWidth: .infinity)
                    .padding()
                    .background(Color.green.cornerRadius(8))
                    .foregroundColor(.white)
                    .padding(.horizontal, 20)
            }
            Spacer()
            Spacer()
        }
    }
}

#Preview {
    WelcomeView(appState: AppState())
}

/// Picks a .kef model, then creates and destroys NC sessions in a loop and shows live malloc and process footprint
/// after each destroy. With krisp-nc-o-med-v7-fp16.kef at 48 kHz every cycle leaks about 2.9 MiB.
struct LeakReproView: View {
    @State private var modelData: Data? = nil
    @State private var modelName = "No model selected"
    @State private var showImporter = false
    @State private var sampleRate = 48000
    @State private var iterations = 10
    @State private var report = ""

    var body: some View {
        VStack(spacing: 16) {
            Text(modelName).font(.subheadline)
            Button("Select Model (.kef)") { showImporter = true }
            Picker("Sample rate", selection: $sampleRate) {
                Text("16 kHz").tag(16000)
                Text("48 kHz").tag(48000)
            }
            .pickerStyle(.segmented)
            Stepper("Iterations: \(iterations)", value: $iterations, in: 1...50)
            Button("Run create/destroy loop") {
                guard let data = modelData else { return }
                report = "Running…"
                DispatchQueue.global(qos: .userInitiated).async {
                    let result = KrispLeakRepro.run(withModelData: data, sampleRate: UInt32(sampleRate), iterations: Int32(iterations), processFrames: 0)
                    DispatchQueue.main.async { report = result ?? "" }
                }
            }
            .disabled(modelData == nil)
            ScrollView { Text(report).font(.system(.caption, design: .monospaced)).frame(maxWidth: .infinity, alignment: .leading) }
        }
        .padding()
        .navigationTitle("Session Leak Repro")
        .fileImporter(isPresented: $showImporter, allowedContentTypes: [FileTypes.model]) { result in
            guard case .success(let url) = result else { return }
            let accessed = url.startAccessingSecurityScopedResource()
            defer { if accessed { url.stopAccessingSecurityScopedResource() } }
            modelData = try? Data(contentsOf: url)
            modelName = url.lastPathComponent
        }
    }
}
