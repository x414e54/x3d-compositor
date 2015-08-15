#include "output/qwindowoutput.h"
#include "compositor/wayland/qwindowcompositor.h"
#include "opengl/x3dopenglrenderer.h"

#include <QGuiApplication>
#include <QFileInfo>
#include <QDir>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QWindowOutput window;
    window.enabled = true;
    X3DOpenGLRenderer renderer;
    renderer.set_viewpoint_output(0, window);
    renderer.set_viewpoint_viewport(0, 800, 600);
    X3DScene scene(&renderer);

    if (argc > 1)
    {
        QString file = argv[1];
        QFileInfo file_info(file);
        QDir::setCurrent(file_info.absoluteDir().absolutePath());
        scene.load(file);
    }

    QWindowCompositor compositor(&window, &scene);

    window.show();

    return app.exec();
}
