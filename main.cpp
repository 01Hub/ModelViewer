#include <QApplication>
#include <QStyleFactory>
#include <QDebug>
#include <QOpenGLFunctions>
#include <QFileInfo>

#include "ModelViewerApplication.h"
#include "MainWindow.h"
#include "ModelViewer.h"

#include <iostream>
#include <string>
#include <sstream>

#include <config.h>

int main(int argc, char** argv)
{
	Q_INIT_RESOURCE(ModelViewer);

	QSurfaceFormat format;
	format.setDepthBufferSize(24);
	format.setStencilBufferSize(8);
	format.setVersion(4, 5); // OpenGL version 4.5
	format.setProfile(QSurfaceFormat::CoreProfile);
	format.setOption(QSurfaceFormat::DebugContext);
	format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
	format.setRenderableType(QSurfaceFormat::OpenGL);

	QApplication::setDesktopSettingsAware(true);
	QCoreApplication::setApplicationName("ModelViewer");
	QCoreApplication::setOrganizationName("Sharjith N");
    
	QString version = QString(APP_VERSION_STRING);
	QCoreApplication::setApplicationVersion(version);

	ModelViewerApplication app(argc, argv);

#if QT_VERSION_MAJOR == 6
	// Disable allocation limit for images
	QImageReader::setAllocationLimit(0);
#endif

#ifdef WIN32
	// qDebug() << QStyleFactory::keys();
	// app.setStyle(QStyleFactory::create("windows"));
#endif


	MainWindow* mw = MainWindow::mainWindow();		
	ModelViewer* viewer = mw->createMdiChild();
	mw->showMaximized();
	if (argc > 1)
	{
		QString fileName(argv[1]);
		QFileInfo fi(fileName);
		if (fi.exists())
		{
			mw->openFile(fileName);
			viewer->parentWidget()->close(); // close the first blank document
		}
	}

	QOpenGLFunctions glFuncs(QOpenGLContext::currentContext());
	auto logOpenGLInfo = [&glFuncs](auto& outputStream) {
		std::map<std::string, GLenum> infoMap = {
			{"Renderer", GL_RENDERER},
			{"Vendor", GL_VENDOR},
			{"OpenGL Version", GL_VERSION},
			{"Shader Version", GL_SHADING_LANGUAGE_VERSION}
		};

		for (const auto& [label, value] : infoMap) {
			const char* info = reinterpret_cast<const char*>(glFuncs.glGetString(value));
			outputStream << label << ": " << info << '\n';
		}
		};

	// Log information to std::cout
	logOpenGLInfo(std::cout);

	// Collect information into std::stringstream and set it to mw
	std::stringstream ss;
	logOpenGLInfo(ss);
	mw->setGraphicsInfo(ss.str().c_str());

	/*
#ifdef QT_DEBUG
	int n = 0;
	glFuncs.glGetIntegerv(GL_NUM_EXTENSIONS, &n);
	for (int i = 0; i < n; i++)
	{
		const char* extension =
				(const char*)glFuncs.glGetStringi(GL_EXTENSIONS, i);
		printf("GL Extension %d: %s\n", i, extension);
	}
	std::cout << std::endl;

#endif // DEBUG
*/

	return app.exec();
}
