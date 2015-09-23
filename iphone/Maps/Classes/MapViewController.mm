#import "AppInfo.h"
#import "Common.h"
#import "EAGLView.h"
#import "MapsAppDelegate.h"
#import "MapViewController.h"
#import "MWMAlertViewController.h"
#import "MWMMapViewControlsManager.h"
#import "MWMPlacePageViewManagerDelegate.h"
#import "MWMPlacePageViewManager.h"
#import "RouteState.h"
#import "RouteView.h"
#import "ShareActionSheet.h"
#import "UIFont+MapsMeFonts.h"
#import "UIKitCategories.h"
#import "UIViewController+Navigation.h"

#import "../../../3party/Alohalytics/src/alohalytics_objc.h"
#import "../../Common/CustomAlertView.h"

#include "Framework.h"

#include "../../../anim/controller.hpp"
#include "../Statistics/Statistics.h"

#include "../../../map/country_status_display.hpp"
#include "../../../map/dialog_settings.hpp"
#include "../../../map/user_mark.hpp"

#include "../../../platform/file_logging.hpp"
#include "../../../platform/platform.hpp"
#include "../../../platform/settings.hpp"

#define ALERT_VIEW_ROUTING_DISCLAIMER 7

extern NSString * const kAlohalyticsTapEventKey = @"$onClick";

typedef NS_ENUM(NSUInteger, ForceRoutingStateChange)
{
  ForceRoutingStateChangeNone,
  ForceRoutingStateChangeRestoreRoute,
  ForceRoutingStateChangeStartFollowing
};

typedef NS_ENUM(NSUInteger, UserTouchesAction)
{
  UserTouchesActionNone,
  UserTouchesActionDrag,
  UserTouchesActionScale
};

typedef NS_OPTIONS(NSUInteger, MapInfoView)
{
  MapInfoViewRoute  = 1 << 0,
  MapInfoViewSearch = 1 << 1
};

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
  self = [super init];
  if (self)
    m_innerValue = value;
  return self;
}

-(BOOL)isEqual:(id)anObject
{
  return [anObject isMemberOfClass:[NSValueWrapper class]];
}

@end

@interface MapViewController () <RouteViewDelegate, SearchViewDelegate, MWMPlacePageViewManagerDelegate>

@property (nonatomic) UIView * routeViewWrapper;
@property (nonatomic) RouteView * routeView;
@property (nonatomic) UIImageView * apiBar;
@property (nonatomic) UILabel * apiTitleLabel;
@property (nonatomic, readwrite) MWMMapViewControlsManager * controlsManager;

@property (nonatomic) ForceRoutingStateChange forceRoutingStateChange;
@property (nonatomic) BOOL disableStandbyOnLocationStateMode;
@property (nonatomic) BOOL disableStandbyOnRouteFollowing;

@property (nonatomic) MWMPlacePageViewManager * placePageManager;

@property (nonatomic) UserTouchesAction userTouchesAction;

@property (nonatomic) MapInfoView mapInfoView;

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
      break;
    }
    case location::ENotSupported:
    {
      UIAlertView * alert = [[CustomAlertView alloc] initWithTitle:nil
                                                           message:L(@"device_doesnot_support_location_services")
                                                          delegate:nil
                                                 cancelButtonTitle:L(@"ok")
                                                 otherButtonTitles:nil];
      [alert show];
      [[MapsAppDelegate theApp].m_locationManager stop:self];
      break;
    }
    default:
      break;
  }
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

    if (self.forceRoutingStateChange == ForceRoutingStateChangeRestoreRoute)
      [self restoreRoute];
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
      if (res.m_turn == routing::turns::TurnDirection::EnterRoundAbout)
        turnTypeValue = @(res.m_exitNum);
      else if (res.m_turn != routing::turns::TurnDirection::StayOnRoundAbout)
        turnTypeValue = nil;
      if (turnTypeValue)
        [routeInfo setObject:turnTypeValue forKey:@"turnTypeValue"];

      [self.routeView updateWithInfo:routeInfo];
    }
  }
}

