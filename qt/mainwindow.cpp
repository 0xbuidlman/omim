#include "mainwindow.hpp"
#include "draw_widget.hpp"
#include "slider_ctrl.hpp"
#include "about.hpp"
#include "preferences_dialog.hpp"
#include "search_panel.hpp"

#include "../defines.hpp"

#include "../platform/settings.hpp"
#include "../platform/platform.hpp"

#include "../std/bind.hpp"

#include <QtGui/QAction>
#include <QtGui/QCloseEvent>
#include <QtGui/QDockWidget>
#include <QtGui/QMenu>
#include <QtGui/QMenuBar>
#include <QtGui/QToolBar>

#define IDM_ABOUT_DIALOG        1001
#define IDM_PREFERENCES_DIALOG  1002

#ifndef NO_DOWNLOADER
#include "update_dialog.hpp"
#include "classificator_tree.hpp"
#include "info_dialog.hpp"

#include "../indexer/classificator.hpp"

#include <QtCore/QFile>

#endif // NO_DOWNLOADER


namespace qt
{

MainWindow::MainWindow()
{
  m_pDrawWidget = new DrawWidget(this);
  m_locationService.reset(CreateDesktopLocationService(*this));

  CreateNavigationBar();
  CreateSearchBarAndPanel();

#ifndef NO_DOWNLOADER
  CreateClassifPanel();
//  CreateGuidePanel();
#endif // NO_DOWNLOADER

  setCentralWidget(m_pDrawWidget);

  setWindowTitle(tr("MapsWithMe"));
  setWindowIcon(QIcon(":/ui/logo.png"));

#ifndef OMIM_OS_WINDOWS
  QMenu * helpMenu = new QMenu(tr("Help"), this);
  menuBar()->addMenu(helpMenu);
  helpMenu->addAction(tr("About"), this, SLOT(OnAbout()));
  helpMenu->addAction(tr("Preferences"), this, SLOT(OnPreferences()));
#else
  {
    // create items in the system menu
    HMENU menu = ::GetSystemMenu(winId(), FALSE);
    MENUITEMINFOA item;
    item.cbSize = sizeof(MENUITEMINFOA);
    item.fMask = MIIM_FTYPE | MIIM_ID | MIIM_STRING;
    item.fType = MFT_STRING;
    item.wID = IDM_PREFERENCES_DIALOG;
    QByteArray const prefsStr = tr("Preferences...").toLocal8Bit();
    item.dwTypeData = const_cast<char *>(prefsStr.data());
    item.cch = prefsStr.size();
    ::InsertMenuItemA(menu, ::GetMenuItemCount(menu) - 1, TRUE, &item);
    item.wID = IDM_ABOUT_DIALOG;
    QByteArray const aboutStr = tr("About MapsWithMe...").toLocal8Bit();
    item.dwTypeData = const_cast<char *>(aboutStr.data());
    item.cch = aboutStr.size();
    ::InsertMenuItemA(menu, ::GetMenuItemCount(menu) - 1, TRUE, &item);
    item.fType = MFT_SEPARATOR;
    ::InsertMenuItemA(menu, ::GetMenuItemCount(menu) - 1, TRUE, &item);
  }
#endif

  LoadState();

#ifndef NO_DOWNLOADER
  // Show intro dialog if necessary
  bool bShow = true;
  (void)Settings::Get("ShowWelcome", bShow);

  if (bShow)
  {
    bool bShowUpdateDialog = true;

    string text;
    try
    {
      ReaderPtr<Reader> reader = GetPlatform().GetReader("welcome.html");
      reader.ReadAsString(text);
    }
    catch (...)
    {}

    if (!text.empty())
    {
      InfoDialog welcomeDlg(tr("Welcome to MapsWithMe!"), text.c_str(),
                            this, QStringList(tr("Download Maps")));
      if (welcomeDlg.exec() == QDialog::Rejected)
        bShowUpdateDialog = false;
    }
    Settings::Set("ShowWelcome", false);

    if (bShowUpdateDialog)
      ShowUpdateDialog();
  }
#endif // NO_DOWNLOADER

  m_pDrawWidget->UpdateAfterSettingsChanged();
}

#if defined(Q_WS_WIN)
bool MainWindow::winEvent(MSG * msg, long * result)
{
  if (msg->message == WM_SYSCOMMAND)
  {
    switch (msg->wParam)
    {
    case IDM_PREFERENCES_DIALOG:
      OnPreferences();
      *result = 0;
      return true;
    case IDM_ABOUT_DIALOG:
      OnAbout();
      *result = 0;
      return true;
    }
  }
  return false;
}
#endif

MainWindow::~MainWindow()
{
  SaveState();
}

void MainWindow::SaveState()
{
  pair<int, int> xAndY(x(), y());
  pair<int, int> widthAndHeight(width(), height());
  Settings::Set("MainWindowXY", xAndY);
  Settings::Set("MainWindowSize", widthAndHeight);

  m_pDrawWidget->SaveState();
}

void MainWindow::LoadState()
{
  pair<int, int> xAndY;
  pair<int, int> widthAndHeight;
  bool loaded = Settings::Get("MainWindowXY", xAndY) &&
                Settings::Get("MainWindowSize", widthAndHeight);
  if (loaded)
  {
    move(xAndY.first, xAndY.second);
    resize(widthAndHeight.first, widthAndHeight.second);

    loaded = m_pDrawWidget->LoadState();
  }

  if (!loaded)
  {
    showMaximized();
    m_pDrawWidget->ShowAll();
  }
  else
    m_pDrawWidget->UpdateNow();
}

namespace
{
  struct button_t
  {
    QString name;
    char const * icon;
    char const * slot;
  };

