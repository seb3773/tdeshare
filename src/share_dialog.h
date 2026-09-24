#ifndef SHARE_DIALOG_H
#define SHARE_DIALOG_H

#include <ntqdialog.h>
#include <ntqstring.h>
#include <ntqlineedit.h>
#include <ntqpushbutton.h>
#include <ntqcombobox.h>
#include <ntqcheckbox.h>
#include <ntqradiobutton.h>
#include <ntqbuttongroup.h>
#include <ntqtable.h>
#include <ntqlabel.h>
#include <ntqgroupbox.h>
#include "share_info.h"
#include "share_backend.h"
#include "fs_permissions.h"

class ShareDialog : public TQDialog {
    TQ_OBJECT

public:
    ShareDialog(ShareBackend* backend, TQWidget* parent = 0, const char* name = 0);
    virtual ~ShareDialog();

    // Setup for adding a new share, optionally pre-filling the directory path
    void setForNewShare(const TQString& prefillPath = TQString::null);

    // Setup for editing an existing share
    void setForEditShare(const ShareInfo& share);

    ShareInfo resultShare() const { return m_share; }

private slots:
    void slotBrowse();
    void slotPathChanged(const TQString& text);
    void slotAccessModeChanged(int id);
    void slotGuestOkToggled(bool checked);
    void slotAddUser();
    void slotRemoveUser();
    void slotTableSelectionChanged(int row, int col);
    void slotTablePermChanged();
    void slotFixPermissions();
    void slotOk();

private:
    void updateFsStatus();
    void refreshAclTable();
    void addUserToTable(const TQString& username, UserAcl::Permission perm);
    void syncUiFromShare();
    void syncShareFromUi();
    void syncShareFromTable();
    bool isNetworkUser(const TQString& username) const;

    ShareBackend* m_backend;
    ShareInfo m_share;
    bool m_isEditMode;
    bool m_initializing;
    TQString m_lastAutoName;
    FsHealth m_currentFsHealth;

    // UI Widgets
    TQLineEdit* m_nameEdit;
    TQLineEdit* m_pathEdit;
    TQPushButton* m_browseBtn;
    TQLineEdit* m_commentEdit;

    TQButtonGroup* m_modeGroup;
    TQRadioButton* m_radioEveryoneRo;
    TQRadioButton* m_radioEveryoneRw;
    TQRadioButton* m_radioCustomAcl;

    TQGroupBox* m_aclContainer;
    TQTable* m_aclTable;
    TQPushButton* m_addUserBtn;
    TQPushButton* m_removeUserBtn;

    TQCheckBox* m_guestOkCheck;

    TQGroupBox* m_fsGroupBox;
    TQLabel* m_fsIconLabel;
    TQLabel* m_fsDetailLabel;
    TQPushButton* m_fixPermBtn;

    TQPushButton* m_okBtn;
    TQPushButton* m_cancelBtn;
};

#endif // SHARE_DIALOG_H
