//
//  AudioCapturer.swift
//  KrispTestApp
//
//  Created by Aram Tatalyan on 09.01.25.
//

import Foundation

import AVFoundation


class AudioCapturer : ObservableObject {

    private var audioEngine: AVAudioEngine?
    private var audioFormat: AVAudioFormat?
    private var inputFormat: AVAudioFormat?

    private var sampleRate: UInt32 = 44100
    private let bytesPerSample: Int = 4
    private let frameDurationMs: UInt32 = 10
    private let channels: UInt32 = 1

    private let krispAudioProcessor: KrispAudioProcessor

    init(krispAudioProcessor:KrispAudioProcessor) {
        self.krispAudioProcessor = krispAudioProcessor
    }

    private var samplesPerFrame: Int {
        return Int(sampleRate * frameDurationMs / 1000)
    }

    private var bytesPerFrame: Int {
        return samplesPerFrame * bytesPerSample
    }

    private var bytesPerMinute: Int {
        return Int(sampleRate) * 60 * bytesPerSample
    }


    private var numberOfMinsProcessed = 0;

    private var processedData = Data()
    private var numberOfBytesProcessed: Int = 0

    private var dataBlockSize = 48000;
    private var dataBlockReserve = 4800;
    private var processedNcDataArray = Array<Data>()
    private var currentDataBlock = Data()
    private var processedNcDataBlock = Data()

    private var onMinuteProcessedCallback: ((Int) -> Void)? = nil

    private var isRealtime  = true

    func startRecording(mode : String,
                        captureDurationInMinutes : Int,
                        captureModeBlockSizeInMinutes : Float,
                        minuteProcessedCallback: @escaping (Int) -> Void
    ) {
        
        if (mode != "Realtime" && mode != "Distributed") {
            print("Invalid mode: \(mode)")
            return
        }
        if mode == "Realtime" {
            self.isRealtime = true
        }
        else {
            self.isRealtime = false
        }
        self.onMinuteProcessedCallback = minuteProcessedCallback
#if os(iOS)
        let session = AVAudioSession.sharedInstance()
        do {
            try session.setCategory(.record, mode: .default)
            try session.setActive(true)
            
            self.sampleRate = UInt32(session.sampleRate)
        } catch {
            print("AVAudioSesion error: \(error)")
            return
        }
#endif
        if !krispAudioProcessor.setSampleRate(self.sampleRate) {
            print("Failed to use Krisp with \(self.sampleRate) sample rate.")
            return
        }

        audioEngine = AVAudioEngine()
        guard let audioEngine = audioEngine else { return }

        let input = audioEngine.inputNode

        let settings: [String: Any] = [
            AVFormatIDKey: kAudioFormatLinearPCM,
            AVSampleRateKey: sampleRate,
            AVNumberOfChannelsKey: channels,
            AVLinearPCMBitDepthKey: 32,
            AVLinearPCMIsFloatKey: true,
            AVLinearPCMIsBigEndianKey: false
        ]
        guard let format = AVAudioFormat(settings: settings) else {
            print("Failed to create AVAudioFormat")
            return
        }
        inputFormat = format

        input.installTap(onBus: 0, bufferSize: 1024, format: format) { buffer, time in
            let audioBufferList = buffer.audioBufferList
            let audioBuffer = audioBufferList.pointee.mBuffers
            if let mData = audioBuffer.mData {
                let numBytes = Int(audioBuffer.mDataByteSize)
                let dataChunk = Data(bytes: mData, count: numBytes)
                if self.isRealtime {
                    self.processAudioDataChunkRealtime(data:dataChunk)
                }
                else {
                    self.processAudioDataDistributed(data:dataChunk)
                }
                
            }
        }
        let isFloat = format.isInterleaved
        self.processedData = Data()
        self.currentDataBlock = Data()
        self.processedNcDataBlock = Data()
        self.processedNcDataArray = Array<Data>()

        if !self.isRealtime {
            let captureBlockSize = Int(ceil(captureModeBlockSizeInMinutes * 60.0 * 100.0 * Float(bytesPerFrame)))
            self.dataBlockSize = captureBlockSize
            self.dataBlockReserve = captureBlockSize / 10
            self.currentDataBlock.reserveCapacity(self.dataBlockSize + self.dataBlockReserve)
            self.processedNcDataBlock.reserveCapacity(self.dataBlockSize + self.dataBlockReserve)
        }
        
        do {
            try audioEngine.start()
            print("Audio Engine started with sample rate: \(format.sampleRate), Float32: \(isFloat)")
        } catch {
            print("Audio Engine error: \(error)")
        }
    }

    private func processAudioDataChunkRealtime(data : Data) {
        currentDataBlock.append(data)
        let completeFramesCount = currentDataBlock.count / bytesPerFrame
        if completeFramesCount == 0 {
            return
        }
        let blockSize = completeFramesCount * bytesPerFrame
        let ncDataBlock = Data(count: blockSize)
        krispAudioProcessor.processAudioFrames(currentDataBlock, processedAudioData: ncDataBlock)
        processedNcDataBlock.append(ncDataBlock)
        currentDataBlock.removeFirst(blockSize)
        self.numberOfBytesProcessed += blockSize
        if processedNcDataBlock.count > dataBlockSize {
            processedNcDataArray.append(processedNcDataBlock)
            processedNcDataBlock = Data()
            processedNcDataBlock.reserveCapacity(dataBlockSize + dataBlockReserve)
        }
        if self.numberOfBytesProcessed / self.bytesPerMinute > self.numberOfMinsProcessed {
            self.numberOfMinsProcessed += 1
            self.onMinuteProcessedCallback?(self.numberOfMinsProcessed)
        }
   }

    private func processAudioDataDistributed(data : Data) {
        currentDataBlock.append(data)
        if currentDataBlock.count > dataBlockSize {
            let completeFramesCount = currentDataBlock.count / bytesPerFrame
            let blockSize = completeFramesCount * bytesPerFrame
            self.processedNcDataBlock = Data(count: blockSize)
            self.krispAudioProcessor.processAudioFrames(self.currentDataBlock, processedAudioData: self.processedNcDataBlock)
            self.currentDataBlock.removeFirst(blockSize)
            self.currentDataBlock.reserveCapacity(dataBlockSize + dataBlockReserve)
            self.processedNcDataArray.append(processedNcDataBlock)
            self.processedNcDataBlock = Data()
            self.processedNcDataBlock.reserveCapacity(dataBlockSize + dataBlockReserve)
            self.numberOfBytesProcessed += blockSize
            if self.numberOfBytesProcessed / self.bytesPerMinute > self.numberOfMinsProcessed {
                self.numberOfMinsProcessed += 1
                self.onMinuteProcessedCallback?(self.numberOfMinsProcessed)
            }
        }
    }

    func stopRecording() {
        self.audioEngine?.stop()
        self.audioEngine?.inputNode.removeTap(onBus: 0)
        self.audioEngine = nil
        print("Audio Engine stopped")
    }
}
