#include "tdeshare_window.h"
#include "share_dialog.h"
#include "global_options_dialog.h"
#include "diag_dialog.h"
#include "fs_permissions.h"
#include "version.h"

#include <ntqlayout.h>
#include <ntqstatusbar.h>
#include <ntqmessagebox.h>
#include <ntqclipboard.h>
#include <ntqapplication.h>
#include <ntqheader.h>
#include <ntqframe.h>
#include <kiconloader.h>
#include <ntqtimer.h>
#include <unistd.h>

TDEShareWindow::TDEShareWindow(ShareBackend* backend, TQWidget* parent, const char* name)
    : TQMainWindow(parent, name),
      m_backend(backend)
{
    setCaption(TDESHARE_TITLE);
    setIcon(DesktopIcon("network"));
    resize(820, 520);
    setMinimumSize(660, 400);

    setupUi();
    refreshShares();
}

TDEShareWindow::~TDEShareWindow()
{
}

void TDEShareWindow::setupUi()
{
    TQWidget* central = new TQWidget(this);
    setCentralWidget(central);

    TQVBoxLayout* mainLayout = new TQVBoxLayout(central, 10, 8);

    // --- Toolbar Buttons Bar ---
    TQHBoxLayout* toolbarLayout = new TQHBoxLayout(mainLayout, 6);
    m_newBtn = new TQPushButton("New Share...", central);
    m_newBtn->setMinimumWidth(100);
    m_editBtn = new TQPushButton("Edit...", central);
    m_editBtn->setMinimumWidth(80);
    m_deleteBtn = new TQPushButton("Remove", central);
    m_deleteBtn->setMinimumWidth(80);
    m_refreshBtn = new TQPushButton("Refresh", central);
    m_refreshBtn->setMinimumWidth(80);
    m_globalBtn = new TQPushButton("Global options...", central);
    m_globalBtn->setMinimumWidth(115);
    m_diagBtn = new TQPushButton("Diagnostics...", central);
    m_diagBtn->setMinimumWidth(110);

    m_editBtn->setEnabled(false);
    m_deleteBtn->setEnabled(false);

    toolbarLayout->addWidget(m_newBtn);
    toolbarLayout->addWidget(m_editBtn);
    toolbarLayout->addWidget(m_deleteBtn);
    toolbarLayout->addWidget(m_refreshBtn);
    toolbarLayout->addStretch(1);
    toolbarLayout->addWidget(m_globalBtn);
    toolbarLayout->addWidget(m_diagBtn);

    // --- Shares List View ---
    m_listView = new TQListView(central);
    m_listView->addColumn("Share Name");
    m_listView->addColumn("Folder Path");
    m_listView->addColumn("Access Rights");
    m_listView->addColumn("Guest");
    m_listView->addColumn("Unix Permissions");
    m_listView->addColumn("Comment");

    m_listView->setAllColumnsShowFocus(true);
    m_listView->setShowSortIndicator(true);
    m_listView->header()->setClickEnabled(true);

    mainLayout->addWidget(m_listView, 1);

    // --- Status Bar ---
    TQStatusBar* bar = statusBar();
    m_statusCountLabel = new TQLabel("0 active shares", bar);
    m_statusSmbdLabel = new TQLabel(bar);
    m_statusGroupLabel = new TQLabel(bar);

    bar->addWidget(m_statusCountLabel, 1);
    bar->addWidget(m_statusSmbdLabel, 0);
    bar->addWidget(m_statusGroupLabel, 0);

    // Connections
    connect(m_newBtn, SIGNAL(clicked()), this, SLOT(slotNewShare()));
    connect(m_editBtn, SIGNAL(clicked()), this, SLOT(slotEditShare()));
    connect(m_deleteBtn, SIGNAL(clicked()), this, SLOT(slotDeleteShare()));
    connect(m_refreshBtn, SIGNAL(clicked()), this, SLOT(refreshShares()));
    connect(m_globalBtn, SIGNAL(clicked()), this, SLOT(slotGlobalOptions()));
    connect(m_diagBtn, SIGNAL(clicked()), this, SLOT(slotShowDiagnostics()));

    connect(m_listView, SIGNAL(doubleClicked(TQListViewItem*)), this, SLOT(slotItemDoubleClicked(TQListViewItem*)));
    connect(m_listView, SIGNAL(rightButtonClicked(TQListViewItem*, const TQPoint&, int)),
            this, SLOT(slotContextMenu(TQListViewItem*, const TQPoint&, int)));
    connect(m_listView, SIGNAL(selectionChanged()), this, SLOT(slotSelectionChanged()));
}

