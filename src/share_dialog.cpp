#include "share_dialog.h"
#include "add_user_dialog.h"

#include <ntqlayout.h>
#include <ntqmessagebox.h>
#include <ntqfiledialog.h>
#include <ntqheader.h>
#include <ntqfont.h>
#include <ntqdir.h>
#include <ntqfileinfo.h>
#include <pwd.h>
#include <kiconloader.h>

ShareDialog::ShareDialog(ShareBackend* backend, TQWidget* parent, const char* name)
    : TQDialog(parent, name, true),
      m_backend(backend),
      m_isEditMode(false),
      m_initializing(false)
{
    setCaption("New smb share");
    setIcon(SmallIcon("network"));

    TQVBoxLayout* mainLayout = new TQVBoxLayout(this, 12, 10);

    // --- General Information Group ---
    TQGroupBox* generalGroup = new TQGroupBox("Share Information", this);
    generalGroup->setColumnLayout(0, TQt::Vertical);
    generalGroup->layout()->setSpacing(6);
    generalGroup->layout()->setMargin(10);
    TQGridLayout* genLayout = new TQGridLayout(generalGroup->layout(), 3, 2);
    genLayout->setSpacing(8);

    TQLabel* lblName = new TQLabel("Share name:", generalGroup);
    m_nameEdit = new TQLineEdit(generalGroup);
    genLayout->addWidget(lblName, 0, 0);
    genLayout->addWidget(m_nameEdit, 0, 1);

    TQLabel* lblPath = new TQLabel("Folder path:", generalGroup);
    TQWidget* pathContainer = new TQWidget(generalGroup);
    TQHBoxLayout* pathLayout = new TQHBoxLayout(pathContainer, 0, 6);
    m_pathEdit = new TQLineEdit(pathContainer);
    m_browseBtn = new TQPushButton("Browse...", pathContainer);
    m_browseBtn->setFixedWidth(90);
    pathLayout->addWidget(m_pathEdit);
    pathLayout->addWidget(m_browseBtn);
    genLayout->addWidget(lblPath, 1, 0);
    genLayout->addWidget(pathContainer, 1, 1);

    TQLabel* lblComment = new TQLabel("Comment:", generalGroup);
    m_commentEdit = new TQLineEdit(generalGroup);
    genLayout->addWidget(lblComment, 2, 0);
    genLayout->addWidget(m_commentEdit, 2, 1);

    mainLayout->addWidget(generalGroup);

    // --- Access Control Group ---
    m_modeGroup = new TQButtonGroup("Access Permissions", this);
    m_modeGroup->setColumnLayout(0, TQt::Vertical);
    m_modeGroup->layout()->setSpacing(6);
    m_modeGroup->layout()->setMargin(10);
    TQVBoxLayout* modeLayout = new TQVBoxLayout(m_modeGroup->layout());
    modeLayout->setSpacing(6);

    m_radioEveryoneRo = new TQRadioButton("Read-only for everyone (Everyone:R)", m_modeGroup);
    m_radioEveryoneRw = new TQRadioButton("Read && write for everyone (Everyone:F)", m_modeGroup);
    m_radioCustomAcl  = new TQRadioButton("Custom user-specific access control (ACLs)", m_modeGroup);

    m_modeGroup->insert(m_radioEveryoneRo, 0);
    m_modeGroup->insert(m_radioEveryoneRw, 1);
    m_modeGroup->insert(m_radioCustomAcl, 2);

    modeLayout->addWidget(m_radioEveryoneRo);
    modeLayout->addWidget(m_radioEveryoneRw);
    modeLayout->addWidget(m_radioCustomAcl);

    mainLayout->addWidget(m_modeGroup);

    // --- Custom ACLs panel ---
    m_aclContainer = new TQGroupBox("User Access List (ACLs)", this);
    m_aclContainer->setColumnLayout(0, TQt::Vertical);
    m_aclContainer->layout()->setSpacing(6);
    m_aclContainer->layout()->setMargin(10);
    TQVBoxLayout* aclLayout = new TQVBoxLayout(m_aclContainer->layout());

    m_aclTable = new TQTable(0, 3, m_aclContainer);
    m_aclTable->horizontalHeader()->setLabel(0, "User / Group");
    m_aclTable->horizontalHeader()->setLabel(1, "Type");
    m_aclTable->horizontalHeader()->setLabel(2, "Permission");
    m_aclTable->setSelectionMode(TQTable::SingleRow);
    m_aclTable->setFocusStyle(TQTable::FollowStyle);
    m_aclTable->verticalHeader()->hide();
    m_aclTable->setLeftMargin(0);
    m_aclTable->setColumnReadOnly(0, true);
    m_aclTable->setColumnReadOnly(1, true);
    m_aclTable->setColumnReadOnly(2, true);
    m_aclTable->setRowMovingEnabled(false);
    m_aclTable->setSorting(false);
    m_aclTable->setColumnWidth(0, 160);
    m_aclTable->setColumnWidth(1, 110);
    m_aclTable->setColumnWidth(2, 170);
    m_aclTable->setMinimumHeight(130);
    aclLayout->addWidget(m_aclTable);

    TQHBoxLayout* aclBtnLayout = new TQHBoxLayout(aclLayout, 8);
    m_addUserBtn = new TQPushButton("Add user to share access", m_aclContainer);
    m_removeUserBtn = new TQPushButton("Remove user from share access", m_aclContainer);
    m_removeUserBtn->setEnabled(false);
    aclBtnLayout->addWidget(m_addUserBtn);
    aclBtnLayout->addWidget(m_removeUserBtn);
    aclBtnLayout->addStretch(1);

    mainLayout->addWidget(m_aclContainer);

    // Guest access
    m_guestOkCheck = new TQCheckBox("Allow guest / anonymous access (no password required)", this);
    mainLayout->addWidget(m_guestOkCheck);

    // --- Filesystem Health Group ---
    m_fsGroupBox = new TQGroupBox("Filesystem Health (Unix Permissions)", this);
    m_fsGroupBox->setColumnLayout(0, TQt::Vertical);
    m_fsGroupBox->layout()->setSpacing(6);
    m_fsGroupBox->layout()->setMargin(10);
    TQVBoxLayout* fsLayout = new TQVBoxLayout(m_fsGroupBox->layout());

    TQHBoxLayout* fsStatusLine = new TQHBoxLayout(fsLayout, 8);
    m_fsIconLabel = new TQLabel(m_fsGroupBox);
    m_fsIconLabel->setFixedWidth(32);
    m_fsIconLabel->setAlignment(TQt::AlignTop | TQt::AlignHCenter);

    m_fsDetailLabel = new TQLabel(m_fsGroupBox);
    m_fsDetailLabel->setMinimumHeight(44);
    m_fixPermBtn = new TQPushButton("Fix Unix Permissions...", m_fsGroupBox);
    m_fixPermBtn->hide();

    fsStatusLine->addWidget(m_fsIconLabel, 0, TQt::AlignTop);
    fsStatusLine->addWidget(m_fsDetailLabel, 1, TQt::AlignVCenter);
    fsStatusLine->addWidget(m_fixPermBtn, 0, TQt::AlignVCenter);

    // Reserve fixed vertical space so changing directory never expands this box and never pushes buttons off-screen!
    m_fsGroupBox->setMinimumHeight(96);

    mainLayout->addWidget(m_fsGroupBox);

    // --- Dialog Bottom Buttons ---
    TQHBoxLayout* btnLayout = new TQHBoxLayout(mainLayout, 8);
    btnLayout->addStretch(1);
    m_cancelBtn = new TQPushButton("Cancel", this);
    m_cancelBtn->setMinimumWidth(80);
    m_okBtn = new TQPushButton("Apply", this);
    m_okBtn->setMinimumWidth(100);
    m_okBtn->setDefault(true);
    btnLayout->addWidget(m_cancelBtn);
    btnLayout->addWidget(m_okBtn);

    // Enforce minimum size to protect frames and buttons
    generalGroup->setMinimumSize(generalGroup->sizeHint());
    m_modeGroup->setMinimumSize(m_modeGroup->sizeHint());
    m_aclContainer->setMinimumSize(m_aclContainer->sizeHint());
    m_fsGroupBox->setMinimumSize(m_fsGroupBox->sizeHint().expandedTo(TQSize(0, 96)));
    mainLayout->setResizeMode(TQLayout::Minimum);
    adjustSize();
    TQSize minSz = sizeHint().expandedTo(TQSize(560, 640));
    resize(minSz);
    setMinimumSize(minSz);

    // Connections
    connect(m_browseBtn, SIGNAL(clicked()), this, SLOT(slotBrowse()));
    connect(m_pathEdit, SIGNAL(textChanged(const TQString&)), this, SLOT(slotPathChanged(const TQString&)));
    connect(m_modeGroup, SIGNAL(clicked(int)), this, SLOT(slotAccessModeChanged(int)));
    connect(m_guestOkCheck, SIGNAL(toggled(bool)), this, SLOT(slotGuestOkToggled(bool)));

    connect(m_aclTable, SIGNAL(currentChanged(int, int)), this, SLOT(slotTableSelectionChanged(int, int)));
    connect(m_addUserBtn, SIGNAL(clicked()), this, SLOT(slotAddUser()));
    connect(m_removeUserBtn, SIGNAL(clicked()), this, SLOT(slotRemoveUser()));
    connect(m_fixPermBtn, SIGNAL(clicked()), this, SLOT(slotFixPermissions()));

    connect(m_okBtn, SIGNAL(clicked()), this, SLOT(slotOk()));
    connect(m_cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));
}

