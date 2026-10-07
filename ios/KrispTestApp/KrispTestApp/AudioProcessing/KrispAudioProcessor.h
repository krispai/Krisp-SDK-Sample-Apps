#ifndef KRISP_AUDIO_PROCESSOR_H
#define KRISP_AUDIO_PROCESSOR_H

#pragma mark - KrispAudioBase

@interface KrispAudioBase : NSObject
- (BOOL)loadKrisp:(NSData *)modelData sampleRate: (UInt32) sampleRate;
- (void)unloadKrisp;
- (UInt32)getSampleRate;
- (BOOL)setSampleRate:(UInt32) sampleRate;
@end


#pragma mark - KrispWavFileProcessor

typedef void (^ProcessingCallback)(NSUInteger processedFrames, NSUInteger processingTimeSeconds);
typedef void (^HeaderCallback)(NSUInteger numberOfFrames);


@interface KrispWavFileProcessor : KrispAudioBase
- (BOOL)processWavAudioData:(NSData *)audioData
    processedAudioData: (NSData **) processedAudioData
    progressCallback:(ProcessingCallback)progressCallback
    headerCallback:(HeaderCallback)headerCallback;
- (void)stopProcessing;
@end

#pragma mark - KrispLeakRepro

/// Creates and destroys NC sessions in a loop and reports live malloc bytes and the process footprint after each
/// destroy. Reproduces a leak of the fp16 model's packed layer weights (about 2.9 MiB per cycle with
/// krisp-nc-o-med-v7-fp16.kef at 48 kHz) that survives Nc destruction and globalDestroy.
@interface KrispLeakRepro : NSObject
+ (NSString *)runWithModelData:(NSData *)modelData
                    sampleRate:(UInt32)sampleRate
                    iterations:(int)iterations
                  processFrames:(int)framesPerSession;
@end

#pragma mark - KrispAudioProcessor

@interface KrispAudioProcessor : KrispAudioBase
- (BOOL)processAudioFrames:(NSData *)audioData
        processedAudioData: (NSData *) processedData;
@end

#endif
