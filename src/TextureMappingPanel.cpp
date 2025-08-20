#include "TextureMappingPanel.h"
#include "ui_TextureMappingPanel.h"

TextureMappingPanel::TextureMappingPanel(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::TextureMappingPanel)
{
    ui->setupUi(this);
}

TextureMappingPanel::~TextureMappingPanel()
{
    delete ui;
}
