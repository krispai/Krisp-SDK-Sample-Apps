#ifndef KrispAudioSDKLoader_h
#define KrispAudioSDKLoader_h

#import <Foundation/Foundation.h>


@interface KrispAudioSDK : NSObject

+ (BOOL)load;
+ (BOOL)unload;
+ (NSString *) getVersion;

@end


#endif /* KrispAudioSDKLoader_h */
