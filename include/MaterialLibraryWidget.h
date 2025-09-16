#pragma once
#include <QTreeWidget>
#include "GLMaterial.h"
#include <QList>
#include <QMap>
#include <QVector>
#include <QPair>

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
    QVector<QPair<QString, QVector<QPair<QString, QString>>>> populateMaterialMapWithBuiltIns(
        QMap<QString, std::function<GLMaterial()>>& materialMap);

private:
    QMap<QString, std::function<GLMaterial()>> materialMap;
};