  void add_buttons(QToolBar * pBar, button_t buttons[], size_t count, QWidget * pReceiver)
  {
    for (size_t i = 0; i < count; ++i)
    {
      if (buttons[i].icon)
        pBar->addAction(QIcon(buttons[i].icon), buttons[i].name, pReceiver, buttons[i].slot);
      else
        pBar->addSeparator();
    }
  }

  struct hotkey_t
  {
    int key;
    char const * slot;
  };
}

void MainWindow::CreateNavigationBar()
{
  QToolBar * pToolBar = new QToolBar(tr("Navigation Bar"), this);
  pToolBar->setOrientation(Qt::Vertical);
  pToolBar->setIconSize(QSize(32, 32));

  {
    // add navigation hot keys
    hotkey_t arr[] = {
      { Qt::Key_Left, SLOT(MoveLeft()) },
      { Qt::Key_Right, SLOT(MoveRight()) },
      { Qt::Key_Up, SLOT(MoveUp()) },
      { Qt::Key_Down, SLOT(MoveDown()) },
      { Qt::Key_Equal, SLOT(ScalePlus()) },
      { Qt::Key_Minus, SLOT(ScaleMinus()) },
      { Qt::ALT + Qt::Key_Equal, SLOT(ScalePlusLight()) },
      { Qt::ALT + Qt::Key_Minus, SLOT(ScaleMinusLight()) },
      { Qt::Key_A, SLOT(ShowAll()) },
      { Qt::Key_S, SLOT(QueryMaxScaleMode()) }
    };

    for (size_t i = 0; i < ARRAY_SIZE(arr); ++i)
    {
      QAction * pAct = new QAction(this);
      pAct->setShortcut(QKeySequence(arr[i].key));
      connect(pAct, SIGNAL(triggered()), m_pDrawWidget, arr[i].slot);
      addAction(pAct);
    }
  }

  {
    // add search button with "checked" behavior
    m_pSearchAction = pToolBar->addAction(QIcon(":/navig64/search.png"),
                                           tr("Search"),
                                           this,
                                           SLOT(OnSearchButtonClicked()));
    m_pSearchAction->setCheckable(true);
    m_pSearchAction->setToolTip(tr("Offline Search"));
    m_pSearchAction->setShortcut(QKeySequence::Find);

    pToolBar->addSeparator();

    // add my position button with "checked" behavior
    m_pMyPositionAction = pToolBar->addAction(QIcon(":/navig64/location.png"),
                                           tr("My Position"),
                                           this,
                                           SLOT(OnMyPosition()));
    m_pMyPositionAction->setCheckable(true);
    m_pMyPositionAction->setToolTip(tr("My Position"));

    // add view actions 1
    button_t arr[] = {
      { QString(), 0, 0 },
      //{ tr("Show all"), ":/navig64/world.png", SLOT(ShowAll()) },
      { tr("Scale +"), ":/navig64/plus.png", SLOT(ScalePlus()) }
    };
    add_buttons(pToolBar, arr, ARRAY_SIZE(arr), m_pDrawWidget);
  }

  // add scale slider
  QScaleSlider * pScale = new QScaleSlider(Qt::Vertical, this, 20);
  pScale->SetRange(2, scales::GetUpperScale());
  pScale->setTickPosition(QSlider::TicksRight);

  pToolBar->addWidget(pScale);
  m_pDrawWidget->SetScaleControl(pScale);

  {
    // add view actions 2
    button_t arr[] = {
      { tr("Scale -"), ":/navig64/minus.png", SLOT(ScaleMinus()) }
    };
    add_buttons(pToolBar, arr, ARRAY_SIZE(arr), m_pDrawWidget);
  }

#ifndef NO_DOWNLOADER
  {
    // add mainframe actions
    button_t arr[] = {
      { QString(), 0, 0 },
      { tr("Download Maps"), ":/navig64/download.png", SLOT(ShowUpdateDialog()) }
    };
    add_buttons(pToolBar, arr, ARRAY_SIZE(arr), this);
  }
#endif // NO_DOWNLOADER

  addToolBar(Qt::RightToolBarArea, pToolBar);
}

void MainWindow::OnAbout()
{
  AboutDialog dlg(this);
  dlg.exec();
}

void MainWindow::OnLocationStatusChanged(location::TLocationStatus newStatus)
{
  switch (newStatus)
  {
  case location::EFirstEvent:
    m_pMyPositionAction->setIcon(QIcon(":/navig64/location.png"));
    m_pMyPositionAction->setToolTip(tr("My Position"));
    break;
  case location::EDisabledByUser:
  case location::ENotSupported:
    m_pMyPositionAction->setChecked(false);
    break;
  default:
    break;
  }
  m_pDrawWidget->GetFramework().OnLocationStatusChanged(newStatus);
}

void MainWindow::OnGpsUpdated(location::GpsInfo const & info)
{
  m_pDrawWidget->GetFramework().OnGpsUpdate(info);
}

void MainWindow::OnMyPosition()
{
  if (m_pMyPositionAction->isChecked())
  {
    m_pMyPositionAction->setIcon(QIcon(":/navig64/location-search.png"));
    m_pMyPositionAction->setToolTip(tr("Looking for position..."));
    m_locationService->Start();
  }
  else
  {
    m_pMyPositionAction->setIcon(QIcon(":/navig64/location.png"));
    m_pMyPositionAction->setToolTip(tr("My Position"));
    m_locationService->Stop();
  }
}

void MainWindow::OnSearchButtonClicked()
{
  if (m_pSearchAction->isChecked())
  {
    m_pDrawWidget->GetFramework().PrepareSearch(false);

    m_Docks[2]->show();
    m_Docks[2]->widget()->setFocus();
  }
  else
  {
    m_Docks[2]->hide();
  }
}

void MainWindow::OnPreferences()
{
  PreferencesDialog dlg(this);
  dlg.exec();

  m_pDrawWidget->UpdateAfterSettingsChanged();
}

#ifndef NO_DOWNLOADER
void MainWindow::ShowUpdateDialog()
{
  UpdateDialog dlg(this, m_pDrawWidget->GetFramework());
  dlg.ShowModal();
}

void MainWindow::ShowClassifPanel()
{
  m_Docks[0]->show();
}

void MainWindow::ShowGuidePanel()
{
  m_Docks[1]->show();
}

void MainWindow::CreateClassifPanel()
{
  CreatePanelImpl(0, Qt::LeftDockWidgetArea, tr("Classificator Bar"),
                  QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_C), SLOT(ShowClassifPanel()));

