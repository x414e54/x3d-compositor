QT += gui gui-private core-private compositor openglextensions
CONFIG += c++11

LIBS += -L ../openvr/lib/linux64
QMAKE_RPATHDIR += $ORIGIN/lib

HEADERS += \
    opengl/opengloutput.h \
    opengl/openglrenderer.h \
    opengl/openglhelper.h \
    opengl/x3dopenglrenderer.h \
    compositor/wayland/qwindowcompositor.h \
    x3d/x3dscene.h \
    x3d/x3drenderer.h \
    input/inputlistener.h \
    output/qwindowoutput.h \
    output/openvroutput.h

SOURCES += main.cpp \
    opengl/opengloutput.cpp \
    opengl/openglrenderer.cpp \
    opengl/openglhelper.cpp \
    opengl/x3dopenglrenderer.cpp \
    compositor/wayland/qwindowcompositor.cpp \
    x3d/x3dscene.cpp \
    output/qwindowoutput.cpp \
    output/openvroutput.cpp

INCLUDEPATH+=/usr/local/include/CyberX3D-1.0 \
             /usr/include/bullet \
             ../openvr/headers
LIBS += -lcx3d-1.0 -lBulletCollision -lBulletDynamics -lBulletSoftBody -lLinearMath -lopenvr_api

RESOURCES += x3d-compositor.qrc

sources.files = $$SOURCES $$HEADERS $$RESOURCES $$FORMS x3d-compositor.pro
INSTALLS += target sources
