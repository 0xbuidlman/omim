#import "MapViewController.h"
#import "MapsAppDelegate.h"
#import "EAGLView.h"
#import "BookmarksRootVC.h"
#import "UIKitCategories.h"
#import "UIViewController+Navigation.h"
#import "ShareActionSheet.h"
#import "AppInfo.h"
#import "ContainerView.h"
#import "ToolbarView.h"
#import "SelectSetVC.h"
#import "BookmarkDescriptionVC.h"
#import "BookmarkNameVC.h"
#import "SettingsAndMoreVC.h"
#import "RouteView.h"
#import "CountryTreeVC.h"
#import "Reachability.h"
#import "MWMAlertViewController.h"

#import "../../Common/CustomAlertView.h"

#include "Framework.h"
#include "RenderContext.hpp"

#include "../../../anim/controller.hpp"
#include "../../../gui/controller.hpp"
#include "../Statistics/Statistics.h"

#include "../../../map/country_status_display.hpp"
#include "../../../map/dialog_settings.hpp"
#include "../../../map/user_mark.hpp"

#include "../../../platform/file_logging.hpp"
#include "../../../platform/platform.hpp"
#include "../../../platform/settings.hpp"


#define ALERT_VIEW_FACEBOOK 1
#define ALERT_VIEW_APPSTORE 2
#define ALERT_VIEW_DOWNLOADER 5
#define ALERT_VIEW_ROUTING_DISCLAIMER 7
#define FACEBOOK_URL @"http://www.facebook.com/MapsWithMe"
#define FACEBOOK_SCHEME @"fb://profile/111923085594432"

@interface NSValueWrapper : NSObject

-(NSValue *)getInnerValue;

@end

@implementation NSValueWrapper
{
  NSValue * m_innerValue;
}

-(NSValue *)getInnerValue
{
  return m_innerValue;
}

-(id)initWithValue:(NSValue *)value
{
  if (self = [super init])
  {
    m_innerValue = value;
  }
  
  return self;
}

-(BOOL)isEqual:(id)anObject
{
  return [anObject isMemberOfClass:[NSValueWrapper class]];
}

@end

@interface MapViewController () <PlacePageViewDelegate, ToolbarViewDelegate, BottomMenuDelegate, SelectSetVCDelegate, BookmarkDescriptionVCDelegate, BookmarkNameVCDelegate, RouteViewDelegate>

@property (nonatomic) ShareActionSheet * shareActionSheet;
@property (nonatomic) ToolbarView * toolbarView;
@property (nonatomic) UIView * routeViewWrapper;
@property (nonatomic) RouteView * routeView;
@property (nonatomic) ContainerView * containerView;
@property (nonatomic) UIImageView * apiBar;
@property (nonatomic) UILabel * apiTitleLabel;

@end


@implementation MapViewController

#pragma mark - LocationManager Callbacks

- (void)onLocationError:(location::TLocationError)errorCode
{
  GetFramework().OnLocationError(errorCode);

  switch (errorCode)
  {
    case location::EDenied:
    {
      UIAlertView * alert = [[CustomAlertView alloc] initWithTitle:nil
                                                           message:L(@"location_is_disabled_long_text")
                                                          delegate:nil
                                                 cancelButtonTitle:L(@"ok")
                                                 otherButtonTitles:nil];
      [alert show];
      [[MapsAppDelegate theApp].m_locationManager stop:self];
    }
    break;

    case location::ENotSupported:
    {
      UIAlertView * alert = [[CustomAlertView alloc] initWithTitle:nil
                                                           message:L(@"device_doesnot_support_location_services")
                                                          delegate:nil
                                                 cancelButtonTitle:L(@"ok")
                                                 otherButtonTitles:nil];
      [alert show];
      [[MapsAppDelegate theApp].m_locationManager stop:self];
    }
    break;

    default:
      break;
  }
  [self.toolbarView.locationButton setImage:[UIImage imageNamed:@"LocationDefault"] forState:UIControlStateSelected];
}

- (void)onLocationUpdate:(location::GpsInfo const &)info
{
  // TODO: Remove this hack for location changing bug
  if (self.navigationController.visibleViewController == self)
  {
    if (info.m_source != location::EPredictor)
      [m_predictor reset:info];
    Framework & frm = GetFramework();
    frm.OnLocationUpdate(info);
    LOG_MEMORY_INFO();

    [self showPopover];
    [self updateRoutingInfo];
  }
}

- (void)updateRoutingInfo
{
  Framework & frm = GetFramework();
  if (frm.IsRoutingActive())
  {
    location::FollowingInfo res;
    frm.GetRouteFollowingInfo(res);

    if (res.IsValid())
    {
      NSMutableDictionary *routeInfo = [NSMutableDictionary dictionaryWithCapacity:7];
      routeInfo[@"timeToTarget"] = @(res.m_time);
      routeInfo[@"targetDistance"] = [NSString stringWithUTF8String:res.m_distToTarget.c_str()];
      routeInfo[@"targetMetrics"] = [NSString stringWithUTF8String:res.m_targetUnitsSuffix.c_str()];
      routeInfo[@"turnDistance"] = [NSString stringWithUTF8String:res.m_distToTurn.c_str()];
      routeInfo[@"turnMetrics"] = [NSString stringWithUTF8String:res.m_turnUnitsSuffix.c_str()];
      routeInfo[@"turnType"] = [self turnTypeToImage:res.m_turn];
      static NSNumber * turnTypeValue;
      if (res.m_turn == routing::turns::EnterRoundAbout)
        turnTypeValue = @(res.m_exitNum);
      else if (res.m_turn != routing::turns::StayOnRoundAbout)
        turnTypeValue = nil;
      if (turnTypeValue) {
        [routeInfo setObject:turnTypeValue forKey:@"turnTypeValue"];
      }
      
      [self.routeView updateWithInfo:routeInfo];
    }
  }
}

- (NSString *)turnTypeToImage:(routing::turns::TurnDirection)type
{
  using namespace routing::turns;
  switch (type)
  {
    case TurnSlightRight: return @"right-1";
    case TurnRight: return @"right-2";
    case TurnSharpRight: return @"right-3";
      
    case TurnSlightLeft: return @"left-1";
    case TurnLeft: return @"left-2";
    case TurnSharpLeft: return @"left-3";
      
    case UTurn: return @"turn-around";
      
    case LeaveRoundAbout:
    case StayOnRoundAbout:
    case EnterRoundAbout: return @"circle";
      
    default: return @"straight";
  }
}

- (void)onCompassUpdate:(location::CompassInfo const &)info
{
  // TODO: Remove this hack for orientation changing bug
  if (self.navigationController.visibleViewController == self)
    GetFramework().OnCompassUpdate(info);
}

