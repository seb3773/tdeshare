#include "global_options_dialog.h"

#include <ntqlayout.h>
#include <ntqgroupbox.h>
#include <ntqlabel.h>
#include <ntqmessagebox.h>
#include <kiconloader.h>

GlobalOptionsDialog::GlobalOptionsDialog(ShareBackend* backend, TQWidget* parent, const char* name)
    : TQDialog(parent, name, true),
      m_backend(backend)
{
    setCaption("Samba Global Options");
    setIcon(SmallIcon("configure"));

    TQVBoxLayout* mainLayout = new TQVBoxLayout(this, 12, 10);

    // --- Group 1: Server & Network Identity ---
    TQGroupBox* netGroup = new TQGroupBox("Server & Network Identity", this);
    netGroup->setColumnLayout(0, TQt::Vertical);
    netGroup->layout()->setSpacing(8);
    netGroup->layout()->setMargin(10);

    TQGridLayout* netGrid = new TQGridLayout(netGroup->layout(), 2, 2);
    netGrid->setSpacing(8);

    netGrid->addWidget(new TQLabel("Workgroup name:", netGroup), 0, 0);
    m_workgroupEdit = new TQLineEdit(netGroup);
    m_workgroupEdit->setMaxLength(15);
    netGrid->addWidget(m_workgroupEdit, 0, 1);

    m_maxDiskCheck = new TQCheckBox("Simulate max disk size (for legacy clients):", netGroup);
    netGrid->addWidget(m_maxDiskCheck, 1, 0);

    TQWidget* spinContainer = new TQWidget(netGroup);
    TQHBoxLayout* spinLayout = new TQHBoxLayout(spinContainer, 0, 6);
    m_maxDiskSpin = new TQSpinBox(10, 1000000, 512, spinContainer);
    m_maxDiskSpin->setSuffix(" MB");
    m_maxDiskSpin->setEnabled(false);
    spinLayout->addWidget(m_maxDiskSpin);
    spinLayout->addStretch(1);
    netGrid->addWidget(spinContainer, 1, 1);

    mainLayout->addWidget(netGroup);

    // --- Group 2: User Shares Global Policy ---
    TQGroupBox* policyGroup = new TQGroupBox("User Shares Global Policy", this);
    policyGroup->setColumnLayout(0, TQt::Vertical);
    policyGroup->layout()->setSpacing(0);
    policyGroup->layout()->setMargin(8);

    TQVBoxLayout* policyLayout = new TQVBoxLayout(policyGroup->layout(), 2);

    m_recycleBinCheck = new TQCheckBox("Enable Network Recycle Bin (.recycle)", policyGroup);
    TQLabel* recycleDesc = new TQLabel(
        "<qt><small><i>Deleted network files are moved into a hidden <code>.recycle</code> folder inside each share instead of being permanently erased.</i></small></qt>",
        policyGroup);
    recycleDesc->setIndent(20);

    m_hideDotFilesCheck = new TQCheckBox("Hide Linux dot-files (.bashrc, .config, etc.)", policyGroup);
    TQLabel* hideDotDesc = new TQLabel(
        "<qt><small><i>Prevents Linux system and hidden dot-files from cluttering Windows network clients.</i></small></qt>",
        policyGroup);
    hideDotDesc->setIndent(20);

    m_customMasksCheck = new TQCheckBox("Enforce collaborative permissions (0664 / 0775)", policyGroup);
    TQLabel* masksDesc = new TQLabel(
        "<qt><small><i>Ensures files and folders created over the network can be modified or deleted by all authorized share users.</i></small></qt>",
        policyGroup);
    masksDesc->setIndent(20);

    m_ownerOnlyCheck = new TQCheckBox("Allow sharing folders owned by other users or drives (usershare owner only = false)", policyGroup);
    TQLabel* ownerOnlyDesc = new TQLabel(
        "<qt><small><i>Permits sharing folders on external drives or directories not strictly owned by your user account.</i></small></qt>",
        policyGroup);
    ownerOnlyDesc->setIndent(20);

    policyLayout->addWidget(m_recycleBinCheck);
    policyLayout->addWidget(recycleDesc);
    policyLayout->addSpacing(6);
    policyLayout->addWidget(m_hideDotFilesCheck);
    policyLayout->addWidget(hideDotDesc);
    policyLayout->addSpacing(6);
    policyLayout->addWidget(m_customMasksCheck);
    policyLayout->addWidget(masksDesc);
    policyLayout->addSpacing(6);
    policyLayout->addWidget(m_ownerOnlyCheck);
    policyLayout->addWidget(ownerOnlyDesc);

    mainLayout->addWidget(policyGroup);

    // --- Information Banner ---
    TQGroupBox* infoBox = new TQGroupBox("Information", this);
    infoBox->setColumnLayout(0, TQt::Vertical);
    infoBox->layout()->setSpacing(0);
    infoBox->layout()->setMargin(8);

    TQVBoxLayout* infoLayout = new TQVBoxLayout(infoBox->layout(), 0);
    TQLabel* infoLabel = new TQLabel(
        "<qt><small><i>These global settings will be written to <code>/etc/samba/smb.conf</code> and "
        "automatically reloaded by the Samba daemon.<br>"
        "Administrator authorization via <b>tdesudo</b> will be requested upon clicking Apply.</i></small></qt>",
        infoBox);
    infoLayout->addWidget(infoLabel);

    mainLayout->addWidget(infoBox);

    // --- Bottom Buttons ---
    TQHBoxLayout* btnLayout = new TQHBoxLayout(mainLayout, 8);
    btnLayout->addStretch(1);
    m_cancelBtn = new TQPushButton("Cancel", this);
    m_cancelBtn->setMinimumWidth(80);
    m_applyBtn = new TQPushButton("Apply", this);
    m_applyBtn->setMinimumWidth(90);
    m_applyBtn->setDefault(true);
    btnLayout->addWidget(m_cancelBtn);
    btnLayout->addWidget(m_applyBtn);

    // Enforce size constraints to prevent layout clipping when resizing
    netGroup->setMinimumSize(netGroup->sizeHint());
    policyGroup->setMinimumSize(policyGroup->sizeHint());
    infoBox->setMinimumSize(infoBox->sizeHint());
    mainLayout->setResizeMode(TQLayout::Minimum);
    adjustSize();
    TQSize minSz = sizeHint().expandedTo(TQSize(540, 480));
    resize(minSz);
    setMinimumSize(minSz);

    // Connections
    connect(m_maxDiskCheck, SIGNAL(toggled(bool)), this, SLOT(slotMaxDiskToggled(bool)));
    connect(m_applyBtn, SIGNAL(clicked()), this, SLOT(slotApply()));
    connect(m_cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));

    loadCurrentOptions();
}