- (NSString *)turnTypeToImage:(routing::turns::TurnDirection)type
{
  using namespace routing::turns;
  switch (type)
  {
    case TurnDirection::TurnSlightRight:
      return @"right-1";
    case TurnDirection::TurnRight:
      return @"right-2";
    case TurnDirection::TurnSharpRight:
      return @"right-3";

    case TurnDirection::TurnSlightLeft:
      return @"left-1";
    case TurnDirection::TurnLeft:
      return @"left-2";
    case TurnDirection::TurnSharpLeft:
      return @"left-3";

    case TurnDirection::UTurn:
      return @"turn-around";

    case TurnDirection::LeaveRoundAbout:
    case TurnDirection::StayOnRoundAbout:
    case TurnDirection::EnterRoundAbout:
      return @"circle";

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
  switch (newMode)
  {
    case location::State::UnknownPosition:
    {
      self.disableStandbyOnLocationStateMode = NO;
      [[MapsAppDelegate theApp].m_locationManager stop:self];
      break;
    }
    case location::State::PendingPosition:
      self.disableStandbyOnLocationStateMode = NO;
      [[MapsAppDelegate theApp].m_locationManager start:self];
      break;
    case location::State::NotFollow:
      self.disableStandbyOnLocationStateMode = NO;
      break;
    case location::State::Follow:
    case location::State::RotateAndFollow:
      self.disableStandbyOnLocationStateMode = YES;
      break;
  }
}

#pragma mark - Restore route

- (void)restoreRoute
{
  GetFramework().BuildRoute(self.restoreRouteDestination);
  self.forceRoutingStateChange = ForceRoutingStateChangeStartFollowing;
}

#pragma mark - Map Navigation

- (void)dismissPlacePage
{
  [self.placePageManager dismissPlacePage];
}

- (void)onUserMarkClicked:(unique_ptr<UserMarkCopy>)mark
{
  [self.placePageManager showPlacePageWithUserMark:std::move(mark)];
}

- (void)processMapClickAtPoint:(CGPoint)point longClick:(BOOL)isLongClick
{
  CGFloat const scaleFactor = self.view.contentScaleFactor;
  m2::PointD const pxClicked(point.x * scaleFactor, point.y * scaleFactor);

  Framework & f = GetFramework();
  UserMark const * userMark = f.GetUserMark(pxClicked, isLongClick);
  if (f.HasActiveUserMark() == false && self.searchView.state == SearchViewStateHidden)
  {
    if (userMark == nullptr)
      self.controlsManager.hidden = !self.controlsManager.hidden;
    else
      self.controlsManager.hidden = NO;
  }
  f.GetBalloonManager().OnShowMark(userMark);
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
  // To cancel single tap timer
  UITouch * theTouch = (UITouch *)[touches anyObject];
  if (theTouch.tapCount > 1)
    [self cancelSingleTap];

  [self updatePointsFromEvent:event];

  Framework & f = GetFramework();

  if ([event allTouches].count == 1)
  {
    //if (f.GetGuiController()->OnTapStarted(m_Pt1))
    //  return;
    self.userTouchesAction = UserTouchesActionDrag;

    // Start long-tap timer
    [self preformLongTapSelector:[NSValue valueWithCGPoint:[theTouch locationInView:self.view]]];
    // Temporary solution to filter long touch
    m_touchDownPoint = m_Pt1;
  }
  else
  {
    self.userTouchesAction = UserTouchesActionScale;
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

  ///@TODO UVR
//  if (f.GetGuiController()->OnTapMoved(m_Pt1))
//    return;

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

  NSUInteger const touchesCount = [event allTouches].count;
  switch (self.userTouchesAction)
  {
    case UserTouchesActionNone:
      if (touchesCount == 1)
        self.userTouchesAction = UserTouchesActionDrag;
      else
        self.userTouchesAction = UserTouchesActionScale;
      break;
    case UserTouchesActionDrag:
      if (touchesCount == 1)
        f.DoDrag(DragEvent(m_Pt1.x, m_Pt1.y));
      else
        self.userTouchesAction = UserTouchesActionNone;
      break;
    case UserTouchesActionScale:
      if (touchesCount == 2)
        f.DoScale(ScaleEvent(m_Pt1.x, m_Pt1.y, m_Pt2.x, m_Pt2.y));
      else
        self.userTouchesAction = UserTouchesActionNone;
      break;
  }
}

- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)event
{
  [self updatePointsFromEvent:event];
  self.userTouchesAction = UserTouchesActionNone;

  UITouch * theTouch = (UITouch *)[touches anyObject];
  NSUInteger const tapCount = theTouch.tapCount;
  NSUInteger const touchesCount = [event allTouches].count;

  Framework & f = GetFramework();

  if (touchesCount == 1)
  {
    // Cancel long-touch timer
    [self cancelLongTap];

    // TapCount could be zero if it was a single long (or moving) tap.
    if (tapCount < 2)
    {
      ///@TODO UVR
//      if (f.GetGuiController()->OnTapEnded(m_Pt1))
//        return;
    }

    if (tapCount == 1)
    {
      // Launch single tap timer
      if (m_isSticking)
        [self performSingleTapSelector: [NSValue valueWithCGPoint:[theTouch locationInView:self.view]]];
    }
    else if (tapCount == 2 && m_isSticking)
    {
      f.ScaleToPoint(ScaleToPointEvent(m_Pt1.x, m_Pt1.y, 2.0));
    }
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
  self.userTouchesAction = UserTouchesActionNone;
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

- (void)willRotateToInterfaceOrientation:(UIInterfaceOrientation)toInterfaceOrientation duration:(NSTimeInterval)duration
{
  [self.placePageManager layoutPlacePageToOrientation:toInterfaceOrientation];
}

- (void)didRotateFromInterfaceOrientation:(UIInterfaceOrientation)fromInterfaceOrientation
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
  [(EAGLView *)self.view deallocateNative];
}

- (void)onEnterBackground
{
  // Save state and notify about entering background.

  Framework & f = GetFramework();
  f.SaveState();
  f.SetUpdatesEnabled(false);
  f.EnterBackground();
}

- (void)setMapStyle:(MapStyle)mapStyle
{
  EAGLView * v = (EAGLView *)self.view;
  [v setMapStyle: mapStyle];
}

- (void)onEnterForeground
{
  // Notify about entering foreground (should be called on the first launch too).
  GetFramework().EnterForeground();

  if (self.isViewLoaded && self.view.window)
    [self invalidate]; // only invalidate when map is displayed on the screen
}

- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
  [[NSNotificationCenter defaultCenter] removeObserver:self name:UIDeviceOrientationDidChangeNotification object:nil];
  [self invalidate];
}

- (void)viewDidLoad
{
  [super viewDidLoad];
  self.view.clipsToBounds = YES;
  [self.view addSubview:self.routeViewWrapper];
  self.placePageManager = [[MWMPlacePageViewManager alloc] initWithViewController:self];
  self.controlsManager = [[MWMMapViewControlsManager alloc] initWithParentController:self];
  [self.view addSubview:self.searchView];
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
  [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(orientationChanged:) name:UIDeviceOrientationDidChangeNotification object:nil];

  [super viewWillDisappear:animated];
}

- (void)orientationChanged:(NSNotification *)notification
{
  [self willRotateToInterfaceOrientation:self.interfaceOrientation duration:0.];
}

- (UIStatusBarStyle)preferredStatusBarStyle
{
  if (self.apiMode)
  {
    return UIStatusBarStyleLightContent;
  }
  else
  {
    if (self.searchView.state != SearchViewStateHidden || self.controlsManager.menuState == MWMSideMenuStateActive || self.placePageManager.isDirectionViewShown || (GetFramework().GetMapStyle() == MapStyleDark && self.routeView.state == RouteViewStateHidden))
      return UIStatusBarStyleLightContent;
    return UIStatusBarStyleDefault;
  }
}

- (void)updateStatusBarStyle
{
  [self setNeedsStatusBarAppearanceUpdate];
}

- (id)initWithCoder:(NSCoder *)coder
{
  NSLog(@"MapViewController initWithCoder Started");

  if ((self = [super initWithCoder:coder]))
  {
    Framework & f = GetFramework();
    if ([[NSUserDefaults standardUserDefaults] boolForKey:FIRST_LAUNCH_KEY])
      Settings::Set("ZoomButtonsEnabled", true);

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

    ///@TODO UVR
//    f.GetLocationState()->AddStateModeListener(bind(locationStateModeFn, self, locationStateModeSelector, _1));
    m_predictor = [[LocationPredictor alloc] initWithObserver:self];

    m_StickyThreshold = 10;

    EAGLView * v = (EAGLView *)self.view;
    [v initRenderPolicy];

    self.forceRoutingStateChange = ForceRoutingStateChangeNone;
    self.userTouchesAction = UserTouchesActionNone;

    // restore previous screen position
    if (!f.LoadState())
      f.SetMaxWorldRect();

    ///@TODO UVR
    //f.Invalidate();
    f.LoadBookmarks();

    ///@TODO UVR
    /*f.GetCountryStatusDisplay()->SetDownloadCountryListener([](storage::TIndex const & idx, int opt)
    {
      ActiveMapsLayout & layout = GetFramework().GetCountryTree().GetActiveMapLayout();
      if (opt == -1)
      {
        layout.RetryDownloading(idx);
      }
      else
      {
        LocalAndRemoteSizeT sizes = layout.GetRemoteCountrySizes(idx);
        uint64_t sizeToDownload = sizes.first;
        TMapOptions options = static_cast<TMapOptions>(opt);
        if(HasOptions(options, TMapOptions::ECarRouting))
          sizeToDownload += sizes.second;

        NSString * name = [NSString stringWithUTF8String:layout.GetCountryName(idx).c_str()];
        Platform::EConnectionType const connection = Platform::ConnectionStatus();
        if (connection != Platform::EConnectionType::CONNECTION_NONE)
        {
          if (connection == Platform::EConnectionType::CONNECTION_WWAN && sizeToDownload > 50 * 1024 * 1024)
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
    });*/

    f.SetRouteBuildingListener([self, &f](routing::IRouter::ResultCode code, vector<storage::TIndex> const & absentCountries, vector<storage::TIndex> const & absentRoutes)
    {
      [self.placePageManager stopBuildingRoute];
      switch (code)
      {
        case routing::IRouter::ResultCode::NoError:
        {
          f.GetBalloonManager().RemovePin();
          f.GetBalloonManager().Dismiss();
          [self.searchView setState:SearchViewStateHidden animated:YES];
          [self performAfterDelay:0.3 block:^
           {
             if (self.forceRoutingStateChange == ForceRoutingStateChangeStartFollowing)
               [self routeViewDidStartFollowing:self.routeView];
             else
               [self.routeView setState:RouteViewStateInfo animated:YES];
             [self updateRoutingInfo];
           }];

          bool isDisclaimerApproved = false;
          (void)Settings::Get("IsDisclaimerApproved", isDisclaimerApproved);
          if (!isDisclaimerApproved)
          {
            UIAlertView * alert = [[UIAlertView alloc] initWithTitle:L(@"routing_disclaimer") message:@"" delegate:self cancelButtonTitle:L(@"cancel") otherButtonTitles:L(@"ok"), nil];
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

- (void)presentDownloaderAlert:(routing::IRouter::ResultCode)type countries:(vector<storage::TIndex> const&)countries {
  if (countries.size())
  {
    MWMAlertViewController *alert = [[MWMAlertViewController alloc] initWithViewController:self];
    [alert presentDownloaderAlertWithCountryIndex:countries[0]];
  }
  else
  {
    [self presentDefaultAlert:type];
  }
}

- (void)presentDefaultAlert:(routing::IRouter::ResultCode)type
{
  MWMAlertViewController *alert = [[MWMAlertViewController alloc] initWithViewController:self];
  [alert presentAlert:type];
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
    CGFloat const routeInfoView = 68.0;
    _routeView = [[RouteView alloc] initWithFrame:CGRectMake(0.0, 0.0, self.view.width, routeInfoView)];
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
    _searchView.delegate = self;
    _searchView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  }
  return _searchView;
}

- (UIImageView *)apiBar
{
  if (!_apiBar)
  {
    UIImage * image = [UIImage imageNamed:@"ApiBarBackground7"];
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
    clearButton.titleLabel.font = [UIFont light17];
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
    _apiTitleLabel.font = [UIFont light17];
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
  NSURL * url = [NSURL URLWithString:[NSString stringWithUTF8String:GetFramework().GetApiDataHolder().m_globalBackUrl.c_str()]];
  [[UIApplication sharedApplication] openURL:url];
}

#pragma mark - Routing

- (void)dismissRouting
{
  GetFramework().CloseRouting();
  [self.routeView setState:RouteViewStateHidden animated:YES];
  self.disableStandbyOnRouteFollowing = NO;
  [RouteState remove];
}

#pragma mark - RouteViewDelegate

- (void)routeViewDidStartFollowing:(RouteView *)routeView
{
  self.forceRoutingStateChange = ForceRoutingStateChangeNone;
  [routeView setState:RouteViewStateTurnInstructions animated:YES];
  self.controlsManager.zoomHidden = NO;
  GetFramework().FollowRoute();
  self.disableStandbyOnRouteFollowing = YES;
  [RouteState save];
}

- (void)routeViewDidCancelRouting:(RouteView *)routeView
{
  [self dismissRouting];
}

- (void)routeViewWillEnterState:(RouteViewState)state
{
  if (state == RouteViewStateHidden)
    [self clearMapInfoViewFlag:MapInfoViewRoute];
  else
    [self setMapInfoViewFlag:MapInfoViewRoute];
}

- (void)routeViewDidEnterState:(RouteViewState)state
{
  [self updateStatusBarStyle];
}

#pragma mark - SearchViewDelegate

- (void)searchViewWillEnterState:(SearchViewState)state
{
  switch (state)
  {
    case SearchViewStateHidden:
      self.controlsManager.hidden = NO;
      [self moveRouteViewAnimatedtoOffset:0.0];
      break;
    case SearchViewStateResults:
      self.controlsManager.hidden = NO;
      [self moveRouteViewAnimatedtoOffset:self.searchView.searchBar.maxY];
      break;
    case SearchViewStateAlpha:
      self.controlsManager.hidden = NO;
      break;
    case SearchViewStateFullscreen:
      self.controlsManager.hidden = YES;
      GetFramework().ActivateUserMark(NULL);
      break;
  }
}

- (void)searchViewDidEnterState:(SearchViewState)state
{
  switch (state)
  {
    case SearchViewStateResults:
      [self setMapInfoViewFlag:MapInfoViewSearch];
      break;
    case SearchViewStateHidden:
    case SearchViewStateAlpha:
    case SearchViewStateFullscreen:
      [self clearMapInfoViewFlag:MapInfoViewSearch];
      break;
  }
  [self updateStatusBarStyle];
}

#pragma mark - Layout

- (void)moveRouteViewAnimatedtoOffset:(CGFloat)offset
{
  if (!GetFramework().IsRoutingActive())
    return;
  [UIView animateWithDuration:0.3 delay:0.0 damping:0.9 initialVelocity:0.0 options:UIViewAnimationOptionCurveEaseInOut animations:^
  {
    self.routeViewWrapper.minY = MAX(offset - [self.searchView defaultSearchBarMinY], 0.0);
  }
  completion:nil];
}

#pragma mark - UIKitViews delegates

- (void)alertView:(UIAlertView *)alertView clickedButtonAtIndex:(NSInteger)buttonIndex
{
  switch (alertView.tag)
  {
    case ALERT_VIEW_ROUTING_DISCLAIMER:
      if (buttonIndex == alertView.cancelButtonIndex)
        [self dismissRouting];
      else
        Settings::Set("IsDisclaimerApproved", true);
      break;
    default:
      break;
  }
}

#pragma mark - MWMPlacePageViewManagerDelegate

- (void)dragPlacePage:(CGPoint)point
{
  [self.controlsManager setBottomBound:point.y];
}

- (void)addPlacePageViews:(NSArray *)views
{
  [views enumerateObjectsUsingBlock:^(UIView * view, NSUInteger idx, BOOL *stop)
  {
    if ([self.view.subviews containsObject:view])
      return;
    [self.view insertSubview:view belowSubview:self.searchView];
  }];
}

#pragma mark - Public methods

- (void)setApiMode:(BOOL)apiMode animated:(BOOL)animated
{
  if (apiMode)
  {
    [self.view addSubview:self.apiBar];
    self.apiBar.maxY = 0;
    [UIView animateWithDuration:(animated ? 0.3 : 0) delay:0 options:UIViewAnimationOptionCurveEaseInOut animations:^
    {
      self.apiBar.minY = 0;
      self.routeViewWrapper.minY = self.apiBar.maxY;
    }
    completion:nil];

    [self.view insertSubview:self.searchView aboveSubview:self.apiBar];

    self.apiTitleLabel.text = [NSString stringWithUTF8String:GetFramework().GetApiDataHolder().m_appTitle.c_str()];
  }
  else
  {
    [UIView animateWithDuration:(animated ? 0.3 : 0) delay:0 options:UIViewAnimationOptionCurveEaseInOut animations:^
    {
      self.apiBar.maxY = 0;
      self.routeViewWrapper.minY = self.apiBar.maxY;
    }
    completion:^(BOOL finished)
    {
      [self.apiBar removeFromSuperview];
    }];
  }

  [self dismissPopover];
  [self.searchView setState:SearchViewStateHidden animated:YES];

  _apiMode = apiMode;

  [self updateStatusBarStyle];
}

- (void)cleanUserMarks
{
  Framework & framework = GetFramework();
  framework.GetBalloonManager().RemovePin();
  framework.GetBalloonManager().Dismiss();
  UserMarkControllerGuard guard(framework.GetBookmarkManager(), UserMarkType::API_MARK);
  guard.m_controller.Clear();
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
  ///@TODO UVR
//  Framework & f = GetFramework();
//  if (!f.SetUpdatesEnabled(true))
//    f.Invalidate();
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

- (void)updateInfoViews
{
  CGFloat topBound = 0.0;
  if ([self testMapInfoViewFlag:MapInfoViewRoute])
  {
    CGRect const routeRect = self.routeViewWrapper.frame;
    topBound = MAX(topBound, routeRect.origin.y + routeRect.size.height);
  }
  if ([self testMapInfoViewFlag:MapInfoViewSearch])
  {
    CGRect const searchRect = self.searchView.infoRect;
    topBound = MAX(topBound, searchRect.origin.y + searchRect.size.height);
  }
  [self.controlsManager setTopBound:topBound];
  [self.placePageManager setTopBound:topBound];
}

#pragma mark - Properties

- (BOOL)testMapInfoViewFlag:(MapInfoView)flag
{
  return (self.mapInfoView & flag) == flag;
}

- (void)setMapInfoViewFlag:(MapInfoView)flag
{
  if (![self testMapInfoViewFlag:flag])
    self.mapInfoView |= flag;
}

- (void)clearMapInfoViewFlag:(MapInfoView)flag
{
  if ([self testMapInfoViewFlag:flag])
    self.mapInfoView &= ~flag;
}

- (void)setMapInfoView:(MapInfoView)mapInfoView
{
  _mapInfoView = mapInfoView;
  [self updateInfoViews];
}

- (void)setRestoreRouteDestination:(m2::PointD)restoreRouteDestination
{
  _restoreRouteDestination = restoreRouteDestination;
  self.forceRoutingStateChange = ForceRoutingStateChangeRestoreRoute;
}

- (void)setDisableStandbyOnLocationStateMode:(BOOL)disableStandbyOnLocationStateMode
{
  if (_disableStandbyOnLocationStateMode == disableStandbyOnLocationStateMode)
    return;
  _disableStandbyOnLocationStateMode = disableStandbyOnLocationStateMode;
  if (disableStandbyOnLocationStateMode)
    [[MapsAppDelegate theApp] disableStandby];
  else
    [[MapsAppDelegate theApp] enableStandby];
}

- (void)setDisableStandbyOnRouteFollowing:(BOOL)disableStandbyOnRouteFollowing
{
  if (_disableStandbyOnRouteFollowing == disableStandbyOnRouteFollowing)
    return;
  _disableStandbyOnRouteFollowing = disableStandbyOnRouteFollowing;
  if (disableStandbyOnRouteFollowing)
    [[MapsAppDelegate theApp] disableStandby];
  else
    [[MapsAppDelegate theApp] enableStandby];
}

- (void)setUserTouchesAction:(UserTouchesAction)userTouchesAction
{
  if (_userTouchesAction == userTouchesAction)
    return;
  Framework & f = GetFramework();
  switch (userTouchesAction)
  {
    case UserTouchesActionNone:
      if (_userTouchesAction == UserTouchesActionDrag)
        f.StopDrag(DragEvent(m_Pt1.x, m_Pt1.y));
      else if (_userTouchesAction == UserTouchesActionScale)
        f.StopScale(ScaleEvent(m_Pt1.x, m_Pt1.y, m_Pt2.x, m_Pt2.y));
      break;
    case UserTouchesActionDrag:
      f.StartDrag(DragEvent(m_Pt1.x, m_Pt1.y));
      break;
    case UserTouchesActionScale:
      f.StartScale(ScaleEvent(m_Pt1.x, m_Pt1.y, m_Pt2.x, m_Pt2.y));
      break;
  }
  _userTouchesAction = userTouchesAction;
}

@end
