//
//  AudioCaptureView.swift
//  KrispTestApp
//
//  Created by Aram Tatalyan on 13.01.25.
//

import SwiftUI


class AudioCaptureViewModel: ObservableObject {
    @Published var isProcessing: Bool = false
    @Published var krispModelLoaded: Bool = false
    @Published var captureDurationMinsText = "30"
    @Published var captureModeText = "Realtime"
    @Published var captureBlockSizeInMinsText = "1"
    @Published var minutesProcessed = 0

    private var captureDurationMins: Int {
        Int(captureDurationMinsText) ?? 0
    }
    
    private var captureBlockSizeInMins: Float {
        Float(captureBlockSizeInMinsText) ?? 0
    }
    
    let krispAudioProcessor: KrispAudioProcessor
    let audioCapturer: AudioCapturer
    
    init() {
        self.krispAudioProcessor = KrispAudioProcessor()
        self.audioCapturer = AudioCapturer(krispAudioProcessor: self.krispAudioProcessor)
    }
    
    func onMinuteProcessed(mins: Int) {
        DispatchQueue.main.async {
            self.minutesProcessed = mins
        }
        if mins > captureDurationMins {
            self.stopCapture()
        }
    }
    
    func startCapture() {
        self.minutesProcessed = 0
        if captureModeText == "Realtime" {
            audioCapturer.startRecording(mode: captureModeText,
                                         captureDurationInMinutes: captureDurationMins,
                                         captureModeBlockSizeInMinutes: 0.0,
                                         minuteProcessedCallback: onMinuteProcessed)
        }
        else if captureModeText == "Distributed" {
            let captureBlockSizeInMins = self.captureBlockSizeInMins
            if abs(captureBlockSizeInMins) < 0.005 {
                print("ERROR: capture block size is around 0")
                return
            }
            audioCapturer.startRecording(mode: captureModeText,
                                         captureDurationInMinutes: captureDurationMins,
                                         captureModeBlockSizeInMinutes: captureBlockSizeInMins,
                                         minuteProcessedCallback: onMinuteProcessed)
        }
        else {
        }
        isProcessing = true
    }
    
    func stopCapture() {
        audioCapturer.stopRecording()
        isProcessing = false
        if captureModeText == "Realtime" {

        }
        else if captureModeText == "Distributed" {
        }
        else {
        }
    }
    
    func loadKrispModel(modelData: Data) -> Bool {
        if self.krispAudioProcessor.loadKrisp(modelData, sampleRate: 44100) {
            krispModelLoaded = true
            return true
        }
        return false
    }
}

struct AudioCaptureView: View {
    
    @StateObject private var viewModel = AudioCaptureViewModel()
    
    private var windowTitle: String = "Audio Capture"
    @State private var showingModelFileImporter = false
    @State private var showErrorAlert = false
    @State private var errorMessage = ""
        
    static private let checkedStatusIconName = "checkmark.square"
    static private let checkedStatusIconColor = Color.green
    static private let uncheckedStatusIconName = "xmark.square"
    static private let unckeckedStatusIconColor = Color.red
    
