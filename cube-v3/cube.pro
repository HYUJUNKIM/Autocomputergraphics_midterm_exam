QT += core gui widgets opengl openglwidgets

TARGET = cube-v3
TEMPLATE = app

SOURCES += main.cpp

SOURCES += \
    mainwidget.cpp \
    mainwindow.cpp \
    controlpanel.cpp \
    linegeometry.cpp \
    geometryengine.cpp

HEADERS += \
    mainwidget.h \
    mainwindow.h \
    controlpanel.h \
    scenestate.h \
    linegeometry.h \
    geometryengine.h

RESOURCES += \
    shaders.qrc \
    textures.qrc

# install
target.path = $$[QT_INSTALL_EXAMPLES]/opengl/cube
INSTALLS += target
