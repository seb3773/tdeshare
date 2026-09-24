#include "share_backend.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <poll.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ntqfile.h>
#include <ntqtextstream.h>

ShareBackend::ShareBackend()
{
}

ShareBackend::~ShareBackend()
{
}

TQString ShareBackend::findExecutable(const TQString& name)
{
    if (name.startsWith("/")) {
        if (access(name.local8Bit(), X_OK) == 0) return name;
        return TQString::null;
    }

    const char* pathEnv = getenv("PATH");
    if (!pathEnv) pathEnv = "/bin:/usr/bin:/usr/local/bin:/opt/trinity/bin";

    TQStringList dirs = TQStringList::split(':', TQString::fromLatin1(pathEnv));
    if (dirs.findIndex("/opt/trinity/bin") == -1) {
        dirs.append("/opt/trinity/bin");
    }
    for (TQStringList::Iterator it = dirs.begin(); it != dirs.end(); ++it) {
        TQString candidate = (*it) + "/" + name;
        if (access(candidate.local8Bit(), X_OK) == 0) {
            return candidate;
        }
    }
    return TQString::null;
}

bool ShareBackend::executeCommand(const TQString& command, const TQStringList& args,
                                 TQString& stdOut, TQString& stdErr, int* exitCode)
{
    stdOut = TQString::null;
    stdErr = TQString::null;
    if (exitCode) *exitCode = -1;

    TQString exe = findExecutable(command);
    if (exe.isEmpty()) {
        stdErr = TQString("Command '%1' not found in PATH.").arg(command);
        return false;
    }

    int pipeOut[2];
    int pipeErr[2];
    if (pipe(pipeOut) != 0 || pipe(pipeErr) != 0) {
        stdErr = "Failed to create IPC pipes.";
        return false;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(pipeOut[0]); close(pipeOut[1]);
        close(pipeErr[0]); close(pipeErr[1]);
        stdErr = "Fork failed.";
        return false;
    }

    if (pid == 0) {
        // Child process
        close(pipeOut[0]);
        close(pipeErr[0]);

        int devNull = open("/dev/null", O_RDONLY);
        if (devNull >= 0) {
            dup2(devNull, STDIN_FILENO);
            close(devNull);
        }

        dup2(pipeOut[1], STDOUT_FILENO);
        dup2(pipeErr[1], STDERR_FILENO);

        close(pipeOut[1]);
        close(pipeErr[1]);

        int argc = args.count() + 1;
        char** argv = (char**)malloc((argc + 1) * sizeof(char*));
        argv[0] = strdup(exe.local8Bit());
        int i = 1;
        for (TQStringList::ConstIterator it = args.begin(); it != args.end(); ++it, ++i) {
            argv[i] = strdup((*it).local8Bit());
        }
        argv[argc] = NULL;

        execv(exe.local8Bit(), argv);
        _exit(127);
    }

    // Parent process
    close(pipeOut[1]);
    close(pipeErr[1]);

    fcntl(pipeOut[0], F_SETFL, O_NONBLOCK);
    fcntl(pipeErr[0], F_SETFL, O_NONBLOCK);

    struct pollfd fds[2];
    fds[0].fd = pipeOut[0];
    fds[0].events = POLLIN;
    fds[1].fd = pipeErr[0];
    fds[1].events = POLLIN;

    TQCString outBuf;
    TQCString errBuf;
    char buffer[4096];

    bool outClosed = false;
    bool errClosed = false;

    int pollTimeoutCount = 0;
    while (!outClosed || !errClosed) {
        int ret = poll(fds, 2, 1000);
        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }

        if (ret == 0) {
            pollTimeoutCount++;
            int statusCheck = 0;
            pid_t wp = waitpid(pid, &statusCheck, WNOHANG);
            if (wp == pid) {
                // Child has already exited
                break;
            }
            if (pollTimeoutCount >= 10) { // 10s maximum timeout protection
                kill(pid, SIGTERM);
                break;
            }
            continue;
        } else {
            pollTimeoutCount = 0;
        }

        if (fds[0].revents & POLLIN) {
            ssize_t n = read(pipeOut[0], buffer, sizeof(buffer));
            if (n > 0) {
                int oldLen = outBuf.size();
                outBuf.resize(oldLen + n);
                memcpy(outBuf.data() + oldLen, buffer, n);
            } else if (n == 0) {
                outClosed = true;
                fds[0].fd = -1;
            }
        }
        if (fds[0].revents & (POLLHUP | POLLERR)) {
            ssize_t n;
            while ((n = read(pipeOut[0], buffer, sizeof(buffer))) > 0) {
                int oldLen = outBuf.size();
                outBuf.resize(oldLen + n);
                memcpy(outBuf.data() + oldLen, buffer, n);
            }
            outClosed = true;
            fds[0].fd = -1;
        }

        if (fds[1].revents & POLLIN) {
            ssize_t n = read(pipeErr[0], buffer, sizeof(buffer));
            if (n > 0) {
                int oldLen = errBuf.size();
                errBuf.resize(oldLen + n);
                memcpy(errBuf.data() + oldLen, buffer, n);
            } else if (n == 0) {
                errClosed = true;
                fds[1].fd = -1;
            }
        }
        if (fds[1].revents & (POLLHUP | POLLERR)) {
            ssize_t n;
            while ((n = read(pipeErr[0], buffer, sizeof(buffer))) > 0) {
                int oldLen = errBuf.size();
                errBuf.resize(oldLen + n);
                memcpy(errBuf.data() + oldLen, buffer, n);
            }
            errClosed = true;
            fds[1].fd = -1;
        }
    }

    close(pipeOut[0]);
    close(pipeErr[0]);

    int status = 0;
    waitpid(pid, &status, 0);

    stdOut = TQString::fromLocal8Bit(outBuf);
    stdErr = TQString::fromLocal8Bit(errBuf);

    int code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    if (exitCode) *exitCode = code;

    return (code == 0);
}

