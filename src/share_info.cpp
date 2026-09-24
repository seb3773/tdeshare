#include "share_info.h"

ShareInfo::ShareInfo()
    : guestOk(false)
{
    acls.append(UserAcl("Everyone", UserAcl::ReadOnly));
}

TQString ShareInfo::aclString() const
{
    if (acls.isEmpty()) {
        return "Everyone:R";
    }

    TQStringList parts;
    for (TQValueList<UserAcl>::ConstIterator it = acls.begin(); it != acls.end(); ++it) {
        if (!(*it).user.isEmpty()) {
            parts.append((*it).user + ":" + (*it).permChar());
        }
    }

    if (parts.isEmpty()) {
        return "Everyone:R";
    }

    return parts.join(",");
}

void ShareInfo::parseAclString(const TQString& str)
{
    acls.clear();
    TQString trimmed = str.stripWhiteSpace();
    if (trimmed.isEmpty()) {
        acls.append(UserAcl("Everyone", UserAcl::ReadOnly));
        return;
    }

    TQStringList items = TQStringList::split(',', trimmed);
    for (TQStringList::Iterator it = items.begin(); it != items.end(); ++it) {
        TQString entry = (*it).stripWhiteSpace();
        int colon = entry.find(':');
        if (colon != -1) {
            TQString user = entry.left(colon).stripWhiteSpace();
            TQString p = entry.mid(colon + 1).stripWhiteSpace();
            char permChar = p.isEmpty() ? 'R' : p[0].latin1();

            // Strip domain or hostname prefix (e.g. "DOMAIN\user" or "Unix User\user")
            int slash = user.find('\\');
            if (slash != -1) {
                user = user.mid(slash + 1);
            }

            if (!user.isEmpty()) {
                acls.append(UserAcl(user, UserAcl::fromChar(permChar)));
            }
        }
    }

    if (acls.isEmpty()) {
        acls.append(UserAcl("Everyone", UserAcl::ReadOnly));
    }
}

bool ShareInfo::isEveryoneReadOnly() const
{
    if (acls.count() == 1 && acls.first().user.lower() == "everyone" && acls.first().perm == UserAcl::ReadOnly) {
        return true;
    }
    return false;
}

bool ShareInfo::isEveryoneFull() const
{
    if (acls.count() == 1 && acls.first().user.lower() == "everyone" && acls.first().perm == UserAcl::FullAccess) {
        return true;
    }
    return false;
}


TQString ShareInfo::accessSummary() const
{
    if (isEveryoneReadOnly()) {
        return "Everyone (Read Only)";
    }
    if (isEveryoneFull()) {
        return "Everyone (Read & Write)";
    }

    TQStringList summary;
    for (TQValueList<UserAcl>::ConstIterator it = acls.begin(); it != acls.end(); ++it) {
        TQString u = (*it).user;
        TQString p = ((*it).perm == UserAcl::FullAccess) ? "RW" : (((*it).perm == UserAcl::ReadOnly) ? "R" : "Deny");
        summary.append(u + " (" + p + ")");
    }
    return summary.join(", ");
}
