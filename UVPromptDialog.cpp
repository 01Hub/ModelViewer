#include "UVPromptDialog.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QPushButton>
#include <QStyle>
#include <QButtonGroup>

UVPromptDialog::UVPromptDialog(const SceneUVPromptInfo& info, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("UV Auto Generation Options");    
    setWindowIcon(QIcon(":/new/prefix1/res/logo.png"));
    setModal(true);

    QVBoxLayout* layout = new QVBoxLayout(this);

    QLabel* iconLabel = new QLabel(this);
    QIcon infoIcon = style()->standardIcon(QStyle::SP_MessageBoxInformation);
    iconLabel->setPixmap(infoIcon.pixmap(48, 48));  // You can change size as needed
    iconLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    layout->addWidget(iconLabel);

    QLabel* label = new QLabel(
        QString("<b>Model:</b> %1<br>"
            "<b>Total Meshes:</b> %2<br>"
            "<b>Total Vertices:</b> %3<br>"
            "<b>Total Triangles:</b> %4<br>"
            "<b>Largest Mesh:</b> %5 (%6 triangles)<br><br>"
			"<b>Note:</b> Auto generation of UVs may take longer to load the<br>" 
            "model depending on the size and complexity."
			"<br>Click Ok only if you need to apply textures, or choose Cancel<br>"
			"<b>Tip:</b> If the model has more than 3000 triangles, consider using<br>"
            "the Hybrid method for faster UV generation.</b><br><br>"
            "Choose a UV generation method:")
        .arg(info.fileName)
        .arg(info.meshCount)
        .arg(info.totalVertices)
        .arg(info.totalTriangles)
        .arg(info.largestMeshName)
        .arg(info.largestTriangleCount));
    label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    layout->addWidget(label);

    _hybridButton = new QRadioButton("Hybrid (Fast)");
    _hybridButton->setToolTip(
        "Uses basic shape detection (planar, cylindrical, spherical).\nFast, but less accurate.");
    _smartButton = new QRadioButton("Smart (Accurate)");
    _smartButton->setToolTip(
        "Performs angle-based segmentation and PCA projection.\nMore accurate, similar to Blender's Smart UV Project.");

    _buttonGroup = new QButtonGroup(this);
    _buttonGroup->addButton(_hybridButton, Hybrid);
    _buttonGroup->addButton(_smartButton, Smart);

    layout->addWidget(_hybridButton);
    layout->addWidget(_smartButton);

    // Auto-select default
    if (info.totalTriangles > 3000)
        _hybridButton->setChecked(true);
    else
        _smartButton->setChecked(true);

    QHBoxLayout* buttonsLayout = new QHBoxLayout;
    _okButton = new QPushButton("OK");
    _cancelButton = new QPushButton("Cancel");
    buttonsLayout->addStretch();
    buttonsLayout->addWidget(_okButton);
    buttonsLayout->addWidget(_cancelButton);
    layout->addLayout(buttonsLayout);

    connect(_okButton, &QPushButton::clicked, this, [=] {
        _choice = static_cast<Choice>(_buttonGroup->checkedId());
        accept();
        });
    connect(_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

UVPromptDialog::Choice UVPromptDialog::selectedChoice() const
{
    return _choice;
}

SceneMeshInfo UVPromptDialog::collectSceneMeshInfo(const aiScene* scene)
{
    SceneMeshInfo info;

    if (!scene || !scene->HasMeshes())
        return info;

    info.meshCount = static_cast<int>(scene->mNumMeshes);

    for (unsigned int i = 0; i < scene->mNumMeshes; ++i)
    {
        const aiMesh* mesh = scene->mMeshes[i];
        int numFaces = static_cast<int>(mesh->mNumFaces);
        int numVerts = static_cast<int>(mesh->mNumVertices);

        info.totalVertices += numVerts;
        info.totalTriangles += numFaces;

        if (numFaces > info.largestMeshTriangles)
        {
            info.largestMeshTriangles = numFaces;
            info.largestMeshName = mesh->mName.C_Str();
        }
    }

    return info;
}
