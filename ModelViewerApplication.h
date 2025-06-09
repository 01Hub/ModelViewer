#ifndef MODELVIEWERAPPLICATION_H
#define MODELVIEWERAPPLICATION_H

#include <QApplication>
#include <QStringList>

class ModelViewerApplication : public QApplication {
public:
    ModelViewerApplication(int& argc, char** argv);

    // Static utility method for supported extensions
    static QStringList supportedImportExtensions();

    private:
        static void initializeSupportedImportExtensions();

private:
    static QStringList _supportedExtensions;
};

#endif // MODELVIEWERAPPLICATION_H
