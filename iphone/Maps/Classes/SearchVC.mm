#import "SearchVC.h"
#import "CompassView.h"
#import "LocationManager.h"
#import "SearchCell.h"
#import "BookmarksVC.h"
#import "CustomNavigationView.h"
#import "MapsAppDelegate.h"
#import "MapViewController.h"

#include "Framework.h"

#include "../../search/result.hpp"
#include "../../search/params.hpp"

#include "../../indexer/mercator.hpp"

#include "../../platform/platform.hpp"
#include "../../platform/preferred_languages.hpp"

#include "../../geometry/angles.hpp"
#include "../../geometry/distance_on_sphere.hpp"

/// When to display compass instead of country flags
#define MIN_COMPASS_DISTANCE 25000.0


SearchVC * g_searchVC = nil;

@interface ResultsWrapper : NSObject
{
  search::Results m_results;
}

// Stores search string which corresponds to these results.
@property(nonatomic, retain) NSString * searchString;

- (id)initWithResults:(search::Results const &)res;

- (int)getCount;
- (search::Result const &)getWithPosition:(int)pos;

- (BOOL)isEndMarker;
- (BOOL)isEndedNormal;
@end

@implementation ResultsWrapper

@synthesize searchString;

- (void)dealloc
{
  [searchString release];
  [super dealloc];
}

- (id)initWithResults:(search::Results const &)res
{
  if ((self = [super init]))
    m_results = res;
  return self;
}

- (int)getCount
{
  return static_cast<int>(m_results.GetCount());
}

- (search::Result const &)getWithPosition:(int)pos
{
  return m_results.GetResult(pos);
}

- (BOOL)isEndMarker
{
  return m_results.IsEndMarker();
}

- (BOOL)isEndedNormal
{
  return m_results.IsEndedNormal();
}

@end


// Last search results are stored between SearchVC sessions
// to appear instantly for the user, they also store last search text query.
//ResultsWrapper * g_lastSearchResults = nil;


NSString *lastSearchRequest = nil;
int scopeSection = 2;
int numberOfRowsInEmptySearch = 0;

static void OnSearchResultCallback(search::Results const & res)
{
  if (g_searchVC)
  {
    ResultsWrapper * w = [[ResultsWrapper alloc] initWithResults:res];
    [g_searchVC performSelectorOnMainThread:@selector(addResult:)
                                 withObject:w waitUntilDone:NO];
    [w release];
  }
}

/////////////////////////////////////////////////////////////////////

@implementation SearchVC
@synthesize searchResults = _searchResults;

- (id) init
{
  if ((self = [super initWithNibName:nil bundle:nil]))
  {
    m_framework = &GetFramework();
    m_locationManager = [MapsAppDelegate theApp].m_locationManager;
    
    double lat, lon;
    bool const hasPt = [m_locationManager getLat:lat Lon:lon];
    m_framework->PrepareSearch(hasPt, lat, lon);
      
    //mycode init array of categories
    categoriesNames = [[NSArray alloc] initWithObjects:
                       @"food",
                       @"shop",
                       @"hotel",
                       @"tourism",
                       @"entertainment",
                       @"atm",
                       @"bank",
                       @"transport",
                       @"fuel",
                       @"parking",
                       @"pharmacy",
                       @"hospital",
                       @"toilet",
                       @"post",
                       @"police",
                       nil];
      _searchResults = [[NSMutableArray alloc] initWithObjects:[[[ResultsWrapper alloc] init]autorelease],[[[ResultsWrapper alloc] init]autorelease], [[[ResultsWrapper alloc] init]autorelease], nil];
      if (!lastSearchRequest)
      {
          lastSearchRequest = @"";
      }
  }
  return self;
}

- (void)hideIndicator
{
  [m_indicator stopAnimating];
  m_searchTextField.leftView = m_originalIndicatorView;
}

- (void)showIndicator
{
  m_searchTextField.leftView = m_indicator;
  [m_indicator startAnimating];
}

- (void)fillSearchParams:(search::SearchParams &)params withText:(NSString *)queryString
{
  params.m_query = [[queryString precomposedStringWithCompatibilityMapping] UTF8String];
  params.m_callback = bind(&OnSearchResultCallback, _1);

  // Set current keyboard input mode
  // Note: input mode was introduced in iOS 4.2 (now we support 4.3+)
  params.SetInputLanguage([[UITextInputMode currentInputMode].primaryLanguage UTF8String]);

  double lat, lon;
  if ([m_locationManager getLat:lat Lon:lon])
    params.SetPosition(lat, lon);
}

