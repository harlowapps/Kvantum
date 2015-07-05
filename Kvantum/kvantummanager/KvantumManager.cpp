#include "KvantumManager.h"
#include <QFileDialog>
#include <QSettings>
#include <QMessageBox>
#include <QStyleFactory>
#include <QDesktopWidget>
#if QT_VERSION >= 0x050000
#include <QFileDevice>
#include <QTextStream>
#endif
//#include <QDebug>

KvantumManager::KvantumManager (QWidget *parent) : QMainWindow (parent), ui (new Ui::KvantumManager)
{
    ui->setupUi (this);

    setWindowTitle ("Kvantum Manager");
    
    lastPath = QDir::home().path();
    process = new QProcess (this);

    char * _xdg_config_home = getenv ("XDG_CONFIG_HOME");
    if (!_xdg_config_home)
        xdg_config_home = QString ("%1/.config").arg (QDir::homePath());
    else
        xdg_config_home = QString (_xdg_config_home);

    ui->comboToolButton->insertItems (0, QStringList() << "Follow Style"
                                                       << "Icon Only"
                                                       << "Text Only"
                                                       << "Text Beside Icon"
                                                       << "Text Under Icon");

    QLabel *statusLabel = new QLabel();
    statusLabel->setTextInteractionFlags (Qt::TextSelectableByMouse);
    ui->statusBar->addWidget (statusLabel);

    /* set kvconfigTheme */
    updateThemeList();

    effect = new QGraphicsOpacityEffect();
    animation = new QPropertyAnimation (effect, "opacity");

    setAttribute (Qt::WA_AlwaysShowToolTips);
    showAnimated (ui->installLabel, 2000);

    connect (ui->quit, SIGNAL (clicked()), this, SLOT (close()));
    connect (ui->openTheme, SIGNAL (clicked()), this, SLOT (openTheme()));
    connect (ui->installTheme, SIGNAL (clicked()), this, SLOT (installTheme()));
    connect (ui->deleteTheme, SIGNAL (clicked()), this, SLOT (deleteTheme()));
    connect (ui->useTheme, SIGNAL (clicked()), this, SLOT (useTheme()));
    connect (ui->saveButton, SIGNAL (clicked()), this, SLOT (writeConfig()));
    connect (ui->restoreButton, SIGNAL (clicked()), this, SLOT (restoreDefault()));
    connect (ui->checkBoxNoComposite, SIGNAL (clicked (bool)), this, SLOT (notCompisited (bool)));
    connect (ui->checkBoxTrans, SIGNAL (clicked (bool)), this, SLOT (isTranslucent (bool)));
    connect (ui->checkBoxBlurWindow, SIGNAL (clicked (bool)), this, SLOT (popupBlurring (bool)));
    connect (ui->lineEdit, SIGNAL (textChanged (const QString &)), this, SLOT (txtChanged (const QString &)));
    connect (ui->toolBox, SIGNAL (currentChanged (int)), this, SLOT (tabChanged (int)));
    connect (ui->comboBox, SIGNAL (currentIndexChanged (const QString &)), this, SLOT (selectionChanged (const QString &)));
    connect (ui->preview, SIGNAL (clicked()), this, SLOT (preview()));
    connect (ui->aboutButton, SIGNAL (clicked()), this, SLOT (aboutDialog()));

    resize (sizeHint().expandedTo (QSize (600, 400)));
}
/*************************/
KvantumManager::~KvantumManager()
{
    delete ui;
    delete animation;
    delete effect;
}
/*************************/
void KvantumManager::closeEvent (QCloseEvent *event)
{
    process->terminate();
    process->waitForFinished();
    event->accept();
}
/*************************/
void KvantumManager::openTheme()
{
    ui->statusBar->clearMessage();
    QString filePath = QFileDialog::getExistingDirectory (this,
                                                          tr ("Open Kvantum Theme Folder..."),
                                                          lastPath,
                                                          QFileDialog::ShowDirsOnly
                                                          | QFileDialog::ReadOnly
                                                          | QFileDialog::DontUseNativeDialog);
    ui->lineEdit->setText (filePath);
    lastPath = filePath;
}
/*************************/
bool KvantumManager::isThemeDir (const QString &folder)
{
    QDir dir = QDir (folder);
    if (!dir.exists())
        return false;

    /* "Default" is reserved for the copied default theme */
    if (dir.dirName() == "Default") return false;
    /* QSettings doesn't accept spaces in the name */
    QString s = (dir.dirName()).simplified();
    if (s.contains (" "))
        return false;

    QStringList files = dir.entryList (QDir::Files, QDir::Name);
    foreach (const QString &file, files)
    {
        if (file == QString ("%1.kvconfig").arg (dir.dirName())
            || file == QString ("%1.svg").arg (dir.dirName()))
        {
            return true;
        }
    }

    return false;
}
/*************************/
void KvantumManager::notWritable()
{
    QMessageBox msgBox (QMessageBox::Warning,
                        tr ("Kvantum"),
                        tr ("<center><b>You have no permission to write!</b></center>"\
                            "<center>Please fix that first!</center>"),
                        QMessageBox::Close,
                        this);
    msgBox.exec();
}
/*************************/
void KvantumManager::installTheme()
{
    QString theme = ui->lineEdit->text();
    if (QDir (theme).dirName() == "Default")
    {
        QMessageBox msgBox (QMessageBox::Warning,
                            tr ("Kvantum"),
                            tr ("<center><b>This is not an installable Kvantum theme!</b></center>"\
                                "<center>The name of an installable themes should not be \"Default\".</center>"\
                                "<center>Please select another directory!</center>"),
                            QMessageBox::Close,
                            this);
        msgBox.exec();
    }
    else if (!isThemeDir (theme))
    {
        QMessageBox msgBox (QMessageBox::Warning,
                            tr ("Kvantum"),
                            tr ("<center><b>This is not a Kvantum theme!</b></center>"\
                                "<center>Please select another directory!</center>"),
                            QMessageBox::Close,
                            this);
        msgBox.exec();
    }
    else if ((QDir (theme).dirName()).contains ("#"))
    {
        QMessageBox msgBox (QMessageBox::Warning,
                            tr ("Kvantum"),
                            tr ("<center><b>This is not an installable Kvantum theme!</b></center>"\
                                "<center>Installable themes should not have # in their names.</center>"\
                                "<center>Please select another directory!</center>"),
                            QMessageBox::Close,
                            this);
        msgBox.exec();
    }
    else
    {
        QString homeDir = QDir::homePath();
        QDir cf = QDir (xdg_config_home);
        cf.mkdir ("Kvantum");
        QDir kv = QDir (xdg_config_home + QString ("/Kvantum"));
        /* if the Kvantum themes directory is created or already exists... */
        if (kv.exists())
        {
            QDir themeDir = QDir (theme);
            QString themeName = themeDir.dirName();
            QDir subDir_ = QDir (QString ("%1/Kvantum/%2#").arg (xdg_config_home).arg (themeName));
            if (subDir_.exists())
            {
                QMessageBox msgBox (QMessageBox::Warning,
                                    tr ("Kvantum"),
                                    tr ("<center><b>The theme already exists in modified form.</b></center>"\
                                        "<center>First You have to delete its modified version!</center>"),
                                    QMessageBox::Close,
                                    this);
                msgBox.exec();
                return;
            }
            /* ... and contains the same theme... */
            if (!kv.mkdir (themeName))
            {
                QDir subDir = QDir (xdg_config_home + QString ("/Kvantum/") + themeName);
                if (subDir == themeDir)
                {
                    QMessageBox msgBox (QMessageBox::Warning,
                                        tr ("Kvantum"),
                                        tr ("<center><b>You have selected an installed theme folder.</b></center>"\
                                            "<center>Please choose another directory!</center>"),
                                        QMessageBox::Close,
                                        this);
                    msgBox.exec();
                    return;
                }
                if (subDir.exists() && QFileInfo (subDir.absolutePath()).isWritable())
                {
                    QMessageBox msgBox (QMessageBox::Warning,
                                        tr ("Confirmation"),
                                        tr ("<center><b>The theme already exists.</b></center>"\
                                            "<center>Do you want to overwrite it?</center>"),
                                        QMessageBox::Yes | QMessageBox::No,
                                        this);
                    switch (msgBox.exec()) {
                    case QMessageBox::Yes:
                        /* ... then, remove the theme files first */
                        QFile::remove (QString ("%1/Kvantum/%2/%2.kvconfig").arg (xdg_config_home).arg (themeName));
                        QFile::remove (QString ("%1/Kvantum/%2/%2.svg").arg (xdg_config_home).arg (themeName));
                        break;
                    case QMessageBox::No:
                    default:
                        return;
                        break;
                    }
                }
                else
                {
                    notWritable();
                    return;
                }
            }
            subDir_ = QDir (QString (DATADIR) + QString ("/Kvantum/") + themeName);
            if (subDir_.exists())
            {
                QMessageBox msgBox (QMessageBox::Information,
                                    tr ("Kvantum"),
                                    tr ("<center><b>This theme is also installed as root.</b></center>"\
                                        "<center>The user installation will take priority.</center>"),
                                    QMessageBox::Close,
                                    this);
                msgBox.exec();
            }
            /* copy the theme files appropriately */
            QFile::copy (QString ("%1/%2.kvconfig").arg (theme).arg (themeName),
                         QString ("%1/Kvantum/%2/%2.kvconfig").arg (xdg_config_home).arg (themeName));
            QFile::copy (QString ("%1/%2.svg").arg (theme).arg (themeName),
                         QString ("%1/Kvantum/%2/%2.svg").arg (xdg_config_home).arg (themeName));

            /* also copy the color scheme file */
            QString colorFile = QString ("%1/%2.colors").arg (theme).arg (themeName);
            if (QFile::exists (colorFile))
            {
                QString kdeApps = QString ("%1/.kde/share/apps").arg (homeDir);
                QDir kdeAppsDir = QDir (kdeApps);
                if (!kdeAppsDir.exists())
                {
                    kdeApps = QString ("%1/.kde4/share/apps").arg (homeDir);
                    kdeAppsDir = QDir (kdeApps);
                }
                if (kdeAppsDir.exists())
                {
                    kdeAppsDir.mkdir ("color-schemes");
                    QString colorScheme = QString ("%1/color-schemes/%2.colors").arg (kdeApps).arg (themeName);
                    if (QFile::exists (colorScheme))
                        QFile::remove (colorScheme);
                    QFile::copy (colorFile, colorScheme);
                }
            }

            ui->statusBar->showMessage (tr ("%1 installed.").arg (themeName), 10000);
        }
        else
            notWritable();
    }
}
/*************************/
static inline bool removeDir(const QString &dirName)
{
    bool res = true;
    QDir dir(dirName);
    if (dir.exists())
    {
        Q_FOREACH (QFileInfo info, dir.entryInfoList (QDir::Files | QDir::AllDirs
                                                      | QDir::NoDotAndDotDot | QDir::System | QDir::Hidden,
                                                      QDir::DirsFirst))
        {
            if (info.isDir())
                res = removeDir (info.absoluteFilePath());
            else
                res = QFile::remove (info.absoluteFilePath());
            if (!res) return false;
        }
        res = dir.rmdir (dirName);
    }
    return res;
}
/*************************/
void KvantumManager::deleteTheme()
{
    QString theme = ui->comboBox->currentText();
    if (theme.isEmpty()) return;

    QMessageBox msgBox (QMessageBox::Question,
                        tr ("Confirmation"),
                        tr ("<center><b>Do you really want to delete <i>%1</i>?</b></center>").arg (theme),
                        QMessageBox::Yes | QMessageBox::No,
                        this);
    msgBox.setInformativeText (tr ("<center><i>It could not be restored unless you have a copy of it.</i></center>"));
    msgBox.setDefaultButton (QMessageBox::No);
    switch (msgBox.exec()) {
        case QMessageBox::No: return;
        case QMessageBox::Yes:
        default: break;
    }

    QString theme_ = theme;
    if (theme == "Kvantum (modified)")
        theme = "Default#";
    else if (theme.endsWith (" (modified)"))
        theme.replace (QString (" (modified)"), QString ("#"));
    else if (theme == "Kvantum (default)")
        return;

    if (!removeDir (QString ("%1/Kvantum/%2").arg (xdg_config_home).arg (theme)))
    {
        QMessageBox msgBox (QMessageBox::Warning,
                            tr ("Kvantum"),
                            tr ("<center><b>Not all objects could be removed!</b></center>"\
                                "<center>You might want to investigate the cause.</center>"),
                            QMessageBox::Close,
                            this);
        msgBox.exec();
        return;
    }
    QString configFile = QString ("%1/Kvantum/kvantum.kvconfig").arg (xdg_config_home);
    if (QFile::exists (configFile))
    {
        QSettings settings (configFile, QSettings::NativeFormat);
        if (settings.contains ("theme") && theme == settings.value ("theme").toString())
        {
            if (isThemeDir (QString ("%1/Kvantum/Default#").arg (xdg_config_home)))
            {
                settings.setValue ("theme", "Default#");
                kvconfigTheme = "Default#";
            }
            else
            {
                settings.remove ("theme");
                kvconfigTheme = QString();
            }
            QCoreApplication::processEvents();
            QApplication::setStyle (QStyleFactory::create ("kvantum"));
            int extra = QApplication::style()->pixelMetric (QStyle::PM_ScrollBarExtent) * 2;
            resize (size().expandedTo (sizeHint() + QSize (extra, extra)));
            if (process->state() == QProcess::Running)
              preview();
        }
    }
    updateThemeList();
    ui->statusBar->showMessage (tr ("%1 deleted.").arg (theme_), 10000);
}
/*************************/
void KvantumManager::showAnimated (QWidget *w, int duration)
{
    w->show();
    w->setGraphicsEffect (effect);
    animation->setDuration (duration);
    animation->setStartValue (0.0);
    animation->setEndValue (1.0);
    animation->start();
}
/*************************/
// Activates the theme and sets kvconfigTheme.
void KvantumManager::useTheme()
{
    kvconfigTheme = ui->comboBox->currentText();
    if (kvconfigTheme.isEmpty()) return;

    QString theme = kvconfigTheme;
    if (kvconfigTheme == "Kvantum (modified)")
        kvconfigTheme = "Default#";
    else if (kvconfigTheme.endsWith (" (modified)"))
         kvconfigTheme.replace (QString (" (modified)"), QString ("#"));
    else if (kvconfigTheme == "Kvantum (default)")
        kvconfigTheme = QString();

    QString configFile = QString ("%1/Kvantum/kvantum.kvconfig").arg (xdg_config_home);
    QSettings settings (configFile, QSettings::NativeFormat);
    if (!settings.isWritable()) return;

    if (kvconfigTheme.isEmpty())
        settings.remove ("theme");
    else if (settings.value ("theme").toString() != kvconfigTheme)
        settings.setValue ("theme", kvconfigTheme);

    QLabel *statusLabel = ui->statusBar->findChild<QLabel *>();
    statusLabel->setText (tr ("<b>Active theme:</b> %1").arg (theme));
    ui->statusBar->showMessage (tr ("Theme changed to %1.").arg (theme), 10000);
    showAnimated (ui->usageLabel, 1000);

    /* this is needed if the config file is created by this method */
    QCoreApplication::processEvents();
    QApplication::setStyle (QStyleFactory::create ("kvantum"));
    int extra = QApplication::style()->pixelMetric (QStyle::PM_ScrollBarExtent) * 2;
    resize (size().expandedTo (sizeHint() + QSize (extra, extra)));
    if (process->state() == QProcess::Running)
      preview();
}
/*************************/
void KvantumManager::txtChanged (const QString &txt)
{
    if (txt.isEmpty())
        ui->installTheme->setEnabled (false);
    else if (!ui->installTheme->isEnabled())
        ui->installTheme->setEnabled (true);
}
/*************************/
void KvantumManager::defaultThemeButtons()
{
    ui->restoreButton->hide();
    /* Someone may have compiled Kvantum with another default theme.
       So, we get its settings directly from its kvconfig file. */
    QString defaultConfig = ":default.kvconfig";
    QSettings defaultSettings (defaultConfig, QSettings::NativeFormat);

    defaultSettings.beginGroup ("Hacks");
    ui->checkBoxDolphin->setChecked (defaultSettings.value ("transparent_dolphin_view").toBool());
    ui->checkBoxKonsole->setChecked (defaultSettings.value ("blur_konsole").toBool());
    ui->checkBoxKtitle->setChecked (defaultSettings.value ("transparent_ktitle_label").toBool());
    ui->checkBoxMenuTitle->setChecked (defaultSettings.value ("transparent_menutitle").toBool());
    ui->checkBoxKCapacity->setChecked (defaultSettings.value ("kcapacitybar_as_progressbar").toBool());
    ui->checkBoxDark->setChecked (defaultSettings.value ("respect_darkness").toBool());
    ui->checkBoxGrip->setChecked (defaultSettings.value ("force_size_grip").toBool());
    defaultSettings.endGroup();

    defaultSettings.beginGroup ("General");
    bool composited = defaultSettings.value ("composite").toBool();
    ui->checkBoxNoComposite->setChecked (!composited);
    ui->checkBoxleftTab->setChecked (defaultSettings.value ("left_tabs").toBool());
    ui->checkBoxJoinTab->setChecked (defaultSettings.value ("joined_tabs").toBool());
    ui->checkBoxAttachTab->setChecked (defaultSettings.value ("attach_active_tab").toBool());
    if (defaultSettings.contains ("scroll_arrows")) // it's true by default
      ui->checkBoxNoScrollArrow->setChecked (!defaultSettings.value ("scroll_arrows").toBool());
    else
      ui->checkBoxNoScrollArrow->setChecked (false);
    ui->checkBoxProgress->setChecked (defaultSettings.value ("textless_progressbar").toBool());
    ui->checkBoxRubber->setChecked (defaultSettings.value ("fill_rubberband").toBool());
    if (defaultSettings.contains ("menubar_mouse_tracking")) // it's true by default
        ui->checkBoxMenubar->setChecked (defaultSettings.value ("menubar_mouse_tracking").toBool());
    else
        ui->checkBoxMenubar->setChecked (true);
    ui->checkBoxMenuToolbar->setChecked (defaultSettings.value ("merge_menubar_with_toolbar").toBool());
    int index = 0;
    if (defaultSettings.contains ("toolbutton_style"))
    {
        index = defaultSettings.value ("toolbutton_style").toInt();
        if (index > 4 || index < 0) index = 0;
    }
    ui->comboToolButton->setCurrentIndex (index);
    ui->checkBoxX11->setChecked (defaultSettings.value ("x11drag").toBool());
    ui->checkBoxClick->setChecked (defaultSettings.value ("double_click").toBool());
    ui->checkBoxSpin->setChecked (defaultSettings.value ("vertical_spin_indicators").toBool());
    if (composited)
    {
        bool translucency = defaultSettings.value ("translucent_windows").toBool();
        QStringList lst = defaultSettings.value ("opaque").toStringList();
        if (!lst.isEmpty())
            ui->opaqueEdit->setText (lst.join (",")); // is cleared when the config page is shown
        ui->checkBoxTrans->setChecked (translucency);
        isTranslucent (translucency);
        if (translucency)
            ui->checkBoxBlurWindow->setChecked (defaultSettings.value ("blurring").toBool());
        popupBlurring (ui->checkBoxBlurWindow->isChecked());
        if (!ui->checkBoxBlurWindow->isChecked())
            ui->checkBoxBlurPopup->setChecked (defaultSettings.value ("popup_blurring").toBool());
    }
    int icnSize = 16;
    if (defaultSettings.contains ("small_icon_size"))
        icnSize = defaultSettings.value ("small_icon_size").toInt();
    icnSize = qMin(qMax(icnSize,16), 48);
    ui->spinSmall->setValue (icnSize);
    icnSize = 32;
    if (defaultSettings.contains ("large_icon_size"))
        icnSize = defaultSettings.value ("large_icon_size").toInt();
    icnSize = qMin(qMax(icnSize,24), 128);
    ui->spinLarge->setValue (icnSize);
    icnSize = 16;
    if (defaultSettings.contains ("button_icon_size"))
        icnSize = defaultSettings.value ("button_icon_size").toInt();
    icnSize = qMin(qMax(icnSize,16), 64);
    ui->spinButton->setValue (icnSize);
    icnSize = 24;
    if (defaultSettings.contains ("toolbar_icon_size"))
        icnSize = defaultSettings.value ("toolbar_icon_size").toInt();
    else if (defaultSettings.value ("slim_toolbars").toBool())
        icnSize = 16;
    icnSize = qMin(qMax(icnSize,16), 64);
    ui->spinToolbar->setValue (icnSize);
    defaultSettings.endGroup();
}
/*************************/
void KvantumManager::resizeConfPage (bool thirdPage)
{
  bool le = true;
  if (!ui->opaqueEdit->isEnabled())
  {
      le = false;
      ui->opaqueEdit->setEnabled (true);
  }
  QSize newSize  = size().expandedTo (sizeHint()
                                      + (thirdPage ?
                                           ui->comboToolButton->sizeHint() + ui->saveButton->sizeHint()
                                             + QSize (0, 3*ui->saveButton->sizeHint().height() + 10)
                                           : QSize (ui->saveButton->sizeHint().width(), 0)));
  newSize = newSize.boundedTo (QApplication::desktop()->availableGeometry().size());
  resize (newSize);
  if (!le) ui->opaqueEdit->setEnabled (false);
}
/*************************/
void KvantumManager::tabChanged (int index)
{
    updateThemeList();
    bool thirdPage (index == 2);
    ui->statusBar->clearMessage();
    if (index == 0)
        showAnimated (ui->installLabel, 1000);
    if (index == 1)
    {
        ui->usageLabel->show();
        QString comment;
        if (kvconfigTheme.isEmpty())
            ui->deleteTheme->setEnabled (false);
        else
        {
            QString themeConfig;
            QDir themeDir = QDir (xdg_config_home + QString ("/Kvantum/") + kvconfigTheme);
            if (!themeDir.exists())
            {
                ui->deleteTheme->setEnabled (false);
                themeConfig = QString (DATADIR) + QString ("/Kvantum/%1/%1.kvconfig").arg (kvconfigTheme);
            }
            else
            {
                ui->deleteTheme->setEnabled (true);
                themeConfig = QString ("%1/Kvantum/%2/%2.kvconfig").arg (xdg_config_home).arg (kvconfigTheme);
            }
            if (QFile::exists (themeConfig))
            {
                QSettings themeSettings (themeConfig, QSettings::NativeFormat);
                themeSettings.beginGroup ("General");
                comment = themeSettings.value ("comment").toString();
                if (comment.isEmpty()) // comma(s) in the comment
                {
                    QStringList lst = themeSettings.value ("comment").toStringList();
                    if (!lst.isEmpty())
                        comment = lst.join (", ");
                }
                themeSettings.endGroup();
            }
        }
        if (comment.isEmpty())
          comment = "No description";
        ui->comboBox->setToolTip (comment);
    }
    else if (index == 2)
    {
        ui->opaqueEdit->clear();
        defaultThemeButtons();

        if (kvconfigTheme.isEmpty())
        {
            ui->configLabel->setText (tr ("These are the settings that can be safely changed..<br>For the others, click <i>Save</i> and then edit this file:<br><i>~/.config/Kvantum/Default#/<b>Default#.kvconfig</b></i>"));
            showAnimated (ui->configLabel, 1000);
        }
        else
        {
            ui->configLabel->setText (tr ("These are the settings that can be safely changed..<br>For the others, edit this file:<br><i>~/.config/Kvantum/%1/<b>%1.kvconfig</b></i>").arg (kvconfigTheme));
            /* a config other than the default Kvantum one */
            QString themeConfig = QString ("%1/Kvantum/%2/%2.kvconfig").arg (xdg_config_home).arg (kvconfigTheme);
            QString userSvg = QString ("%1/Kvantum/%2/%2.svg").arg (xdg_config_home).arg (kvconfigTheme);

            if (!QFile::exists (themeConfig) && !QFile::exists (userSvg))
            { // no user theme but a root one
                ui->configLabel->setText (tr ("These are the settings that can be safely changed..<br>For the others, click <i>Save</i> and then edit this file:<br><i>~/.config/Kvantum/%1#/<b>%1#.kvconfig</b></i>").arg (kvconfigTheme));
            }
            showAnimated (ui->configLabel, 1000);

            if (kvconfigTheme.endsWith ("#"))
                ui->restoreButton->show();
            else
                ui->restoreButton->hide();

            /* copy Kvantum's default theme config or find the root
               theme config if this user theme doesn't have a config */
            if (!QFile::exists (themeConfig))
            {
                if (QFile::exists (userSvg)) // a user theme without config
                    copyDefaultTheme (QString(), kvconfigTheme);
                else // a root theme
                    themeConfig = QString (DATADIR) + QString ("/Kvantum/%1/%1.kvconfig").arg (kvconfigTheme);
            }

            if (QFile::exists (themeConfig)) // doesn't exist for a root theme without config
            {
                QSettings themeSettings (themeConfig, QSettings::NativeFormat);
                /* consult the default config file if a key doesn't exist */
                themeSettings.beginGroup ("General");
                bool composited = true;
                if (themeSettings.contains ("composite"))
                    composited = themeSettings.value ("composite").toBool();
                else
                    composited = false;
                ui->checkBoxNoComposite->setChecked (!composited);
                notCompisited (!composited);
                if (themeSettings.contains ("left_tabs"))
                    ui->checkBoxleftTab->setChecked (themeSettings.value ("left_tabs").toBool());
                if (themeSettings.contains ("joined_tabs"))
                    ui->checkBoxJoinTab->setChecked (themeSettings.value ("joined_tabs").toBool());
                if (themeSettings.contains ("attach_active_tab"))
                    ui->checkBoxAttachTab->setChecked (themeSettings.value ("attach_active_tab").toBool());
                if (themeSettings.contains ("scroll_arrows"))
                    ui->checkBoxNoScrollArrow->setChecked (!themeSettings.value ("scroll_arrows").toBool());
                if (themeSettings.contains ("textless_progressbar"))
                    ui->checkBoxProgress->setChecked (themeSettings.value ("textless_progressbar").toBool());
                if (themeSettings.contains ("fill_rubberband"))
                    ui->checkBoxRubber->setChecked (themeSettings.value ("fill_rubberband").toBool());
                if (themeSettings.contains ("menubar_mouse_tracking"))
                    ui->checkBoxMenubar->setChecked (themeSettings.value ("menubar_mouse_tracking").toBool());
                if (themeSettings.contains ("merge_menubar_with_toolbar"))
                    ui->checkBoxMenuToolbar->setChecked (themeSettings.value ("merge_menubar_with_toolbar").toBool());
                if (themeSettings.contains ("toolbutton_style"))
                {
                    int index = themeSettings.value ("toolbutton_style").toInt();
                    if (index > 4 || index < 0) index = 0;
                    ui->comboToolButton->setCurrentIndex (index);
                }
                if (themeSettings.contains ("x11drag"))
                    ui->checkBoxX11->setChecked (themeSettings.value ("x11drag").toBool());
                if (themeSettings.contains ("double_click"))
                    ui->checkBoxClick->setChecked (themeSettings.value ("double_click").toBool());
                if (themeSettings.contains ("vertical_spin_indicators"))
                    ui->checkBoxSpin->setChecked (themeSettings.value ("vertical_spin_indicators").toBool());
                if (composited)
                {
                    bool translucency = false;
                    if (themeSettings.contains ("translucent_windows"))
                        translucency = themeSettings.value ("translucent_windows").toBool();
                    QStringList lst = themeSettings.value ("opaque").toStringList();
                    if (!lst.isEmpty())
                        ui->opaqueEdit->setText (lst.join (","));
                    ui->checkBoxTrans->setChecked (translucency);
                    isTranslucent (translucency);
                    if (translucency && themeSettings.contains ("blurring"))
                        ui->checkBoxBlurWindow->setChecked (themeSettings.value ("blurring").toBool());
                    /* popup_blurring is required by blurring */
                    popupBlurring (ui->checkBoxBlurWindow->isChecked());
                    if (!ui->checkBoxBlurWindow->isChecked() && themeSettings.contains ("popup_blurring"))
                        ui->checkBoxBlurPopup->setChecked (themeSettings.value ("popup_blurring").toBool());
                }
                if (themeSettings.contains ("small_icon_size"))
                {
                    int icnSize = themeSettings.value ("small_icon_size").toInt();
                    icnSize = qMin(qMax(icnSize,16), 48);
                    ui->spinSmall->setValue (icnSize);
                }
                if (themeSettings.contains ("large_icon_size"))
                {
                    int icnSize = themeSettings.value ("large_icon_size").toInt();
                    icnSize = qMin(qMax(icnSize,24), 128);
                    ui->spinLarge->setValue (icnSize);
                }
                if (themeSettings.contains ("button_icon_size"))
                {
                    int icnSize = themeSettings.value ("button_icon_size").toInt();
                    icnSize = qMin(qMax(icnSize,16), 64);
                    ui->spinButton->setValue (icnSize);
                }
                if (themeSettings.contains ("toolbar_icon_size"))
                {
                    int icnSize = themeSettings.value ("toolbar_icon_size").toInt();
                    icnSize = qMin(qMax(icnSize,16), 64);
                    ui->spinToolbar->setValue (icnSize);
                }
                else if (themeSettings.contains ("slim_toolbars"))
                    ui->spinToolbar->setValue (16);
                themeSettings.endGroup();

                themeSettings.beginGroup ("Hacks");
                ui->checkBoxDolphin->setChecked (themeSettings.value ("transparent_dolphin_view").toBool());
                ui->checkBoxKonsole->setChecked (themeSettings.value ("blur_konsole").toBool());
                ui->checkBoxKtitle->setChecked (themeSettings.value ("transparent_ktitle_label").toBool());
                ui->checkBoxMenuTitle->setChecked (themeSettings.value ("transparent_menutitle").toBool());
                ui->checkBoxKCapacity->setChecked (themeSettings.value ("kcapacitybar_as_progressbar").toBool());
                ui->checkBoxDark->setChecked (themeSettings.value ("respect_darkness").toBool());
                ui->checkBoxGrip->setChecked (themeSettings.value ("force_size_grip").toBool());
                themeSettings.endGroup();
            }
        }
    }
    resizeConfPage (thirdPage);
}
/*************************/
void KvantumManager::selectionChanged (const QString &txt)
{
    ui->statusBar->clearMessage();

    QString theme;
    if (kvconfigTheme.isEmpty())
        theme = "Kvantum (default)";
    else if (kvconfigTheme == "Default#")
        theme = "Kvantum (modified)";
    else if (kvconfigTheme.endsWith ("#"))
        theme = QString ("%1 (modified)").arg (kvconfigTheme.left (kvconfigTheme.length() - 1));
    else
        theme = kvconfigTheme;

    if (txt == theme)
        showAnimated (ui->usageLabel, 1000);
    else
        ui->usageLabel->hide();

    if (txt == "Kvantum (default)"
        || (!txt.endsWith (" (modified)")
            && !QDir (xdg_config_home + QString ("/Kvantum/") + txt).exists())) // root
    {
        ui->deleteTheme->setEnabled (false);
    }
    else if (!ui->deleteTheme->isEnabled())
        ui->deleteTheme->setEnabled (true);

    QString comment;
    QString text = txt;
    if (text == "Kvantum (modified)")
        text = "Default#";
    else if (text.endsWith (" (modified)"))
        text.replace (QString (" (modified)"), QString ("#"));
    if (text != "Kvantum (default)")
    {
        QString themeConfig = QString ("%1/Kvantum/%2/%2.kvconfig").arg (xdg_config_home).arg (text);
        if (QFile::exists (themeConfig))
        {
            QSettings themeSettings (themeConfig, QSettings::NativeFormat);
            themeSettings.beginGroup ("General");
            comment = themeSettings.value ("comment").toString();
            if (comment.isEmpty()) // comma(s) in the comment
            {
                QStringList lst = themeSettings.value ("comment").toStringList();
                if (!lst.isEmpty())
                    comment = lst.join (", ");
            }
            themeSettings.endGroup();
        }
    }
    if (comment.isEmpty())
      comment = "No description";
    ui->comboBox->setToolTip (comment);
}
/*************************/
void KvantumManager::updateThemeList()
{
    ui->comboBox->clear();

    QStringList list;

    /* add all themes */
    QDir kv = QDir (QString ("%1/Kvantum").arg (xdg_config_home));
    if (kv.exists())
    {
        QStringList folders = kv.entryList (QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        foreach (const QString &folder, folders)
        {
            if (isThemeDir (QString ("%1/Kvantum/%2").arg (xdg_config_home).arg (folder)))
            {
                if (folder == "Default#")
                    list.prepend ("Kvantum (modified)");
                else if (folder.endsWith ("#"))
                {
                    /* see if there's a valid root installtion */
                    QString folder_ = folder.left (folder.length() - 1);
                    if (!folder_.contains ("#")
                        && isThemeDir (QString (DATADIR) + QString ("/Kvantum/%1").arg (folder_)))
                    {
                        list.append (QString ("%1 (modified)").arg (folder_));
                    }
                }
                else
                    list.append (folder);
            }
        }
    }
    int rootSep = 0;
    if (!list.isEmpty())
        rootSep = list.length();
    kv = QDir (QString (DATADIR) + QString ("/Kvantum"));
    if (kv.exists())
    {
        QStringList folders = kv.entryList (QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        foreach (const QString &folder, folders)
        {
            if (!folder.contains ("#")
                && isThemeDir (QString (DATADIR) + QString ("/Kvantum/%1").arg (folder)))
            {
                if (!list.contains (folder) // a user theme with the same name takes priority
                    && !list.contains (QString ("%1 (modified)").arg (folder)))
                {
                    list.append (folder);
                }
            }
        }
    }
    if (list.isEmpty() || list.first() != "Kvantum (modified)")
    {
        list.prepend ("Kvantum (default)");
        rootSep++;
    }
    ui->comboBox->insertItems (0, list);
    ui->comboBox->insertSeparator (1);
    if (rootSep)
        ui->comboBox->insertSeparator (rootSep + 1);

    /* select the active theme and set kvconfigTheme */
    QString theme;
    QString configFile = QString ("%1/Kvantum/kvantum.kvconfig").arg (xdg_config_home);
    bool noConfig = false;
    if (QFile::exists (configFile))
    {
        QSettings settings (configFile, QSettings::NativeFormat);
        if (settings.contains ("theme"))
        {
            kvconfigTheme = settings.value ("theme").toString();
            if (kvconfigTheme.isEmpty())
                theme = "Kvantum (default)";
            else if (kvconfigTheme == "Default#")
                theme = "Kvantum (modified)";
            else if (kvconfigTheme.endsWith ("#"))
                theme = QString ("%1 (modified)").arg (kvconfigTheme.left (kvconfigTheme.length() - 1));
            else
                theme = kvconfigTheme;
            int index = ui->comboBox->findText (theme);
            if (index > -1)
                ui->comboBox->setCurrentIndex (index);
            else // remove from settings if its folder is deleted
            {
                if (list.contains ("Kvantum (modified)"))
                {
                    settings.setValue ("theme", "Default#");
                    kvconfigTheme = "Default#";
                    theme = "Kvantum (modified)";
                }
                else
                {
                    settings.remove ("theme");
                    kvconfigTheme = QString();
                    theme = "Kvantum (default)";
                }
            }
        }
        else noConfig = true;
    }
    else noConfig = true;

    if (noConfig)
    {
        kvconfigTheme = QString();
        theme = "Kvantum (default)";
        /* remove Default# because there's no config */
        QString theCopy = QString ("%1/Kvantum/Default#/Default#.kvconfig").arg (xdg_config_home);
        QFile::remove (theCopy);
    }

    QLabel *statusLabel = ui->statusBar->findChild<QLabel *>();
    statusLabel->setText (tr ("<b>Active theme:</b> %1").arg (theme));
}
/*************************/
void KvantumManager::preview()
{
    QString binDir = QApplication::applicationDirPath();
    QString previewExe = binDir + "/kvantumpreview";
    process->terminate();
    process->waitForFinished();
    process->start (previewExe);
}
/*************************/
void KvantumManager::copyDefaultTheme (QString source, QString target)
{
    QDir cf = QDir (xdg_config_home);
    cf.mkdir ("Kvantum");
    QDir kv = QDir (xdg_config_home + QString ("/Kvantum"));
    if (kv.exists())
    {
        kv.mkdir (target);
        QString theCopy = QString ("%1/Kvantum/%2/%2.kvconfig").arg (xdg_config_home).arg (target);
        if (!QFile::exists (theCopy))
        {
            QString themeConfig = ":default.kvconfig";
            if (!source.isEmpty())
                themeConfig = QString (DATADIR) + QString ("/Kvantum/%1/%1.kvconfig").arg (source);
            if (QFile::copy (themeConfig, theCopy))
            {
#if QT_VERSION < 0x050000
                QFile::setPermissions (theCopy, QFile::permissions (theCopy) | QFile::WriteOwner);
#else
                QFile::setPermissions (theCopy, QFile::permissions (theCopy) | QFileDevice::WriteOwner);
#endif
                ui->statusBar->showMessage (tr ("A copy of the default config is created."), 10000);
            }
            else // either there's no source or the target isn't writable
                notWritable();
        }
        else
        {
            ui->statusBar->clearMessage();
            ui->statusBar->showMessage (tr ("A copy was already created."), 10000);
        }
    }
    else
        notWritable();
}
#if QT_VERSION >= 0x050000
/*************************/
static QString boolToStr (bool b)
{
    if (b) return QString ("true");
    else return QString ("false");
}
#endif
/*************************/
void KvantumManager::writeConfig()
{
    bool wasDefault = false;
    if (kvconfigTheme.isEmpty()) // default theme
    {
        wasDefault = true;
        QFile::remove (QString ("%1/Kvantum/Default#/Default#.kvconfig").arg (xdg_config_home));
        kvconfigTheme = "Default#";
        copyDefaultTheme (QString(), kvconfigTheme);
    }

    QString themeConfig = QString ("%1/Kvantum/%2/%2.kvconfig").arg (xdg_config_home).arg (kvconfigTheme);
    if (!QFile::exists (themeConfig))
    { // root theme (because config files are copied for user themes at tabChanged())
        wasDefault = true;
        QString themeConfig_ = QString (DATADIR) + QString ("/Kvantum/%1/%1.kvconfig").arg (kvconfigTheme);
        if (QFile::exists (themeConfig_))
        {
            QFile::remove (QString ("%1/Kvantum/%2#/%2#.kvconfig").arg (xdg_config_home).arg (kvconfigTheme));
            copyDefaultTheme (kvconfigTheme, kvconfigTheme + "#");
            kvconfigTheme = kvconfigTheme + "#";
        }
        else // root theme is just an SVG image
        {
            kvconfigTheme = kvconfigTheme + "#";
            copyDefaultTheme (QString(), kvconfigTheme);
        }
    }

    QString configFile = QString ("%1/Kvantum/kvantum.kvconfig").arg (xdg_config_home);
    QSettings settings (configFile, QSettings::NativeFormat);
    if (!settings.isWritable()) return;
    settings.setValue ("theme", kvconfigTheme);

    themeConfig = QString ("%1/Kvantum/%2/%2.kvconfig").arg (xdg_config_home).arg (kvconfigTheme);
    if (QFile::exists (themeConfig)) // user theme
    {
        QSettings themeSettings (themeConfig, QSettings::NativeFormat);
        if (!themeSettings.isWritable())
        {
            notWritable();
            return;
        }
#if QT_VERSION >= 0x050000
        /*************************************************************************
          WARNING! Damn! The Qt5 QSettings changes the order of keys on writing.
                         We should write the settings directly!
        **************************************************************************/
        QMap<QString, QString> hackKeys, hackKeysMissing, generalKeys, generalKeysMissing;
        QString str;
        hackKeys.insert("transparent_dolphin_view", boolToStr (ui->checkBoxDolphin->isChecked()));
        hackKeys.insert("blur_konsole", boolToStr (ui->checkBoxKonsole->isChecked()));
        hackKeys.insert("transparent_ktitle_label", boolToStr (ui->checkBoxKtitle->isChecked()));
        hackKeys.insert("transparent_menutitle", boolToStr (ui->checkBoxMenuTitle->isChecked()));
        hackKeys.insert("kcapacitybar_as_progressbar", boolToStr (ui->checkBoxKCapacity->isChecked()));
        hackKeys.insert("respect_darkness", boolToStr (ui->checkBoxDark->isChecked()));
        hackKeys.insert("force_size_grip", boolToStr (ui->checkBoxGrip->isChecked()));

        generalKeys.insert("composite", boolToStr (!ui->checkBoxNoComposite->isChecked()));
        generalKeys.insert("left_tabs", boolToStr (ui->checkBoxleftTab->isChecked()));
        generalKeys.insert("joined_tabs", boolToStr (ui->checkBoxJoinTab->isChecked()));
        generalKeys.insert("attach_active_tab", boolToStr (ui->checkBoxAttachTab->isChecked()));
        generalKeys.insert("scroll_arrows", boolToStr (!ui->checkBoxNoScrollArrow->isChecked()));
        generalKeys.insert("textless_progressbar", boolToStr (ui->checkBoxProgress->isChecked()));
        generalKeys.insert("fill_rubberband", boolToStr (ui->checkBoxRubber->isChecked()));
        generalKeys.insert("menubar_mouse_tracking",  boolToStr (ui->checkBoxMenubar->isChecked()));
        generalKeys.insert("merge_menubar_with_toolbar", boolToStr (ui->checkBoxMenuToolbar->isChecked()));
        generalKeys.insert("toolbutton_style", str.setNum (ui->comboToolButton->currentIndex()));
        generalKeys.insert("x11drag", boolToStr (ui->checkBoxX11->isChecked()));
        generalKeys.insert("double_click", boolToStr (ui->checkBoxClick->isChecked()));
        generalKeys.insert("vertical_spin_indicators", boolToStr (ui->checkBoxSpin->isChecked()));
        generalKeys.insert("translucent_windows", boolToStr (ui->checkBoxTrans->isChecked()));
        generalKeys.insert("popup_blurring", boolToStr (ui->checkBoxBlurPopup->isChecked()));
        generalKeys.insert("blurring", boolToStr (ui->checkBoxBlurWindow->isChecked()));
        generalKeys.insert("small_icon_size", str.setNum (ui->spinSmall->value()));
        generalKeys.insert("large_icon_size", str.setNum (ui->spinLarge->value()));
        generalKeys.insert("button_icon_size", str.setNum (ui->spinButton->value()));
        generalKeys.insert("toolbar_icon_size", str.setNum (ui->spinToolbar->value()));

        QString opaque = ui->opaqueEdit->text();
        opaque = opaque.simplified();
        opaque.remove (" ");
        generalKeys.insert("opaque", opaque);
#endif
        themeSettings.beginGroup ("Hacks");
#if QT_VERSION >= 0x050000
        QMap<QString, QString>::iterator it;
        it = hackKeys.begin();
        while (!hackKeys.isEmpty() && it != hackKeys.end())
        {
            if (!themeSettings.contains (it.key()))
            {
                hackKeysMissing.insert (it.key(), hackKeys.value (it.key()));
                it = hackKeys.erase (it);
            }
            else ++it;
        }
#else
        themeSettings.setValue ("transparent_dolphin_view", ui->checkBoxDolphin->isChecked());
        themeSettings.setValue ("blur_konsole", ui->checkBoxKonsole->isChecked());
        themeSettings.setValue ("transparent_ktitle_label", ui->checkBoxKtitle->isChecked());
        themeSettings.setValue ("transparent_menutitle", ui->checkBoxMenuTitle->isChecked());
        themeSettings.setValue ("kcapacitybar_as_progressbar", ui->checkBoxKCapacity->isChecked());
        themeSettings.setValue ("respect_darkness", ui->checkBoxDark->isChecked());
        themeSettings.setValue ("force_size_grip", ui->checkBoxGrip->isChecked());
#endif
        themeSettings.endGroup();

        themeSettings.beginGroup ("General");
        bool translucenceChanged = false;
        if (themeSettings.value ("composite").toBool() == ui->checkBoxNoComposite->isChecked()
            || themeSettings.value ("translucent_windows").toBool() != ui->checkBoxTrans->isChecked())
        {
            translucenceChanged = true;
        }
#if QT_VERSION >= 0x050000
        it = generalKeys.begin();
        while (!generalKeys.isEmpty() && it != generalKeys.end())
        {
            if (!themeSettings.contains (it.key()))
            {
                generalKeysMissing.insert (it.key(), generalKeys.value (it.key()));
                it = generalKeys.erase (it);
            }
            else ++it;
        }
#else
        themeSettings.setValue ("composite", !ui->checkBoxNoComposite->isChecked());
        themeSettings.setValue ("left_tabs", ui->checkBoxleftTab->isChecked());
        themeSettings.setValue ("joined_tabs", ui->checkBoxJoinTab->isChecked());
        themeSettings.setValue ("attach_active_tab", ui->checkBoxAttachTab->isChecked());
        themeSettings.setValue ("scroll_arrows", !ui->checkBoxNoScrollArrow->isChecked());
        themeSettings.setValue ("textless_progressbar", ui->checkBoxProgress->isChecked());
        themeSettings.setValue ("fill_rubberband", ui->checkBoxRubber->isChecked());
        themeSettings.setValue ("menubar_mouse_tracking", ui->checkBoxMenubar->isChecked());
        themeSettings.setValue ("merge_menubar_with_toolbar", ui->checkBoxMenuToolbar->isChecked());
        themeSettings.setValue ("toolbutton_style", ui->comboToolButton->currentIndex());
        themeSettings.setValue ("x11drag", ui->checkBoxX11->isChecked());
        themeSettings.setValue ("double_click", ui->checkBoxClick->isChecked());
        themeSettings.setValue ("vertical_spin_indicators", ui->checkBoxSpin->isChecked());
        themeSettings.setValue ("translucent_windows", ui->checkBoxTrans->isChecked());
        themeSettings.setValue ("blurring", ui->checkBoxBlurWindow->isChecked());
        themeSettings.setValue ("popup_blurring", ui->checkBoxBlurPopup->isChecked());
        themeSettings.setValue ("small_icon_size", ui->spinSmall->value());
        themeSettings.setValue ("large_icon_size", ui->spinLarge->value());
        themeSettings.setValue ("button_icon_size", ui->spinButton->value());
        themeSettings.setValue ("toolbar_icon_size", ui->spinToolbar->value());
        QString opaque = ui->opaqueEdit->text();
        opaque = opaque.simplified();
        opaque.remove (" ");
        QStringList opaqueList = opaque.split (",");
        themeSettings.setValue ("opaque", opaqueList);
#endif
        themeSettings.endGroup();
#if QT_VERSION >= 0x050000
        QFile file (themeConfig);
        if (!file.open (QIODevice::ReadOnly | QIODevice::Text))
            return;
        QStringList lines;
        QTextStream in (&file);
        while (!in.atEnd())
        {
            bool found = false;
            QString line = in.readLine();
            if (!hackKeys.isEmpty())
            {
                for (it = hackKeys.begin(); it != hackKeys.end(); ++it)
                {   /* one "\\b" is enough because if "keyA" is in the file, "key" isn't in hackKeys */
                    if (line.contains (QRegExp ("^\\s*\\b" + it.key() + "(?=\\s*\\=)")))
                    {
                        line = QString ("%1=%2").arg (it.key()).arg (hackKeys.value (it.key()));
                        hackKeys.remove (it.key());
                        found = true;
                        break;
                    }
                }
            }
            if (!found && !generalKeys.isEmpty())
            {
                for (it = generalKeys.begin(); it != generalKeys.end(); ++it)
                {
                    if (line.contains (QRegExp ("^\\s*\\b" + it.key() + "(?=\\s*\\=)")))
                    {
                        line = QString ("%1=%2").arg (it.key()).arg (generalKeys.value (it.key()));
                        generalKeys.remove (it.key());
                        break;
                    }
                }
            }
            lines.append (line);
        }
        file.close();

        int i, j;
        if (!hackKeysMissing.isEmpty())
        {
            for (i = 0; i < lines.count(); ++i)
            {
                if (lines.at (i).contains (QRegExp ("^\\s*\\[\\s*\\bHacks\\b\\s*\\]")))
                    break;
            }
            if (i == lines.count())
            {
                lines << "" << QString ("[Hacks]");
                ++i;
            }
            for (j = i+1; j < lines.count(); ++j)
            {
                if (lines.at (j).contains (QRegExp("^\\s*\\[")))
                    break;
            }
            while (j-1 >= 0 && lines.at (j-1).isEmpty()) --j;
            for (it = hackKeysMissing.begin(); it != hackKeysMissing.end(); ++it)
            {
                lines.insert (j, QString ("%1=%2").arg (it.key()).arg (hackKeysMissing.value (it.key())));
                ++j;
            }
        }
        if (!generalKeysMissing.isEmpty())
        {
            for (i = 0; i < lines.count(); ++i)
            {
                if (lines.at (i).contains (QRegExp ("^\\s*\\[\\s*%\\bGeneral\\b\\s*\\]")))
                    break;
            }
            if (i == lines.count())
            {
                lines << "" << QString ("[%General]");
                ++i;
            }
            for (j = i+1; j < lines.count(); ++j)
            {
                if (lines.at (j).contains (QRegExp("^\\s*\\[")))
                    break;
            }
            while (j-1 >= 0 && lines.at (j-1).isEmpty()) --j;
            for (it = generalKeysMissing.begin(); it != generalKeysMissing.end(); ++it)
            {
                lines.insert (j, QString ("%1=%2").arg (it.key()).arg (generalKeysMissing.value (it.key())));
                ++j;
            }
        }

        if (!lines.isEmpty())
        {
            if (!file.open (QIODevice::WriteOnly | QIODevice::Text))
                return;
            QTextStream out (&file);
            for (int i = 0; i < lines.count(); ++i)
                out << lines.at(i) << "\n";
            file.close();
        }
#endif
        ui->statusBar->showMessage (tr ("Configuration saved."), 10000);
        QString theme;
        if (kvconfigTheme == "Default#")
            theme = "Kvantum (modified)";
        else if (kvconfigTheme.endsWith ("#"))
            theme = QString ("%1 (modified)").arg (kvconfigTheme.left (kvconfigTheme.length() - 1));
        else
            theme = kvconfigTheme;
        QLabel *statusLabel = ui->statusBar->findChild<QLabel *>();
        statusLabel->setText (tr ("<b>Active theme:</b> %1").arg (theme));

        if (wasDefault && kvconfigTheme.endsWith ("#"))
        {
            ui->restoreButton->show();
            ui->configLabel->setText (tr ("These are the settings that can be safely changed..<br>For the others, edit this file:<br><i>~/.config/Kvantum/%1/<b>%1.kvconfig</b></i>").arg (kvconfigTheme));
            showAnimated (ui->configLabel, 1000);
        }

        QCoreApplication::processEvents();
        if (translucenceChanged)
        {
            QApplication::setStyle (QStyleFactory::create ("kvantum"));
            resizeConfPage (true);
        }
        if (process->state() == QProcess::Running)
          preview();
    }
}
/*************************/
void KvantumManager::restoreDefault()
{ // The restore button is shown only when kvconfigTheme ends with "#" (-> tabChanged()).
    QFile::remove (QString ("%1/Kvantum/%2/%2.kvconfig").arg (xdg_config_home).arg (kvconfigTheme));
    QString kvconfigTheme_ (kvconfigTheme);
    if (kvconfigTheme == "Default#")
        copyDefaultTheme (QString(), kvconfigTheme);
    else
    {
        kvconfigTheme_.remove(QString("#"));
        QString themeConfig_ = QString (DATADIR) + QString ("/Kvantum/%1/%1.kvconfig").arg (kvconfigTheme_);
        if (QFile::exists (themeConfig_))
            copyDefaultTheme (kvconfigTheme_, kvconfigTheme);
        else // root theme is just an SVG image
            copyDefaultTheme (QString(), kvconfigTheme);
    }

    /* correct buttons and label */
    tabChanged (2);

    ui->statusBar->showMessage (tr ("Restored the rool default settings of %1")
                                   .arg (kvconfigTheme == "Default#" ? "the default theme" : kvconfigTheme_),
                                10000);

    QCoreApplication::processEvents();
    QApplication::setStyle (QStyleFactory::create ("kvantum"));
    resizeConfPage (true);
    if (process->state() == QProcess::Running)
      preview();
}
/*************************/
void KvantumManager::notCompisited (bool checked)
{
    ui->checkBoxBlurPopup->setEnabled (!checked && !ui->checkBoxBlurWindow->isChecked());
    ui->checkBoxTrans->setEnabled (!checked);
    isTranslucent (!checked && ui->checkBoxTrans->isChecked());
    if (checked)
    {
        ui->checkBoxTrans->setChecked (false);
        ui->checkBoxBlurPopup->setChecked (false);
    }
}
/*************************/
void KvantumManager::isTranslucent (bool checked)
{
    ui->opaqueLabel->setEnabled (checked);
    ui->opaqueEdit->setEnabled (checked);
    ui->checkBoxBlurWindow->setEnabled (checked);
    if (!checked)
    {
        ui->checkBoxBlurWindow->setChecked (false);
        if (!ui->checkBoxNoComposite->isChecked())
          ui->checkBoxBlurPopup->setEnabled (true);
    }
}
/*************************/
void KvantumManager::popupBlurring (bool checked)
{
    if (checked)
      ui->checkBoxBlurPopup->setChecked (true);
    ui->checkBoxBlurPopup->setEnabled (!checked);
}
/*************************/
void KvantumManager::aboutDialog()
{
    QString qt ("Qt5");
#if QT_VERSION < 0x050000
    qt = "Qt4";
#endif
    QMessageBox::about (this, tr ("About Kvantum Manager"),
                        tr ("<center><b><big>Kvantum Manager 0.8.21</big></b></center><br>"\
                            "<center>A %1 tool for intsalling, selecting</center>\n"\
                            "<center>and configuring <a href='https://github.com/tsujan/Kvantum'>Kvantum</a> themes</center><br>"\
                            "<center>Author: <a href='mailto:tsujan2000@gmail.com?Subject=My%20Subject'>Pedram Pourang (aka. Tsu Jan)</a> </center><p></p>")
                            .arg (qt));
}
