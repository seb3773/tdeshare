#include "diag_dialog.h"

#include <ntqlayout.h>
#include <ntqgroupbox.h>
#include <ntqmessagebox.h>
#include <kiconloader.h>

DiagDialog::DiagDialog(ShareBackend* backend, TQWidget* parent, const char* name)
    : TQDialog(parent, name, true),
      m_backend(backend)
{
    setCaption("Samba System Status & Diagnostics");
    setIcon(SmallIcon("network"));
    resize(580, 480);

    TQVBoxLayout* mainLayout = new TQVBoxLayout(this, 12, 10);

    TQGroupBox* checksGroup = new TQGroupBox("System Diagnostics", this);
    checksGroup->setColumnLayout(0, TQt::Vertical);
    checksGroup->layout()->setSpacing(8);
    checksGroup->layout()->setMargin(10);
    TQGridLayout* grid = new TQGridLayout(checksGroup->layout(), 5, 3);
    grid->setSpacing(8);
    mainLayout->addWidget(checksGroup);

    // Row 0: Samba Binaries
    m_sambaBinIcon = new TQLabel(checksGroup);
    m_sambaBinIcon->setFixedWidth(32);
    m_sambaBinIcon->setAlignment(TQt::AlignCenter);
    m_sambaBinText = new TQLabel(checksGroup);
    grid->addWidget(m_sambaBinIcon, 0, 0, TQt::AlignVCenter);
    grid->addMultiCellWidget(m_sambaBinText, 0, 0, 1, 2);

    // Row 1: smbd service
    m_smbdIcon = new TQLabel(checksGroup);
    m_smbdIcon->setFixedWidth(32);
    m_smbdIcon->setAlignment(TQt::AlignCenter);
    m_smbdText = new TQLabel(checksGroup);
    m_startSmbdBtn = new TQPushButton("Start smbd", checksGroup);
    m_startSmbdBtn->setFixedWidth(120);
    grid->addWidget(m_smbdIcon, 1, 0, TQt::AlignVCenter);
    grid->addWidget(m_smbdText, 1, 1, TQt::AlignVCenter);
    grid->addWidget(m_startSmbdBtn, 1, 2, TQt::AlignVCenter);

    // Row 2: sambashare group
    m_groupIcon = new TQLabel(checksGroup);
    m_groupIcon->setFixedWidth(32);
    m_groupIcon->setAlignment(TQt::AlignCenter);
    m_groupText = new TQLabel(checksGroup);
    m_joinGroupBtn = new TQPushButton("Join Group", checksGroup);
    m_joinGroupBtn->setFixedWidth(120);
    grid->addWidget(m_groupIcon, 2, 0, TQt::AlignVCenter);
    grid->addWidget(m_groupText, 2, 1, TQt::AlignVCenter);
    grid->addWidget(m_joinGroupBtn, 2, 2, TQt::AlignVCenter);

    // Row 3: usershares directory
    m_dirIcon = new TQLabel(checksGroup);
    m_dirIcon->setFixedWidth(32);
    m_dirIcon->setAlignment(TQt::AlignCenter);
    m_dirText = new TQLabel(checksGroup);
    grid->addWidget(m_dirIcon, 3, 0, TQt::AlignVCenter);
    grid->addMultiCellWidget(m_dirText, 3, 3, 1, 2);

    // Row 4: Config summary
    TQLabel* cfgIcon = new TQLabel("<qt><font color='gray'>[i]</font></qt>", checksGroup);
    cfgIcon->setFixedWidth(32);
    cfgIcon->setAlignment(TQt::AlignCenter);
    m_configText = new TQLabel(checksGroup);
    grid->addWidget(cfgIcon, 4, 0, TQt::AlignVCenter);
    grid->addMultiCellWidget(m_configText, 4, 4, 1, 2);

    // Informative note
    TQLabel* note = new TQLabel(
        "<qt><small><b>Important note:</b> After joining the <i>sambashare</i> group, "
        "you may need to log out and log back in to your Trinity session for the new "
        "group membership to take effect desktop-wide.</small></qt>", this);
    mainLayout->addWidget(note);

    mainLayout->addStretch(1);

    // Bottom buttons
    TQHBoxLayout* btnLayout = new TQHBoxLayout(mainLayout, 8);
    m_refreshBtn = new TQPushButton("Refresh", this);
    m_refreshBtn->setMinimumWidth(80);
    btnLayout->addWidget(m_refreshBtn);
    btnLayout->addStretch(1);
    m_closeBtn = new TQPushButton("Close", this);
    m_closeBtn->setMinimumWidth(80);
    btnLayout->addWidget(m_closeBtn);

    connect(m_refreshBtn, SIGNAL(clicked()), this, SLOT(refreshDiagnostics()));
    connect(m_closeBtn, SIGNAL(clicked()), this, SLOT(accept()));
    connect(m_startSmbdBtn, SIGNAL(clicked()), this, SLOT(slotStartSmbd()));
    connect(m_joinGroupBtn, SIGNAL(clicked()), this, SLOT(slotJoinGroup()));

    refreshDiagnostics();

    // Enforce size constraints to prevent layout clipping when resizing
    checksGroup->setMinimumSize(checksGroup->sizeHint());
    note->setMinimumSize(note->sizeHint());
    mainLayout->setResizeMode(TQLayout::Minimum);
    adjustSize();
    TQSize minSz = sizeHint().expandedTo(TQSize(560, 450));
    resize(minSz);
    setMinimumSize(minSz);
}