ShareDialog::~ShareDialog()
{
}

void ShareDialog::setForNewShare(const TQString& prefillPath)
{
    m_initializing = true;
    m_isEditMode = false;
    m_lastAutoName = "";
    setCaption("New smb share");
    m_okBtn->setText("Apply");

    m_share = ShareInfo();
    m_share.acls.clear();
    m_share.acls.append(UserAcl("Everyone", UserAcl::ReadOnly));
    m_share.guestOk = false;

    m_nameEdit->setReadOnly(false);
    m_nameEdit->setText("");
    m_pathEdit->setText(prefillPath);
    m_commentEdit->setText("");

    m_initializing = false;

    if (!prefillPath.isEmpty()) {
        slotPathChanged(prefillPath);
    }
    syncUiFromShare();
}

void ShareDialog::setForEditShare(const ShareInfo& share)
{
    m_initializing = true;
    m_isEditMode = true;
    m_lastAutoName = "";
    m_share = share;

    setCaption(TQString("Edit smb share '%1'").arg(share.name));
    m_okBtn->setText("Apply");

    m_nameEdit->setReadOnly(true);
    m_nameEdit->setText(share.name);
    m_pathEdit->setText(share.path);
    m_commentEdit->setText(share.comment);

    m_initializing = false;

    syncUiFromShare();
}

