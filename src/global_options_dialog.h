#ifndef GLOBAL_OPTIONS_DIALOG_H
#define GLOBAL_OPTIONS_DIALOG_H

#include <ntqdialog.h>
#include <ntqlineedit.h>
#include <ntqcheckbox.h>
#include <ntqspinbox.h>
#include <ntqpushbutton.h>
#include "share_backend.h"

class GlobalOptionsDialog : public TQDialog {
    TQ_OBJECT

public:
    GlobalOptionsDialog(ShareBackend* backend, TQWidget* parent = 0, const char* name = 0);
    virtual ~GlobalOptionsDialog();

private slots:
    void slotApply();
    void slotMaxDiskToggled(bool checked);

private:
    void loadCurrentOptions();

    ShareBackend* m_backend;

    TQLineEdit* m_workgroupEdit;
    TQCheckBox* m_maxDiskCheck;
    TQSpinBox* m_maxDiskSpin;

    TQCheckBox* m_recycleBinCheck;
    TQCheckBox* m_hideDotFilesCheck;
    TQCheckBox* m_customMasksCheck;
    TQCheckBox* m_ownerOnlyCheck;

    TQPushButton* m_applyBtn;
    TQPushButton* m_cancelBtn;
};

#endif // GLOBAL_OPTIONS_DIALOG_H
