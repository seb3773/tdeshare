#ifndef SHARE_BACKEND_H
#define SHARE_BACKEND_H

#include <ntqstring.h>
#include <ntqstringlist.h>
#include <ntqvaluelist.h>
#include "share_info.h"

class ShareBackend {
public:
    ShareBackend();
    virtual ~ShareBackend();

    // Process execution helper (fork/pipe/execve, no shell expansion)
    static bool executeCommand(const TQString& command, const TQStringList& args,
                               TQString& stdOut, TQString& stdErr, int* exitCode = NULL);

    // Asynchronously launch a GUI application or command detached from parent
    static bool launchDetached(const TQString& command, const TQStringList& args);

    // Samba Environment Diagnostics
    enum ServiceState {
        ServiceRunning,
        ServicePending,
        ServiceStopped
    };

    enum UsershareState {
        UsershareReady,
        UsersharePending,
        UsershareSetupRequired
    };

    bool isSambaInstalled() const;
    ServiceState getSmbdState() const;
    UsershareState getUsershareState(bool lastListSharesOk = true) const;
    bool isUserInSambashareGroup() const;
    bool isUsershareDirectoryReady() const;
    TQString usershareDirectoryPath() const;
    int usershareMaxShares() const;

    // Samba Shares CRUD
    TQValueList<ShareInfo> listShares(TQString* errorMsg = NULL);
    bool addOrUpdateShare(const ShareInfo& share, TQString* errorMsg = NULL);
    bool deleteShare(const TQString& shareName, TQString* errorMsg = NULL);

    // Local & Samba Users
    TQStringList getLocalUsers() const;
    bool createSambaUser(const TQString& username, const TQString& password, TQString* errorMsg = NULL);

    // Remediation Actions (run via tdesudo)
    bool joinSambashareGroup(TQString* errorMsg = NULL);
    bool startSmbdService(TQString* errorMsg = NULL);
    bool enableUsershareInSmbConf(TQString* errorMsg = NULL);

    // Utility: get local hostname for smb:// URL
    TQString getHostName() const;

    // Samba Global Options
    struct GlobalOptions {
        TQString workgroup;
        int maxDiskSizeMb;          // 0 = unlimited / disabled
        bool enableRecycleBin;      // vfs objects = recycle
        bool hideDotFiles;          // hide dot files = yes
        bool customMasks;           // create mask = 0664, directory mask = 0775
        bool allowNonOwnedFolders;  // usershare owner only = false
    };

    bool readGlobalOptions(GlobalOptions& opts, TQString* errorMsg = NULL);
    bool writeGlobalOptions(const GlobalOptions& opts, TQString* errorMsg = NULL);

private:
    bool executeNetUsershare(const TQStringList& netArgs, TQString& stdOut, TQString& stdErr, int* exitCode);
    TQValueList<ShareInfo> parseUsershareInfoOutput(const TQString& output);
    static TQString findExecutable(const TQString& name);
    static bool launchPrivileged(const TQString& cmd, const TQStringList& args,
                                 const TQString& comment = TQString::null,
                                 const TQString& caption = TQString::null,
                                 TQString* errorMsg = NULL);
};

#endif // SHARE_BACKEND_H
