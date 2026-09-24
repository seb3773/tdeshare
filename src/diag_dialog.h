#ifndef DIAG_DIALOG_H
#define DIAG_DIALOG_H

#include <ntqdialog.h>
#include <ntqlabel.h>
#include <ntqpushbutton.h>
#include "share_backend.h"

class DiagDialog : public TQDialog {
    TQ_OBJECT

public:
    DiagDialog(ShareBackend* backend, TQWidget* parent = 0, const char* name = 0);
    virtual ~DiagDialog();

public slots:
    void refreshDiagnostics();

private slots:
    void slotJoinGroup();
    void slotStartSmbd();

private:
    ShareBackend* m_backend;

    TQLabel* m_sambaBinIcon;
    TQLabel* m_sambaBinText;

    TQLabel* m_smbdIcon;
    TQLabel* m_smbdText;
    TQPushButton* m_startSmbdBtn;

    TQLabel* m_groupIcon;
    TQLabel* m_groupText;
    TQPushButton* m_joinGroupBtn;

    TQLabel* m_dirIcon;
    TQLabel* m_dirText;

    TQLabel* m_configText;

    TQPushButton* m_refreshBtn;
    TQPushButton* m_closeBtn;
};

#endif // DIAG_DIALOG_H
