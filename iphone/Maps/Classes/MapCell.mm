
#import "MapCell.h"
#import "UIKitCategories.h"

@interface MapCell () <ProgressViewDelegate>

@property (nonatomic) UILabel * titleLabel;
@property (nonatomic) UILabel * subtitleLabel;
@property (nonatomic) UILabel * statusLabel;
@property (nonatomic) UILabel * sizeLabel;
@property (nonatomic) ProgressView * progressView;
@property (nonatomic) UIImageView * arrowView;
@property (nonatomic) BadgeView * badgeView;
@property (nonatomic) UIImageView * routingImageView;
@property (nonatomic) UIImageView * separator;

@property (nonatomic, readonly) BOOL progressMode;

@end

@implementation MapCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style reuseIdentifier:(NSString *)reuseIdentifier
{
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];

  NSArray * subviews = @[self.titleLabel, self.subtitleLabel, self.statusLabel, self.sizeLabel, self.progressView, self.arrowView, self.badgeView, self.routingImageView, self.separator];
  for (UIView * subview in subviews)
    [self.contentView addSubview:subview];

  return self;
}

- (void)setStatus:(TStatus)status options:(TMapOptions)options animated:(BOOL)animated
{
  self.status = status;
  self.options = options;

  self.progressView.failedMode = NO;

  if (options == TMapOptions::EMapOnly)
    self.routingImageView.image = [UIImage imageNamed:@"DownloadRoutingButton"];
  else
    self.routingImageView.image = [UIImage imageNamed:@"RoutingDownloadedButton"];

  switch (status)
  {
    case TStatus::ENotDownloaded:
    case TStatus::EOnDiskOutOfDate:
      if (status == TStatus::ENotDownloaded)
        self.statusLabel.text = L(@"download").uppercaseString;
      else
       self.statusLabel.text = L(@"downloader_status_outdated").uppercaseString;

      self.statusLabel.textColor = [UIColor colorWithColorCode:@"179E4D"];
      [self setProgressMode:NO withAnimatedLayout:animated];
      break;

    case TStatus::EInQueue:
    case TStatus::EDownloading:
      self.statusLabel.textColor = [UIColor colorWithColorCode:@"999999"];
      [self setDownloadProgress:self.downloadProgress animated:animated];
      if (status == TStatus::EInQueue)
        self.statusLabel.text = L(@"downloader_queued").uppercaseString;
      break;

    case TStatus::EOnDisk:
    {
      self.statusLabel.text = L(@"downloader_downloaded").uppercaseString;
      self.statusLabel.textColor = [UIColor colorWithColorCode:@"999999"];
      if (animated)
      {
        [self alignSubviews];
        [self performAfterDelay:0.3 block:^{
          [self setProgressMode:NO withAnimatedLayout:YES];
        }];
      }
      else
      {
        [self setProgressMode:NO withAnimatedLayout:NO];
      }
      break;
    }

    case TStatus::EOutOfMemFailed:
    case TStatus::EDownloadFailed:
      self.progressView.failedMode = YES;
      self.statusLabel.text = L(@"downloader_retry").uppercaseString;
      self.statusLabel.textColor = [UIColor colorWithColorCode:@"FF4436"];
      [self setProgressMode:YES withAnimatedLayout:animated];
      break;

    case TStatus::EUnknown:
      break;
  }
}

- (void)setDownloadProgress:(double)downloadProgress animated:(BOOL)animated
{
  self.downloadProgress = downloadProgress;
  self.statusLabel.text = [NSString stringWithFormat:@"%i%%", NSInteger(downloadProgress * 100)];
  [self.progressView setProgress:downloadProgress animated:animated];
  if (!self.progressMode)
    [self setProgressMode:YES withAnimatedLayout:animated];
}

- (void)setProgressMode:(BOOL)progressMode withAnimatedLayout:(BOOL)withLayout
{
  _progressMode = progressMode;

  self.progressView.progress = self.downloadProgress;
  if (withLayout)
  {
    if (progressMode)
      self.progressView.hidden = NO;
    [UIView animateWithDuration:0.5 delay:0 damping:0.9 initialVelocity:0 options:UIViewAnimationOptionCurveEaseIn animations:^{
      [self alignProgressView];
      [self alignSubviews];
    } completion:^(BOOL finished) {
      if (!progressMode)
        self.progressView.hidden = YES;
    }];
  }
  else
  {
    [self alignProgressView];
    [self alignSubviews];
  }
}

- (void)alignProgressView
{
  self.progressView.minX = self.progressMode ? self.width - [self rightOffset] + 2 : self.width;
}

