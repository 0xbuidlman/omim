#import "AddSetVC.h"
#import "BalloonView.h"
#import "Framework.h"

#define TEXT_FIELD_TAG 666

@implementation AddSetVC

- (id) initWithBalloonView:(BalloonView *)view andRootNavigationController:(UINavigationController *)navC
{
  self = [super initWithStyle:UITableViewStyleGrouped];
  if (self)
  {
    m_balloon = view;
    m_rootNavigationController = navC;

    self.navigationItem.rightBarButtonItem = [[[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemSave target:self action:@selector(onSaveClicked)] autorelease];
    self.navigationItem.leftBarButtonItem = [[[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemCancel target:self action:@selector(onCancelClicked)] autorelease];
    self.title = NSLocalizedString(@"add_new_set", @"Add New Bookmark Set dialog title");
  }
  return self;
}

- (void)onCancelClicked
{
  [self dismissModalViewControllerAnimated:YES];
}

- (void)onSaveClicked
{
  UITextField * textField = (UITextField *)[[self.tableView cellForRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:0]] viewWithTag:TEXT_FIELD_TAG];
  NSString * text = textField.text;
  if (text.length)
  {
    m_balloon.setName = text;
    [m_balloon deleteBookmark];

    Framework &f = GetFramework();
    size_t pos = f.AddCategory([text UTF8String]);
    [m_balloon addBookmarkToCategory:pos];

    // Show Place Page dialog with new set selected
    [self onCancelClicked];
    NSArray * vcs = m_rootNavigationController.viewControllers;
    UITableViewController * addBookmarkVC = (UITableViewController *)[vcs objectAtIndex:[vcs count] - 2];
    [m_rootNavigationController popToViewController:addBookmarkVC animated:YES];
  }
}

- (void)viewDidAppear:(BOOL)animated
{
  // Set focus to editor
  UITextField * textField = (UITextField *)[self.tableView cellForRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:0]].accessoryView;  
  [textField becomeFirstResponder];

  [super viewDidAppear:animated];
}

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
  return YES;
}

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView
{
  return 1;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
  return 1;
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
  UITableViewCell * cell = [tableView dequeueReusableCellWithIdentifier:@"EditSetNameCell"];
  if (!cell)
  {
    cell = [[[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:@"EditSetNameCell"] autorelease];
    cell.textLabel.text =  @"Temporary Name";
    [cell layoutSubviews];
    CGRect rect = cell.textLabel.frame;
    rect.size.width = cell.contentView.bounds.size.width - cell.textLabel.frame.origin.x;
    rect.origin.x = cell.textLabel.frame.origin.x;
    UITextField * f = [[UITextField alloc] initWithFrame:rect];
    f.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    f.returnKeyType = UIReturnKeyDone;
    f.clearButtonMode = UITextFieldViewModeWhileEditing;
    f.autocorrectionType = UITextAutocorrectionTypeNo;
    f.contentVerticalAlignment = UIControlContentVerticalAlignmentCenter;
    f.delegate = self;
    f.textColor = cell.textLabel.textColor;
    f.placeholder = NSLocalizedString(@"bookmark_set_name", @"Add Bookmark Set dialog - hint when set name is empty");
    f.autocapitalizationType = UITextAutocapitalizationTypeWords;
    f.font = [cell.textLabel.font fontWithSize:[cell.textLabel.font pointSize]];
    f.tag = TEXT_FIELD_TAG;
    [cell.contentView addSubview:f];
    [f becomeFirstResponder];
    [f release];
    cell.selectionStyle = UITableViewCellSelectionStyleNone;
    cell.textLabel.text =  @"";
  }
  return cell;
}

- (BOOL)textFieldShouldReturn:(UITextField *)textField
{
  if (textField.text.length == 0)
    return YES;
  
  [textField resignFirstResponder];
  [self onSaveClicked];
  return NO;
}

@end
