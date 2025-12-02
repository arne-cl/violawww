/*
 * Sound Library - macOS implementation using AppKit/AVFoundation
 */
#ifdef __APPLE__

#import <AppKit/AppKit.h>
#import <AVFoundation/AVFoundation.h>

static float g_bellVolume = 1.0f;  /* 0.0 to 1.0 */
static AVAudioPlayer *g_audioPlayer = nil;

void SLBell_Darwin(void) {
    @autoreleasepool {
        NSString *path = @"/System/Library/Sounds/Tink.aiff";
        NSSound *sound = [[NSSound alloc] initWithContentsOfFile:path byReference:NO];
        if (sound) {
            sound.volume = g_bellVolume;
            [sound play];
        }
    }
}

int SLBellVolume_Darwin(int percent) {
    if (percent >= 0 && percent <= 100) {
        g_bellVolume = percent / 100.0f;
    }
    return (int)(g_bellVolume * 100);
}

void SLPlaySound_Darwin(const char *path) {
    if (!path) return;
    
    @autoreleasepool {
        NSString *soundPath = [NSString stringWithUTF8String:path];
        NSURL *soundURL = [NSURL fileURLWithPath:soundPath];
        
        NSError *error = nil;
        AVAudioPlayer *player = [[AVAudioPlayer alloc] initWithContentsOfURL:soundURL error:&error];
        
        if (player && !error) {
            player.volume = g_bellVolume;
            [player play];
            g_audioPlayer = player;
        }
    }
}

#endif /* __APPLE__ */