- (void)loadView
{    
    m_searchBar = [[UISearchBar alloc] init];
    [m_searchBar sizeToFit];
    m_searchBar.showsScopeBar = YES;
    m_searchBar.showsCancelButton = YES;
    // restore previous search query
    
    if (lastSearchRequest)
      [m_searchBar setText:lastSearchRequest];
    m_searchBar.delegate = self;
    m_searchBar.placeholder = NSLocalizedString(@"search_map", @"Search box placeholder text");
    
    // Add search in progress indicator
    for(UIView * v in m_searchBar.subviews)
    {
        if ([v isKindOfClass:[UITextField class]])
        {
            // Save textField to show/hide activity indicator in it
            m_searchTextField = (UITextField *)v;
            m_originalIndicatorView = [m_searchTextField.leftView retain];
            m_indicator = [[UIActivityIndicatorView alloc] initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleGray];
            m_indicator.bounds = m_originalIndicatorView.bounds;
            break;
        }
    }

    
    m_table = [[UITableView alloc] init];
    m_table.autoresizingMask = UIViewAutoresizingFlexibleWidth|UIViewAutoresizingFlexibleHeight;
    m_table.delegate = self;
    m_table.dataSource = self;
    
    [m_searchBar setScopeButtonTitles:[NSArray arrayWithObjects:
                                              NSLocalizedString(@"search_mode_nearme", nil),
                                              NSLocalizedString(@"search_mode_viewport", nil),
                                              NSLocalizedString(@"search_mode_all", nil), nil]];
    [self setSearchBarHeight];
    [m_table setTableHeaderView:m_searchBar];
    self.view = m_table;
}

- (void)dealloc
{
  g_searchVC = nil;
  [m_searchBar release];
  [m_table release];
  [categoriesNames release];
  [_searchResults release];
  [super dealloc];
}

- (void)viewDidLoad
{
  g_searchVC = self;
}

- (void)viewDidUnload
{
  g_searchVC = nil;
  // to correctly free memory
  [m_indicator release];
  m_indicator = nil;
  [m_originalIndicatorView release];
  m_originalIndicatorView = nil;
  [m_searchBar release];
  m_searchBar = nil;
  [m_table release];
  m_table = nil;
  [categoriesNames release];
  categoriesNames = nil;
  [_searchResults release];
  _searchResults = nil;
  
  [super viewDidUnload];
}

// Banner dialog handler
- (void)alertView:(UIAlertView *)alertView didDismissWithButtonIndex:(NSInteger)buttonIndex
{
  if (buttonIndex != alertView.cancelButtonIndex)
  {
    // Launch appstore
    [[UIApplication sharedApplication] openURL:[NSURL URLWithString:MAPSWITHME_PREMIUM_APPSTORE_URL]];
  }
  // Close view
  [self dismissModalViewControllerAnimated:YES];
}

- (void)viewWillAppear:(BOOL)animated
{
  // Disable search for free version
  if (!GetPlatform().IsPro())
  {
    // Display banner for paid version
    UIAlertView * alert = [[UIAlertView alloc] initWithTitle:NSLocalizedString(@"search_available_in_pro_version", nil)
                                    message:nil
                                    delegate:self
                                    cancelButtonTitle:NSLocalizedString(@"cancel", nil)
                                    otherButtonTitles:NSLocalizedString(@"get_it_now", nil), nil];

    [alert show];
    [alert release];
  }
  else
  {
    [m_locationManager start:self];
    // show keyboard
    [m_searchBar becomeFirstResponder];
     m_searchBar.selectedScopeButtonIndex = scopeSection;
    [self proceedSearchWithString:lastSearchRequest andForceSearch:YES];
  }
  
  [super viewWillAppear:animated];
}

- (void)viewWillDisappear:(BOOL)animated
{
  [m_locationManager stop:self];
  
  // hide keyboard immediately
  [m_searchBar resignFirstResponder];
  [super viewWillDisappear:animated];
  numberOfRowsInEmptySearch = 0;
}

- (void) didRotateFromInterfaceOrientation: (UIInterfaceOrientation) fromInterfaceOrientation
{
  [m_locationManager setOrientation:self.interfaceOrientation];
}

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
  return YES;  // All orientations are supported.
}

