QT += gui gui-private core-private compositor

LIBS += -L ../../lib
#include (../../src/qt-compositor/qt-compositor.pri)

HEADERS += \
    compositorwindow.h \
    qwindowcompositor.h \
    x3dscene.h \
    x3drenderer.h

SOURCES += main.cpp \
    compositorwindow.cpp \
    qwindowcompositor.cpp \
    x3dscene.cpp \
    x3drenderer.cpp

INCLUDEPATH+=/usr/local/include/CyberX3D-1.0 \
             /usr/include/bullet
LIBS += -lcx3d-1.0 -lBulletCollision -lBulletDynamics -lBulletSoftBody -lLinearMath

RESOURCES += x3d-compositor.qrc

sources.files = $$SOURCES $$HEADERS $$RESOURCES $$FORMS x3d-compositor.pro
INSTALLS += target sources
