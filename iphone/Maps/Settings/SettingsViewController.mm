
#import "SettingsViewController.h"
#import "SwitchCell.h"
#import "SelectableCell.h"
#import "LinkCell.h"
#import "WebViewController.h"
#import "UIKitCategories.h"
#include "../../../platform/settings.hpp"
#include "../../../platform/platform.hpp"
#include "../../../platform/preferred_languages.hpp"
#import "MapViewController.h"
#import "MapsAppDelegate.h"
#import "Framework.h"

typedef NS_ENUM(NSUInteger, Section)
{
  SectionMetrics,
  SectionZoomButtons,
  SectionStatistics,
  SectionCount
};

@interface SettingsViewController () <SwitchCellDelegate>

@end

@implementation SettingsViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
  self.title = NSLocalizedString(@"settings", nil);
  self.tableView.backgroundView = nil;
  self.tableView.backgroundColor = [UIColor applicationBackgroundColor];
}

- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
}

- (void)viewWillDisappear:(BOOL)animated
{
    [super viewWillDisappear:animated];
    GetFramework().Invalidate(true);
}

#pragma mark - Table view data source

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView
{
  return SectionCount;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
  if (section == SectionMetrics)
    return 2;
  else
    return 1;
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
  UITableViewCell * cell = nil;
  if (indexPath.section == SectionMetrics)
  {
    cell = [tableView dequeueReusableCellWithIdentifier:[SelectableCell className]];
    Settings::Units units;
    if (!Settings::Get("Units", units))
      units = Settings::Metric;
    BOOL selected = units == unitsForIndex(indexPath.row);

    SelectableCell * customCell = (SelectableCell *)cell;
    customCell.accessoryType = selected ? UITableViewCellAccessoryCheckmark : UITableViewCellAccessoryNone;
    customCell.titleLabel.text = indexPath.row == 0 ? NSLocalizedString(@"kilometres", nil) : NSLocalizedString(@"miles", nil);
  }
  else if (indexPath.section == SectionStatistics)
  {
    cell = [tableView dequeueReusableCellWithIdentifier:[SwitchCell className]];
    SwitchCell * customCell = (SwitchCell *)cell;
    bool on;
    if (!Settings::Get("StatisticsEnabled", on))
      on = true;
    customCell.switchButton.on = on;
    customCell.titleLabel.text = NSLocalizedString(@"allow_statistics", nil);
    customCell.delegate = self;
  }
  else if (indexPath.section == SectionZoomButtons)
  {
    cell = [tableView dequeueReusableCellWithIdentifier:[SwitchCell className]];
    SwitchCell * customCell = (SwitchCell *)cell;
    bool on;
    if (!Settings::Get("ZoomButtonsEnabled", on))
      on = false;
    customCell.switchButton.on = on;
    customCell.titleLabel.text = NSLocalizedString(@"pref_zoom_title", nil);
    customCell.delegate = self;
  }

  return cell;
}

- (NSString *)tableView:(UITableView *)tableView titleForFooterInSection:(NSInteger)section
{
  if (section == SectionStatistics)
    return NSLocalizedString(@"allow_statistics_hint", nil);
  else if (section == SectionZoomButtons)
    return NSLocalizedString(@"pref_zoom_summary", nil);
  else
    return nil;
}

- (void)switchCell:(SwitchCell *)cell didChangeValue:(BOOL)value
{
  NSIndexPath * indexPath = [self.tableView indexPathForCell:cell];
  if (indexPath.section == SectionStatistics)
  {
    Settings::Set("StatisticsEnabled", (bool)value);
  }
  else if (indexPath.section == SectionZoomButtons)
  {
    Settings::Set("ZoomButtonsEnabled", (bool)value);
    [MapsAppDelegate theApp].m_mapViewController.zoomButtonsView.hidden = !value;
  }
}

Settings::Units unitsForIndex(NSInteger index)
{
  return index == 0 ? Settings::Metric : Settings::Foot;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
  if (indexPath.section == SectionMetrics)
  {
    Settings::Units units = unitsForIndex(indexPath.row);
    Settings::Set("Units", units);
    [tableView reloadSections:[NSIndexSet indexSetWithIndex:SectionMetrics] withRowAnimation:UITableViewRowAnimationFade];
    [[MapsAppDelegate theApp].m_mapViewController setupMeasurementSystem];
  }
}

- (NSString *)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section
{
  if (section == SectionMetrics)
    return NSLocalizedString(@"measurement_units", nil);
  else
    return nil;
}

@end