bool ShareDialog::isNetworkUser(const TQString& username) const
{
    struct passwd* pw = getpwnam(username.local8Bit());
    if (pw && pw->pw_shell && strcmp(pw->pw_shell, "/usr/sbin/nologin") == 0) {
        return true;
    }
    return false;
}

void ShareDialog::addUserToTable(const TQString& username, UserAcl::Permission perm)
{
    int r = m_aclTable->numRows();
    m_aclTable->setNumRows(r + 1);
    m_aclTable->setText(r, 0, username);

    TQString typeStr = "Local User";
    if (username.lower() == "everyone") {
        typeStr = "Everyone";
    } else if (isNetworkUser(username)) {
        typeStr = "Network User";
    }
    m_aclTable->setText(r, 1, typeStr);

    TQComboBox* cb = new TQComboBox(false, m_aclTable->viewport());
    cb->insertItem("Read only");
    cb->insertItem("Full access (RW)");
    cb->insertItem("Denied");

    if (perm == UserAcl::FullAccess) {
        cb->setCurrentItem(1);
    } else if (perm == UserAcl::Deny) {
        cb->setCurrentItem(2);
    } else {
        cb->setCurrentItem(0);
    }

    connect(cb, SIGNAL(activated(int)), this, SLOT(slotTablePermChanged()));
    m_aclTable->setCellWidget(r, 2, cb);
}