bool ShareBackend::launchDetached(const TQString& command, const TQStringList& args)
{
    TQString exe = findExecutable(command);
    if (exe.isEmpty()) return false;

    pid_t pid = fork();
    if (pid < 0) return false;

    if (pid == 0) {
        // Double fork technique to decouple completely from parent
        pid_t grandChild = fork();
        if (grandChild < 0) _exit(1);
        if (grandChild > 0) {
            _exit(0); // Intermediate child terminates immediately
        }

        // Grandchild: independent session
        setsid();

        // Redirect standard I/O to /dev/null
        int devNull = open("/dev/null", O_RDWR);
        if (devNull >= 0) {
            dup2(devNull, STDIN_FILENO);
            dup2(devNull, STDOUT_FILENO);
            dup2(devNull, STDERR_FILENO);
            if (devNull > STDERR_FILENO) close(devNull);
        }

        // Close any open file descriptors
        for (int fd = 3; fd < 256; ++fd) {
            close(fd);
        }

        int argc = args.count() + 1;
        char** argv = (char**)malloc((argc + 1) * sizeof(char*));
        argv[0] = strdup(exe.local8Bit());
        int i = 1;
        for (TQStringList::ConstIterator it = args.begin(); it != args.end(); ++it, ++i) {
            argv[i] = strdup((*it).local8Bit());
        }
        argv[argc] = NULL;

        execv(exe.local8Bit(), argv);
        _exit(127);
    }

    // Parent waits for intermediate child (exits immediately)
    int status = 0;
    waitpid(pid, &status, 0);
    return true;
}

bool ShareBackend::isSambaInstalled() const
{
    return !findExecutable("net").isEmpty();
}

ShareBackend::ServiceState ShareBackend::getSmbdState() const
{
    TQString out, err;
    int code = -1;
    TQStringList args;
    args << "is-active" << "smbd";
    if (executeCommand("systemctl", args, out, err, &code)) {
        TQString state = out.stripWhiteSpace().lower();
        if (state == "active") {
            return ServiceRunning;
        }
        if (state == "reloading" || state == "activating") {
            return ServicePending;
        }
    }

    if (access("/run/samba/smbd.pid", F_OK) == 0) {
        return ServiceRunning;
    }

    args.clear();
    args << "-x" << "smbd";
    if (executeCommand("pgrep", args, out, err, &code) && code == 0) {
        return ServiceRunning;
    }

    return ServiceStopped;
}

ShareBackend::UsershareState ShareBackend::getUsershareState(bool lastListSharesOk) const
{
    if (lastListSharesOk) {
        return UsershareReady;
    }

    ServiceState sState = getSmbdState();
    if (sState == ServicePending) {
        return UsersharePending;
    }

    if (!isUserInSambashareGroup() || !isUsershareDirectoryReady()) {
        return UsershareSetupRequired;
    }

    return UsershareReady;
}

