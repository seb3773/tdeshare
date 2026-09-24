#ifndef TDESHARE_WINDOW_H
#define TDESHARE_WINDOW_H

#include <ntqmainwindow.h>
#include <ntqlistview.h>
#include <ntqpushbutton.h>
#include <ntqlabel.h>
#include <ntqpopupmenu.h>
#include <ntqvaluelist.h>
#include <ntqpixmap.h>
#include "share_backend.h"
#include "share_info.h"

class TDEShareWindow : public TQMainWindow {
    TQ_OBJECT

public:
    TDEShareWindow(ShareBackend* backend, TQWidget* parent = 0, const char* name = 0);
    virtual ~TDEShareWindow();
 
    bool selectShareByName(const TQString& name);
    bool selectShareByPath(const TQString& path);

public slots:
    void refreshShares();
    void slotNewShare();
    void slotEditShare();
    void slotDeleteShare();
    void slotGlobalOptions();
    void slotShowDiagnostics();

private slots:
    void slotItemDoubleClicked(TQListViewItem* item);
    void slotContextMenu(TQListViewItem* item, const TQPoint& pos, int col);
    void slotSelectionChanged();
    void slotOpenInKonqueror();
    void slotCopySmbUrl();
    void slotFixSelectedPermissions();

private:
    void setupUi();
    void updateStatus(bool listSharesOk = true);
    ShareInfo* getSelectedShare();

    ShareBackend* m_backend;
    TQValueList<ShareInfo> m_shares;

    // Actions & Buttons
    TQPushButton* m_newBtn;
    TQPushButton* m_editBtn;
    TQPushButton* m_deleteBtn;
    TQPushButton* m_refreshBtn;
    TQPushButton* m_globalBtn;
    TQPushButton* m_diagBtn;

    // Main view
    TQListView* m_listView;

    // Status bar labels
    TQLabel* m_statusCountLabel;
    TQLabel* m_statusSmbdLabel;
    TQLabel* m_statusGroupLabel;
};

#endif // TDESHARE_WINDOW_H