- (void)onLocationStateModeChanged:(location::State::Mode)newMode
{
  [UIApplication sharedApplication].idleTimerDisabled = NO;

  switch (newMode)
  {
    case location::State::UnknownPosition:
    {
      [[MapsAppDelegate theApp] enableStandby];
      [[MapsAppDelegate theApp].m_locationManager stop:self];
      
      PlacePageView * placePage = self.containerView.placePage;
      [[MapsAppDelegate theApp].m_locationManager stop:placePage];
      if ([placePage isMyPosition])
        [placePage setState:PlacePageStateHidden animated:YES withCallback:YES];
      else
        [placePage setState:placePage.state animated:YES withCallback:YES];
      
      self.toolbarView.locationButton.selected = NO;
      [self.toolbarView.locationButton setImage:[UIImage imageNamed:@"LocationDefault"] forState:UIControlStateSelected];
      break;
    }
    case location::State::PendingPosition:
    {
      self.toolbarView.locationButton.selected = YES;
      [self.toolbarView.locationButton setImage:[UIImage imageNamed:@"LocationSearch"] forState:UIControlStateSelected];
      [self.toolbarView.locationButton setSearching];
      
      [[MapsAppDelegate theApp] disableStandby];
      [[MapsAppDelegate theApp].m_locationManager start:self];
      [[NSNotificationCenter defaultCenter] postNotificationName:LOCATION_MANAGER_STARTED_NOTIFICATION object:nil];

      break;
    }
    case location::State::NotFollow:
    case location::State::Follow:
    {
      [self.toolbarView.locationButton setImage:[UIImage imageNamed:@"LocationSelected"] forState:UIControlStateSelected];
      self.toolbarView.locationButton.selected = YES;
      break;
    }
    case location::State::RotateAndFollow:
    {
      [self.toolbarView.locationButton setImage:[UIImage imageNamed:@"LocationFollow"] forState:UIControlStateSelected];
      self.toolbarView.locationButton.selected = YES;
      [UIApplication sharedApplication].idleTimerDisabled = YES;
      break;
    }
  }
}

#pragma mark - Map Navigation

- (void)dismissPlacePage
{
  [self.containerView.placePage setState:PlacePageStateHidden animated:YES withCallback:YES];
}

- (void)onUserMarkClicked:(unique_ptr<UserMarkCopy>)mark
{
  if (self.searchView.state != SearchViewStateFullscreen)
  {
    [self.containerView.placePage showUserMark:std::move(mark)];
    [self.containerView.placePage setState:PlacePageStatePreview animated:YES withCallback:YES];
  }
}

- (void)onMyPositionClicked:(id)sender
{
  GetFramework().GetLocationState()->SwitchToNextMode();
}

- (IBAction)zoomInPressed:(id)sender
{
  GetFramework().Scale(2.0);
}

- (IBAction)zoomOutPressed:(id)sender
{
  GetFramework().Scale(0.5);
}

- (void)processMapClickAtPoint:(CGPoint)point longClick:(BOOL)isLongClick
{
  CGFloat const scaleFactor = self.view.contentScaleFactor;
  m2::PointD const pxClicked(point.x * scaleFactor, point.y * scaleFactor);

  Framework & f = GetFramework();
  f.GetBalloonManager().OnShowMark(f.GetUserMark(pxClicked, isLongClick));
}

- (void)onSingleTap:(NSValueWrapper *)point
{
  [self processMapClickAtPoint:[[point getInnerValue] CGPointValue] longClick:NO];
}

- (void)onLongTap:(NSValueWrapper *)point
{
  [self processMapClickAtPoint:[[point getInnerValue] CGPointValue] longClick:YES];
}

- (void)popoverControllerDidDismissPopover:(UIPopoverController *)popoverController
{
  [self destroyPopover];
  [self invalidate];
}

- (void)updatePointsFromEvent:(UIEvent *)event
{
	NSSet * allTouches = [event allTouches];

  UIView * v = self.view;
  CGFloat const scaleFactor = v.contentScaleFactor;

  // 0 touches are possible from touchesCancelled.
  switch ([allTouches count])
  {
    case 0:
      break;

    case 1:
    {
      CGPoint const pt = [[[allTouches allObjects] objectAtIndex:0] locationInView:v];
      m_Pt1 = m2::PointD(pt.x * scaleFactor, pt.y * scaleFactor);
      break;
    }

    default:
    {
      NSArray * sortedTouches = [[allTouches allObjects] sortedArrayUsingFunction:compareAddress context:NULL];
      CGPoint const pt1 = [[sortedTouches objectAtIndex:0] locationInView:v];
      CGPoint const pt2 = [[sortedTouches objectAtIndex:1] locationInView:v];

      m_Pt1 = m2::PointD(pt1.x * scaleFactor, pt1.y * scaleFactor);
      m_Pt2 = m2::PointD(pt2.x * scaleFactor, pt2.y * scaleFactor);
      break;
    }
  }
}

- (void)stopCurrentAction
{
	switch (m_CurrentAction)
	{
		case NOTHING:
			break;
		case DRAGGING:
			GetFramework().StopDrag(DragEvent(m_Pt1.x, m_Pt1.y));
			break;
		case SCALING:
			GetFramework().StopScale(ScaleEvent(m_Pt1.x, m_Pt1.y, m_Pt2.x, m_Pt2.y));
			break;
	}

	m_CurrentAction = NOTHING;
}

-(void)preformLongTapSelector:(NSValue *)object
{
  [self performSelector:@selector(onLongTap:) withObject:[[NSValueWrapper alloc] initWithValue:object] afterDelay:1.0];
}

-(void)performSingleTapSelector:(NSValue *)object
{
  [self performSelector:@selector(onSingleTap:) withObject:[[NSValueWrapper alloc] initWithValue:object] afterDelay:0.3];
}

-(void)cancelLongTap
{
  [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(onLongTap:) object:[[NSValueWrapper alloc] initWithValue:nil]];
}

-(void)cancelSingleTap
{
  [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(onSingleTap:) object:[[NSValueWrapper alloc] initWithValue:nil]];
}

- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event
{
//  [NSValue valueWithCGPoint:[theTouch locationInView:self.view]]
  // To cancel single tap timer
  UITouch * theTouch = (UITouch *)[touches anyObject];
  if (theTouch.tapCount > 1)
    [self cancelSingleTap];

	[self updatePointsFromEvent:event];

  Framework & f = GetFramework();

	if ([[event allTouches] count] == 1)
	{
    if (f.GetGuiController()->OnTapStarted(m_Pt1))
      return;

		f.StartDrag(DragEvent(m_Pt1.x, m_Pt1.y));
		m_CurrentAction = DRAGGING;

    // Start long-tap timer
    [self preformLongTapSelector:[NSValue valueWithCGPoint:[theTouch locationInView:self.view]]];
    // Temporary solution to filter long touch
    m_touchDownPoint = m_Pt1;
	}
	else
	{
		f.StartScale(ScaleEvent(m_Pt1.x, m_Pt1.y, m_Pt2.x, m_Pt2.y));
		m_CurrentAction = SCALING;
	}

	m_isSticking = true;
}

- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event
{
  m2::PointD const TempPt1 = m_Pt1;
	m2::PointD const TempPt2 = m_Pt2;

	[self updatePointsFromEvent:event];

  // Cancel long-touch timer
  if (!m_touchDownPoint.EqualDxDy(m_Pt1, 9))
    [self cancelLongTap];

  Framework & f = GetFramework();

  if (f.GetGuiController()->OnTapMoved(m_Pt1))
    return;

	if (m_isSticking)
	{
		if ((TempPt1.Length(m_Pt1) > m_StickyThreshold) || (TempPt2.Length(m_Pt2) > m_StickyThreshold))
    {
			m_isSticking = false;
    }
		else
		{
			// Still stickying. Restoring old points and return.
			m_Pt1 = TempPt1;
			m_Pt2 = TempPt2;
			return;
		}
	}

	switch (m_CurrentAction)
	{
    case DRAGGING:
      f.DoDrag(DragEvent(m_Pt1.x, m_Pt1.y));
      //		needRedraw = true;
      break;
    case SCALING:
      if ([[event allTouches] count] < 2)
        [self stopCurrentAction];
      else
      {
        f.DoScale(ScaleEvent(m_Pt1.x, m_Pt1.y, m_Pt2.x, m_Pt2.y));
        //			needRedraw = true;
      }
      break;
    case NOTHING:
      return;
	}
}

- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)event
{
  [self updatePointsFromEvent:event];
  [self stopCurrentAction];

  UITouch * theTouch = (UITouch *)[touches anyObject];
  NSUInteger const tapCount = theTouch.tapCount;
  NSUInteger const touchesCount = [[event allTouches] count];

  Framework & f = GetFramework();

  if (touchesCount == 1)
  {
    // Cancel long-touch timer
    [self cancelLongTap];

    // TapCount could be zero if it was a single long (or moving) tap.
    if (tapCount < 2)
    {
      if (f.GetGuiController()->OnTapEnded(m_Pt1))
        return;
    }

    if (tapCount == 1)
    {
      // Launch single tap timer
      if (m_isSticking)
        [self performSingleTapSelector: [NSValue valueWithCGPoint:[theTouch locationInView:self.view]]];
    }
    else if (tapCount == 2 && m_isSticking)
      f.ScaleToPoint(ScaleToPointEvent(m_Pt1.x, m_Pt1.y, 2.0));
  }

  if (touchesCount == 2 && tapCount == 1 && m_isSticking)
  {
    f.Scale(0.5);
    if (!m_touchDownPoint.EqualDxDy(m_Pt1, 9))
    {
      [self cancelLongTap];
      [self cancelSingleTap];
    }
    m_isSticking = NO;
  }
}

- (void)touchesCancelled:(NSSet *)touches withEvent:(UIEvent *)event
{
  [self cancelLongTap];
  [self cancelSingleTap];

  [self updatePointsFromEvent:event];
  [self stopCurrentAction];
}

#pragma mark - ViewController lifecycle

- (void)dealloc
{
  [self destroyPopover];
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (BOOL)shouldAutorotateToInterfaceOrientation: (UIInterfaceOrientation)interfaceOrientation
{
	return YES; // We support all orientations
}

- (void)didRotateFromInterfaceOrientation: (UIInterfaceOrientation)fromInterfaceOrientation
{
  [self showPopover];
  [self invalidate];
}

- (void)didReceiveMemoryWarning
{
	GetFramework().MemoryWarning();
  [super didReceiveMemoryWarning];
}

- (void)onTerminate
{
  GetFramework().SaveState();
}

- (void)onEnterBackground
{
  // Save state and notify about entering background.

  Framework & f = GetFramework();
  f.SaveState();
  f.SetUpdatesEnabled(false);
  f.EnterBackground();
}

- (void)onEnterForeground
{
  // Notify about entering foreground (should be called on the first launch too).
  GetFramework().EnterForeground();

  if (self.isViewLoaded && self.view.window)
    [self invalidate]; // only invalidate when map is displayed on the screen

  Reachability * reachability = [Reachability reachabilityForInternetConnection];
  if ([reachability isReachable])
  {
    if (dlg_settings::ShouldShow(dlg_settings::AppStore))
      [self showAppStoreRatingMenu];
    else if (dlg_settings::ShouldShow(dlg_settings::FacebookDlg))
      [self showFacebookRatingMenu];
  }
}

- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
  [self invalidate];
}

- (void)viewDidLoad
{
  [super viewDidLoad];

  [self.view addSubview:self.zoomButtonsView];
  self.zoomButtonsView.minY = IPAD ? 560 : 176;
  self.zoomButtonsView.maxX = self.zoomButtonsView.superview.width;
  bool zoomButtonsEnabled;
  if (!Settings::Get("ZoomButtonsEnabled", zoomButtonsEnabled))
    zoomButtonsEnabled = false;
  self.zoomButtonsView.hidden = !zoomButtonsEnabled;

  self.view.clipsToBounds = YES;

  [self.view addSubview:self.toolbarView];
  self.toolbarView.maxY = self.toolbarView.superview.height;

  [self.view addSubview:self.routeViewWrapper];

  [self.view addSubview:self.searchView];

  [self.view addSubview:self.containerView];

  [self.view addSubview:self.bottomMenu];

  [self showRoutingFeatureDialog];
}

- (void)viewDidAppear:(BOOL)animated
{
  [super viewDidAppear:animated];
  static BOOL firstTime = YES;
  if (firstTime)
  {
    firstTime = NO;
    [self setApiMode:_apiMode animated:NO];
  }
}

- (void)viewWillDisappear:(BOOL)animated
{
  GetFramework().SetUpdatesEnabled(false);

  [super viewWillDisappear:animated];
}

- (void)viewDidDisappear:(BOOL)animated
{
  [super viewDidDisappear:animated];
  if (!self.bottomMenu.menuHidden)
    [self.bottomMenu setMenuHidden:YES animated:NO];
}

- (UIStatusBarStyle)preferredStatusBarStyle
{
  if (self.apiMode)
    return UIStatusBarStyleLightContent;
  else
  {
    UIStatusBarStyle style = UIStatusBarStyleDefault;
    if (self.searchView.state != SearchViewStateHidden)
      style = UIStatusBarStyleLightContent;
    if (self.containerView.placePage.state != PlacePageStateHidden)
      style = UIStatusBarStyleDefault;
    return style;
  }
}