bool ShareBackend::isUserInSambashareGroup() const
{
    uid_t uid = getuid();
    struct passwd* pw = getpwuid(uid);
    TQString currentUsername;
    gid_t userPrimaryGid = (gid_t)-1;
    if (pw) {
        currentUsername = TQString::fromLocal8Bit(pw->pw_name);
        userPrimaryGid = pw->pw_gid;
    }

    int numGroups = getgroups(0, NULL);
    gid_t* gids = NULL;
    if (numGroups > 0) {
        gids = (gid_t*)malloc(numGroups * sizeof(gid_t));
        if (getgroups(numGroups, gids) != numGroups) {
            free(gids);
            gids = NULL;
            numGroups = 0;
        }
    }

    setgrent();
    struct group* gr = getgrnam("sambashare");
    if (!gr) {
        endgrent();
        if (gids) free(gids);
        return false;
    }

    gid_t sambaGid = gr->gr_gid;

    if (userPrimaryGid == sambaGid) {
        endgrent();
        if (gids) free(gids);
        return true;
    }

    if (gids) {
        for (int i = 0; i < numGroups; ++i) {
            if (gids[i] == sambaGid) {
                free(gids);
                endgrent();
                return true;
            }
        }
        free(gids);
        gids = NULL;
    }

    if (!currentUsername.isEmpty() && gr->gr_mem) {
        for (char** mem = gr->gr_mem; *mem != NULL; ++mem) {
            if (currentUsername == TQString::fromLocal8Bit(*mem)) {
                endgrent();
                return true;
            }
        }
    }

    endgrent();
    return false;
}

bool ShareBackend::isUsershareDirectoryReady() const
{
    TQString p = usershareDirectoryPath();
    if (p.isEmpty()) p = "/var/lib/samba/usershares";

    struct stat st;
    if (stat(p.local8Bit(), &st) != 0) {
        return false;
    }
    if (!S_ISDIR(st.st_mode)) {
        return false;
    }
    if (access(p.local8Bit(), W_OK | R_OK) == 0) {
        return true;
    }
    if (isUserInSambashareGroup() && (st.st_mode & S_IWGRP)) {
        return true;
    }
    return false;
}

TQString ShareBackend::usershareDirectoryPath() const
{
    // Fast path: standard Linux path check without subprocess overhead
    struct stat st;
    if (stat("/var/lib/samba/usershares", &st) == 0 && S_ISDIR(st.st_mode)) {
        return "/var/lib/samba/usershares";
    }

    TQString out, err;
    int code = -1;
    TQStringList args;
    args << "-s" << "--parameter-name=usershare path";
    if (executeCommand("testparm", args, out, err, &code) && code == 0) {
        TQString trimmed = out.stripWhiteSpace();
        if (!trimmed.isEmpty()) return trimmed;
    }
    return "/var/lib/samba/usershares";
}

int ShareBackend::usershareMaxShares() const
{
    TQString out, err;
    int code = -1;
    TQStringList args;
    args << "-s" << "--parameter-name=usershare max shares";
    if (executeCommand("testparm", args, out, err, &code) && code == 0) {
        bool ok = false;
        int val = out.stripWhiteSpace().toInt(&ok);
        if (ok) return val;
    }
    return 100;
}

TQValueList<ShareInfo> ShareBackend::parseUsershareInfoOutput(const TQString& output)
{
    TQValueList<ShareInfo> shares;
    TQStringList lines = TQStringList::split('\n', output);

    ShareInfo current;
    bool inSection = false;

    for (TQStringList::ConstIterator it = lines.begin(); it != lines.end(); ++it) {
        TQString line = (*it).stripWhiteSpace();
        if (line.isEmpty() || line.startsWith("#")) continue;

        if (line.startsWith("[") && line.endsWith("]")) {
            if (inSection && !current.name.isEmpty()) {
                shares.append(current);
            }
            current = ShareInfo();
            current.name = line.mid(1, line.length() - 2).stripWhiteSpace();
            inSection = true;
            continue;
        }

        if (!inSection) continue;

        int eq = line.find('=');
        if (eq == -1) continue;

        TQString key = line.left(eq).stripWhiteSpace().lower();
        TQString val = line.mid(eq + 1).stripWhiteSpace();

        if (key == "path") {
            current.path = val;
        } else if (key == "comment") {
            current.comment = val;
        } else if (key == "usershare_acl") {
            current.parseAclString(val);
        } else if (key == "guest_ok") {
            current.guestOk = (val.lower() == "y" || val.lower() == "yes" || val == "1");
        }
    }

    if (inSection && !current.name.isEmpty()) {
        shares.append(current);
    }

    return shares;
}

