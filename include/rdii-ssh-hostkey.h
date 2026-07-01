// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

extern int rdii_ssh_hostkey_backup(const char *device, const char *backup_dir);
extern int rdii_ssh_hostkey_restore(const char *device, const char *backup_dir);
