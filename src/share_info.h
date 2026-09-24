#ifndef SHARE_INFO_H
#define SHARE_INFO_H

#include <ntqstring.h>
#include <ntqstringlist.h>
#include <ntqvaluelist.h>

struct UserAcl {
    enum Permission {
        FullAccess = 0, // "F"
        ReadOnly   = 1, // "R"
        Deny       = 2  // "D"
    };

    TQString user;
    Permission perm;

    UserAcl() : user("Everyone"), perm(ReadOnly) {}
    UserAcl(const TQString& u, Permission p) : user(u), perm(p) {}

    TQString permChar() const {
        switch (perm) {
            case FullAccess: return "F";
            case ReadOnly:   return "R";
            case Deny:       return "D";
        }
        return "R";
    }

    static Permission fromChar(char c) {
        if (c == 'F' || c == 'f') return FullAccess;
        if (c == 'D' || c == 'd') return Deny;
        return ReadOnly;
    }
};

class ShareInfo {
public:
    TQString name;
    TQString path;
    TQString comment;
    bool guestOk;
    TQValueList<UserAcl> acls;

    ShareInfo();

    // Serialize ACLs to Samba format (e.g. "Everyone:R,alice:F")
    TQString aclString() const;

    // Parse Samba ACL string into ACL list
    void parseAclString(const TQString& str);

    // Helpers to check ACL mode
    bool isEveryoneReadOnly() const;
    bool isEveryoneFull() const;

    // Human-readable summary of permissions
    TQString accessSummary() const;
};

#endif // SHARE_INFO_H
