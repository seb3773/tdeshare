#ifndef FS_PERMISSIONS_H
#define FS_PERMISSIONS_H

#include <ntqstring.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "share_info.h"

struct FsHealth {
    enum Status {
        StatusOk = 0,
        StatusWarning = 1,
        StatusError = 2
    };

    Status status;
    bool exists;
    bool isDirectory;
    mode_t currentMode;
    TQString ownerName;
    TQString groupName;
    TQString modeString; // e.g. "rwxr-xr-x (0755)"
    TQString summary;
    TQString detail;
    mode_t suggestedMode;

    FsHealth()
        : status(StatusOk), exists(false), isDirectory(false),
          currentMode(0), suggestedMode(0755)
    {}

    bool isOk() const { return status == StatusOk; }
};

class FsPermissions {
public:
    // Analyze filesystem permissions against intended Samba share settings
    static FsHealth check(const TQString& path, const ShareInfo& share);

    // Apply safe chmod to folder
    static bool applyMode(const TQString& path, mode_t mode, bool recursive = false, TQString* errorMsg = NULL);

    // Convert mode_t to string representation (e.g. "drwxr-xr-x (0755)")
    static TQString formatMode(mode_t mode, bool isDir);

    // Check if path is traversable by others from root down to folder
    static bool checkTraversal(const TQString& path, TQString* failingParent = NULL);
};

#endif // FS_PERMISSIONS_H
