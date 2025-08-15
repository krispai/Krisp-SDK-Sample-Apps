#import <Foundation/Foundation.h>
#import <dlfcn.h>

#import "KrispTestApp-Bridging-Header.h"

#include "KrispAudioSDK/krisp-audio-sdk.hpp"
#include "KrispAudioSDK/krisp-audio-sdk-nc.hpp"


static BOOL isLoaded = NO;

@implementation KrispAudioSDK


+ (BOOL) load {
    static NSObject *mutex = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        mutex = [[NSObject alloc] init];
    });
    @synchronized (mutex) {
        if (isLoaded) {
            NSLog(@"Krisp Audio SDK is already loaded");
            return YES;
        }
        Krisp::AudioSdk::globalInit(L"");
        isLoaded = YES;
        return YES;
    }
}

+ (BOOL) unload {
    return NO;
}

+ (NSString *) getVersion {
    Krisp::AudioSdk::VersionInfo version;
    Krisp::AudioSdk::getVersion(version);
    return [NSString stringWithFormat:@"%d.%d.%d.%d",
            version.major,
            version.minor,
            version.patch,
            version.build];
}

@end
