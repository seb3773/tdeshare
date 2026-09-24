#include "add_user_dialog.h"
#include "new_user_dialog.h"

#include <ntqlayout.h>
#include <ntqlabel.h>
#include <kiconloader.h>

AddUserDialog::AddUserDialog(ShareBackend* backend, const TQStringList& currentUsers,
                             TQWidget* parent, const char* name)
    : TQDialog(parent, name, true),
      m_backend(backend),
      m_currentUsers(currentUsers)
{
    setCaption("Add User to Share");
    setIcon(SmallIcon("network"));

    TQVBoxLayout* mainLayout = new TQVBoxLayout(this, 12, 10);

    TQLabel* infoLabel = new TQLabel("Select a user to grant share access:", this);
    mainLayout->addWidget(infoLabel);

    TQHBoxLayout* userRow = new TQHBoxLayout(mainLayout, 8);
    TQLabel* userLbl = new TQLabel("User:", this);
    m_userCombo = new TQComboBox(false, this);
    userRow->addWidget(userLbl);
    userRow->addWidget(m_userCombo, 1);

    populateUsers();

    // Bottom action buttons
    TQHBoxLayout* btnLayout = new TQHBoxLayout(mainLayout, 8);
    btnLayout->addStretch(1);
    m_cancelBtn = new TQPushButton("Cancel", this);
    m_cancelBtn->setMinimumWidth(80);
    m_addBtn = new TQPushButton("Add user to share access", this);
    m_addBtn->setMinimumWidth(160);
    m_addBtn->setDefault(true);
    btnLayout->addWidget(m_cancelBtn);
    btnLayout->addWidget(m_addBtn);

    // Prevent clipping on resize
    mainLayout->setResizeMode(TQLayout::Minimum);
    adjustSize();
    TQSize minSz = sizeHint().expandedTo(TQSize(380, 130));
    resize(minSz);
    setMinimumSize(minSz);

    connect(m_addBtn, SIGNAL(clicked()), this, SLOT(slotAdd()));
    connect(m_cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));
    connect(m_userCombo, SIGNAL(activated(int)), this, SLOT(slotUserActivated(int)));
}

AddUserDialog::~AddUserDialog()
{
}

static bool userListContains(const TQStringList& list, const TQString& str)
{
    for (TQStringList::ConstIterator it = list.begin(); it != list.end(); ++it) {
        if ((*it).lower() == str.lower()) {
            return true;
        }
    }
    return false;
}

void AddUserDialog::populateUsers()
{
    m_userCombo->clear();

    // Check if Everyone can be added
    if (!userListContains(m_currentUsers, "Everyone")) {
        m_userCombo->insertItem("Everyone");
    }

    if (m_backend) {
        TQStringList localUsers = m_backend->getLocalUsers();
        for (TQStringList::ConstIterator it = localUsers.begin(); it != localUsers.end(); ++it) {
            if (!userListContains(m_currentUsers, *it)) {
                m_userCombo->insertItem(*it);
            }
        }
    }

    // Special entry at the end to create a new network Samba user
    m_userCombo->insertItem("Add a new network user...");
}

void AddUserDialog::slotUserActivated(int index)
{
    // The last item is "Add a new network user..."
    if (index >= 0 && index == m_userCombo->count() - 1) {
        // Open NewUserDialog with AddUserDialog as parent (do NOT hide AddUserDialog)
        NewUserDialog newDlg(m_backend, this);
        if (newDlg.exec() == TQDialog::Accepted) {
            m_selectedUser = newDlg.createdUsername();
            if (!m_selectedUser.isEmpty()) {
                accept();
                return;
            }
        }
        // Cancelled: reset combo back to first item and remain on AddUserDialog
        m_userCombo->setCurrentItem(0);
    }
}

void AddUserDialog::slotAdd()
{
    int index = m_userCombo->currentItem();
    if (index >= 0 && index == m_userCombo->count() - 1) {
        slotUserActivated(index);
        return;
    }

    m_selectedUser = m_userCombo->currentText();
    if (m_selectedUser.isEmpty()) {
        reject();
        return;
    }

    accept();
}
