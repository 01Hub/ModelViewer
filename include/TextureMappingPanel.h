#ifndef TEXTUREMAPPINGPANEL_H
#define TEXTUREMAPPINGPANEL_H

#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
class TextureMappingPanel;
}
QT_END_NAMESPACE

class TextureMappingPanel : public QWidget
{
    Q_OBJECT

public:
    TextureMappingPanel(QWidget *parent = nullptr);
    ~TextureMappingPanel();

private:
    Ui::TextureMappingPanel *ui;
};
#endif // TEXTUREMAPPINGPANEL_H
