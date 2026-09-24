#include "fs_permissions.h"
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>

TQString FsPermissions::formatMode(mode_t mode, bool isDir)
{
    char buf[12];
    buf[0] = isDir ? 'd' : '-';
    buf[1] = (mode & S_IRUSR) ? 'r' : '-';
    buf[2] = (mode & S_IWUSR) ? 'w' : '-';
    buf[3] = (mode & S_IXUSR) ? 'x' : '-';
    buf[4] = (mode & S_IRGRP) ? 'r' : '-';
    buf[5] = (mode & S_IWGRP) ? 'w' : '-';
    buf[6] = (mode & S_IXGRP) ? 'x' : '-';
    buf[7] = (mode & S_IROTH) ? 'r' : '-';
    buf[8] = (mode & S_IWOTH) ? 'w' : '-';
    buf[9] = (mode & S_IXOTH) ? 'x' : '-';
    buf[10] = '\0';

    char fullStr[64];
    snprintf(fullStr, sizeof(fullStr), "%s (%04o)", buf, (unsigned int)(mode & 07777));
    return TQString::fromLatin1(fullStr);
}

bool FsPermissions::checkTraversal(const TQString& path, TQString* failingParent)
{
    if (path.isEmpty() || path[0] != '/') {
        return true;
    }

    TQString cur = path;
    while (cur.length() > 1 && cur.endsWith("/")) {
        cur.truncate(cur.length() - 1);
    }

    while (cur.length() > 1) {
        int idx = cur.findRev('/');
        if (idx <= 0) break;
        cur = cur.left(idx);

        struct stat st;
        if (stat(cur.local8Bit(), &st) != 0) {
            if (failingParent) *failingParent = cur;
            return false;
        }

        // Must have +x for others
        if (!(st.st_mode & S_IXOTH)) {
            if (failingParent) *failingParent = cur;
            return false;
        }
    }

    return true;
}

FsHealth FsPermissions::check(const TQString& path, const ShareInfo& share)
{
    FsHealth h;
    if (path.isEmpty()) {
        h.status = FsHealth::StatusError;
        h.summary = "Empty path";
        h.detail = "No folder path specified for this share.";
        return h;
    }

    struct stat st;
    if (stat(path.local8Bit(), &st) != 0) {
        h.status = FsHealth::StatusError;
        h.exists = false;
        h.summary = "Folder not found";
        h.detail = TQString("Cannot access path '%1': %2")
                       .arg(path).arg(strerror(errno));
        return h;
    }

    h.exists = true;
    h.isDirectory = S_ISDIR(st.st_mode);
    h.currentMode = st.st_mode;

    struct passwd* pw = getpwuid(st.st_uid);
    h.ownerName = pw ? TQString::fromLocal8Bit(pw->pw_name) : TQString::number(st.st_uid);

    struct group* gr = getgrgid(st.st_gid);
    h.groupName = gr ? TQString::fromLocal8Bit(gr->gr_name) : TQString::number(st.st_gid);

    h.modeString = formatMode(st.st_mode, h.isDirectory);

    if (!h.isDirectory) {
        h.status = FsHealth::StatusError;
        h.summary = "Not a directory";
        h.detail = "The selected path is a file, not a directory.";
        return h;
    }

    // Check parent traversal
    TQString failingParent;
    if (!checkTraversal(path, &failingParent)) {
        h.status = FsHealth::StatusWarning;
        h.summary = "Path traversal blocked by parent directory";
        h.detail = TQString("Parent directory '%1' lacks execute (+x) permissions for other users. Samba will not be able to navigate into this folder.")
                       .arg(failingParent);
        h.suggestedMode = 0755;
        return h;
    }

    // Determine what access level is requested
    bool wantsWrite = false;
    for (TQValueList<UserAcl>::ConstIterator it = share.acls.begin(); it != share.acls.end(); ++it) {
        if ((*it).perm == UserAcl::FullAccess) {
            wantsWrite = true;
            break;
        }
    }

    bool isPublicOrGuest = share.guestOk || share.isEveryoneReadOnly() || share.isEveryoneFull();

    // Check if others can read/traverse folder
    bool othersCanRead = (st.st_mode & S_IROTH) && (st.st_mode & S_IXOTH);
    bool othersCanWrite = (st.st_mode & S_IWOTH);

    if (isPublicOrGuest) {
        if (!othersCanRead) {
            h.status = FsHealth::StatusWarning;
            h.summary = "Restrictive Unix permissions";
            h.detail = TQString("Folder permissions are %1. Network users will get 'Access Denied' because filesystem permissions do not allow read/execute access for others.")
                           .arg(h.modeString);
            h.suggestedMode = wantsWrite ? 0777 : 0755;
            return h;
        }

        if (wantsWrite && !othersCanWrite) {
            h.status = FsHealth::StatusWarning;
            h.summary = "Write access not allowed on filesystem";
            h.detail = TQString("Samba share allows write access, but Unix permissions are %1 (only owner/group can write). Remote network users will not be able to create or modify files.")
                           .arg(h.modeString);
            h.suggestedMode = 0777;
            return h;
        }
    } else {
        // User-specific share: at least group or others traversal needed if different users
        if (!(st.st_mode & S_IXGRP) && !(st.st_mode & S_IXOTH)) {
            h.status = FsHealth::StatusWarning;
            h.summary = "Private folder (owner only)";
            h.detail = TQString("Folder is in mode %1. Only user '%2' has access in Linux. Other Samba users will likely be blocked.")
                           .arg(h.modeString).arg(h.ownerName);
            h.suggestedMode = wantsWrite ? 0775 : 0755;
            return h;
        }
    }

    h.status = FsHealth::StatusOk;
    h.summary = "Permissions OK";
    h.detail = TQString("Unix permissions (%1) are compatible with this Samba share configuration.")
                   .arg(h.modeString);
    return h;
}

static bool applyChmodRecursive(const TQString& path, mode_t dirMode, mode_t fileMode)
{
    if (chmod(path.local8Bit(), dirMode) != 0) {
        return false;
    }

    DIR* dir = opendir(path.local8Bit());
    if (!dir) return false;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        TQString childPath = path;
        if (!childPath.endsWith("/")) childPath += "/";
        childPath += TQString::fromLocal8Bit(entry->d_name);

        struct stat st;
        if (lstat(childPath.local8Bit(), &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                applyChmodRecursive(childPath, dirMode, fileMode);
            } else if (S_ISREG(st.st_mode)) {
                chmod(childPath.local8Bit(), fileMode);
            }
        }
    }
    closedir(dir);
    return true;
}

bool FsPermissions::applyMode(const TQString& path, mode_t mode, bool recursive, TQString* errorMsg)
{
    if (path.isEmpty()) {
        if (errorMsg) *errorMsg = "Empty path";
        return false;
    }

    if (recursive) {
        mode_t fileMode = (mode & 0666);
        if (mode & 0111) {
            fileMode |= (mode & 0111);
        }
        if (!applyChmodRecursive(path, mode, fileMode)) {
            if (errorMsg) *errorMsg = TQString::fromLocal8Bit(strerror(errno));
            return false;
        }
        return true;
    }

    if (chmod(path.local8Bit(), mode) != 0) {
        if (errorMsg) *errorMsg = TQString::fromLocal8Bit(strerror(errno));
        return false;
    }

    return true;
}
