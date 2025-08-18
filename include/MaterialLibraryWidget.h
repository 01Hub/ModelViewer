#pragma once
#include <QTreeWidget>
#include "GLMaterial.h"

class MaterialLibraryWidget : public QTreeWidget
{
    Q_OBJECT
public:
    explicit MaterialLibraryWidget(QWidget *parent = nullptr);

signals:
    void materialSelected(const GLMaterial &mat);

private slots:
    void onItemClicked(QTreeWidgetItem *item, int column);
    void handleItemEntered(QTreeWidgetItem* item, int column);

private:
    void populateMaterials();

private:
    QMap<QString, std::function<GLMaterial()>> materialMap;
};