- (BOOL)prefersStatusBarHidden
{
  return NO;
}

- (id)initWithCoder:(NSCoder *)coder
{
  NSLog(@"MapViewController initWithCoder Started");

  if ((self = [super initWithCoder:coder]))
  {
    Framework & f = GetFramework();

    typedef void (*UserMarkActivatedFnT)(id, SEL, unique_ptr<UserMarkCopy>);
    typedef void (*PlacePageDismissedFnT)(id, SEL);

    PinClickManager & manager = f.GetBalloonManager();
    
    SEL userMarkSelector = @selector(onUserMarkClicked:);
    UserMarkActivatedFnT userMarkFn = (UserMarkActivatedFnT)[self methodForSelector:userMarkSelector];
    manager.ConnectUserMarkListener(bind(userMarkFn, self, userMarkSelector, _1));

    SEL dismissSelector = @selector(dismissPlacePage);
    PlacePageDismissedFnT dismissFn = (PlacePageDismissedFnT)[self methodForSelector:dismissSelector];
    manager.ConnectDismissListener(bind(dismissFn, self, dismissSelector));

    typedef void (*LocationStateModeFnT)(id, SEL, location::State::Mode);
    SEL locationStateModeSelector = @selector(onLocationStateModeChanged:);
    LocationStateModeFnT locationStateModeFn = (LocationStateModeFnT)[self methodForSelector:locationStateModeSelector];

    f.GetLocationState()->AddStateModeListener(bind(locationStateModeFn, self, locationStateModeSelector, _1));
    
    m_predictor = [[LocationPredictor alloc] initWithObserver:self];

    m_StickyThreshold = 10;

    m_CurrentAction = NOTHING;

    EAGLView * v = (EAGLView *)self.view;
    [v initRenderPolicy];

    // restore previous screen position
    if (!f.LoadState())
      f.SetMaxWorldRect();

    f.Invalidate();
    f.LoadBookmarks();
    
    f.GetCountryStatusDisplay()->SetDownloadCountryListener([](storage::TIndex const & idx, int opt)
    {
      ActiveMapsLayout & layout = GetFramework().GetCountryTree().GetActiveMapLayout();
      if (opt == -1)
        layout.RetryDownloading(idx);
      else
      {
        LocalAndRemoteSizeT sizes = layout.GetRemoteCountrySizes(idx);
        size_t sizeToDownload = sizes.first;
        TMapOptions options = static_cast<TMapOptions>(opt);
        if(options & TMapOptions::ECarRouting)
          sizeToDownload += sizes.second;
        
        NSString * name = [NSString stringWithUTF8String:layout.GetCountryName(idx).c_str()];
        Reachability * reachability = [Reachability reachabilityForInternetConnection];
        if ([reachability isReachable])
        {
          if ([reachability isReachableViaWWAN] && sizeToDownload > 50 * 1024 * 1024)
          {
            NSString * title = [NSString stringWithFormat:L(@"no_wifi_ask_cellular_download"), name];
            
            CustomAlertView * alertView = [[CustomAlertView alloc] initWithTitle:title message:nil delegate:nil cancelButtonTitle:L(@"cancel") otherButtonTitles:L(@"use_cellular_data"), nil];
            alertView.tapBlock = ^(UIAlertView *alertView, NSInteger buttonIndex)
            {
              if (buttonIndex != alertView.cancelButtonIndex)
                layout.DownloadMap(idx, static_cast<TMapOptions>(opt));
            };
            [alertView show];
            return;
          }
        }
        else
        {
          [[[CustomAlertView alloc] initWithTitle:L(@"no_internet_connection_detected") message:L(@"use_wifi_recommendation_text") delegate:nil cancelButtonTitle:L(@"ok") otherButtonTitles:nil] show];
          return;
        }
        
        layout.DownloadMap(idx, static_cast<TMapOptions>(opt));
      }
    });
    
    f.SetRouteBuildingListener([self, &f](routing::IRouter::ResultCode code, vector<storage::TIndex> const & absentCountries)
    {
      [self.containerView.placePage showBuildingRoutingActivity:NO];
      
      switch (code) {
        case routing::IRouter::ResultCode::NoError: {
          f.GetBalloonManager().RemovePin();
          f.GetBalloonManager().Dismiss();
          [self.containerView.placePage setState:PlacePageStateHidden animated:YES withCallback:YES];
          [self.searchView setState:SearchViewStateHidden animated:YES withCallback:YES];
          [self performAfterDelay:0.3 block:^{
            [self.routeView setState:RouteViewStateInfo animated:YES];
            [self updateRoutingInfo];
          }];
          
          bool isDisclaimerApproved = false;
          (void)Settings::Get("IsDisclaimerApproved", isDisclaimerApproved);
          if (!isDisclaimerApproved)
          {
            NSString * title;
            NSString * message;
            if (SYSTEM_VERSION_IS_LESS_THAN(@"7.0"))
              message = L(@"routing_disclaimer");
            else
              title = L(@"routing_disclaimer");
            UIAlertView * alert = [[UIAlertView alloc] initWithTitle:title message:message delegate:self cancelButtonTitle:L(@"cancel") otherButtonTitles:L(@"ok"), nil];
            alert.tag = ALERT_VIEW_ROUTING_DISCLAIMER;
            [alert show];
          }
          break;
        }
          
        case routing::IRouter::RouteFileNotExist:
        case routing::IRouter::RouteNotFound:
        case routing::IRouter::InconsistentMWMandRoute:
          [self presentDownloaderAlert:code countries:absentCountries];
          break;
          
        case routing::IRouter::Cancelled:
          break;
          
        default:
          [self presentDefaultAlert:code];
          break;
      }
    });
  }

  NSLog(@"MapViewController initWithCoder Ended");
  return self;
}

#pragma mark - ShowDialog callback

- (void)showRoutingFeatureDialog
{
  int const outOfDateCount = GetFramework().GetCountryTree().GetActiveMapLayout().GetOutOfDateCount();
  bool isFirstRoutingRun = true;
  (void)Settings::Get("IsFirstRoutingRun", isFirstRoutingRun);
  if (isFirstRoutingRun && outOfDateCount > 0)
  {
    [[[UIAlertView alloc] initWithTitle:L(@"routing_update_maps") message:nil delegate:self cancelButtonTitle:L(@"ok") otherButtonTitles:nil] show];
    Settings::Set("IsFirstRoutingRun", false);
  }
}

- (void)showDialogWithMessageID:(string const &)message
{
  [[[UIAlertView alloc] initWithTitle:[NSString stringWithUTF8String:message.c_str()] message:nil delegate:self cancelButtonTitle:L(@"ok") otherButtonTitles:nil] show];
}