- (void)alignSubviews
{
  self.progressView.hidden = self.parentMode || !self.progressMode;
  self.progressView.midY = self.height / 2;

  self.arrowView.center = CGPointMake(self.width - [self minimumRightOffset] - 4, self.height / 2);
  self.arrowView.hidden = !self.parentMode;

  [self.statusLabel sizeToIntegralFit];
  self.statusLabel.width = MAX(self.statusLabel.width, 60);
  [self.sizeLabel sizeToIntegralFit];
  self.statusLabel.frame = CGRectMake(self.width - [self rightOffset] - self.statusLabel.width, 14, self.statusLabel.width, 16);
  self.statusLabel.hidden = self.parentMode;

  CGFloat const sizeLabelMinY = self.statusLabel.maxY;
  self.sizeLabel.frame = CGRectMake(self.width - [self rightOffset] - self.sizeLabel.width, sizeLabelMinY, self.sizeLabel.width, 16);
  self.sizeLabel.textColor = [UIColor colorWithColorCode:@"999999"];
  self.sizeLabel.hidden = self.parentMode;

  CGFloat const rightLabelsMaxWidth = self.parentMode ? 10 : MAX(self.statusLabel.width, self.sizeLabel.width);
  CGFloat const leftLabelsWidth = self.width - [self leftOffset] - [self betweenSpace] - rightLabelsMaxWidth - [self rightOffset];

  CGFloat const titleLabelWidth = [self.titleLabel.text sizeWithDrawSize:CGSizeMake(1000, 20) font:self.titleLabel.font].width;
  self.titleLabel.frame = CGRectMake([self leftOffset], self.subtitleLabel.text == nil ? 19 : 10, MIN(titleLabelWidth, leftLabelsWidth), 20);
  self.subtitleLabel.frame = CGRectMake([self leftOffset], self.titleLabel.maxY + 1, leftLabelsWidth, 18);
  self.subtitleLabel.hidden = self.subtitleLabel.text == nil;

  self.routingImageView.center = CGPointMake(self.width - 25, self.height / 2 - 1);
  self.routingImageView.alpha = [self shouldShowRoutingView];
}

- (void)layoutSubviews
{
  [super layoutSubviews];
  [self alignSubviews];
  if (!self.parentMode)
  {
    [self alignProgressView];
    [self setStatus:self.status options:self.options animated:NO];
  }
  self.badgeView.minX = self.titleLabel.maxX + 3;
  self.badgeView.minY = self.titleLabel.minY - 5;

  self.separator.minX = self.titleLabel.minX;
  self.separator.size = CGSizeMake(self.width - 2 * self.separator.minX, 1);
  self.separator.maxY = self.height + 0.5;
}


- (BOOL)shouldShowRoutingView
{
  return !self.progressMode && !self.parentMode && self.status != TStatus::ENotDownloaded;
}

- (CGFloat)leftOffset
{
  return 12;
}

- (CGFloat)betweenSpace
{
  return 10;
}

- (CGFloat)rightOffset
{
  return self.progressMode || [self shouldShowRoutingView] ? 50 : [self minimumRightOffset];
}

- (CGFloat)minimumRightOffset
{
  return 12;
}

+ (CGFloat)cellHeight
{
  return 59;
}

- (void)rightTap:(id)sender
{
  [self.delegate mapCellDidStartDownloading:self];
}

- (void)progressViewDidStart:(ProgressView *)progress
{
  [self.delegate mapCellDidStartDownloading:self];
}

- (void)progressViewDidCancel:(ProgressView *)progress
{
  [self.delegate mapCellDidCancelDownloading:self];
}

- (UIImageView *)arrowView
{
  if (!_arrowView)
    _arrowView = [[UIImageView alloc] initWithImage:[UIImage imageNamed:@"AccessoryView"]];
  return _arrowView;
}

- (ProgressView *)progressView
{
  if (!_progressView)
  {
    _progressView = [[ProgressView alloc] init];
    _progressView.delegate = self;
  }
  return _progressView;
}

- (UILabel *)titleLabel
{
  if (!_titleLabel)
  {
    _titleLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _titleLabel.backgroundColor = [UIColor clearColor];
    _titleLabel.textColor = [UIColor blackColor];
    _titleLabel.font = [UIFont fontWithName:@"HelveticaNeue" size:17];
  }
  return _titleLabel;
}

- (UILabel *)subtitleLabel
{
  if (!_subtitleLabel)
  {
    _subtitleLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _subtitleLabel.backgroundColor = [UIColor clearColor];
    _subtitleLabel.textColor = [UIColor colorWithColorCode:@"999999"];
    _subtitleLabel.font = [UIFont fontWithName:@"HelveticaNeue-Light" size:13];
  }
  return _subtitleLabel;
}

- (UILabel *)statusLabel
{
  if (!_statusLabel)
  {
    _statusLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _statusLabel.backgroundColor = [UIColor clearColor];
    _statusLabel.textAlignment = NSTextAlignmentRight;
    _statusLabel.font = [UIFont fontWithName:@"HelveticaNeue-Light" size:13];
  }
  return _statusLabel;
}

- (UILabel *)sizeLabel
{
  if (!_sizeLabel)
  {
    _sizeLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _sizeLabel.backgroundColor = [UIColor clearColor];
    _sizeLabel.textAlignment = NSTextAlignmentRight;
    _sizeLabel.font = [UIFont fontWithName:@"HelveticaNeue-Light" size:13];
  }
  return _sizeLabel;
}

- (UIImageView *)routingImageView
{
  if (!_routingImageView)
    _routingImageView = [[UIImageView alloc] initWithImage:[UIImage imageNamed:@"DownloadRoutingButton"]];
  return _routingImageView;
}

- (BadgeView *)badgeView
{
  if (!_badgeView)
    _badgeView = [[BadgeView alloc] init];
  return _badgeView;
}

- (UIImageView *)separator
{
  if (!_separator)
  {
    UIImage * separatorImage = [[UIImage imageNamed:@"MapCellSeparator"] resizableImageWithCapInsets:UIEdgeInsetsZero];
    _separator = [[UIImageView alloc] initWithFrame:CGRectZero];
    _separator.image = separatorImage;
  }
  return _separator;
}

@end
