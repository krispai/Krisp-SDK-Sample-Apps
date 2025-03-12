//
//  ContentView.swift
//  KrispTestApp
//
//  Created by Aram Tatalyan on 07.01.25.
//

import SwiftUI
import UniformTypeIdentifiers


// TODO create ViewModel


struct FileProcessingView: View {
    // MARK: - States for file data
    @State private var isProcessing: Bool = false
    @State private var audioData: Data?
    @State private var processedAudioData: Data?
    @State private var krispModelLoaded: Bool = false
    @State private var processedAudioDataSaved: Bool = false
    private var krispAudioProcessor: KrispWavFileProcessor = KrispWavFileProcessor()
    
    // MARK: - States for tracking processing progress
    @State private var totalFrames: UInt = 0
    @State private var processedFrames: UInt = 0

    // MARK: - States for showing file importers/exporters
    @State private var showingAudioFileImporter = false
    @State private var showingModelFileImporter = false
    @State private var showingFileExporter = false
    
    static private let checkedStatusIconName = "checkmark.square"
    static private let checkedStatusIconColor = Color.green
    static private let uncheckedStatusIconName = "xmark.square"
    static private let unckeckedStatusIconColor = Color.red
    
    @State private var wavStatusIconName = uncheckedStatusIconName
    @State private var wavStatusIconColor = unckeckedStatusIconColor
    @State private var modelStatusIconName = uncheckedStatusIconName
    @State private var modelStatusIconColor = unckeckedStatusIconColor
    
    @State private var loadedWavFileName = "no WAV file loaded"
    @State private var loadedModelFileName = "no model loaded"
    
    private var windowTitle: String = "File Processing"
    
    @State private var showErrorAlert = false
    @State private var errorMessage = ""
    
