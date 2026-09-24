#include "new_user_dialog.h"

#include <ntqlayout.h>
#include <ntqgroupbox.h>
#include <ntqmessagebox.h>
#include <kiconloader.h>

NewUserDialog::NewUserDialog(ShareBackend* backend, TQWidget* parent, const char* name)
    : TQDialog(parent, name, true),
      m_backend(backend)
{
    setCaption("Create Network User");
    setIcon(SmallIcon("network"));

    TQVBoxLayout* mainLayout = new TQVBoxLayout(this, 12, 10);

    // --- User Credentials Group ---
    TQGroupBox* credGroup = new TQGroupBox("User Credentials", this);
    credGroup->setColumnLayout(0, TQt::Vertical);
    credGroup->layout()->setSpacing(6);
    credGroup->layout()->setMargin(10);

    TQGridLayout* grid = new TQGridLayout(credGroup->layout(), 3, 2);
    grid->setSpacing(8);

    m_usernameEdit = new TQLineEdit(credGroup);
    m_passwordEdit = new TQLineEdit(credGroup);
    m_passwordEdit->setEchoMode(TQLineEdit::Password);
    m_confirmEdit = new TQLineEdit(credGroup);
    m_confirmEdit->setEchoMode(TQLineEdit::Password);

    grid->addWidget(new TQLabel("Username:", credGroup), 0, 0);
    grid->addWidget(m_usernameEdit, 0, 1);
    grid->addWidget(new TQLabel("Password:", credGroup), 1, 0);
    grid->addWidget(m_passwordEdit, 1, 1);
    grid->addWidget(new TQLabel("Confirm password:", credGroup), 2, 0);
    grid->addWidget(m_confirmEdit, 2, 1);

    mainLayout->addWidget(credGroup);

    // --- Informational Note Box ---
    TQGroupBox* infoBox = new TQGroupBox("Account Details", this);
    infoBox->setColumnLayout(0, TQt::Vertical);
    infoBox->layout()->setSpacing(6);
    infoBox->layout()->setMargin(10);

    TQVBoxLayout* infoLayout = new TQVBoxLayout(infoBox->layout());
    TQLabel* infoLabel = new TQLabel(
        "<qt><font color='gray'><b>[i] Information:</b></font><br>"
        "This will create a dedicated system account with no home directory and its "
        "login shell set to <code>/usr/sbin/nologin</code>, then register it in Samba.<br>"
        "This user will be able to authenticate and access shared network folders, "
        "but will <b>not</b> be able to log in to the Trinity desktop or console.</qt>",
        infoBox);
    infoLayout->addWidget(infoLabel);

    mainLayout->addWidget(infoBox);

    // --- Bottom Buttons ---
    TQHBoxLayout* btnLayout = new TQHBoxLayout(mainLayout, 8);
    btnLayout->addStretch(1);
    m_cancelBtn = new TQPushButton("Cancel", this);
    m_cancelBtn->setMinimumWidth(80);
    m_createBtn = new TQPushButton("Create User", this);
    m_createBtn->setMinimumWidth(110);
    m_createBtn->setDefault(true);
    btnLayout->addWidget(m_cancelBtn);
    btnLayout->addWidget(m_createBtn);

    // Enforce size constraints so frames and buttons never get clipped
    credGroup->setMinimumSize(credGroup->sizeHint());
    infoBox->setMinimumSize(infoBox->sizeHint());
    mainLayout->setResizeMode(TQLayout::Minimum);
    adjustSize();
    TQSize minSz = sizeHint().expandedTo(TQSize(460, 410));
    resize(minSz);
    setMinimumSize(minSz);

    connect(m_createBtn, SIGNAL(clicked()), this, SLOT(slotCreate()));
    connect(m_cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));
}

NewUserDialog::~NewUserDialog()
{
}

void NewUserDialog::slotCreate()
{
    TQString user = m_usernameEdit->text().stripWhiteSpace();
    TQString pass = m_passwordEdit->text();
    TQString confirm = m_confirmEdit->text();

    if (user.isEmpty()) {
        TQMessageBox::warning(this, "Missing Username", "Please enter a username.");
        m_usernameEdit->setFocus();
        return;
    }

    // Basic username validation: lowercase alphanumeric, -, _
    for (unsigned int i = 0; i < user.length(); ++i) {
        char c = user[i].latin1();
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) {
            TQMessageBox::warning(this, "Invalid Username",
                                  "Usernames may only contain lowercase letters, numbers, hyphens, and underscores.");
            m_usernameEdit->setFocus();
            return;
        }
    }

    if (pass.isEmpty()) {
        TQMessageBox::warning(this, "Missing Password", "Please enter a password for this user.");
        m_passwordEdit->setFocus();
        return;
    }

    if (pass != confirm) {
        TQMessageBox::warning(this, "Password Mismatch", "Passwords do not match. Please re-enter them.");
        m_confirmEdit->setFocus();
        return;
    }

    TQString err;
    if (!m_backend->createSambaUser(user, pass, &err)) {
        TQMessageBox::critical(this, "Error Creating User",
                               TQString("Failed to create network user:\n%1").arg(err));
        return;
    }

    m_createdUsername = user;
    TQMessageBox::information(this, "User Created",
                              TQString("User '%1' has been created and registered with Samba.").arg(user));
    accept();
}