bool ShareBackend::executeNetUsershare(const TQStringList& netArgs, TQString& stdOut, TQString& stdErr, int* exitCode)
{
    stdOut = TQString::null;
    stdErr = TQString::null;
    if (exitCode) *exitCode = -1;

    TQString dir = usershareDirectoryPath();
    if (dir.isEmpty()) dir = "/var/lib/samba/usershares";

    // 1. Direct execution if usershares directory is writable in current session
    if (access(dir.local8Bit(), W_OK | R_OK) == 0) {
        if (executeCommand("net", netArgs, stdOut, stdErr, exitCode) && (*exitCode == 0)) {
            return true;
        }
    }

    // 2. Execution via 'sg sambashare' (activates sambashare group instantly without password)
    TQString sgExe = findExecutable("sg");
    if (!sgExe.isEmpty() && isUserInSambashareGroup()) {
        TQString netExe = findExecutable("net");
        if (netExe.isEmpty()) netExe = "net";

        TQString fullCmd = "\"" + netExe + "\"";
        for (TQStringList::ConstIterator it = netArgs.begin(); it != netArgs.end(); ++it) {
            TQString a = *it;
            a.replace("\"", "\\\"");
            fullCmd += " \"" + a + "\"";
        }

        TQStringList sgArgs;
        sgArgs << "sambashare" << "-c" << fullCmd;

        int sgCode = -1;
        TQString sgOut, sgErr;
        bool ok = executeCommand("sg", sgArgs, sgOut, sgErr, &sgCode);
        if (ok && sgCode == 0) {
            stdOut = sgOut;
            stdErr = sgErr;
            if (exitCode) *exitCode = 0;
            return true;
        } else {
            stdOut = sgOut;
            stdErr = sgErr;
            if (exitCode) *exitCode = sgCode;
        }
    }

    // 3. If direct execution was not tried yet, try it now
    if (access(dir.local8Bit(), W_OK | R_OK) != 0) {
        int dirCode = -1;
        TQString dirOut, dirErr;
        if (executeCommand("net", netArgs, dirOut, dirErr, &dirCode) && dirCode == 0) {
            stdOut = dirOut;
            stdErr = dirErr;
            if (exitCode) *exitCode = 0;
            return true;
        } else {
            if (stdErr.isEmpty()) stdErr = dirErr;
            if (stdOut.isEmpty()) stdOut = dirOut;
            if (exitCode) *exitCode = dirCode;
        }
    }

    // 4. Privileged fallback via tdesudo if permission error occurs or not in group
    bool isPermErr = stdErr.contains("Permission non accordée", false) ||
                     stdErr.contains("Permission denied", false) ||
                     stdErr.contains("cannot open usershare directory", false) ||
                     stdErr.contains("permission to create a usershare", false) ||
                     !isUserInSambashareGroup();

    if (isPermErr) {
        uid_t uid = getuid();
        struct passwd* pw = getpwuid(uid);
        TQString currentUser = pw ? TQString::fromLocal8Bit(pw->pw_name) : TQString("cdef");

        char scriptTemplate[] = "/tmp/.smbnet_XXXXXX";
        int sfd = mkstemp(scriptTemplate);
        if (sfd >= 0) {
            fchmod(sfd, 0700);
            const char* scriptContent =
                "#!/bin/sh\n"
                "USER=\"$1\"\n"
                "DIR=\"$2\"\n"
                "shift 2\n"
                "getent group sambashare >/dev/null || groupadd -r sambashare\n"
                "if ! id -nG \"$USER\" 2>/dev/null | grep -qw sambashare; then\n"
                "    usermod -aG sambashare \"$USER\"\n"
                "fi\n"
                "mkdir -p \"$DIR\"\n"
                "chgrp sambashare \"$DIR\"\n"
                "chmod 1770 \"$DIR\"\n"
                "if command -v sg >/dev/null 2>&1; then\n"
                "    exec sg sambashare -c \"net $*\"\n"
                "else\n"
                "    exec su -s /bin/sh \"$USER\" -c \"net $*\"\n"
                "fi\n";
            write(sfd, scriptContent, strlen(scriptContent));
            close(sfd);

            TQStringList privArgs;
            privArgs << scriptTemplate << currentUser << dir;
            for (TQStringList::ConstIterator it = netArgs.begin(); it != netArgs.end(); ++it) {
                privArgs << *it;
            }

            TQString privErr;
            bool privOk = launchPrivileged(
                "/bin/sh", privArgs,
                "Administrator authorization is required to configure Samba share permissions.",
                "Samba Share Privileges",
                &privErr);

            unlink(scriptTemplate);

            if (privOk) {
                if (exitCode) *exitCode = 0;
                stdErr = TQString::null;
                return true;
            } else {
                stdErr = privErr;
                return false;
            }
        }
    }

    return false;
}