- (void)presentDownloaderAlert:(routing::IRouter::ResultCode)type countries:(vector<storage::TIndex> const&)countries {
  if (countries.size()) {
    MWMAlertViewController *alert = [[MWMAlertViewController alloc] initWithViewController:self];
    [alert presentDownloaderAlertWithCountrieIndex:countries[0]];
  } else {
    [self presentDefaultAlert:type];
  }
}

- (void)presentDefaultAlert:(routing::IRouter::ResultCode)type {
  MWMAlertViewController *alert = [[MWMAlertViewController alloc] initWithViewController:self];
  [alert presentAlert:type];
}

#pragma mark - Getters

- (BottomMenu *)bottomMenu
{
  if (!_bottomMenu)
  {
    _bottomMenu = [[BottomMenu alloc] initWithFrame:self.view.bounds];
    _bottomMenu.delegate = self;
  }
  return _bottomMenu;
}

- (ToolbarView *)toolbarView
{
  if (!_toolbarView)
  {
    _toolbarView = [[ToolbarView alloc] initWithFrame:CGRectMake(0, 0, self.view.width, 44)];
    _toolbarView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleTopMargin;
    _toolbarView.delegate = self;
  }
  return _toolbarView;
}

- (UIView *)zoomButtonsView
{
  if (!_zoomButtonsView)
  {
    _zoomButtonsView = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 48, 88)];
    _zoomButtonsView.autoresizingMask = UIViewAutoresizingFlexibleLeftMargin | UIViewAutoresizingFlexibleTopMargin | UIViewAutoresizingFlexibleBottomMargin;

    UIButton * zoomIn = [self zoomInButton];
    [_zoomButtonsView addSubview:zoomIn];
    zoomIn.midX = zoomIn.superview.width / 2.0;

    UIButton * zoomOut = [self zoomOutButton];
    [_zoomButtonsView addSubview:zoomOut];
    zoomOut.midX = zoomOut.superview.width / 2.0;
    zoomOut.minY = zoomIn.maxY;
  }
  return _zoomButtonsView;
}

- (UIButton *)zoomInButton
{

  UIFont * font = [UIFont fontWithName:@"HelveticaNeue" size:26];

  UIButton * zoomInButton = [[UIButton alloc] initWithFrame:CGRectMake(0, 0, 44, 44)];
  zoomInButton.autoresizingMask = UIViewAutoresizingFlexibleLeftMargin | UIViewAutoresizingFlexibleBottomMargin;
  UIImage * backgroundImage = [[UIImage imageNamed:@"RoutingButtonBackground"] resizableImageWithCapInsets:UIEdgeInsetsMake(5, 5, 5, 5)];
  [zoomInButton setBackgroundImage:backgroundImage forState:UIControlStateNormal];

  zoomInButton.alpha = 0.65;
  zoomInButton.titleLabel.font = font;
  zoomInButton.titleEdgeInsets = UIEdgeInsetsMake(0, 0, 3, 0);
  [zoomInButton setTitle:@"+" forState:UIControlStateNormal];
  [zoomInButton setTitleColor:[UIColor blackColor] forState:UIControlStateNormal];

  [zoomInButton addTarget:self action:@selector(zoomInPressed:) forControlEvents:UIControlEventTouchUpInside];

  return zoomInButton;
}

- (UIButton *)zoomOutButton
{

  UIFont * font = [UIFont fontWithName:@"HelveticaNeue" size:26];

  UIButton * zoomOutButton = [[UIButton alloc] initWithFrame:CGRectMake(0, 0, 44, 44)];
  zoomOutButton.autoresizingMask = UIViewAutoresizingFlexibleLeftMargin | UIViewAutoresizingFlexibleTopMargin;
  UIImage * backgroundImage = [[UIImage imageNamed:@"RoutingButtonBackground"] resizableImageWithCapInsets:UIEdgeInsetsMake(5, 5, 5, 5)];
  [zoomOutButton setBackgroundImage:backgroundImage forState:UIControlStateNormal];

  zoomOutButton.alpha = 0.65;
  zoomOutButton.titleLabel.font = font;
  zoomOutButton.titleEdgeInsets = UIEdgeInsetsMake(0, 0, 3, 0);
  [zoomOutButton setTitle:@"–" forState:UIControlStateNormal];
  [zoomOutButton setTitleColor:[UIColor blackColor] forState:UIControlStateNormal];

  [zoomOutButton addTarget:self action:@selector(zoomOutPressed:) forControlEvents:UIControlEventTouchUpInside];

  return zoomOutButton;
}

- (UIView *)routeViewWrapper
{
  if (!_routeViewWrapper)
  {
    _routeViewWrapper = [[UIView alloc] initWithFrame:self.routeView.bounds];
    _routeViewWrapper.backgroundColor = [UIColor clearColor];
    _routeViewWrapper.autoresizingMask = UIViewAutoresizingFlexibleWidth;
    [_routeViewWrapper addSubview:self.routeView];
  }
  return _routeViewWrapper;
}

- (RouteView *)routeView
{
  if (!_routeView)
  {
    _routeView = [[RouteView alloc] initWithFrame:CGRectMake(0, 0, self.view.width, 80)];
    _routeView.delegate = self;
    _routeView.autoresizingMask = UIViewAutoresizingFlexibleWidth;
  }
  return _routeView;
}

- (SearchView *)searchView
{
  if (!_searchView)
  {
    _searchView = [[SearchView alloc] initWithFrame:self.view.bounds];
    _searchView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    [_searchView addObserver:self forKeyPath:@"state" options:NSKeyValueObservingOptionNew context:nil];
  }
  return _searchView;
}

- (ContainerView *)containerView
{
  if (!_containerView)
  {
    _containerView = [[ContainerView alloc] initWithFrame:self.view.bounds];
    _containerView.placePage.delegate = self;
    [_containerView.placePage addObserver:self forKeyPath:@"state" options:(NSKeyValueObservingOptionNew | NSKeyValueObservingOptionOld) context:nil];
  }
  return _containerView;
}

