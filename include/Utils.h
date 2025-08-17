#ifndef UTILS_H_INCLUDED
#define UTILS_H_INCLUDED

#include <QImage>
#include <QColor>
#include <QString>


inline QImage convertToGLFormat(const QImage& image)
{
    return image.convertToFormat(QImage::Format_RGBA8888).mirrored();//flipped(); // flipped 32bit RGBA
}

inline QString makeButtonStyleSheet(const QColor& color)
{
    // Compute perceived brightness (luminance)
    int brightness = int(0.299 * color.red() +
        0.587 * color.green() +
        0.114 * color.blue());

    // Choose text color
    QString textColor = (brightness > 186) ? "black" : "white";

    return QString("background-color: rgb(%1, %2, %3); color: %4;")
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue())
        .arg(textColor);
}


#endif