  ClassifTreeHolder * pCTree = new ClassifTreeHolder(m_Docks[0], m_pDrawWidget, SLOT(Repaint()));
  pCTree->SetRoot(classif().GetMutableRoot());

  m_Docks[0]->setWidget(pCTree);
}

//void MainWindow::CreateGuidePanel()
//{
//  CreatePanelImpl(1, Qt::LeftDockWidgetArea, tr("Guide Bar"),
//                  QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_G), SLOT(ShowGuidePanel()));

//  qt::GuidePageHolder * pGPage = new qt::GuidePageHolder(m_Docks[1]);

//  m_Docks[1]->setWidget(pGPage);
//}
#endif // NO_DOWNLOADER

void MainWindow::CreateSearchBarAndPanel()
{
  CreatePanelImpl(2, Qt::RightDockWidgetArea, tr("Search"),
                  QKeySequence(), 0);
  SearchPanel * panel = new SearchPanel(m_pDrawWidget, m_Docks[2]);
  m_Docks[2]->setWidget(panel);
}

void MainWindow::CreatePanelImpl(size_t i, Qt::DockWidgetArea area, QString const & name,
                                 QKeySequence const & hotkey, char const * slot)
{
  m_Docks[i] = new QDockWidget(name, this);

  addDockWidget(area, m_Docks[i]);

  // hide by default
  m_Docks[i]->hide();

  // register a hotkey to show classificator panel
  if (!hotkey.isEmpty())
  {
    QAction * pAct = new QAction(this);
    pAct->setShortcut(hotkey);
    connect(pAct, SIGNAL(triggered()), this, slot);
    addAction(pAct);
  }
}

void MainWindow::closeEvent(QCloseEvent * e)
{
  m_pDrawWidget->PrepareShutdown();
  e->accept();
}

}