- (UIImageView *)apiBar
{
  if (!_apiBar)
  {
    UIImage * image = SYSTEM_VERSION_IS_LESS_THAN(@"7") ? [UIImage imageNamed:@"ApiBarBackground6"] : [UIImage imageNamed:@"ApiBarBackground7"];
    _apiBar = [[UIImageView alloc] initWithImage:[image resizableImageWithCapInsets:UIEdgeInsetsZero]];
    _apiBar.width = self.view.width;
    _apiBar.userInteractionEnabled = YES;
    _apiBar.autoresizingMask = UIViewAutoresizingFlexibleWidth;

    UIButton * backButton = [[UIButton alloc] initWithFrame:CGRectMake(0, 0, 60, 44)];
    backButton.contentMode = UIViewContentModeCenter;
    [backButton addTarget:self action:@selector(backToApiApp:) forControlEvents:UIControlEventTouchUpInside];
    [backButton setImage:[UIImage imageNamed:@"ApiBackButton"] forState:UIControlStateNormal];
    backButton.autoresizingMask = UIViewAutoresizingFlexibleRightMargin;
    [_apiBar addSubview:backButton];

    UIButton * clearButton = [[UIButton alloc] initWithFrame:CGRectMake(0, 0, 60, 44)];
    [clearButton setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    [clearButton setTitleColor:[UIColor lightGrayColor] forState:UIControlStateHighlighted];
    [clearButton setTitle:L(@"clear") forState:UIControlStateNormal];
    [clearButton addTarget:self action:@selector(clearApiMode:) forControlEvents:UIControlEventTouchUpInside];
    clearButton.autoresizingMask = UIViewAutoresizingFlexibleLeftMargin;
    clearButton.titleLabel.font = [UIFont fontWithName:@"HelveticaNeue-Light" size:17];
    [_apiBar addSubview:clearButton];

    [_apiBar addSubview:self.apiTitleLabel];

    backButton.minX = -4;
    backButton.maxY = _apiBar.height;
    clearButton.maxX = _apiBar.width - 5;
    clearButton.maxY = _apiBar.height;
    self.apiTitleLabel.midX = _apiBar.width / 2;
    self.apiTitleLabel.maxY = _apiBar.height - 10;
  }
  return _apiBar;
}

- (UILabel *)apiTitleLabel
{
  if (!_apiTitleLabel)
  {
    _apiTitleLabel = [[UILabel alloc] initWithFrame:CGRectMake(0, 0, 240, 26)];
    _apiTitleLabel.font = [UIFont fontWithName:@"HelveticaNeue-Light" size:17];
    _apiTitleLabel.textColor = [UIColor whiteColor];
    _apiTitleLabel.textAlignment = NSTextAlignmentCenter;
    _apiTitleLabel.alpha = 0.5;
    _apiTitleLabel.autoresizingMask = UIViewAutoresizingFlexibleWidth;
  }
  return _apiTitleLabel;
}

#pragma mark - Api methods

- (void)clearApiMode:(id)sender
{
  [self setApiMode:NO animated:YES];
  [self cleanUserMarks];
}

- (void)backToApiApp:(id)sender
{
  NSURL * url = [NSURL URLWithString:[NSString stringWithUTF8String:GetFramework().GetApiDataHolder().GetGlobalBackUrl().c_str()]];
  [[UIApplication sharedApplication] openURL:url];
}

#pragma mark - ToolbarView delegate

- (void)toolbar:(ToolbarView *)toolbar didPressItemWithName:(NSString *)itemName
{
  if ([itemName isEqualToString:@"Location"])
    [self onMyPositionClicked:nil];
  else if ([itemName isEqualToString:@"Search"])
    [self.searchView setState:SearchViewStateFullscreen animated:YES withCallback:YES];
  else if ([itemName isEqualToString:@"Bookmarks"])
  {
    BookmarksRootVC * vc = [[BookmarksRootVC alloc] init];
    [self.navigationController pushViewController:vc animated:YES];
  }
  else if ([itemName isEqualToString:@"Menu"])
    [self.bottomMenu setMenuHidden:NO animated:YES];
}

#pragma mark - Routing

- (void)tryToBuildRoute
{
  [self.routeView updateWithInfo:nil];
  [self.containerView.placePage showBuildingRoutingActivity:YES];
  GetFramework().BuildRoute([self.containerView.placePage pinPoint]);
}

- (void)routeViewDidStartFollowing:(RouteView *)routeView
{
  [UIApplication sharedApplication].idleTimerDisabled = YES;
  [routeView setState:RouteViewStateTurnInstructions animated:YES];
  GetFramework().FollowRoute();
}

- (void)routeViewDidCancelRouting:(RouteView *)routeView
{
  [self dismissRouting];
}

- (void)dismissRouting
{
  [UIApplication sharedApplication].idleTimerDisabled = NO;
  GetFramework().CloseRouting();
  [self.routeView setState:RouteViewStateHidden animated:YES];
}

#pragma mark - PlacePageViewDelegate

- (void)placePageViewDidStartRouting:(PlacePageView *)placePage
{
  [self tryToBuildRoute];
}

- (void)placePageView:(PlacePageView *)placePage willShareText:(NSString *)text point:(m2::PointD)point
{
  ShareInfo * info = [[ShareInfo alloc] initWithText:text gX:point.x gY:point.y myPosition:NO];
  self.shareActionSheet = [[ShareActionSheet alloc] initWithInfo:info viewController:self];
  [self.shareActionSheet showFromRect:CGRectMake(placePage.midX, placePage.maxY, 0, 0)];
}

- (void)placePageView:(PlacePageView *)placePage willEditProperty:(NSString *)propertyName inBookmarkAndCategory:(BookmarkAndCategory const &)bookmarkAndCategory
{
  if ([propertyName isEqualToString:@"Set"])
  {
    SelectSetVC * vc = [[SelectSetVC alloc] initWithBookmarkAndCategory:bookmarkAndCategory];
    vc.delegate = self;
    [self.navigationController pushViewController:vc animated:YES];
  }
  else if ([propertyName isEqualToString:@"Description"])
  {
    BookmarkDescriptionVC * vc = [self.mainStoryboard instantiateViewControllerWithIdentifier:[BookmarkDescriptionVC className]];
    vc.delegate = self;
    vc.bookmarkAndCategory = bookmarkAndCategory;
    [self.navigationController pushViewController:vc animated:YES];
  }
  else if ([propertyName isEqualToString:@"Name"])
  {
    BookmarkNameVC * vc = [self.mainStoryboard instantiateViewControllerWithIdentifier:[BookmarkNameVC className]];
    vc.delegate = self;
    vc.temporaryName = placePage.temporaryTitle;
    vc.bookmarkAndCategory = bookmarkAndCategory;
    [self.navigationController pushViewController:vc animated:YES];
  }
}

- (void)selectSetVC:(SelectSetVC *)vc didUpdateBookmarkAndCategory:(BookmarkAndCategory const &)bookmarkAndCategory
{
  [self updatePlacePageWithBookmarkAndCategory:bookmarkAndCategory];
}

- (void)bookmarkDescriptionVC:(BookmarkDescriptionVC *)vc didUpdateBookmarkAndCategory:(BookmarkAndCategory const &)bookmarkAndCategory
{
  [self updatePlacePageWithBookmarkAndCategory:bookmarkAndCategory];
}

- (void)bookmarkNameVC:(BookmarkNameVC *)vc didUpdateBookmarkAndCategory:(BookmarkAndCategory const &)bookmarkAndCategory
{
  [self updatePlacePageWithBookmarkAndCategory:bookmarkAndCategory];
}

- (void)updatePlacePageWithBookmarkAndCategory:(BookmarkAndCategory const &)bookmarkAndCategory
{
  BookmarkCategory const * category = GetFramework().GetBookmarkManager().GetBmCategory(bookmarkAndCategory.first);
  Bookmark const * bookmark = category->GetBookmark(bookmarkAndCategory.second);

  [self.containerView.placePage showUserMark:bookmark->Copy()];
  [self.containerView.placePage setState:self.containerView.placePage.state animated:YES withCallback:NO];
}

- (void)placePageView:(PlacePageView *)placePage willShareApiPoint:(ApiMarkPoint const *)point
{
  NSString * urlString = [NSString stringWithUTF8String:GetFramework().GenerateApiBackUrl(*point).c_str()];
  [[UIApplication sharedApplication] openURL:[NSURL URLWithString:urlString]];
}

#pragma mark - BottomMenuDelegate

- (void)bottomMenu:(BottomMenu *)menu didPressItemWithName:(NSString *)itemName appURL:(NSString *)appURL webURL:(NSString *)webURL
{
  if ([itemName isEqualToString:@"Maps"])
  {
    CountryTreeVC * vc = [[CountryTreeVC alloc] initWithNodePosition:-1];
    [self.navigationController pushViewController:vc animated:YES];
  }
  else if ([itemName isEqualToString:@"Settings"])
  {
    SettingsAndMoreVC * vc = [[SettingsAndMoreVC alloc] initWithStyle:UITableViewStyleGrouped];
    [self.navigationController pushViewController:vc animated:YES];
  }
  else if ([itemName isEqualToString:@"Share"])
  {
    CLLocation * location = [MapsAppDelegate theApp].m_locationManager.lastLocation;
    if (location)
    {
      double gX = MercatorBounds::LonToX(location.coordinate.longitude);
      double gY = MercatorBounds::LatToY(location.coordinate.latitude);
      ShareInfo * info = [[ShareInfo alloc] initWithText:nil gX:gX gY:gY myPosition:YES];
      self.shareActionSheet = [[ShareActionSheet alloc] initWithInfo:info viewController:self];
      [self.shareActionSheet showFromRect:CGRectMake(menu.midX, self.view.height - 40, 0, 0)];
    }
    else
    {
      [[[UIAlertView alloc] initWithTitle:L(@"unknown_current_position") message:nil delegate:nil cancelButtonTitle:L(@"ok") otherButtonTitles:nil] show];
    }
  }
  else
  {
    [menu setMenuHidden:YES animated:YES];
    [[Statistics instance] logEvent:@"Bottom menu item clicked" withParameters:@{@"Item" : itemName, @"Country": [AppInfo sharedInfo].countryCode}];
    UIApplication * application = [UIApplication sharedApplication];
    NSURL * url = [NSURL URLWithString:appURL];
    if ([application canOpenURL:url])
      [application openURL:url];
    else
      [application openURL:[NSURL URLWithString:webURL]];
  }
}

#pragma mark - UIKitViews delegates

- (void)alertView:(UIAlertView *)alertView clickedButtonAtIndex:(NSInteger)buttonIndex
{
  switch (alertView.tag)
  {
    case ALERT_VIEW_APPSTORE:
    {
      if (buttonIndex == 0)
      {
        dlg_settings::SaveResult(dlg_settings::AppStore, dlg_settings::Never);
      }
      else if (buttonIndex == 1)
      {
        dlg_settings::SaveResult(dlg_settings::AppStore, dlg_settings::OK);
        [[UIApplication sharedApplication] rateVersionFrom:@"ios_pro_popup"];
      }
      else if (buttonIndex == 2)
      {
        dlg_settings::SaveResult(dlg_settings::AppStore, dlg_settings::Later);
      }

      break;
    }
    case ALERT_VIEW_FACEBOOK:
    {
      if (buttonIndex == 0)
      {
        dlg_settings::SaveResult(dlg_settings::FacebookDlg, dlg_settings::Never);
      }
      else if (buttonIndex == 1)
      {
        dlg_settings::SaveResult(dlg_settings::FacebookDlg, dlg_settings::OK);

        NSString * url = [NSString stringWithFormat:FACEBOOK_SCHEME];
        if (![[UIApplication sharedApplication] canOpenURL:[NSURL URLWithString:url]])
          url = [NSString stringWithFormat:FACEBOOK_URL];
        [[UIApplication sharedApplication] openURL:[NSURL URLWithString:url]];
      }
      else if (buttonIndex == 2)
      {
        dlg_settings::SaveResult(dlg_settings::FacebookDlg, dlg_settings::Later);
      }
      break;
    }
    case ALERT_VIEW_ROUTING_DISCLAIMER:
    {
      if (buttonIndex == alertView.cancelButtonIndex)
      {
        [self dismissRouting];
      }
      else
      {
        Settings::Set("IsDisclaimerApproved", true);
      }
      break;
    }
    
    default:
      break;
  }
}


- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
  if (object == self.containerView.placePage && [keyPath isEqualToString:@"state"])
  {
    if ([self respondsToSelector:@selector(setNeedsStatusBarAppearanceUpdate)])
      [self setNeedsStatusBarAppearanceUpdate];
    switch (self.containerView.placePage.state)
    {
    case PlacePageStateHidden:
      {
        if (self.searchView.state == SearchViewStateAlpha)
          [self.searchView setState:SearchViewStateResults animated:YES withCallback:NO];

        GetFramework().GetBalloonManager().RemovePin();

        [UIView animateWithDuration:0.3 animations:^{
          self.toolbarView.maxY = self.view.height;
          if (GetFramework().IsRoutingActive())
          {
            if (self.searchView.state == SearchViewStateResults)
              [self observeValueForKeyPath:@"state" ofObject:self.searchView change:nil context:nil];
            else
            {
              self.routeView.alpha = 1;
              self.routeView.minY = 0;
            }
          }
        }];
        break;
      }
    case PlacePageStatePreview:
      {
        if (self.searchView.state == SearchViewStateResults)
          [self.searchView setState:SearchViewStateAlpha animated:YES withCallback:NO];

        if ([change[@"old"] integerValue] == PlacePageStatePreview || [change[@"old"] integerValue] == PlacePageStateHidden)
        {
          m2::PointD const pinPoint = [self.containerView.placePage pinPoint];
          CGPoint viewPinPoint = [(EAGLView *)self.view globalPoint2ViewPoint:CGPointMake(pinPoint.x, pinPoint.y)];

          if (CGRectContainsPoint(self.view.bounds, viewPinPoint))
          {
            CGFloat const minOffset = 40;
            viewPinPoint.x = MIN(self.view.width - minOffset, viewPinPoint.x);
            viewPinPoint.x = MAX(minOffset, viewPinPoint.x);
            viewPinPoint.y = MIN(self.view.height - minOffset - self.toolbarView.height, viewPinPoint.y);
            viewPinPoint.y = MAX(minOffset + self.containerView.placePage.maxY, viewPinPoint.y);

            CGPoint const center = [(EAGLView *)self.view viewPoint2GlobalPoint:viewPinPoint];
            m2::PointD const offset = [self.containerView.placePage pinPoint] - m2::PointD(center.x, center.y);
            Framework & framework = GetFramework();
            framework.SetViewportCenterAnimated(framework.GetViewportCenter() + offset);
          }
        }
        [UIView animateWithDuration:0.3 delay:0 options:UIViewAnimationOptionCurveEaseOut animations:^{
          self.toolbarView.maxY = self.view.height;
          if (GetFramework().IsRoutingActive())
          {
            self.routeView.alpha = 1;
          }
        } completion:^(BOOL finished) {}];

        break;
      }
    case PlacePageStateOpened:
      {
        [UIView animateWithDuration:0.3 animations:^{
          self.toolbarView.minY = self.view.height;
          self.routeView.alpha = 0;
        }];
      }
      break;
    }
  }
  else if (object == self.searchView && [keyPath isEqualToString:@"state"])
  {
    if ([self respondsToSelector:@selector(setNeedsStatusBarAppearanceUpdate)])
      [self setNeedsStatusBarAppearanceUpdate];
    if (self.searchView.state == SearchViewStateFullscreen)
    {
      GetFramework().ActivateUserMark(NULL);
      [self.containerView.placePage setState:PlacePageStateHidden animated:YES withCallback:NO];
    }
    else if (self.searchView.state == SearchViewStateResults)
    {
      [UIView animateWithDuration:0.3 animations:^{
        if (GetFramework().IsRoutingActive())
          self.routeView.minY = self.searchView.searchBar.maxY - 20;
      }];
    }
    else if (self.searchView.state == SearchViewStateHidden)
    {
      [UIView animateWithDuration:0.3 animations:^{
        if (GetFramework().IsRoutingActive())
          self.routeView.minY = 0;
      }];
    }
  }
}