    @State private var fileProcessingTimeSeconds: UInt = 0

    
    var body: some View {
        
        let buttonBackgroundColor = Color.accentColor.opacity(0.8)
        let buttonForegroundColor = Color.primary
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
                Button(" Load WAV File   ") {
                    showingAudioFileImporter = true
                }
                .fileImporter(
                    isPresented: $showingAudioFileImporter,
                    allowedContentTypes: [UTType.wav],
                    allowsMultipleSelection: false
                ) { result in
                    handleFileImport(result: result, forAudio: true)
                }
                .padding()
                .background(buttonBackgroundColor)
                .foregroundColor(buttonForegroundColor)
                .buttonStyle(.plain)
                .cornerRadius(buttonCornerRadius)
                .shadow(radius: buttonShadowRadius)
                .disabled(isProcessing)

                Image(systemName: wavStatusIconName)
                    .imageScale(.large)
                    .foregroundStyle(wavStatusIconColor)
                    .font(.title)
                    .fontWeight(.bold)
            }
            .alert("File Import Error", isPresented: $showErrorAlert, actions: {
                Button("OK", role: .cancel) {}
            }, message: {
                Text(errorMessage)
            })
            HStack {
                Button("  Select Model    ") {
                    showingModelFileImporter = true
                }
                .fileImporter(
                    isPresented: $showingModelFileImporter,
                    allowedContentTypes: [FileTypes.model],
                    allowsMultipleSelection: false
                ) { result in
                    handleFileImport(result: result, forAudio: false)
                }
                .padding()
                .buttonStyle(.plain)
                .background(buttonBackgroundColor)
                .foregroundColor(buttonForegroundColor)
                .cornerRadius(buttonCornerRadius)
                .shadow(radius: buttonShadowRadius)
                .disabled(isProcessing)
                Image(systemName: modelStatusIconName)
                    .imageScale(.large)
                    .foregroundStyle(modelStatusIconColor)
                    .font(.title)
                    .fontWeight(.bold)
            }
            HStack {
                Button(isProcessing ? "       Stop      " : "  Process Audio  ") {
                    if isProcessing {
                        krispAudioProcessor.stopProcessing()
                    }
                    else {
                        isProcessing = true
                        Task {
                            await processAudio()
                        }
                    }
                }
                .padding()
                .buttonStyle(.plain)
                .background(buttonBackgroundColor)
                .foregroundColor(buttonForegroundColor)
                .cornerRadius(buttonCornerRadius)
                .shadow(radius: buttonShadowRadius)
                .disabled(!krispModelLoaded || audioData == nil)
                
                Image(systemName: (processedAudioData != nil) ? FileProcessingView.checkedStatusIconName :      FileProcessingView.uncheckedStatusIconName)
                    .imageScale(.large)
                    .foregroundStyle((processedAudioData != nil) ? FileProcessingView.checkedStatusIconColor : FileProcessingView.unckeckedStatusIconColor)
                    .font(.title)
                    .fontWeight(.bold)

            }
            Spacer()
            Text(loadedWavFileName)
                .font(.callout)
                .padding()
            Text(loadedModelFileName)
                .font(.callout)
                .padding()
            Spacer()
            Text("Audio frames processed: \(processedFrames) / \(totalFrames)")
                .font(.callout)
                .padding()
            Text("Audio Proccesed In Seconds: \(fileProcessingTimeSeconds)")
                .font(.callout)
                .padding()
            Spacer()
            HStack {
                Button("Save Processed Audio") {
                    showingFileExporter = true
                }
                .fileExporter(
                    isPresented: $showingFileExporter,
                    document: ProcessedAudioDocument(data: processedAudioData ?? Data()),
                    contentType: .wav,
                    defaultFilename: "processed_audio"
                ) { result in
                    switch result {
                    case .success:
                        print("Successfully saved processed file.")
                        processedAudioDataSaved = true
                    case .failure(let error):
                        print("Error saving processed file: \(error.localizedDescription)")
                    }
                }
                .padding()
                .buttonStyle(.plain)
                .background(buttonBackgroundColor)
                .foregroundColor(buttonForegroundColor)
                .cornerRadius(buttonCornerRadius)
                .shadow(radius: buttonShadowRadius)
                .disabled(isProcessing || processedAudioData == nil)
                Image(systemName: processedAudioDataSaved ? FileProcessingView.checkedStatusIconName : FileProcessingView.uncheckedStatusIconName)
                    .imageScale(.large)
                    .foregroundStyle(processedAudioDataSaved ? FileProcessingView.checkedStatusIconColor : FileProcessingView.unckeckedStatusIconColor)
                    .font(.title)
                    .fontWeight(.bold)
            }
        }
        .padding()
    }
    
    private func handleFileImport(result: Result<[URL], Error>, forAudio: Bool) {
        do {
            let selectedFiles = try result.get()
            guard let fileURL = selectedFiles.first else { return }
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
            if forAudio {
                audioData = fileData
                print("Audio file loaded. Size: \(fileData.count) bytes.")
                wavStatusIconName = FileProcessingView.checkedStatusIconName
                wavStatusIconColor = FileProcessingView.checkedStatusIconColor
                if loadedWavFileName != fileURL.lastPathComponent {
                    loadedWavFileName = "\(fileURL.lastPathComponent) audio is loaded"
                    processedAudioData = nil
                    processedAudioDataSaved = false
                }
            } else {
                if krispAudioProcessor.loadKrisp(fileData, sampleRate: 16000) {
                    krispModelLoaded = true
                    print("Krisp Model file loaded. Size: \(fileData.count) bytes.")
                    modelStatusIconName = FileProcessingView.checkedStatusIconName
                    modelStatusIconColor = FileProcessingView.checkedStatusIconColor
                    loadedModelFileName = "\(fileURL.lastPathComponent) model is loaded"
                    processedAudioData = nil
                    processedAudioDataSaved = false
                }
                else {
                    errorMessage = "Error loading Krisp model: \(fileURL.lastPathComponent)"
                    print(errorMessage)
                    showErrorAlert = true
                }
            }
        } catch {
            print("File import error: \(error.localizedDescription)")
            errorMessage = error.localizedDescription
            showErrorAlert = true
        }
    }
    
    private func processAudio() async {
        guard let audioData = audioData else {
            print("No audio data loaded.")
            return
        }
        
        if !krispModelLoaded {
            print("No model data loaded.")
            return
        }
        let progressCallback: (UInt, UInt) -> Void = { numberOfFramesProcessed, processingTimeInSeconds in
            processedFrames = numberOfFramesProcessed
            fileProcessingTimeSeconds = processingTimeInSeconds
        }
        let headerCallback: (UInt) -> Void = { numberOfFrames in
            totalFrames = numberOfFrames
        }
        var success = false
        
        var tmpProcessedNSData: NSData?
        await withCheckedContinuation { continuation in
            DispatchQueue.global(qos: .userInitiated).async {
                success = krispAudioProcessor.processWavAudioData(
                    audioData,
                    processedAudioData: &tmpProcessedNSData,
                    progressCallback: progressCallback,
                    headerCallback: headerCallback
                )
                continuation.resume()
            }
        }
        if success {
            processedAudioData = tmpProcessedNSData as? Data
        } else {
            processedAudioData = nil
            processedAudioDataSaved = false
        }
        isProcessing = false
    }
}

// MARK: - A simple FileDocument to export the processed audio
struct ProcessedAudioDocument: FileDocument {
    static var readableContentTypes = [UTType.wav]
    var data: Data
    
    init(data: Data) {
        self.data = data
    }
    
    init(configuration: ReadConfiguration) throws {
        data = Data()
    }
     
    func fileWrapper(configuration: WriteConfiguration) throws -> FileWrapper {
        return .init(regularFileWithContents: data)
    }
}

#Preview {
    FileProcessingView()
}