TQValueList<ShareInfo> ShareBackend::listShares(TQString* errorMsg)
{
    TQString out, err;
    int code = -1;
    TQStringList args;
    args << "usershare" << "info";

    bool ok = executeNetUsershare(args, out, err, &code);
    if (!ok && code != 0) {
        if (errorMsg) {
            *errorMsg = err.stripWhiteSpace();
            if (errorMsg->isEmpty()) *errorMsg = out.stripWhiteSpace();
        }
        return TQValueList<ShareInfo>();
    }

    return parseUsershareInfoOutput(out);
}

bool ShareBackend::addOrUpdateShare(const ShareInfo& share, TQString* errorMsg)
{
    if (share.name.isEmpty()) {
        if (errorMsg) *errorMsg = "Share name cannot be empty.";
        return false;
    }
    if (share.path.isEmpty()) {
        if (errorMsg) *errorMsg = "Folder path cannot be empty.";
        return false;
    }

    TQString out, err;
    int code = -1;
    TQStringList args;
    args << "usershare" << "add";
    args << share.name;
    args << share.path;
    args << share.comment;
    args << share.aclString();
    args << TQString("guest_ok=%1").arg(share.guestOk ? "y" : "n");

    bool ok = executeNetUsershare(args, out, err, &code);
    if (!ok || code != 0) {
        if (errorMsg) {
            *errorMsg = err.stripWhiteSpace();
            if (errorMsg->isEmpty()) *errorMsg = out.stripWhiteSpace();
        }
        return false;
    }

    return true;
}

bool ShareBackend::deleteShare(const TQString& shareName, TQString* errorMsg)
{
    if (shareName.isEmpty()) return true;

    TQString out, err;
    int code = -1;
    TQStringList args;
    args << "usershare" << "delete" << shareName;

    bool ok = executeNetUsershare(args, out, err, &code);
    if (!ok || code != 0) {
        if (errorMsg) {
            *errorMsg = err.stripWhiteSpace();
            if (errorMsg->isEmpty()) *errorMsg = out.stripWhiteSpace();
        }
        return false;
    }

    return true;
}

TQStringList ShareBackend::getLocalUsers() const
{
    TQStringList users;
    setpwent();
    struct passwd* pw;
    while ((pw = getpwent()) != NULL) {
        if (pw->pw_uid >= 1000 && pw->pw_uid != 65534) {
            TQString u = TQString::fromLocal8Bit(pw->pw_name);
            if (!users.contains(u)) {
                users.append(u);
            }
        }
    }
    endpwent();
    users.sort();
    return users;
}

bool ShareBackend::launchPrivileged(const TQString& cmd, const TQStringList& args,
                                   const TQString& comment, const TQString& caption,
                                   TQString* errorMsg)
{
    // Native Trinity privilege elevation dialog: strictly tdesudo
    TQString tool = findExecutable("tdesudo");

    if (!tool.isEmpty()) {
        TQString fullCmd = cmd;
        for (TQStringList::ConstIterator it = args.begin(); it != args.end(); ++it) {
            fullCmd += " \"" + *it + "\"";
        }

        TQStringList toolArgs;
        if (!caption.isEmpty()) {
            toolArgs << "--caption" << caption;
        }
        toolArgs << "--icon" << "dialog-password";
        toolArgs << "-i" << "network";
        toolArgs << "-d";  // Do not show technical command line in dialog

        if (!comment.isEmpty()) {
            toolArgs << "--comment" << comment;
        }

        toolArgs << "-c" << fullCmd;

        TQString out, err;
        int code = -1;
        bool ok = executeCommand(tool, toolArgs, out, err, &code);
        if (!ok || code != 0) {
            if (errorMsg) {
                if (!err.isEmpty()) *errorMsg = err.stripWhiteSpace();
                else *errorMsg = TQString("Action cancelled or privilege elevation failed (exit code %1).").arg(code);
            }
            return false;
        }
        return true;
    }

    if (errorMsg) *errorMsg = "Trinity elevation tool 'tdesudo' not found in /opt/trinity/bin or PATH.";
    return false;
}

