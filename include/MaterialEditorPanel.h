#pragma once
#include <QWidget>
#include <QTreeWidget>
#include <QFormLayout>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QToolButton>
#include "MaterialLibraryWidget.h"
#include "MaterialPreviewWidget.h"

// Forward declaration for ui file
namespace Ui
{
	class MaterialEditorPanel;
}

class MaterialEditorPanel : public QWidget
{
	Q_OBJECT
public:
	explicit MaterialEditorPanel(QWidget* parent = nullptr);

	~MaterialEditorPanel();

	void onSaveButtonClicked();
	void onDeleteButtonClicked();

	bool isDetached() const { return _detached; }
	void setDetached(bool detached);

signals:
	void materialChanged(const GLMaterial& mat);
	void materialApplied(const GLMaterial& mat);
	void detachRequested();

private slots:
	void onMaterialPreview(const GLMaterial& mat);
	void onMaterialSelected(const GLMaterial& mat);
	void updateUI(const GLMaterial& mat);
	void onDetachButtonClicked();

private:
	
	GLMaterial _currentMaterial = GLMaterial::METAL_ALUMINUM();
	bool _detached = false;

	std::unique_ptr<Ui::MaterialEditorPanel> ui;
};
