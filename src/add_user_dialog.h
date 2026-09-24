#ifndef ADD_USER_DIALOG_H
#define ADD_USER_DIALOG_H

#include <ntqdialog.h>
#include <ntqcombobox.h>
#include <ntqpushbutton.h>
#include <ntqstring.h>
#include <ntqstringlist.h>
#include "share_backend.h"

class AddUserDialog : public TQDialog {
    TQ_OBJECT

public:
    AddUserDialog(ShareBackend* backend, const TQStringList& currentUsers,
                  TQWidget* parent = 0, const char* name = 0);
    virtual ~AddUserDialog();

    TQString selectedUser() const { return m_selectedUser; }

private slots:
    void slotAdd();
    void slotUserActivated(int index);

private:
    void populateUsers();

    ShareBackend* m_backend;
    TQStringList m_currentUsers;
    TQString m_selectedUser;

    TQComboBox* m_userCombo;
    TQPushButton* m_addBtn;
    TQPushButton* m_cancelBtn;
};

#endif // ADD_USER_DIALOG_H