void ShareDialog::refreshAclTable()
{
    m_aclTable->setNumRows(0);
    for (TQValueList<UserAcl>::ConstIterator it = m_share.acls.begin(); it != m_share.acls.end(); ++it) {
        addUserToTable((*it).user, (*it).perm);
    }
    slotTableSelectionChanged(m_aclTable->currentRow(), 0);
}

void ShareDialog::syncShareFromTable()
{
    m_share.acls.clear();
    for (int r = 0; r < m_aclTable->numRows(); ++r) {
        TQString user = m_aclTable->text(r, 0);
        if (user.isEmpty()) continue;
        TQComboBox* cb = static_cast<TQComboBox*>(m_aclTable->cellWidget(r, 2));
        UserAcl::Permission p = UserAcl::ReadOnly;
        if (cb) {
            int idx = cb->currentItem();
            if (idx == 1) p = UserAcl::FullAccess;
            else if (idx == 2) p = UserAcl::Deny;
            else p = UserAcl::ReadOnly;
        }
        m_share.acls.append(UserAcl(user, p));
    }
}

void ShareDialog::syncUiFromShare()
{
    m_guestOkCheck->setChecked(m_share.guestOk);

    if (m_share.isEveryoneReadOnly()) {
        m_radioEveryoneRo->setChecked(true);
        m_aclContainer->setEnabled(false);
    } else if (m_share.isEveryoneFull()) {
        m_radioEveryoneRw->setChecked(true);
        m_aclContainer->setEnabled(false);
    } else {
        m_radioCustomAcl->setChecked(true);
        m_aclContainer->setEnabled(true);
    }

    refreshAclTable();
    updateFsStatus();
}

void ShareDialog::syncShareFromUi()
{
    m_share.name = m_nameEdit->text().stripWhiteSpace();
    m_share.path = m_pathEdit->text().stripWhiteSpace();
    m_share.comment = m_commentEdit->text().stripWhiteSpace();
    m_share.guestOk = m_guestOkCheck->isChecked();

    if (m_radioEveryoneRo->isChecked()) {
        m_share.acls.clear();
        m_share.acls.append(UserAcl("Everyone", UserAcl::ReadOnly));
    } else if (m_radioEveryoneRw->isChecked()) {
        m_share.acls.clear();
        m_share.acls.append(UserAcl("Everyone", UserAcl::FullAccess));
    } else {
        syncShareFromTable();
    }
}

void ShareDialog::slotBrowse()
{
    TQString initialDir = m_pathEdit->text();
    if (initialDir.isEmpty() || !TQDir(initialDir).exists()) {
        initialDir = TQDir::homeDirPath();
    }

    TQString dir = TQFileDialog::getExistingDirectory(initialDir, this, "select_dir",
                                                      "Select Folder to Share", true);
    if (dir.isEmpty()) return;

    TQString cleanPath = dir.stripWhiteSpace();
    while (cleanPath.length() > 1 && (cleanPath.endsWith("/") || cleanPath.endsWith("\\"))) {
        cleanPath.truncate(cleanPath.length() - 1);
    }

    // Check if the selected folder is already shared on the system
    if (!m_isEditMode && m_backend) {
        ShareInfo existing;
        bool alreadyShared = false;
        TQValueList<ShareInfo> shares = m_backend->listShares();
        for (TQValueList<ShareInfo>::ConstIterator it = shares.begin(); it != shares.end(); ++it) {
            TQString p = (*it).path.stripWhiteSpace();
            while (p.length() > 1 && (p.endsWith("/") || p.endsWith("\\"))) p.truncate(p.length() - 1);
            if (p == cleanPath) {
                existing = *it;
                alreadyShared = true;
                break;
            }
        }

        if (alreadyShared) {
            int choice = TQMessageBox::question(
                this, "Folder Already Shared",
                TQString("The folder '%1' is already shared on the network as '%2'.\n\n"
                         "Would you like to edit the settings of this existing share instead?")
                    .arg(cleanPath).arg(existing.name),
                "Edit Existing Share", "Choose Another Folder", 0, 0);

            if (choice == 0) {
                setForEditShare(existing);
                return;
            } else {
                return;
            }
        }
    }

    m_pathEdit->setText(dir);
}

