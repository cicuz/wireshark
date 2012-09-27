/* capture_file_dialog.h
 *
 * $Id$
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef CAPTURE_FILE_DIALOG_H
#define CAPTURE_FILE_DIALOG_H

#include "display_filter_edit.h"

#include "packet_list_record.h"
#include "cfile.h"

#include "ui/file_dialog.h"

#include <QFileDialog>

class CaptureFileDialog : public QFileDialog
{
    // The GTK+ Open Capture File dialog has the following elements and features:
    //   - The ability to select a capture file from a list of known extensions
    //   - A display filter entry
    //   - Name resolution checkboxes
    //   - Capture file preview information
    // Ideally we should provide similar functionality here.
    //
    // You can subclass QFileDialog (which we've done here) and add widgets as
    // described at
    // http://developer.qt.nokia.com/faq/answer/how_can_i_add_widgets_to_my_qfiledialog_instance
    // However, Qt's idea of what a file dialog looks like isn't what Microsoft
    // and Apple think a file dialog looks like.
    //
    // On Windows Vista and later we should probably use IFileOpenDialog. On earlier
    // versions of Windows (including XP) we should use GetOpenFileName, which is
    // what we do in ui/win32/file_dlg_win32.c. On OS X we should use NSOpenPanel. On
    // other platforms we should fall back to QFileDialog.
    //
    // Yes, that's four implementations of the same window.
    //
    // If a plain native open file dialog is good enough we can just the static
    // version of QFileDialog::getOpenFileName. (Commenting out Q_OBJECT and
    // "explicit" below has the same effect.)

    Q_OBJECT
public:
    explicit CaptureFileDialog(QWidget *parent = NULL, QString &display_filter = *new QString());
    static check_savability_t checkSaveAsWithComments(QWidget *
#if defined(Q_WS_WIN)
            parent
#endif
            , capture_file *cf, int file_type);

    int mergeType();
    int selectedFileType();
    bool isCompressed();

private:
    void addMergeControls(QVBoxLayout &v_box);
    void addDisplayFilterEdit();
    void addPreview(QVBoxLayout &v_box);
    QString fileType(int ft, bool extension_globs = true);
    QStringList buildFileOpenTypeList(void);

    QVBoxLayout left_v_box_;
    QVBoxLayout right_v_box_;

    QString &display_filter_;
    DisplayFilterEdit* display_filter_edit_;
    int df_row_;

    QLabel preview_format_;
    QLabel preview_size_;
    QLabel preview_packets_;
    QLabel preview_first_;
    QLabel preview_elapsed_;
    QList<QLabel *> preview_labels_;

    QRadioButton merge_prepend_;
    QRadioButton merge_chrono_;
    QRadioButton merge_append_;

    QHash<QString, int>type_hash_;

#if !defined(Q_WS_WIN)
    void addResolutionControls(QVBoxLayout &v_box);
    void addGzipControls(QVBoxLayout &v_box, capture_file *cf);

    QStringList buildFileSaveAsTypeList(capture_file *cf, bool must_support_comments);

    int default_ft_;

    QCheckBox mac_res_;
    QCheckBox transport_res_;
    QCheckBox network_res_;
    QCheckBox external_res_;

    QCheckBox compress_;
#else // Q_WS_WIN
    int file_type_;
    int merge_type_;
    gboolean compressed_;
#endif // Q_WS_WIN

signals:

public slots:

    int exec();
    int open(QString &file_name);
    check_savability_t saveAs(capture_file *cf, QString &file_name, bool must_support_comments);
    int merge(QString &file_name);

private slots:
    void preview(const QString & path);
};

#endif // CAPTURE_FILE_DIALOG_H

/*
 * Editor modelines
 *
 * Local Variables:
 * c-basic-offset: 4
 * tab-width: 8
 * indent-tabs-mode: nil
 * End:
 *
 * ex: set shiftwidth=4 tabstop=8 expandtab:
 * :indentSize=4:tabSize=8:noTabs=true:
 */