//**************************************************************************
//*********** SearchBar handlers *******************************************
- (void)searchBar:(UISearchBar *)sender textDidChange:(NSString *)searchText
{
    numberOfRowsInEmptySearch = 0;
    [lastSearchRequest release];
    lastSearchRequest = [[NSString alloc] initWithString:m_searchBar.text];
    [self clearCacheResults];
    [self proceedSearchWithString:m_searchBar.text andForceSearch:NO];
}

- (void)onCloseButton:(id)sender
{
  [self dismissModalViewControllerAnimated:YES];
}
//*********** End of SearchBar handlers *************************************
//***************************************************************************

- (void)setSearchBoxText:(NSString *)text
{
  m_searchBar.text = text;
  // Manually send text change notification if control has no focus,
  // OR if iOS 6 - it doesn't send textDidChange notification after text property update
  if (![m_searchBar isFirstResponder]
      || [UIDevice currentDevice].systemVersion.floatValue > 5.999)
    [self searchBar:m_searchBar textDidChange:text];
}

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView
{
  return 1;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
  m_suggestionsCount = m_searchBar.text.length ? 0 : 1;
  //No text in search show categories
  if (m_suggestionsCount)
  {
      return [categoriesNames count];
  }
  //If no results we should show 0 strings if search is in progress or 1 string with message thaht there is no results
  if (![[_searchResults objectAtIndex:scopeSection] getCount])
  {
    return numberOfRowsInEmptySearch;
  }
  return [[_searchResults objectAtIndex:scopeSection] getCount];
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
  NSInteger realRowIndex = indexPath.row;
  if (m_suggestionsCount)
  {
    static NSString *CellIdentifier = @"categoryCell";

    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:CellIdentifier];
    if (cell == nil)
    {
        cell = [[[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:CellIdentifier] autorelease];
    }

    cell.textLabel.text =  NSLocalizedString([categoriesNames objectAtIndex:indexPath.row], nil);
    cell.imageView.image = [UIImage imageNamed:[categoriesNames objectAtIndex:indexPath.row]];

    return cell;
  }
  //No search results
  if ([m_searchBar.text length] != 0 && ![[_searchResults objectAtIndex:scopeSection] getCount])
  {      
      UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:@"NoResultsCell"];
      if (cell == nil)
      {
          cell = [[[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:@"NoResultsCell"] autorelease];
      }
      cell.textLabel.text =  NSLocalizedString(@"no_search_results_found", nil);
      return cell;
  }

  if ([_searchResults objectAtIndex:scopeSection] == nil || realRowIndex >= (NSInteger)[[_searchResults objectAtIndex:scopeSection] getCount])
  {
    ASSERT(false, ("Invalid m_results with size", [[_searchResults objectAtIndex:scopeSection] getCount]));
    return nil;
  }

  search::Result const & r = [[_searchResults objectAtIndex:scopeSection] getWithPosition:realRowIndex];
  switch (r.GetResultType())
  {
    case search::Result::RESULT_FEATURE:
    {
      SearchCell * cell = (SearchCell *)[tableView dequeueReusableCellWithIdentifier:@"FeatureCell"];
      if (!cell)
        cell = [[[SearchCell alloc] initWithReuseIdentifier:@"FeatureCell"] autorelease];

      // Init common parameters
      cell.featureName.text = [NSString stringWithUTF8String:r.GetString()];
      cell.featureCountry.text = [NSString stringWithUTF8String:r.GetRegionString()];
      cell.featureType.text = [NSString stringWithUTF8String:r.GetFeatureType()];

      // Get current position and compass "north" direction
      double azimut = -1.0;
      double lat, lon;

      if ([m_locationManager getLat:lat Lon:lon])
      {
        double north = -1.0;
        [m_locationManager getNorthRad:north];

        string distance;
        if (!m_framework->GetDistanceAndAzimut(r.GetFeatureCenter(),
                                               lat, lon, north, distance, azimut))
        {
          // do not draw arrow for far away features
          azimut = -1.0;
        }

        cell.featureDistance.text = [NSString stringWithUTF8String:distance.c_str()];
      }
      else
        cell.featureDistance.text = nil;

      // Show flags only if it has one and no azimut to feature
      char const * flag = r.GetRegionFlag();
      if (flag && azimut < 0.0)
      {
        UIImage * flagImage = [UIImage imageNamed:[NSString stringWithFormat:@"%s.png", flag]];
        UIImageView * imgView = [[[UIImageView alloc] initWithImage:flagImage] autorelease];
        cell.accessoryView = imgView;
      }
      else
      {
        // Try to reuse existing compass view
        CompassView * compass;
        if ([cell.accessoryView isKindOfClass:[CompassView class]])
          compass = (CompassView *)cell.accessoryView;
        else
        {
          // Create compass view
          float const h = (int)(m_table.rowHeight * 0.6);
          compass = [[CompassView alloc] initWithFrame:CGRectMake(0, 0, h, h)];
          cell.accessoryView = compass;
          [compass release];
        }

        // Show arrow for valid azimut and if feature is not a continent (flag is exist)
        compass.showArrow = (azimut >= 0.0 && flag) ? YES : NO;
        compass.angle = azimut;
      }
      return cell;
    }
    break;

    case search::Result::RESULT_SUGGESTION:
    {
      UITableViewCell * cell = [tableView dequeueReusableCellWithIdentifier:@"SuggestionCell"];
      if (!cell)
        cell = [[[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:@"SuggestionCell"] autorelease];
      cell.textLabel.text = [NSString stringWithUTF8String:r.GetString()];
      return cell;
    }
    break;

    default:
      ASSERT(false, ("Unsupported search result type"));
    return nil;
  }
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
  NSInteger realRowIndex = indexPath.row;
  // Suggestion cell was clicked
  if (m_suggestionsCount)
  {
    [self setSearchBoxText:[NSLocalizedString([categoriesNames objectAtIndex:realRowIndex], Search Suggestion) stringByAppendingString:@" "]];
    [m_table scrollRectToVisible:CGRectMake(0, 0, 1, 1) animated:YES];
    return;
  }
  //row that says that there is no results
  if ([m_searchBar.text length] != 0 && ![[_searchResults objectAtIndex:scopeSection] getCount])
  {
      [m_table deselectRowAtIndexPath:indexPath animated:YES];
      return;
  }

  if (realRowIndex < (NSInteger)[[_searchResults objectAtIndex:scopeSection] getCount])
  {
    search::Result const & res = [[_searchResults objectAtIndex:scopeSection] getWithPosition:realRowIndex];
    switch(res.GetResultType())
    {
      // Zoom to the feature
    case search::Result::RESULT_FEATURE:
      {
        m_framework->ShowSearchResult(res);

        Framework::AddressInfo info;
        info.MakeFrom(res);

        [[MapsAppDelegate theApp].m_mapViewController showSearchResultAsBookmarkAtMercatorPoint:res.GetFeatureCenter() withInfo:info];

        [self onCloseButton:nil];
      }
      break;

    case search::Result::RESULT_SUGGESTION:
      [self setSearchBoxText:[NSString stringWithUTF8String:res.GetSuggestionString()]];
      // Remove blue selection from the row
      [tableView deselectRowAtIndexPath: indexPath animated:YES];
      break;
    }
  }
}

// Called on UI thread from search threads
- (void)addResult:(id)res
{
  ResultsWrapper * w = (ResultsWrapper *)res;
  
  if ([w isEndMarker])
  {
    if ([w isEndedNormal])
    {
      [self hideIndicator];
       numberOfRowsInEmptySearch = 1;
      [m_table reloadData];
    }
  }
  else
  {
     numberOfRowsInEmptySearch = 0;
     [_searchResults replaceObjectAtIndex:scopeSection withObject:w];
     [m_table reloadData];
  }
}

void setSearchType(search::SearchParams& params)
{
  switch (scopeSection)
  {
    case 0:
      params.SetSearchMode(search::SearchParams::AROUND_POSITION);
      break;
    case 1:
      params.SetSearchMode(search::SearchParams::IN_VIEWPORT);
      break;
    case 2:
      params.SetSearchMode(search::SearchParams::ALL);
      break;
    default:
      params.SetSearchMode(search::SearchParams::ALL);
      break;
  }
}

//****************************************************************** 
//*********** Location manager callbacks ***************************
- (void)onLocationError:(location::TLocationError)errorCode
{
  // Handle location status changes if necessary
}

- (void)onLocationUpdate:(location::GpsInfo const &)info
{
  // Refresh search results with newer location.
  if (![m_searchBar.text length])
      return;
  search::SearchParams params;
  setSearchType(params);
  if (m_searchBar.text)
  {
    [self fillSearchParams:params withText:m_searchBar.text];
    
    //hack, fillSearch Params return invalid position
    params.SetPosition(info.m_latitude, info.m_longitude);
    
    if (m_framework->Search(params))
    {
      [self clearCacheResults];
      numberOfRowsInEmptySearch = 0;
      [self showIndicator];
    }
    else
    {
      numberOfRowsInEmptySearch = 1;
    }
  }
}

- (void)onCompassUpdate:(location::CompassInfo const &)info
{
  double lat, lon;
  if (![m_locationManager getLat:lat Lon:lon])
    return;
  //check if categories are on the screen
  if (!m_suggestionsCount && [[_searchResults objectAtIndex:scopeSection] getCount])
  {
    double const northRad = (info.m_trueHeading < 0) ? info.m_magneticHeading : info.m_trueHeading;
    NSArray * cells = m_table.visibleCells;
    for (NSUInteger i = 0; i < cells.count; ++i)
    {
      UITableViewCell * cell = (UITableViewCell *)[cells objectAtIndex:i];
      NSInteger realRowIndex = [m_table indexPathForCell:cell].row;
      search::Result const & res = [[_searchResults objectAtIndex:scopeSection] getWithPosition:realRowIndex];
      if (res.GetResultType() == search::Result::RESULT_FEATURE)
      {
        // Show compass only for cells without flags
        if ([cell.accessoryView isKindOfClass:[CompassView class]])
        {
          CompassView * compass = (CompassView *)cell.accessoryView;
          m2::PointD const center = res.GetFeatureCenter();
          compass.angle = ang::AngleTo(m2::PointD(MercatorBounds::LonToX(lon),
                                                MercatorBounds::LatToY(lat)), center) + northRad;
        }
      }
    }
  }
}
//*********** End of Location manager callbacks ********************
//****************************************************************** 

// Dismiss virtual keyboard when touching tableview
- (void)scrollViewWillBeginDragging:(UIScrollView *)scrollView
{
  [m_searchBar resignFirstResponder];
  [self enableCancelButton];
}

// Dismiss virtual keyboard when "Search" button is pressed
- (void)searchBarSearchButtonClicked:(UISearchBar *)searchBar
{
  [m_searchBar resignFirstResponder];
}

// Callback from suggestion cell, called when icon is selected
- (void)onSuggestionSelected:(NSString *)suggestion
{
  [self setSearchBoxText:[suggestion stringByAppendingString:@" "]];
}

//segmentedControl delegate
- (void)searchBar:(UISearchBar *)searchBar selectedScopeButtonIndexDidChange:(NSInteger)selectedScope
{
    scopeSection = selectedScope;
    numberOfRowsInEmptySearch = 0;
    if (![[_searchResults objectAtIndex:selectedScope] getCount])
    {
      [self proceedSearchWithString:m_searchBar.text andForceSearch:NO];
    }
    else
    {
      [m_table reloadData];
    }
}

-(void)setSearchBarHeight
{
    CGRect r = m_searchBar.frame;
    r.size.height *= 2;
    [m_searchBar setFrame:r];
}

- (void)searchBarCancelButtonClicked:(UISearchBar *)searchBar
{
    [self onCloseButton:nil];
}

-(void)clearCacheResults
{
    for (int i = 0; i < [_searchResults count];++i)
    {
        [_searchResults replaceObjectAtIndex:i withObject:[[[ResultsWrapper alloc] init]autorelease]];
    }
}

-(void)proceedSearchWithString:(NSString *)searchText andForceSearch:(BOOL)forceSearch
{
    // Clear old results immediately
    [_searchResults replaceObjectAtIndex:scopeSection withObject:[[[ResultsWrapper alloc] init]autorelease]];
    [m_table reloadData];
    if (![searchText length])
        return;
    search::SearchParams params;
    setSearchType(params);
    if(forceSearch)
    {
        params.SetForceSearch(true);
    }
    [self fillSearchParams:params withText:searchText];
    if (m_framework->Search(params))
    {
        [self showIndicator];
        numberOfRowsInEmptySearch = 0;   
    }
    else
    {
        numberOfRowsInEmptySearch = 1;
        [m_table reloadData];
    }
}

-(void)enableCancelButton
{
   for (UIView *v in m_searchBar.subviews)
   {
        if ([v isKindOfClass:[UIButton class]])
        {
            UIButton *cancelButton = (UIButton*)v;
            cancelButton.enabled = YES;
            break;
        }
    }
}
@end