void ShareDialog::slotPathChanged(const TQString& text)
{
    if (m_initializing) return;

    TQString cleanPath = text.stripWhiteSpace();
    while (cleanPath.length() > 1 && (cleanPath.endsWith("/") || cleanPath.endsWith("\\"))) {
        cleanPath.truncate(cleanPath.length() - 1);
    }

    TQFileInfo fi(cleanPath);
    if (!m_isEditMode && fi.exists() && fi.isDir()) {
        TQString folderName = fi.fileName();
        if (!folderName.isEmpty()) {
            if (m_nameEdit->text().isEmpty() || m_nameEdit->text() == m_lastAutoName) {
                m_nameEdit->setText(folderName);
                m_lastAutoName = folderName;
            }
        }
    }
    updateFsStatus();
}

void ShareDialog::slotAccessModeChanged(int id)
{
    if (m_initializing) return;
    if (id == 0) { // Everyone RO
        m_aclContainer->setEnabled(false);
        m_share.acls.clear();
        m_share.acls.append(UserAcl("Everyone", UserAcl::ReadOnly));
        refreshAclTable();
    } else if (id == 1) { // Everyone RW
        m_aclContainer->setEnabled(false);
        m_share.acls.clear();
        m_share.acls.append(UserAcl("Everyone", UserAcl::FullAccess));
        refreshAclTable();
    } else { // Custom ACLs
        m_aclContainer->setEnabled(true);
        refreshAclTable();
    }
    updateFsStatus();
}

void ShareDialog::slotGuestOkToggled(bool checked)
{
    if (m_initializing) return;
    m_share.guestOk = checked;
    updateFsStatus();
}

void ShareDialog::slotTablePermChanged()
{
    syncShareFromTable();
    updateFsStatus();
}

void ShareDialog::slotTableSelectionChanged(int row, int col)
{
    (void)col;
    bool hasSelection = (row >= 0 && row < m_aclTable->numRows());
    m_removeUserBtn->setEnabled(hasSelection);
}

void ShareDialog::slotAddUser()
{
    TQStringList currentUsers;
    for (int r = 0; r < m_aclTable->numRows(); ++r) {
        currentUsers << m_aclTable->text(r, 0);
    }

    AddUserDialog dlg(m_backend, currentUsers, this);
    if (dlg.exec() == TQDialog::Accepted) {
        TQString newUser = dlg.selectedUser();
        if (!newUser.isEmpty()) {
            // Check if user already in table (case-insensitive)
            int existingRow = -1;
            for (int r = 0; r < m_aclTable->numRows(); ++r) {
                if (m_aclTable->text(r, 0).lower() == newUser.lower()) {
                    existingRow = r;
                    break;
                }
            }

            if (existingRow >= 0) {
                m_aclTable->setCurrentCell(existingRow, 0);
            } else {
                // Add with default Read only permission
                addUserToTable(newUser, UserAcl::ReadOnly);
                syncShareFromTable();
                updateFsStatus();
                m_aclTable->setCurrentCell(m_aclTable->numRows() - 1, 0);
            }
        }
    }
}

void ShareDialog::slotRemoveUser()
{
    int r = m_aclTable->currentRow();
    if (r >= 0 && r < m_aclTable->numRows()) {
        m_aclTable->removeRow(r);
        syncShareFromTable();
        updateFsStatus();
        slotTableSelectionChanged(m_aclTable->currentRow(), 0);
    }
}

