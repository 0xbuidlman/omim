#include "about.hpp"

#include "../platform/platform.hpp"

#include <QtCore/QFile>
#include <QtGui/QIcon>
#include <QtGui/QMenuBar>
#include <QtGui/QHBoxLayout>
#include <QtGui/QVBoxLayout>
#include <QtGui/QLabel>
#include <QtGui/QTextBrowser>


AboutDialog::AboutDialog(QWidget * parent)
  : QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint)
{
  QIcon icon(":/ui/logo.png");
  setWindowIcon(icon);
  setWindowTitle(QMenuBar::tr("About"));

  QLabel * labelIcon = new QLabel();
  labelIcon->setPixmap(icon.pixmap(128));

  QLabel * labelVersion = new QLabel(QString::fromAscii("TODO: Insert version to bundle."));

  QHBoxLayout * hBox = new QHBoxLayout();
  hBox->addWidget(labelIcon);
  hBox->addWidget(labelVersion);

  string aboutText;
  try
  {
    ReaderPtr<Reader> reader = GetPlatform().GetReader("about.html");
    reader.ReadAsString(aboutText);
  }
  catch (RootException const & ex)
  {
    LOG(LWARNING, ("About text error: ", ex.Msg()));
  }

  if (!aboutText.empty())
  {
    QTextBrowser * aboutTextBrowser = new QTextBrowser();
    aboutTextBrowser->setReadOnly(true);
    aboutTextBrowser->setOpenLinks(true);
    aboutTextBrowser->setOpenExternalLinks(true);
    aboutTextBrowser->setText(aboutText.c_str());

    QVBoxLayout * vBox = new QVBoxLayout();
    vBox->addLayout(hBox);
    vBox->addWidget(aboutTextBrowser);
    setLayout(vBox);
  }
  else
    setLayout(hBox);
}
