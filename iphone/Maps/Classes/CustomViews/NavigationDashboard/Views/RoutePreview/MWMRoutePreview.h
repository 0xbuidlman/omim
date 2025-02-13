#import "MWMCircularProgress.h"
#import "MWMNavigationDashboardInfoProtocol.h"
#import "MWMNavigationView.h"

#include "routing/router.hpp"

@class MWMNavigationDashboardEntity;
@class MWMNavigationDashboardManager;

@interface MWMRoutePreview : MWMNavigationView<MWMNavigationDashboardInfoProtocol>

@property(weak, nonatomic, readonly) IBOutlet UIButton * extendButton;
@property(weak, nonatomic) MWMNavigationDashboardManager * dashboardManager;

- (void)statePrepare;
- (void)statePlanning;
- (void)stateError;
- (void)stateReady;
- (void)reloadData;
- (void)selectRouter:(routing::RouterType)routerType;
- (void)router:(routing::RouterType)routerType setState:(MWMCircularProgressState)state;
- (void)router:(routing::RouterType)routerType setProgress:(CGFloat)progress;

@end