void ShareDialog::updateFsStatus()
{
    if (m_initializing) return;

    TQString path = m_pathEdit->text().stripWhiteSpace();
    syncShareFromUi();

    m_currentFsHealth = FsPermissions::check(path, m_share);

    if (path.isEmpty()) {
        m_fsIconLabel->setText("<qt><font color='gray'>[i]</font></qt>");
        m_fsDetailLabel->setText("<qt>Please select a folder to share.</qt>");
        m_fixPermBtn->hide();
        return;
    }

    if (m_currentFsHealth.status == FsHealth::StatusOk) {
        m_fsIconLabel->setText("<qt><font color='green'><b>[OK]</b></font></qt>");
        m_fsDetailLabel->setText(TQString("<qt><font color='green'><b>%1</b></font><br>%2</qt>")
                                     .arg(m_currentFsHealth.summary)
                                     .arg(m_currentFsHealth.detail));
        m_fixPermBtn->hide();
    } else if (m_currentFsHealth.status == FsHealth::StatusWarning) {
        m_fsIconLabel->setText("<qt><font color='#b45309'><b>[!]</b></font></qt>");
        m_fsDetailLabel->setText(TQString("<qt><font color='#b45309'><b>%1</b></font><br>%2</qt>")
                                     .arg(m_currentFsHealth.summary)
                                     .arg(m_currentFsHealth.detail));
        m_fixPermBtn->setText(TQString("Change to %1").arg(FsPermissions::formatMode(m_currentFsHealth.suggestedMode, true)));
        m_fixPermBtn->show();
    } else {
        m_fsIconLabel->setText("<qt><font color='red'><b>[X]</b></font></qt>");
        m_fsDetailLabel->setText(TQString("<qt><font color='red'><b>%1</b></font><br>%2</qt>")
                                     .arg(m_currentFsHealth.summary)
                                     .arg(m_currentFsHealth.detail));
        m_fixPermBtn->hide();
    }
}

void ShareDialog::slotFixPermissions()
{
    TQString path = m_pathEdit->text().stripWhiteSpace();
    if (path.isEmpty()) return;

    int res = TQMessageBox::question(
        this, "Adjust Unix Permissions",
        TQString("Do you want to change permissions of '%1' to %2 ?\n\n"
                 "This allows authorized network users and guests to access the folder.")
            .arg(path).arg(FsPermissions::formatMode(m_currentFsHealth.suggestedMode, true)),
        "Apply to folder only", "Apply recursively", "Cancel", 0, 2);

    if (res == 2) return; // Cancel

    bool recursive = (res == 1);
    TQString err;
    if (!FsPermissions::applyMode(path, m_currentFsHealth.suggestedMode, recursive, &err)) {
        TQMessageBox::critical(this, "Error",
                               TQString("Failed to adjust permissions: %1").arg(err));
    } else {
        updateFsStatus();
    }
}

void ShareDialog::slotOk()
{
    syncShareFromUi();

    if (m_share.name.isEmpty()) {
        TQMessageBox::warning(this, "Required Field", "Please enter a share name.");
        m_nameEdit->setFocus();
        return;
    }

    if (m_share.path.isEmpty() || !TQDir(m_share.path).exists()) {
        TQMessageBox::warning(this, "Invalid Folder", "The specified folder path does not exist or is not a directory.");
        m_pathEdit->setFocus();
        return;
    }

    // Check duplicate share name when creating a new share
    if (!m_isEditMode && m_backend) {
        TQValueList<ShareInfo> shares = m_backend->listShares();
        for (TQValueList<ShareInfo>::ConstIterator it = shares.begin(); it != shares.end(); ++it) {
            if ((*it).name.lower() == m_share.name.lower()) {
                TQMessageBox::warning(
                    this, "Share Name Already Exists",
                    TQString("A network share named '%1' already exists.\nPlease choose a different share name.")
                        .arg(m_share.name));
                m_nameEdit->setFocus();
                return;
            }
        }
    }

    if (m_currentFsHealth.status == FsHealth::StatusWarning) {
        int choice = TQMessageBox::warning(
            this, "Filesystem Permission Warning",
            TQString("%1\n\nDo you want to save this share anyway without fixing permissions?")
                .arg(m_currentFsHealth.detail),
            "Save Anyway", "Fix Permissions First", "Cancel", 0, 1);
        if (choice == 1) {
            slotFixPermissions();
            return;
        } else if (choice == 2) {
            return;
        }
    }

    TQString err;
    if (!m_backend->addOrUpdateShare(m_share, &err)) {
        TQMessageBox::critical(this, "Samba Error",
                               TQString("Failed to create/update share via 'net usershare':\n\n%1").arg(err));
        return;
    }

    accept();
}
