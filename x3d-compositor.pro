QT += gui gui-private core-private compositor

LIBS += -L ../../lib

HEADERS += \
    opengloutput.h \
    qwindowoutput.h \
    qwindowcompositor.h \
    x3dscene.h \
    x3drenderer.h \
    x3dopenglrenderer.h \
    openglrenderer.h

SOURCES += main.cpp \
    opengloutput.cpp \
    qwindowoutput.cpp \
    qwindowcompositor.cpp \
    x3dscene.cpp \
    x3dopenglrenderer.cpp \
    openglrenderer.cpp

INCLUDEPATH+=/usr/local/include/CyberX3D-1.0 \
             /usr/include/bullet
LIBS += -lcx3d-1.0 -lBulletCollision -lBulletDynamics -lBulletSoftBody -lLinearMath

RESOURCES += x3d-compositor.qrc

sources.files = $$SOURCES $$HEADERS $$RESOURCES $$FORMS x3d-compositor.pro
INSTALLS += target sources