bool ShareBackend::createSambaUser(const TQString& username, const TQString& password, TQString* errorMsg)
{
    if (username.isEmpty()) {
        if (errorMsg) *errorMsg = "Username cannot be empty.";
        return false;
    }

    // 1. Write password to a secure temporary file (mode 0600)
    char passTemplate[] = "/tmp/.smbpass_XXXXXX";
    int pfd = mkstemp(passTemplate);
    if (pfd < 0) {
        if (errorMsg) *errorMsg = "Failed to create secure temporary password file.";
        return false;
    }
    fchmod(pfd, 0600);
    TQCString passBytes = password.local8Bit();
    write(pfd, passBytes.data(), passBytes.size());
    close(pfd);

    // 2. Create helper script in /tmp (mode 0700)
    char scriptTemplate[] = "/tmp/.smbscript_XXXXXX";
    int sfd = mkstemp(scriptTemplate);
    if (sfd < 0) {
        unlink(passTemplate);
        if (errorMsg) *errorMsg = "Failed to create temporary script file.";
        return false;
    }
    fchmod(sfd, 0700);

    const char* scriptContent =
        "#!/bin/sh\n"
        "set -e\n"
        "USER=\"$1\"\n"
        "PASSFILE=\"$2\"\n"
        "if ! id \"$USER\" >/dev/null 2>&1; then\n"
        "    useradd -M -s /usr/sbin/nologin -c \"Samba Network User\" \"$USER\"\n"
        "fi\n"
        "if [ -f \"$PASSFILE\" ]; then\n"
        "    (cat \"$PASSFILE\"; echo \"\"; cat \"$PASSFILE\") | smbpasswd -a -s \"$USER\"\n"
        "    rm -f \"$PASSFILE\"\n"
        "fi\n";

    write(sfd, scriptContent, strlen(scriptContent));
    close(sfd);

    // 3. Launch privileged script with clean English comment and no command line displayed
    TQStringList args;
    args << scriptTemplate << username << passTemplate;

    bool ok = launchPrivileged(
        "/bin/sh", args,
        "Administrator authorization is required to create a network Samba user.",
        "Create Network User",
        errorMsg);

    // 4. Cleanup
    unlink(passTemplate);
    unlink(scriptTemplate);

    return ok;
}

bool ShareBackend::joinSambashareGroup(TQString* errorMsg)
{
    uid_t uid = getuid();
    struct passwd* pw = getpwuid(uid);
    if (!pw) {
        if (errorMsg) *errorMsg = "Unable to determine current user.";
        return false;
    }

    TQStringList args;
    args << "-aG" << "sambashare" << TQString::fromLocal8Bit(pw->pw_name);
    return launchPrivileged(
        "usermod", args,
        "Administrator authorization is required to join the sambashare group.",
        "Configure Samba Permissions",
        errorMsg);
}

bool ShareBackend::startSmbdService(TQString* errorMsg)
{
    TQStringList args;
    args << "start" << "smbd";
    return launchPrivileged(
        "systemctl", args,
        "Administrator authorization is required to start the Samba service.",
        "Start Samba Service",
        errorMsg);
}

bool ShareBackend::enableUsershareInSmbConf(TQString* errorMsg)
{
    (void)errorMsg;
    return false;
}

TQString ShareBackend::getHostName() const
{
    char host[256] = {0};
    if (gethostname(host, sizeof(host) - 1) == 0) {
        return TQString::fromLocal8Bit(host);
    }
    return "localhost";
}