    @State private var modelStatusIconName = uncheckedStatusIconName
    @State private var modelStatusIconColor = unckeckedStatusIconColor
    @State private var loadedModelFileName = "no model loaded"

    
    var body: some View {
        
        //let buttonBackgroundColor = Color.accentColor.opacity(0.8)
        //let buttonForegroundColor = Color.primary
        let buttonCornerRadius = 12.0
        let buttonShadowRadius = 12.0
        
        VStack {
            HStack {
                Spacer()
                Image("KrispIcon")
                    .resizable()
                    .aspectRatio(contentMode: .fit)
                    .padding()
                    .frame(minWidth: 100, maxWidth: 115)
                Text("Krisp Test\nApplication")
                    .multilineTextAlignment(TextAlignment.leading)
                    .font(.largeTitle)
                    .fontWeight(.bold)
                    .lineLimit(2, reservesSpace: true)
                    .minimumScaleFactor(0.5)
                Spacer()
            }
            Text(windowTitle).padding().font(.title).bold()
            Spacer()
            HStack {
                Spacer()
                Spacer()
                Button("Select Model") {
                    showingModelFileImporter = true
                }
                .fileImporter(
                    isPresented: $showingModelFileImporter,
                    allowedContentTypes: [FileTypes.model],
                    allowsMultipleSelection: false
                ) { result in
                    handleFileImport(result: result)
                }
                .padding()
                .buttonStyle(.plain)
                .font(.headline)
                .frame(maxWidth: 250)
                .background(Color.blue)
                .cornerRadius(buttonCornerRadius)
                .shadow(radius: buttonShadowRadius)
                .foregroundColor(.white)
                .disabled(viewModel.isProcessing)
                Spacer()
                Image(systemName: modelStatusIconName)
                    .imageScale(.large)
                    .foregroundStyle(modelStatusIconColor)
                    .font(.title)
                    .fontWeight(.bold)
                Spacer()
            }

            HStack {
                Spacer()
                Spacer()
                Button(viewModel.isProcessing ? "Stop Capture" : "Start Capture") {
                    if viewModel.isProcessing {
                        viewModel.stopCapture()
                    }
                    else {
                        viewModel.startCapture()
                    }
                }
                .padding()
                .buttonStyle(.plain)
                .font(.headline)
                .frame(maxWidth: 250)
                .background(Color.blue)
                .cornerRadius(buttonCornerRadius)
                .shadow(radius: buttonShadowRadius)
                .foregroundColor(.white)
                .disabled(!viewModel.krispModelLoaded)
                Spacer()
                Image(systemName: (viewModel.krispModelLoaded ? "microphone" : "microphone.badge.xmark.fill"))
                    .imageScale(.large)
                    .foregroundStyle((!viewModel.krispModelLoaded || viewModel.isProcessing) ? Color.red : Color.green)
                    .font(.title)
                    .fontWeight(.bold)
                Spacer()
            }
            HStack {
                Spacer()
                Spacer()
                Picker("Capture Mode", selection: $viewModel.captureModeText) {
                    Text("Realtime").tag("Realtime")
                    Text("Distributed").tag("Distributed")
                }
                .padding()
                .pickerStyle(.segmented)
                .font(.headline)
                .labelsVisibility(.visible)
                .frame(maxWidth: 250)
                .disabled(viewModel.isProcessing)
                Spacer()
                Image(systemName: ("microphone.badge.xmark.fill"))
                    .imageScale(.large)
                    .foregroundStyle((!viewModel.krispModelLoaded || viewModel.isProcessing) ? Color.red : Color.green)
                    .font(.title)
                    .fontWeight(.bold)
                    .hidden()
                Spacer()

            }

            .alert("File Import Error", isPresented: $showErrorAlert, actions: {
                Button("OK", role: .cancel) {}
            }, message: {
                Text(errorMessage)
            })
            HStack {
                Text("Duration")
                TextField("Duration", text: $viewModel.captureDurationMinsText) {
                }
                .padding()
                .keyboardType(.numberPad)
                .lineLimit(1)
                .font(.headline)
                .frame(maxWidth: 70, maxHeight: 30)
                .border(.secondary)
                .multilineTextAlignment(.center)
                .disabled(viewModel.isProcessing)
                .toolbar { // Adds a Done button to dismiss keyboard
                    ToolbarItemGroup(placement: .keyboard) {
                        Spacer()
                        Button("DONE") {
                            hideKeyboard()
                        }.font(.headline).bold()
                    }
                }
                Text("Minutes")

            }.padding()
            HStack {
                Group {
                    Text("Block Size")
                    TextField("", text: $viewModel.captureBlockSizeInMinsText) {
                    }
                    .padding()
                    .keyboardType(.decimalPad)
                    .frame(maxWidth: 70, maxHeight: 30)
                    .lineLimit(1)
                    .font(.headline)
                    .border(.secondary)
                    .multilineTextAlignment(.center)
                    .disabled(viewModel.isProcessing)
                    Text("Minutes   ")
                }
                .opacity(viewModel.captureModeText == "Distributed" ? 1 : 0)
            }
            .padding()
 
            Text(loadedModelFileName)
                .font(.callout)
                .padding()
            Text("Processing Duration: \(viewModel.minutesProcessed) minutes    `")
                .font(.callout)
                .padding()
            Spacer()
        }
        .onTapGesture {
            hideKeyboard()
        }
    }

    private func handleFileImport(result: Result<[URL], Error>) {
        do {
            let selectedFiles = try result.get()
            
            guard let fileURL = selectedFiles.first else {
                errorMessage = "No model file selected."
                showErrorAlert = true
                return
            }
            guard fileURL.startAccessingSecurityScopedResource() else {
                errorMessage = "Unable to access the selected file."
                showErrorAlert = true
                return
            }
            defer {
                fileURL.stopAccessingSecurityScopedResource()
            }
            let fileData = try Data(contentsOf: fileURL)
            if fileData.isEmpty {
                errorMessage = "File is empty"
                showErrorAlert = true
                return
            }
            if viewModel.loadKrispModel(modelData: fileData) {
                print("Krisp Model file loaded. Size: \(fileData.count) bytes.")
                modelStatusIconName = AudioCaptureView.checkedStatusIconName
                modelStatusIconColor = AudioCaptureView.checkedStatusIconColor
                loadedModelFileName = "\(fileURL.lastPathComponent) model is loaded"
            }
            else {
                errorMessage = "Error loading Krisp model: \(fileURL.lastPathComponent)"
                print(errorMessage)
                showErrorAlert = true
            }
        } catch {
            print("File import error: \(error.localizedDescription)")
            errorMessage = error.localizedDescription
            showErrorAlert = true
        }
    }
}

extension View {
    func hideKeyboard() {
        UIApplication.shared.sendAction(#selector(UIResponder.resignFirstResponder), to: nil, from: nil, for: nil)
    }
}

#Preview {
    AudioCaptureView()
}
