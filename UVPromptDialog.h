#pragma once

#include <QDialog>
#include <AssImpMesh.h>

class QLabel;
class QRadioButton;
class QPushButton;
class QButtonGroup;

struct SceneMeshInfo
{
    int totalVertices = 0;
    int totalTriangles = 0;
    int meshCount = 0;
    std::string largestMeshName;
    int largestMeshTriangles = 0;
};

struct SceneUVPromptInfo
{
    QString fileName;
    int meshCount;
    int totalVertices;
    int totalTriangles;
    QString largestMeshName;
    int largestTriangleCount;
};


class UVPromptDialog : public QDialog
{
    Q_OBJECT

public:
    enum Choice { None, Hybrid, Smart };

    UVPromptDialog(const SceneUVPromptInfo& info, QWidget* parent = nullptr);
    Choice selectedChoice() const;

    static SceneMeshInfo collectSceneMeshInfo(const aiScene* scene);

private:
    QButtonGroup* _buttonGroup;
    QRadioButton* _hybridButton;
    QRadioButton* _smartButton;
    QPushButton* _okButton;
    QPushButton* _cancelButton;
    Choice _choice = None;
};