bool ShareBackend::readGlobalOptions(GlobalOptions& opts, TQString* errorMsg)
{
    (void)errorMsg;
    opts.workgroup = "WORKGROUP";
    opts.maxDiskSizeMb = 0;
    opts.enableRecycleBin = false;
    opts.hideDotFiles = true;
    opts.customMasks = false;
    opts.allowNonOwnedFolders = false;

    TQFile file("/etc/samba/smb.conf");
    if (!file.open(IO_ReadOnly)) {
        return true;
    }

    TQTextStream ts(&file);
    TQString content = ts.read();
    file.close();

    TQStringList lines = TQStringList::split('\n', content, true);
    TQString currentSection = "";

    for (TQStringList::Iterator it = lines.begin(); it != lines.end(); ++it) {
        TQString line = (*it).stripWhiteSpace();
        if (line.startsWith("[") && line.endsWith("]")) {
            currentSection = line.mid(1, line.length() - 2).lower();
            continue;
        }

        if (line.startsWith("#") || line.startsWith(";")) continue;

        int eqPos = line.find('=');
        if (eqPos == -1) continue;

        TQString key = line.left(eqPos).stripWhiteSpace().lower();
        TQString val = line.mid(eqPos + 1).stripWhiteSpace();

        if (currentSection == "global") {
            if (key == "workgroup") {
                opts.workgroup = val;
            } else if (key == "max disk size") {
                opts.maxDiskSizeMb = val.toInt();
            } else if (key == "usershare owner only") {
                opts.allowNonOwnedFolders = (val.lower() == "no" || val.lower() == "false" || val == "0");
            }
        } else if (currentSection == "usershare_template") {
            if (key == "vfs objects" && val.find("recycle") != -1) {
                opts.enableRecycleBin = true;
            } else if (key == "hide dot files") {
                opts.hideDotFiles = (val.lower() == "yes" || val.lower() == "true" || val == "1");
            } else if (key == "create mask" && val == "0664") {
                opts.customMasks = true;
            }
        }
    }
    return true;
}