DiagDialog::~DiagDialog()
{
}

void DiagDialog::refreshDiagnostics()
{
    if (!m_backend) return;

    // 1. Binaries
    if (m_backend->isSambaInstalled()) {
        m_sambaBinIcon->setPixmap(SmallIcon("checkmark"));
        m_sambaBinText->setText("<qt>Samba utilities (<code>net</code>, <code>testparm</code>) are installed.</qt>");
    } else {
        m_sambaBinIcon->setPixmap(SmallIcon("stop"));
        m_sambaBinText->setText("<qt><b>Error:</b> Samba utilities (<code>/usr/bin/net</code>) not found.</qt>");
    }

    // 2. smbd daemon
    ShareBackend::ServiceState smbdState = m_backend->getSmbdState();
    if (smbdState == ShareBackend::ServiceRunning) {
        m_smbdIcon->setPixmap(SmallIcon("checkmark"));
        m_smbdText->setText("<qt>Samba daemon (<code>smbd</code>) is active and running.</qt>");
        m_startSmbdBtn->hide();
    } else if (smbdState == ShareBackend::ServicePending) {
        m_smbdIcon->setText("<qt><font color='#d97706'><b>[~]</b></font></qt>");
        m_smbdText->setText("<qt>Samba daemon (<code>smbd</code>) is reloading...</qt>");
        m_startSmbdBtn->hide();
    } else {
        m_smbdIcon->setPixmap(SmallIcon("stop"));
        m_smbdText->setText("<qt>Samba daemon (<code>smbd</code>) is stopped.</qt>");
        m_startSmbdBtn->show();
    }

    // 3. sambashare group
    if (m_backend->isUserInSambashareGroup()) {
        m_groupIcon->setPixmap(SmallIcon("checkmark"));
        m_groupText->setText("<qt>Current user is a member of the <b>sambashare</b> group.</qt>");
        m_joinGroupBtn->hide();
    } else {
        m_groupIcon->setPixmap(SmallIcon("stop"));
        m_groupText->setText("<qt><b>Action required:</b> User is not in the <b>sambashare</b> group.</qt>");
        m_joinGroupBtn->show();
    }

    // 4. Usershare dir
    TQString dirPath = m_backend->usershareDirectoryPath();
    if (m_backend->isUsershareDirectoryReady()) {
        m_dirIcon->setPixmap(SmallIcon("checkmark"));
        m_dirText->setText(TQString("<qt>Usershare directory ready: <code>%1</code></qt>").arg(dirPath));
    } else {
        m_dirIcon->setPixmap(SmallIcon("stop"));
        m_dirText->setText(TQString("<qt>Usershare directory not initialized or inaccessible: <code>%1</code></qt>").arg(dirPath));
    }

    // 5. Config
    int maxShares = m_backend->usershareMaxShares();
    m_configText->setText(TQString("<qt>Maximum allowed user shares (usershare max shares): <b>%1</b></qt>")
                              .arg(maxShares));
}

void DiagDialog::slotJoinGroup()
{
    TQString err;
    if (!m_backend->joinSambashareGroup(&err)) {
        TQMessageBox::critical(this, "Error",
                               TQString("Failed to add user to sambashare group:\n%1").arg(err));
    } else {
        TQMessageBox::information(this, "sambashare Group",
                                  "User has been successfully added to the 'sambashare' group.\n\n"
                                  "Please log out and log back in to your Trinity session for the changes "
                                  "to be fully applied.");
        refreshDiagnostics();
    }
}

void DiagDialog::slotStartSmbd()
{
    TQString err;
    if (!m_backend->startSmbdService(&err)) {
        TQMessageBox::critical(this, "Error",
                               TQString("Failed to start smbd service:\n%1").arg(err));
    } else {
        refreshDiagnostics();
    }
}
