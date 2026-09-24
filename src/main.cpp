#include <tdeapplication.h>
#include <tdeaboutdata.h>
#include <tdecmdlineargs.h>
#include <kurl.h>
#include <ntqfileinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <tqapplication.h>
#include <tqmessagebox.h>

#include "version.h"
#include "share_backend.h"
#include "tdeshare_window.h"
#include "diag_dialog.h"
#include "share_dialog.h"



static TDECmdLineOptions options[] = {
    { "diag", "Directly open system diagnostics dialog", 0 },
    { "new-share", "Directly open new share dialog", 0 },
    { "folder <path>", "Path or URL of a folder to share", 0 },
    { "+[folder]", "Path or URL of a folder to share", 0 },
    TDECmdLineLastOption
};

int main(int argc, char** argv)
{
    // Allow command-line help / version queries even as root without GUI popup
    bool isHelpOrVersion = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0 ||
            strcmp(argv[i], "--help-all") == 0 || strcmp(argv[i], "--help-tde") == 0 ||
            strcmp(argv[i], "--help-qt") == 0 || strcmp(argv[i], "--version") == 0 ||
            strcmp(argv[i], "-v") == 0) {
            isHelpOrVersion = true;
            break;
        }
    }

    if (!isHelpOrVersion && (getuid() == 0 || geteuid() == 0)) {
        // Intercept root BEFORE TDEApplication initializes.
        // TDEApplication's constructor triggers Trinity's internal
        // "Running as root - configuration will not be saved" warning.
        // By handling this via raw TQt3, only our clean dialog appears.
        fprintf(stderr, "Error: Running tdeshare as root is not permitted.\n");

        if (getenv("DISPLAY") && strlen(getenv("DISPLAY")) > 0) {
            TQApplication rawApp(argc, argv, true);
            TQString rootMsg =
                "Running TDE Samba Shares as root is not permitted.\n\n"
                "This application is designed for regular desktop users to manage "
                "their user network shares safely without administrator privileges.\n\n"
                "Running as root risks corrupting user profile file permissions "
                "and locking usershare definitions.\n\n"
                "How to proceed:\n"
                "- Launch this application from your standard user account (member of the 'sambashare' group).\n"
                "- For permanent system-wide shares, configure /etc/samba/smb.conf directly or use the command line.";

            TQMessageBox::critical(NULL, "Root Execution Not Allowed", rootMsg);
        }
        return 1;
    }

    TDEAboutData aboutData("tdeshare", "TDE Samba Share Manager", TDESHARE_VERSION,
                           "Samba Network Shares Manager for Trinity Desktop (TDE)",
                           TDEAboutData::License_GPL);
    TDECmdLineArgs::init(argc, argv, &aboutData);
    TDECmdLineArgs::addCmdLineOptions(options);

    TDEApplication app;

    ShareBackend backend;

    TDECmdLineArgs* args = TDECmdLineArgs::parsedArgs();
    bool openDiag = args->isSet("diag");
    bool openNewOnly = args->isSet("new-share");

    TQString folderArg;
    if (args->isSet("folder")) {
        folderArg = TQString::fromLocal8Bit(args->getOption("folder"));
    } else if (args->count() > 0) {
        KURL url = args->url(0);
        if (url.isLocalFile()) {
            folderArg = url.path();
        } else {
            folderArg = TQString::fromLocal8Bit(args->arg(0));
        }
    }

    if (openDiag) {
        DiagDialog dlg(&backend);
        return dlg.exec();
    }

    if (!folderArg.isEmpty()) {
        if (folderArg.startsWith("file://")) {
            KURL url(folderArg);
            if (url.isLocalFile()) {
                folderArg = url.path();
            } else {
                folderArg = folderArg.mid(7);
            }
        } else if (folderArg.startsWith("file:")) {
            folderArg = folderArg.mid(5);
        }

        TQFileInfo fi(folderArg);
        if (fi.exists() && fi.isDir()) {
            folderArg = fi.absFilePath();
        } else {
            fprintf(stderr, "Warning: '%s' is not a valid directory.\n",
                    folderArg.local8Bit().data());
            folderArg = TQString::null;
        }
    }

    if (!folderArg.isEmpty()) {
        TQString cleanFolder = folderArg.stripWhiteSpace();
        while (cleanFolder.length() > 1 && (cleanFolder.endsWith("/") || cleanFolder.endsWith("\\"))) {
            cleanFolder.truncate(cleanFolder.length() - 1);
        }

        ShareInfo existing;
        bool alreadyShared = false;
        TQValueList<ShareInfo> shares = backend.listShares();
        for (TQValueList<ShareInfo>::ConstIterator it = shares.begin(); it != shares.end(); ++it) {
            TQString p = (*it).path.stripWhiteSpace();
            while (p.length() > 1 && (p.endsWith("/") || p.endsWith("\\"))) {
                p.truncate(p.length() - 1);
            }
            if (p == cleanFolder) {
                existing = *it;
                alreadyShared = true;
                break;
            }
        }

        ShareDialog dlg(&backend);
        if (alreadyShared) {
            dlg.setForEditShare(existing);
        } else {
            dlg.setForNewShare(cleanFolder);
        }

        int res = dlg.exec();
        if (res == TQDialog::Accepted) {
            TDEShareWindow mainWindow(&backend);
            app.setMainWidget(&mainWindow);
            if (!mainWindow.selectShareByName(dlg.resultShare().name)) {
                mainWindow.selectShareByPath(cleanFolder);
            }
            mainWindow.show();
            return app.exec();
        } else {
            return 0;
        }
    }

    if (openNewOnly) {
        ShareDialog dlg(&backend);
        dlg.setForNewShare();
        int res = dlg.exec();
        if (res == TQDialog::Accepted) {
            TDEShareWindow mainWindow(&backend);
            app.setMainWidget(&mainWindow);
            mainWindow.selectShareByName(dlg.resultShare().name);
            mainWindow.show();
            return app.exec();
        } else {
            return 0;
        }
    }

    TDEShareWindow mainWindow(&backend);
    app.setMainWidget(&mainWindow);
    mainWindow.show();

    return app.exec();
}