bool ShareBackend::writeGlobalOptions(const GlobalOptions& opts, TQString* errorMsg)
{
    TQFile file("/etc/samba/smb.conf");
    if (!file.open(IO_ReadOnly)) {
        if (errorMsg) *errorMsg = "Cannot read /etc/samba/smb.conf.";
        return false;
    }
    TQTextStream inStream(&file);
    TQString original = inStream.read();
    file.close();

    TQStringList lines = TQStringList::split('\n', original, true);
    TQStringList output;

    TQString currentSection = "";
    bool inTdeTemplate = false;
    bool foundWorkgroup = false;
    bool foundMaxDiskSize = false;
    bool foundTemplateShare = false;
    bool foundOwnerOnly = false;

    for (TQStringList::Iterator it = lines.begin(); it != lines.end(); ++it) {
        TQString rawLine = *it;
        TQString trimmed = rawLine.stripWhiteSpace();

        if (trimmed == "### BEGIN TDESHARE USERSHARE TEMPLATE ###") {
            inTdeTemplate = true;
            continue;
        }
        if (trimmed == "### END TDESHARE USERSHARE TEMPLATE ###") {
            inTdeTemplate = false;
            continue;
        }
        if (inTdeTemplate) {
            continue;
        }

        if (trimmed.startsWith("[") && trimmed.endsWith("]")) {
            if (currentSection == "global") {
                if (!foundWorkgroup && !opts.workgroup.isEmpty()) {
                    output.append("   workgroup = " + opts.workgroup);
                    foundWorkgroup = true;
                }
                if (!foundMaxDiskSize && opts.maxDiskSizeMb > 0) {
                    output.append(TQString("   max disk size = %1").arg(opts.maxDiskSizeMb));
                    foundMaxDiskSize = true;
                }
                if (!foundTemplateShare) {
                    output.append("   usershare template share = usershare_template");
                    foundTemplateShare = true;
                }
                if (!foundOwnerOnly) {
                    output.append(TQString("   usershare owner only = %1").arg(opts.allowNonOwnedFolders ? "no" : "yes"));
                    foundOwnerOnly = true;
                }
            }
            currentSection = trimmed.mid(1, trimmed.length() - 2).lower();
            if (currentSection == "usershare_template") {
                inTdeTemplate = true;
                continue;
            }
            output.append(rawLine);
            continue;
        }

        if (currentSection == "global" && !trimmed.startsWith("#") && !trimmed.startsWith(";")) {
            int eqPos = trimmed.find('=');
            if (eqPos != -1) {
                TQString key = trimmed.left(eqPos).stripWhiteSpace().lower();
                if (key == "workgroup") {
                    output.append("   workgroup = " + opts.workgroup);
                    foundWorkgroup = true;
                    continue;
                } else if (key == "max disk size") {
                    if (opts.maxDiskSizeMb > 0) {
                        output.append(TQString("   max disk size = %1").arg(opts.maxDiskSizeMb));
                        foundMaxDiskSize = true;
                    }
                    continue;
                } else if (key == "usershare template share") {
                    output.append("   usershare template share = usershare_template");
                    foundTemplateShare = true;
                    continue;
                } else if (key == "usershare owner only") {
                    output.append(TQString("   usershare owner only = %1").arg(opts.allowNonOwnedFolders ? "no" : "yes"));
                    foundOwnerOnly = true;
                    continue;
                }
            }
        }

        output.append(rawLine);
    }

    if (currentSection == "global") {
        if (!foundWorkgroup && !opts.workgroup.isEmpty()) {
            output.append("   workgroup = " + opts.workgroup);
        }
        if (!foundMaxDiskSize && opts.maxDiskSizeMb > 0) {
            output.append(TQString("   max disk size = %1").arg(opts.maxDiskSizeMb));
        }
        if (!foundTemplateShare) {
            output.append("   usershare template share = usershare_template");
        }
        if (!foundOwnerOnly) {
            output.append(TQString("   usershare owner only = %1").arg(opts.allowNonOwnedFolders ? "no" : "yes"));
        }
    }

    // Append managed [usershare_template] section
    output.append("");
    output.append("### BEGIN TDESHARE USERSHARE TEMPLATE ###");
    output.append("[usershare_template]");
    output.append("   -valid = no");
    output.append("   comment = Usershare Template managed by tdeshare");
    if (opts.enableRecycleBin) {
        output.append("   vfs objects = recycle");
        output.append("   recycle:repository = .recycle");
        output.append("   recycle:keeptree = yes");
        output.append("   recycle:versions = yes");
    }
    if (opts.hideDotFiles) {
        output.append("   hide dot files = yes");
    } else {
        output.append("   hide dot files = no");
    }
    if (opts.customMasks) {
        output.append("   create mask = 0664");
        output.append("   directory mask = 0775");
        output.append("   force create mode = 0664");
        output.append("   force directory mode = 0775");
    }
    output.append("### END TDESHARE USERSHARE TEMPLATE ###");
    output.append("");

    TQString finalContent = output.join("\n");

    // Write to secure temporary config file
    char confTemplate[] = "/tmp/.smbconf_XXXXXX";
    int cfd = mkstemp(confTemplate);
    if (cfd < 0) {
        if (errorMsg) *errorMsg = "Failed to create temporary configuration file.";
        return false;
    }
    fchmod(cfd, 0644);
    TQCString bytes = finalContent.local8Bit();
    write(cfd, bytes.data(), bytes.size());
    close(cfd);

    // Validate with testparm
    TQString testOut, testErr;
    int testCode = -1;
    TQStringList testArgs;
    testArgs << "-s" << confTemplate;
    bool testOk = executeCommand("testparm", testArgs, testOut, testErr, &testCode);
    if (!testOk || testCode != 0) {
        unlink(confTemplate);
        if (errorMsg) {
            *errorMsg = "Configuration validation failed (testparm error): " + testErr.stripWhiteSpace();
        }
        return false;
    }

    // Helper script to install config and reload smbd
    char scriptTemplate[] = "/tmp/.smbapply_XXXXXX";
    int sfd = mkstemp(scriptTemplate);
    if (sfd < 0) {
        unlink(confTemplate);
        if (errorMsg) *errorMsg = "Failed to create temporary installation script.";
        return false;
    }
    fchmod(sfd, 0700);

    const char* scriptContent =
        "#!/bin/sh\n"
        "set -e\n"
        "CONF=\"$1\"\n"
        "[ -f \"$CONF\" ] || exit 1\n"
        "cp /etc/samba/smb.conf /etc/samba/smb.conf.tdeshare.bak 2>/dev/null || true\n"
        "cp \"$CONF\" /etc/samba/smb.conf\n"
        "chmod 0644 /etc/samba/smb.conf\n"
        "rm -f \"$CONF\"\n"
        "if systemctl is-active --quiet smbd 2>/dev/null; then\n"
        "    systemctl reload smbd 2>/dev/null || systemctl restart smbd 2>/dev/null || true\n"
        "fi\n";

    write(sfd, scriptContent, strlen(scriptContent));
    close(sfd);

    TQStringList args;
    args << scriptTemplate << confTemplate;

    bool ok = launchPrivileged(
        "/bin/sh", args,
        "Administrator authorization is required to save Samba global configuration.",
        "Configure Samba Global Options",
        errorMsg);

    unlink(confTemplate);
    unlink(scriptTemplate);

    return ok;
}