#pragma mark - Public methods

- (void)setApiMode:(BOOL)apiMode animated:(BOOL)animated
{
  if (apiMode)
  {
    [self.view addSubview:self.apiBar];
    self.apiBar.maxY = 0;
    [UIView animateWithDuration:(animated ? 0.3 : 0) delay:0 options:UIViewAnimationOptionCurveEaseInOut animations:^{
      self.apiBar.minY = 0;
      self.containerView.frame = CGRectMake(0, self.apiBar.maxY, self.view.width, self.view.height - self.apiBar.maxY);
      self.routeViewWrapper.minY = self.apiBar.maxY;
    } completion:nil];

    [self.view insertSubview:self.searchView aboveSubview:self.apiBar];
    self.containerView.placePage.statusBarIncluded = NO;

    self.apiTitleLabel.text = [NSString stringWithUTF8String:GetFramework().GetApiDataHolder().GetAppTitle().c_str()];
  }
  else
  {
    [UIView animateWithDuration:(animated ? 0.3 : 0) delay:0 options:UIViewAnimationOptionCurveEaseInOut animations:^{
      self.apiBar.maxY = 0;
      self.containerView.frame = self.view.bounds;
      self.routeViewWrapper.minY = self.apiBar.maxY;
    } completion:^(BOOL finished) {
      [self.apiBar removeFromSuperview];
    }];

    [self.view insertSubview:self.searchView belowSubview:self.containerView];
    self.containerView.placePage.statusBarIncluded = YES;
  }

  [self dismissPopover];
  [self.containerView.placePage setState:self.containerView.placePage.state animated:YES withCallback:YES];
  [self.searchView setState:SearchViewStateHidden animated:YES withCallback:YES];
  [self.bottomMenu setMenuHidden:YES animated:YES];

  _apiMode = apiMode;

  if ([self respondsToSelector:@selector(setNeedsStatusBarAppearanceUpdate)])
    [self setNeedsStatusBarAppearanceUpdate];
}

