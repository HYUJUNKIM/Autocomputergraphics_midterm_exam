// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#include <QApplication>
#include <QLabel>
#include <QSurfaceFormat>

#ifndef QT_NO_OPENGL
#include "mainwindow.h"
#endif

int main(int argc, char *argv[])
{
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication app(argc, argv);

    app.setApplicationName("cube-v3");
    app.setApplicationVersion("0.1");
#ifndef QT_NO_OPENGL
    MainWindow window;
    window.setWindowTitle("cube-v3");
    window.show();
#else
    QLabel note("OpenGL Support required");
    note.show();
#endif
    return app.exec();
}