void TDEShareWindow::updateStatus(bool listSharesOk)
{
    if (!m_backend) return;

    ShareBackend::ServiceState smbdState = m_backend->getSmbdState();
    ShareBackend::UsershareState shareState = m_backend->getUsershareState(listSharesOk);

    // 1. Smbd daemon label
    switch (smbdState) {
    case ShareBackend::ServiceRunning:
        m_statusSmbdLabel->setText("Samba: running  | ");
        break;
    case ShareBackend::ServicePending:
        m_statusSmbdLabel->setText("<qt><font color='#d97706'><b>Samba: reloading...</b></font>  | </qt>");
        TQTimer::singleShot(1500, this, SLOT(refreshShares()));
        break;
    case ShareBackend::ServiceStopped:
    default:
        m_statusSmbdLabel->setText("<qt><font color='red'><b>Samba: stopped</b></font>  | </qt>");
        break;
    }

    // 2. Usershares subsystem label
    switch (shareState) {
    case ShareBackend::UsershareReady:
        m_statusGroupLabel->setText("Usershares: ready  ");
        break;
    case ShareBackend::UsersharePending:
        m_statusGroupLabel->setText("<qt><font color='#d97706'><b>Usershares: pending...</b></font>  </qt>");
        TQTimer::singleShot(1500, this, SLOT(refreshShares()));
        break;
    case ShareBackend::UsershareSetupRequired:
    default:
        m_statusGroupLabel->setText("<qt><font color='red'><b>Usershares: setup required</b></font>  </qt>");
        break;
    }
}

void TDEShareWindow::refreshShares()
{
    m_listView->clear();
    TQString err;
    m_shares = m_backend->listShares(&err);

    bool listOk = err.isEmpty();
    updateStatus(listOk);

    for (TQValueList<ShareInfo>::ConstIterator it = m_shares.begin(); it != m_shares.end(); ++it) {
        TQListViewItem* item = new TQListViewItem(m_listView);
        item->setPixmap(0, SmallIcon("network"));
        item->setText(0, (*it).name);
        item->setText(1, (*it).path);
        item->setText(2, (*it).accessSummary());
        item->setText(3, (*it).guestOk ? "Yes" : "No");

        FsHealth health = FsPermissions::check((*it).path, *it);
        if (health.status == FsHealth::StatusOk) {
            item->setText(4, "[OK] " + health.modeString);
        } else if (health.status == FsHealth::StatusWarning) {
            item->setText(4, "[!] " + health.modeString);
        } else {
            item->setText(4, "[X] Inaccessible");
        }

        item->setText(5, (*it).comment);
    }

    int count = m_shares.count();
    m_statusCountLabel->setText(count <= 1 ? TQString("%1 active share").arg(count)
                                           : TQString("%1 active shares").arg(count));

    slotSelectionChanged();
}

bool TDEShareWindow::selectShareByName(const TQString& name)
{
    if (name.isEmpty()) return false;

    for (TQListViewItem* item = m_listView->firstChild(); item; item = item->nextSibling()) {
        if (item->text(0) == name) {
            m_listView->setCurrentItem(item);
            m_listView->setSelected(item, true);
            m_listView->ensureItemVisible(item);
            slotSelectionChanged();
            return true;
        }
    }
    return false;
}

bool TDEShareWindow::selectShareByPath(const TQString& path)
{
    if (path.isEmpty()) return false;

    TQString cleanPath = path.stripWhiteSpace();
    while (cleanPath.length() > 1 && (cleanPath.endsWith("/") || cleanPath.endsWith("\\"))) {
        cleanPath.truncate(cleanPath.length() - 1);
    }

    for (TQListViewItem* item = m_listView->firstChild(); item; item = item->nextSibling()) {
        TQString p = item->text(1).stripWhiteSpace();
        while (p.length() > 1 && (p.endsWith("/") || p.endsWith("\\"))) {
            p.truncate(p.length() - 1);
        }
        if (p == cleanPath) {
            m_listView->setCurrentItem(item);
            m_listView->setSelected(item, true);
            m_listView->ensureItemVisible(item);
            slotSelectionChanged();
            return true;
        }
    }
    return false;
}

ShareInfo* TDEShareWindow::getSelectedShare()
{
    TQListViewItem* item = m_listView->currentItem();
    if (!item) return NULL;

    TQString name = item->text(0);
    for (TQValueList<ShareInfo>::Iterator it = m_shares.begin(); it != m_shares.end(); ++it) {
        if ((*it).name == name) {
            return &(*it);
        }
    }
    return NULL;
}