- (void)cleanUserMarks
{
  Framework & framework = GetFramework();
  framework.GetBalloonManager().RemovePin();
  framework.GetBalloonManager().Dismiss();
  framework.GetBookmarkManager().UserMarksClear(UserMarkContainer::API_MARK);
  framework.Invalidate();
}

- (void)setupMeasurementSystem
{
  GetFramework().SetupMeasurementSystem();
}

#pragma mark - Private methods

NSInteger compareAddress(id l, id r, void * context)
{
	return l < r;
}

- (void)invalidate
{
  Framework & f = GetFramework();
  if (!f.SetUpdatesEnabled(true))
    f.Invalidate();
}

- (void)showAppStoreRatingMenu
{
  UIAlertView * alertView = [[UIAlertView alloc] initWithTitle:@"App Store"
                                                       message:L(@"appStore_message")
                                                      delegate:self
                                             cancelButtonTitle:L(@"no_thanks")
                                             otherButtonTitles:L(@"ok"), L(@"remind_me_later"), nil];
  alertView.tag = ALERT_VIEW_APPSTORE;
  [alertView show];
}

- (void)showFacebookRatingMenu
{
  UIAlertView * alertView = [[UIAlertView alloc] initWithTitle:@"Facebook"
                                                       message:L(@"share_on_facebook_text")
                                                      delegate:self
                                             cancelButtonTitle:L(@"no_thanks")
                                             otherButtonTitles:L(@"ok"), L(@"remind_me_later"),  nil];
  alertView.tag = ALERT_VIEW_FACEBOOK;
  [alertView show];
}

- (void)destroyPopover
{
  self.popoverVC = nil;
}

- (void)showPopover
{
  if (self.popoverVC)
    GetFramework().GetBalloonManager().Hide();

  double const sf = self.view.contentScaleFactor;

  Framework & f = GetFramework();
  m2::PointD tmp = m2::PointD(f.GtoP(m2::PointD(m_popoverPos.x, m_popoverPos.y)));

  [self.popoverVC presentPopoverFromRect:CGRectMake(tmp.x / sf, tmp.y / sf, 1, 1) inView:self.view permittedArrowDirections:UIPopoverArrowDirectionAny animated:YES];
}

- (void)dismissPopover
{
  [self.popoverVC dismissPopoverAnimated:YES];
  [self destroyPopover];
  [self invalidate];
}

@end