GlobalOptionsDialog::~GlobalOptionsDialog()
{
}

void GlobalOptionsDialog::slotMaxDiskToggled(bool checked)
{
    m_maxDiskSpin->setEnabled(checked);
}

void GlobalOptionsDialog::loadCurrentOptions()
{
    if (!m_backend) return;

    ShareBackend::GlobalOptions opts;
    TQString err;
    m_backend->readGlobalOptions(opts, &err);

    m_workgroupEdit->setText(opts.workgroup.isEmpty() ? "WORKGROUP" : opts.workgroup);

    if (opts.maxDiskSizeMb > 0) {
        m_maxDiskCheck->setChecked(true);
        m_maxDiskSpin->setValue(opts.maxDiskSizeMb);
        m_maxDiskSpin->setEnabled(true);
    } else {
        m_maxDiskCheck->setChecked(false);
        m_maxDiskSpin->setValue(1024);
        m_maxDiskSpin->setEnabled(false);
    }

    m_recycleBinCheck->setChecked(opts.enableRecycleBin);
    m_hideDotFilesCheck->setChecked(opts.hideDotFiles);
    m_customMasksCheck->setChecked(opts.customMasks);
    m_ownerOnlyCheck->setChecked(opts.allowNonOwnedFolders);
}

void GlobalOptionsDialog::slotApply()
{
    if (!m_backend) return;

    TQString wg = m_workgroupEdit->text().stripWhiteSpace().upper();
    if (wg.isEmpty()) {
        TQMessageBox::warning(this, "Invalid Workgroup", "Please enter a workgroup name (e.g. WORKGROUP).");
        m_workgroupEdit->setFocus();
        return;
    }

    for (unsigned int i = 0; i < wg.length(); ++i) {
        char c = wg[i].latin1();
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) {
            TQMessageBox::warning(this, "Invalid Workgroup",
                                  "Workgroup name may only contain alphanumeric characters, hyphens, and underscores.");
            m_workgroupEdit->setFocus();
            return;
        }
    }

    ShareBackend::GlobalOptions opts;
    opts.workgroup = wg;
    opts.maxDiskSizeMb = m_maxDiskCheck->isChecked() ? m_maxDiskSpin->value() : 0;
    opts.enableRecycleBin = m_recycleBinCheck->isChecked();
    opts.hideDotFiles = m_hideDotFilesCheck->isChecked();
    opts.customMasks = m_customMasksCheck->isChecked();
    opts.allowNonOwnedFolders = m_ownerOnlyCheck->isChecked();

    TQString err;
    if (!m_backend->writeGlobalOptions(opts, &err)) {
        TQMessageBox::critical(this, "Failed to Apply Settings",
                               "Could not save Samba global options:\n\n" + err);
        return;
    }

    TQMessageBox::information(this, "Configuration Updated",
                              "Samba global options were successfully saved and applied.");
    accept();
}
