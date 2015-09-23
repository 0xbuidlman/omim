//
//  MWMBookmarkColorViewController.m
//  Maps
//
//  Created by v.mikhaylenko on 27.05.15.
//  Copyright (c) 2015 MapsWithMe. All rights reserved.
//

#import "MWMBookmarkColorViewController.h"
#import "UIKitCategories.h"
#import "MWMBookmarkColorCell.h"
#import "MWMPlacePageEntity.h"
#import "MWMPlacePageViewManager.h"
#import "UIViewController+navigation.h"

extern NSArray * const kBookmarkColorsVariant;

static NSString * const kBookmarkColorCellIdentifier = @"MWMBookmarkColorCell";

@interface MWMBookmarkColorViewController ()

@property (weak, nonatomic) IBOutlet UITableView * tableView;
@property (nonatomic) BOOL colorWasChanged;

@end

@interface MWMBookmarkColorViewController (TableView) <UITableViewDataSource, UITableViewDelegate>
@end

@implementation MWMBookmarkColorViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
  self.title = L(@"bookmark_color");
  [self.tableView registerNib:[UINib nibWithNibName:kBookmarkColorCellIdentifier bundle:nil] forCellReuseIdentifier:kBookmarkColorCellIdentifier];
  self.colorWasChanged = NO;
}

- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
  [self configureTableViewForOrientation:self.interfaceOrientation];
  [self.tableView reloadData];
  if (!self.iPadOwnerNavigationController)
    return;

  [self.iPadOwnerNavigationController setNavigationBarHidden:NO];
  CGFloat const bottomOffset = 88.;
  self.iPadOwnerNavigationController.view.height = self.tableView.height + bottomOffset;
  [self showBackButton];
}

- (void)backTap
{
  if (self.iPadOwnerNavigationController)
   [self.iPadOwnerNavigationController setNavigationBarHidden:YES];

  [self.navigationController popViewControllerAnimated:YES];
}

- (void)configureTableViewForOrientation:(UIInterfaceOrientation)orientation
{
  if (self.iPadOwnerNavigationController)
    return;

  CGFloat const defaultHeight = 352.;
  CGSize const size = self.navigationController.view.bounds.size;
  CGFloat const topOffset = 24.;

  switch (orientation)
  {
    case UIInterfaceOrientationUnknown:
      break;

    case UIInterfaceOrientationPortraitUpsideDown:
    case UIInterfaceOrientationPortrait:
    {
      CGFloat const width = MIN(size.width, size.height);
      CGFloat const height = MAX(size.width, size.height);
      CGFloat const externalHeight = self.navigationController.navigationBar.height + [[UIApplication sharedApplication] statusBarFrame].size.height;
      CGFloat const actualHeight = defaultHeight > (height - externalHeight) ? height : defaultHeight;
      self.tableView.frame = CGRectMake(0., topOffset, width, actualHeight);
      break;
    }

    case UIInterfaceOrientationLandscapeLeft:
    case UIInterfaceOrientationLandscapeRight:
    {
      CGFloat const navBarHeight = self.navigationController.navigationBar.height + [UIApplication sharedApplication].statusBarFrame.size.height;
      CGFloat const width = MAX(size.width, size.height);
      CGFloat const height = MIN(size.width, size.height);
      CGFloat const currentHeight = height - navBarHeight - 2 * topOffset;
      CGFloat const actualHeight = currentHeight > defaultHeight ? defaultHeight : currentHeight;
      self.tableView.frame = CGRectMake(0., topOffset, width, actualHeight);
      break;
    }
  }
}

- (void)didRotateFromInterfaceOrientation:(UIInterfaceOrientation)fromInterfaceOrientation
{
  [self configureTableViewForOrientation:self.interfaceOrientation];
}

- (BOOL)shouldAutorotate
{
  return YES;
}

- (void)viewWillDisappear:(BOOL)animated
{
  [super viewWillDisappear:animated];
  if (self.colorWasChanged && !self.iPadOwnerNavigationController)
    [self.placePageManager reloadBookmark];
}

@end

@implementation MWMBookmarkColorViewController (TableView)

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
  MWMBookmarkColorCell * cell = (MWMBookmarkColorCell *)[tableView dequeueReusableCellWithIdentifier:kBookmarkColorCellIdentifier];
  if (!cell)
    cell = [[MWMBookmarkColorCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:kBookmarkColorCellIdentifier];

  NSString * const currentColor = kBookmarkColorsVariant[indexPath.row];
  [cell configureWithColorString:kBookmarkColorsVariant[indexPath.row]];

  if ([currentColor isEqualToString:self.placePageManager.entity.bookmarkColor] && !cell.selected)
    [tableView selectRowAtIndexPath:indexPath animated:NO scrollPosition:UITableViewScrollPositionNone];

  return cell;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
  return kBookmarkColorsVariant.count;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
  self.colorWasChanged = YES;
  self.placePageManager.entity.bookmarkColor = kBookmarkColorsVariant[indexPath.row];
  if (!self.iPadOwnerNavigationController)
    return;

  [self.placePageManager reloadBookmark];
}

@end
