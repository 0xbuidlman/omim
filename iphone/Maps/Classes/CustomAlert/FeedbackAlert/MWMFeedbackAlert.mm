//
//  MWMFeedbackAlert.m
//  Maps
//
//  Created by v.mikhaylenko on 25.03.15.
//  Copyright (c) 2015 MapsWithMe. All rights reserved.
//

#import "MWMFeedbackAlert.h"
#import "MWMAlertViewController.h"
#import <MessageUI/MFMailComposeViewController.h>
#import "UIKitCategories.h"
#import <sys/utsname.h>

#include "../../platform/platform.hpp"

@interface MWMFeedbackAlert () <MFMailComposeViewControllerDelegate>

@property (nonatomic, weak) IBOutlet UIView *specsView;
@property (nonatomic, weak) IBOutlet UILabel *titleLabel;
@property (nonatomic, weak) IBOutlet UILabel *messageLabel;
@property (nonatomic, weak) IBOutlet UIButton *notNowButton;
@property (nonatomic, weak) IBOutlet UIButton *writeFeedbackButton;
@property (nonatomic, weak) IBOutlet UILabel *writeFeedbackLabel;
@property (nonatomic, assign) NSUInteger starsCount;

@end

static NSString * const kFeedbackAlertNibName = @"MWMFeedbackAlert";
static NSString * const kRateEmail = @"rating@maps.me";
extern NSDictionary * const deviceNames;
extern UIColor * const kActiveDownloaderViewColor;
extern NSString * const kLocaleUsedInSupportEmails;

@implementation MWMFeedbackAlert

+ (instancetype)alertWithStarsCount:(NSUInteger)starsCount {
  MWMFeedbackAlert *alert = [[[NSBundle mainBundle] loadNibNamed:kFeedbackAlertNibName owner:self options:nil] firstObject];
  alert.starsCount = starsCount;
  [alert configure];
  return alert;
}

#pragma mark - Actions

- (IBAction)notNowButtonTap:(id)sender {
  [self.alertController closeAlert];
}

- (IBAction)writeFeedbackButtonTap:(id)sender {
  [UIView animateWithDuration:0.2f animations:^{
    self.specsView.backgroundColor = kActiveDownloaderViewColor;
  } completion:^(BOOL finished) {
    self.alpha = 0.;
    self.alertController.view.alpha = 0.;
    if ([MFMailComposeViewController canSendMail])
    {
      struct utsname systemInfo;
      uname(&systemInfo);
      NSString * machine = [NSString stringWithCString:systemInfo.machine encoding:NSUTF8StringEncoding];
      NSString * device = deviceNames[machine];
      if (!device)
        device = machine;
      NSString * languageCode = [[NSLocale preferredLanguages] firstObject];
      NSString * language = [[NSLocale localeWithLocaleIdentifier:kLocaleUsedInSupportEmails] displayNameForKey:NSLocaleLanguageCode value:languageCode];
      NSString * locale = [[NSLocale currentLocale] objectForKey:NSLocaleCountryCode];
      NSString * country = [[NSLocale localeWithLocaleIdentifier:kLocaleUsedInSupportEmails] displayNameForKey:NSLocaleCountryCode value:locale];
      NSString * bundleVersion = [[NSBundle mainBundle] objectForInfoDictionaryKey:(NSString *)kCFBundleVersionKey];
      NSString * text = [NSString stringWithFormat:@"\n\n\n\n- %@ (%@)\n- MAPS.ME %@\n- %@/%@", device, [UIDevice currentDevice].systemVersion, bundleVersion, language, country];
      MFMailComposeViewController *mailController = [[MFMailComposeViewController alloc] init];
      mailController.mailComposeDelegate = self;
      [mailController setSubject:[NSString stringWithFormat:@"%@ : %@", L(@"rating_just_rated"),@(self.starsCount)]];
      [mailController setToRecipients:@[kRateEmail]];
      [mailController setMessageBody:text isHTML:NO];
      [self.alertController.ownerViewController presentViewController:mailController animated:YES completion:nil];
    } else {
      NSString * text = [NSString stringWithFormat:L(@"email_error_body"), kRateEmail];
      [[[UIAlertView alloc] initWithTitle:L(@"email_error_title") message:text delegate:nil cancelButtonTitle:L(@"ok") otherButtonTitles:nil] show];
    }
  }];
}

#pragma mark - MFMailComposeViewControllerDelegate

- (void)mailComposeController:(MFMailComposeViewController *)controller didFinishWithResult:(MFMailComposeResult)result error:(NSError *)error {
  [self.alertController.ownerViewController dismissViewControllerAnimated:YES completion:^{
    [self.alertController closeAlert];
  }];
}

#pragma mark - Configure

- (void)configure {
  [self.titleLabel sizeToFit];
  [self.messageLabel sizeToFit];
  [self configureMainViewSize];
}

- (void)configureMainViewSize {
  CGFloat const topMainViewOffset = 17.;
  CGFloat const secondMainViewOffset = 14.;
  CGFloat const thirdMainViewOffset = 20.;
  CGFloat const bottomMainViewOffset = 52.;
  CGFloat const mainViewHeight = topMainViewOffset + self.titleLabel.height + secondMainViewOffset + self.messageLabel.height + thirdMainViewOffset + self.specsView.height + bottomMainViewOffset;
  self.height = mainViewHeight;
  self.titleLabel.minY = topMainViewOffset;
  self.messageLabel.minY = self.titleLabel.minY + self.titleLabel.height + secondMainViewOffset;
  self.specsView.minY = self.messageLabel.minY + self.messageLabel.height + thirdMainViewOffset;
  self.notNowButton.minY = self.specsView.minY + self.specsView.height;
}

@end