void TDEShareWindow::slotSelectionChanged()
{
    bool hasSelection = (m_listView->currentItem() != NULL);
    m_editBtn->setEnabled(hasSelection);
    m_deleteBtn->setEnabled(hasSelection);
}

void TDEShareWindow::slotNewShare()
{
    ShareDialog dlg(m_backend, this);
    dlg.setForNewShare();
    if (dlg.exec() == TQDialog::Accepted) {
        refreshShares();
    }
}


void TDEShareWindow::slotEditShare()
{
    ShareInfo* s = getSelectedShare();
    if (!s) return;

    ShareDialog dlg(m_backend, this);
    dlg.setForEditShare(*s);
    if (dlg.exec() == TQDialog::Accepted) {
        refreshShares();
    }
}

void TDEShareWindow::slotDeleteShare()
{
    ShareInfo* s = getSelectedShare();
    if (!s) return;

    int res = TQMessageBox::warning(
        this, "Confirm Share Removal",
        TQString("Are you sure you want to remove the Samba share '%1'?\n\n"
                 "(Local folder '%2' and its files will NOT be deleted).")
            .arg(s->name).arg(s->path),
        "Remove Share", "Cancel", 0, 1);

    if (res == 0) {
        TQString err;
        if (!m_backend->deleteShare(s->name, &err)) {
            TQMessageBox::critical(this, "Error",
                                   TQString("Failed to remove share '%1':\n%2").arg(s->name).arg(err));
        } else {
            refreshShares();
        }
    }
}

void TDEShareWindow::slotGlobalOptions()
{
    GlobalOptionsDialog dlg(m_backend, this);
    if (dlg.exec() == TQDialog::Accepted) {
        refreshShares();
    }
}

void TDEShareWindow::slotShowDiagnostics()
{
    DiagDialog dlg(m_backend, this);
    dlg.exec();
    refreshShares();
}


void TDEShareWindow::slotItemDoubleClicked(TQListViewItem* item)
{
    if (item) {
        slotEditShare();
    }
}

void TDEShareWindow::slotContextMenu(TQListViewItem* item, const TQPoint& pos, int col)
{
    (void)col;
    if (!item) return;

    TQPopupMenu menu(this);
    menu.insertItem("Edit Share...", this, SLOT(slotEditShare()));
    menu.insertItem("Remove Share", this, SLOT(slotDeleteShare()));
    menu.insertSeparator();
    menu.insertItem("Open Folder in Konqueror", this, SLOT(slotOpenInKonqueror()));
    menu.insertItem("Copy Network Link (smb://...)", this, SLOT(slotCopySmbUrl()));
    menu.insertSeparator();
    menu.insertItem("Fix Folder Unix Permissions...", this, SLOT(slotFixSelectedPermissions()));

    menu.exec(pos);
}

void TDEShareWindow::slotOpenInKonqueror()
{
    ShareInfo* s = getSelectedShare();
    if (!s || s->path.isEmpty()) return;

    TQStringList args;
    args << s->path;

    // Asynchronously launch file manager without blocking GUI or holding pipes open
    if (!ShareBackend::launchDetached("konqueror", args)) {
        if (!ShareBackend::launchDetached("kfmclient", TQStringList() << "openURL" << s->path)) {
            ShareBackend::launchDetached("dolphin", args);
        }
    }
}

void TDEShareWindow::slotCopySmbUrl()
{
    ShareInfo* s = getSelectedShare();
    if (!s) return;

    TQString host = m_backend->getHostName();
    TQString url = TQString("smb://%1/%2").arg(host).arg(s->name);

    TQApplication::clipboard()->setText(url);
    statusBar()->message(TQString("Link copied to clipboard: %1").arg(url), 3000);
}

void TDEShareWindow::slotFixSelectedPermissions()
{
    ShareInfo* s = getSelectedShare();
    if (!s) return;

    FsHealth h = FsPermissions::check(s->path, *s);
    int res = TQMessageBox::question(
        this, "Unix Permissions",
        TQString("Do you want to adjust permissions of '%1' to %2?")
            .arg(s->path).arg(FsPermissions::formatMode(h.suggestedMode, true)),
        "Folder only", "Recursive (folder and files)", "Cancel", 0, 2);

    if (res == 2) return;

    bool recursive = (res == 1);
    TQString err;
    if (!FsPermissions::applyMode(s->path, h.suggestedMode, recursive, &err)) {
        TQMessageBox::critical(this, "Error",
                               TQString("Failed to adjust permissions: %1").arg(err));
    } else {
        refreshShares();
    }
}
