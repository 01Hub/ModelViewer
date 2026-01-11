#include "LanguageManager.h"
#include "MainWindow.h"
#include "ModelViewer.h"
#include "ModelViewerApplication.h"
#include <iostream>
#include <QApplication>
#include <QDebug>
#include <QFileInfo>
#include <QOpenGLFunctions>
#include <QStyleFactory>
#include <sstream>
#include <string>


int main(int argc, char** argv)
{
	Q_INIT_RESOURCE(ModelViewer);

	ModelViewerApplication app(argc, argv);

#if QT_VERSION_MAJOR == 6
	// Disable allocation limit for images
	QImageReader::setAllocationLimit(0);
#endif

#ifdef WIN32
	// qDebug() << QStyleFactory::keys();
	// app.setStyle(QStyleFactory::create("windows"));
#endif

	QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
	// Set the language based on settings or system locale
	QString langCode = settings.value("App/Language").toString();
	if (langCode.isEmpty())
	{
		langCode = QLocale::system().name(); // e.g., "en_US"
	}
	// Load the language settings
	LanguageManager::instance().loadLanguage(langCode);

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

	QList<QByteArray> formats = QImageReader::supportedImageFormats();
	if (formats.contains("webp"))
	{
		qDebug() << "WebP support is available";
	}
	else
	{
		qDebug() << "WebP support is NOT available";
	}

#ifdef NDEBUG
	// Suppress stdout/stderr in Release builds
#ifdef _WIN32
	std::ofstream devNull("NUL");
#else
	std::ofstream devNull("/dev/null");
#endif

	std::cout.rdbuf(devNull.rdbuf());
	std::cerr.rdbuf(devNull.rdbuf());
#endif

	return app.exec();
}
