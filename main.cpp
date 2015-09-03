#include "output/qwindowoutput.h"
#include "compositor/wayland/qwindowcompositor.h"
#include "opengl/x3dopenglrenderer.h"
#include "x3d/x3dscene.h"

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
    renderer.set_viewpoint_viewport(0, 1920, 1080);
    X3DScene scene(&renderer);
    window.set_input_listener(&scene);

    if (argc > 1)
    {
        QString file = argv[1];
        QFileInfo file_info(file);
        QDir::setCurrent(file_info.absoluteDir().absolutePath());
        scene.load(file);
    }

#if USE_COMPOSITOR
    QWindowCompositor compositor(&window, &scene);
#endif

    window.show();

    return app.exec();
}
