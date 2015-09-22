
#import <Foundation/Foundation.h>

@interface LocalNotificationManager : NSObject

+ (instancetype)sharedManager;

- (void)showDownloadMapNotificationIfNeeded:(void (^)(UIBackgroundFetchResult))completionHandler;
- (void)updateLocalNotifications;
- (void)processNotification:(UILocalNotification *)notification onLaunch:(BOOL)onLaunch;

@end
