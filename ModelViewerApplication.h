#ifndef MODELVIEWERAPPLICATION_H
#define MODELVIEWERAPPLICATION_H

#include <QApplication>
#include <QStringList>

class ModelViewerApplication : public QApplication {
public:
    ModelViewerApplication(int& argc, char** argv);

    // Static utility method for supported extensions
    static QStringList supportedImportExtensions();
	
    static void setSupportedMSAASamples(int samples) {
        _supportedMSAASamples = samples;
	}

    static int supportedMSAASamples() {
        return _supportedMSAASamples;
	}

    private:
        static void initializeSupportedImportExtensions();

private:
    static QStringList _supportedExtensions;
	static int _supportedMSAASamples;
};

#endif // MODELVIEWERAPPLICATION_H
