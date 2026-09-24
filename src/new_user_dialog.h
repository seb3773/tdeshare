#ifndef NEW_USER_DIALOG_H
#define NEW_USER_DIALOG_H

#include <ntqdialog.h>
#include <ntqlineedit.h>
#include <ntqpushbutton.h>
#include <ntqlabel.h>
#include "share_backend.h"

class NewUserDialog : public TQDialog {
    TQ_OBJECT

public:
    NewUserDialog(ShareBackend* backend, TQWidget* parent = 0, const char* name = 0);
    virtual ~NewUserDialog();

    TQString createdUsername() const { return m_createdUsername; }

private slots:
    void slotCreate();

private:
    ShareBackend* m_backend;
    TQString m_createdUsername;

    TQLineEdit* m_usernameEdit;
    TQLineEdit* m_passwordEdit;
    TQLineEdit* m_confirmEdit;

    TQPushButton* m_createBtn;
    TQPushButton* m_cancelBtn;
};

#endif // NEW_USER_DIALOG_H
