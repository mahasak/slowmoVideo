/*
This file is part of slowmoVideo.
Copyright (C) 2011  Simon A. Eugster (Granjow)  <simon.eu@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "renderingDialog.h"
#include "ui_renderingDialog.h"

#include "lib/defs_sV.hpp"
#include "project/motionBlur_sV.h"
#include "project/project_sV.h"
#include "project/projectPreferences_sV.h"
#include "project/renderTask_sV.h"
#include "project/imagesRenderTarget_sV.h"
#include "project/videoRenderTarget_sV.h"
#include "project/emptyFrameSource_sV.h"

#include <QButtonGroup>
#include <QFileDialog>

RenderingDialog::RenderingDialog(Project_sV *project, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::RenderingDialog),
    m_project(project)
{
    ui->setupUi(this);

    // Render section
    m_sectionGroup = new QButtonGroup(this);
    m_sectionGroup->addButton(ui->radioFullProject);
    m_sectionGroup->addButton(ui->radioSection);
    m_sectionGroup->addButton(ui->radioTagSection);
    QString mode(m_project->preferences()->renderSectionMode());
    if (mode == "full") {
        ui->radioFullProject->setChecked(true);
    } else if (mode == "expr") {
        ui->radioSection->setChecked(true);
    } else if (mode == "tags") {
        ui->radioTagSection->setChecked(true);
    } else {
        qDebug() << "Unknown render section mode: " << mode;
        Q_ASSERT(false);
    }

    // Optical flow
    ui->lambda->setValue(m_project->preferences()->flowV3DLambda());

    // Motion blur
    ui->maxSamples->setValue(m_project->motionBlur()->maxSamples());
    ui->slowmoSamples->setValue(m_project->motionBlur()->slowmoSamples());
    m_blurGroup = new QButtonGroup(this);
    m_blurGroup->addButton(ui->radioBlurConvolution);
    m_blurGroup->addButton(ui->radioBlurStacking);
    m_blurGroup->addButton(ui->radioBlurNearest);
    if (m_project->preferences()->renderMotionblurType() == MotionblurType_Convolving) {
        ui->radioBlurConvolution->setChecked(true);
    } else if (m_project->preferences()->renderMotionblurType() == MotionblurType_Stacking) {
        ui->radioBlurStacking->setChecked(true);
    } else {
        ui->radioBlurNearest->setChecked(true);
    }

    fillTagLists();

    // Output target type
    m_targetGroup = new QButtonGroup(this);
    m_targetGroup->addButton(ui->radioImages);
    m_targetGroup->addButton(ui->radioVideo);
    if (m_project->preferences()->renderTarget() == "images") {
        ui->radioImages->setChecked(true);
    } else {
        ui->radioVideo->setChecked(true);
    }

    // Output target files
    ui->imagesOutputDir->setText(m_project->preferences()->imagesOutputDir());
    ui->imagesFilenamePattern->setText(m_project->preferences()->imagesFilenamePattern());
    ui->videoOutputFile->setText(m_project->preferences()->videoFilename());
    ui->vcodec->setText(m_project->preferences()->videoCodec());

    // FPS
    QString fps = QVariant(m_project->preferences()->renderFPS().fps()).toString();
    if (ui->cbFps->findText(fps) < 0 && fps.toFloat() > 0) {
        ui->cbFps->addItem(fps);
    }
    ui->cbFps->setCurrentIndex(ui->cbFps->findText(fps));

    // Output size
    ui->cbSize->addItem(tr("Original size"), QVariant(FrameSize_Orig));
    ui->cbSize->addItem(tr("Small"), QVariant(FrameSize_Small));
    ui->cbSize->setCurrentIndex(ui->cbSize->findData(QVariant(m_project->preferences()->renderFrameSize())));

    // Interpolation type
    ui->cbInterpolation->addItem(toString(InterpolationType_Forward), QVariant(InterpolationType_Forward));
    ui->cbInterpolation->addItem(toString(InterpolationType_ForwardNew), QVariant(InterpolationType_ForwardNew));
    ui->cbInterpolation->addItem(toString(InterpolationType_Twoway), QVariant(InterpolationType_Twoway));
    ui->cbInterpolation->addItem(toString(InterpolationType_TwowayNew), QVariant(InterpolationType_TwowayNew));
    ui->cbInterpolation->addItem(toString(InterpolationType_Bezier), QVariant(InterpolationType_Bezier));
    if (ui->cbInterpolation->findData(QVariant(m_project->preferences()->renderInterpolationType())) >= 0) {
        ui->cbInterpolation->setCurrentIndex(ui->cbInterpolation->findData(QVariant(m_project->preferences()->renderInterpolationType())));
    }

    bool b = true;
    b &= connect(m_targetGroup, SIGNAL(buttonClicked(int)), this, SLOT(slotUpdateRenderTarget()));
    b &= connect(m_sectionGroup, SIGNAL(buttonClicked(int)), this, SLOT(slotSectionModeChanged()));
    b &= connect(ui->timeStart, SIGNAL(textChanged(QString)), this, SLOT(slotValidate()));
    b &= connect(ui->timeEnd, SIGNAL(textChanged(QString)), this, SLOT(slotValidate()));

    b &= connect(ui->cbStartTag, SIGNAL(currentIndexChanged(int)), this, SLOT(slotTagIndexChanged()));
    b &= connect(ui->cbEndTag, SIGNAL(currentIndexChanged(int)), this, SLOT(slotTagIndexChanged()));

    b &= connect(ui->bAbort, SIGNAL(clicked()), this, SLOT(reject()));
    b &= connect(ui->bOk, SIGNAL(clicked()), this, SLOT(accept()));
    b &= connect(ui->bSave, SIGNAL(clicked()), this, SLOT(slotSaveSettings()));

    b &= connect(ui->cbFps, SIGNAL(editTextChanged(QString)), this, SLOT(slotValidate()));

    b &= connect(ui->imagesOutputDir, SIGNAL(textChanged(QString)), this, SLOT(slotValidate()));
    b &= connect(ui->imagesFilenamePattern, SIGNAL(textChanged(QString)), this, SLOT(slotValidate()));
    b &= connect(ui->videoOutputFile, SIGNAL(textChanged(QString)), this, SLOT(slotValidate()));
    b &= connect(ui->bImagesBrowseDir, SIGNAL(clicked()), this, SLOT(slotBrowseImagesDir()));
    b &= connect(ui->bBrowseVideoOutputFile, SIGNAL(clicked()), this, SLOT(slotBrowseVideoFile()));
    Q_ASSERT(b);

    // Restore rendering start/end
    int index;
    index = ui->cbStartTag->findText(m_project->preferences()->renderStartTag());
    if (index >= 0) {
        ui->cbStartTag->setCurrentIndex(index);
    }
    index = ui->cbEndTag->findText(m_project->preferences()->renderEndTag());
    if (index >= 0) {
        ui->cbEndTag->setCurrentIndex(index);
    }
    if (m_project->preferences()->renderStartTime().length() > 0) {
        ui->timeStart->setText(m_project->preferences()->renderStartTime());
    }
    if (m_project->preferences()->renderEndTime().length() > 0) {
        ui->timeEnd->setText(m_project->preferences()->renderEndTime());
    }

#if QT_VERSION >= 0x040700
    ui->timeStart->setPlaceholderText(QVariant(m_project->nodes()->startTime()).toString());
    ui->timeEnd->setPlaceholderText(QVariant(m_project->nodes()->endTime()).toString());
#endif

    slotUpdateRenderTarget();
    slotSectionModeChanged();
}

RenderingDialog::~RenderingDialog()
{
    delete m_targetGroup;
    delete ui;
}

RenderTask_sV* RenderingDialog::buildTask()
{
    if (slotValidate()) {
        slotSaveSettings();

        ProjectPreferences_sV *prefs = m_project->preferences();

        const QString imagesOutputDir = ui->imagesOutputDir->text();
        const QString imagesFilenamePattern = ui->imagesFilenamePattern->text();

        RenderTask_sV *task = new RenderTask_sV(m_project);
        task->renderPreferences().setFps(prefs->renderFPS());
        task->renderPreferences().size = prefs->renderFrameSize();
        task->renderPreferences().interpolation = prefs->renderInterpolationType();
        task->renderPreferences().motionblur = prefs->renderMotionblurType();


        if (ui->radioImages->isChecked()) {
            ImagesRenderTarget_sV *renderTarget = new ImagesRenderTarget_sV(task);
            renderTarget->setFilenamePattern(imagesFilenamePattern);
            renderTarget->setTargetDir(imagesOutputDir);
            task->setRenderTarget(renderTarget);
        } else if (ui->radioVideo->isChecked()) {
            VideoRenderTarget_sV *renderTarget = new VideoRenderTarget_sV(task);
            renderTarget->setTargetFile(ui->videoOutputFile->text());
            renderTarget->setVcodec(ui->vcodec->text());
            task->setRenderTarget(renderTarget);
        } else {
            qDebug() << "Render target is neither images nor video. Not implemented?";
            Q_ASSERT(false);
        }

        if (ui->radioTagSection->isChecked()) {
            bool b;
            qreal start = ui->cbStartTag->itemData(ui->cbStartTag->currentIndex()).toFloat(&b);
            Q_ASSERT(b);
            qreal end = ui->cbEndTag->itemData(ui->cbEndTag->currentIndex()).toFloat(&b);
            Q_ASSERT(b);
            qDebug() << QString("Rendering tag section from %1 (%2) to %3 (%4)")
                        .arg(ui->cbStartTag->currentText())
                        .arg(start).arg(ui->cbEndTag->currentText()).arg(end);
            Q_ASSERT(start <= end);
            task->setTimeRange(start, end);
        } else if (ui->radioSection->isChecked()) {
            qDebug() << QString("Rendering time section from %1 to %3")
                        .arg(ui->cbStartTag->currentText())
                        .arg(ui->cbEndTag->currentText());
            task->setTimeRange(ui->timeStart->text(), ui->timeEnd->text());
        }

        QString mode;
        if (ui->radioFullProject->isChecked()) {
            mode = "full";
        } else if (ui->radioSection->isChecked()) {
            mode = "time";
            m_project->preferences()->renderStartTime() = ui->timeStart->text();
            m_project->preferences()->renderEndTime() = ui->timeEnd->text();
        } else if (ui->radioTagSection->isChecked()) {
            mode = "tags";
            m_project->preferences()->renderStartTag() = ui->cbStartTag->currentText();
            m_project->preferences()->renderEndTag() = ui->cbEndTag->currentText();
        } else {
            qDebug() << "No section mode selected?";
            Q_ASSERT(false);
        }
        return task;
    } else {
        return NULL;
    }
}

void RenderingDialog::fillTagLists()
{
    QList<Tag_sV> list;
    for (int i = 0; i < m_project->tags()->size(); i++) {
        if (m_project->tags()->at(i).axis() == TagAxis_Output
                && m_project->tags()->at(i).time() > m_project->nodes()->startTime()
                && m_project->tags()->at(i).time() < m_project->nodes()->endTime()) {
            list << m_project->tags()->at(i);
        }
    }
    qSort(list);
    ui->cbStartTag->addItem(tr("<Start>"), QVariant(m_project->nodes()->startTime()));
    for (int i = 0; i < list.size(); i++) {
        ui->cbStartTag->addItem(list.at(i).description(), QVariant(list.at(i).time()));
        ui->cbEndTag->addItem(list.at(i).description(), QVariant(list.at(i).time()));
    }
    ui->cbEndTag->addItem(tr("<End>"), QVariant(m_project->nodes()->endTime()));
}

void RenderingDialog::slotSaveSettings()
{

    const InterpolationType interpolation = (InterpolationType)ui->cbInterpolation->itemData(ui->cbInterpolation->currentIndex()).toInt();
    const FrameSize size = (FrameSize)ui->cbSize->itemData(ui->cbSize->currentIndex()).toInt();
    const QString imagesOutputDir = ui->imagesOutputDir->text();
    const QString imagesFilenamePattern = ui->imagesFilenamePattern->text();
    const float fps = ui->cbFps->currentText().toFloat();

    m_project->motionBlur()->setMaxSamples(ui->maxSamples->value());
    m_project->motionBlur()->setSlowmoSamples(ui->slowmoSamples->value());
    m_project->preferences()->flowV3DLambda() = ui->lambda->value();

    if (ui->radioBlurConvolution->isChecked()) {
        m_project->preferences()->renderMotionblurType() = MotionblurType_Convolving;
    } else if (ui->radioBlurStacking->isChecked()) {
        m_project->preferences()->renderMotionblurType() = MotionblurType_Stacking;
    } else {
        m_project->preferences()->renderMotionblurType() = MotionblurType_Nearest;
    }

    QString mode;
    if (ui->radioFullProject->isChecked()) {
        mode = "full";
    } else if (ui->radioSection->isChecked()) {
        mode = "expr";
        m_project->preferences()->renderStartTime() = ui->timeStart->text();
        m_project->preferences()->renderEndTime() = ui->timeEnd->text();
    } else if (ui->radioTagSection->isChecked()) {
        mode = "tags";
        m_project->preferences()->renderStartTag() = ui->cbStartTag->currentText();
        m_project->preferences()->renderEndTag() = ui->cbEndTag->currentText();
    } else {
        qDebug() << "No section mode selected?";
        Q_ASSERT(false);
    }
    m_project->preferences()->renderSectionMode() = mode;
    m_project->preferences()->imagesOutputDir() = imagesOutputDir;
    m_project->preferences()->imagesFilenamePattern() = imagesFilenamePattern;
    m_project->preferences()->videoFilename() = ui->videoOutputFile->text();
    m_project->preferences()->videoCodec() = ui->vcodec->text();
    m_project->preferences()->renderInterpolationType() = interpolation;
    m_project->preferences()->renderFrameSize() = size;
    m_project->preferences()->renderFPS() = fps;
    m_project->preferences()->renderTarget() = ui->radioImages->isChecked() ? "images" : "video";

    accept();
}

bool RenderingDialog::slotValidate()
{
    bool ok = true;

    float fps = ui->cbFps->currentText().toFloat(&ok);
    ok &= fps > 0;
    if (ok) {
        ui->cbFps->setStyleSheet(QString("QComboBox { background-color: %1; }").arg(Colours_sV::colOk.name()));
    } else {
        ui->cbFps->setStyleSheet(QString("QComboBox { background-color: %1; }").arg(Colours_sV::colBad.name()));
    }

    if (ui->radioImages->isChecked()) {
        if (ui->imagesFilenamePattern->text().contains("%1")) {
            ui->imagesFilenamePattern->setStyleSheet(QString("QLineEdit { background-color: %1; }").arg(Colours_sV::colOk.name()));
        } else {
            ok = false;
            ui->imagesFilenamePattern->setStyleSheet(QString("QLineEdit { background-color: %1; }").arg(Colours_sV::colBad.name()));
        }

        if (ui->imagesOutputDir->text().length() > 0) {
            ui->imagesOutputDir->setStyleSheet(QString("QLineEdit { background-color: %1; }").arg(Colours_sV::colOk.name()));
        } else {
            ok = false;
            ui->imagesOutputDir->setStyleSheet(QString("QLineEdit { background-color: %1; }").arg(Colours_sV::colBad.name()));
        }
    } else if (ui->radioVideo->isChecked()) {
        if (ui->videoOutputFile->text().length() > 0) {
            ui->videoOutputFile->setStyleSheet(QString("QLineEdit { background-color: %1; }").arg(Colours_sV::colOk.name()));
        } else {
            ok = false;
            ui->videoOutputFile->setStyleSheet(QString("QLineEdit { background-color: %1; }").arg(Colours_sV::colBad.name()));
        }
    } else {
        Q_ASSERT(false);
    }

    if (ui->radioSection->isChecked()) {
        bool startOk = false;
        bool endOk = false;
        qreal timeStart = 0;
        qreal timeEnd = 0;
        QStringList messages;

        Fps_sV currentFps(ui->cbFps->currentText().toFloat());
        try {
            timeStart = m_project->toOutTime(ui->timeStart->text(), currentFps);
            startOk = true;
        } catch (Error_sV &err) {
            messages << err.message();
        }
        try {
            timeEnd = m_project->toOutTime(ui->timeEnd->text(), currentFps);
            endOk = true;
        } catch (Error_sV &err) {
            messages << err.message();
        }
        if (timeEnd <= timeStart) {
            endOk = false;
            messages << tr("Start time must be < end time!");
        }

        messages << tr("Rendering from %1 s to %2 s.").arg(timeStart).arg(timeEnd);
        ui->sectionMessage->setText(messages.join("\n"));

        ok &= startOk && endOk;

        if (!startOk) {
            ui->timeStart->setStyleSheet(QString("QLineEdit { background-color: %1; }").arg(Colours_sV::colBad.name()));
        } else {
            ui->timeStart->setStyleSheet(QString("QLineEdit { background-color: %1; }").arg(Colours_sV::colOk.name()));
        }
        if (!endOk) {
            ui->timeEnd->setStyleSheet(QString("QLineEdit { background-color: %1; }").arg(Colours_sV::colBad.name()));
        } else {
            ui->timeEnd->setStyleSheet(QString("QLineEdit { background-color: %1; }").arg(Colours_sV::colOk.name()));
        }
    }

    ok &= dynamic_cast<EmptyFrameSource_sV*>(m_project->frameSource()) == NULL;

    ok &= m_project->nodes()->size() >= 2;

    ui->bOk->setEnabled(ok);

    return ok;
}

void RenderingDialog::slotUpdateRenderTarget()
{
    ui->groupImages->setVisible(ui->radioImages->isChecked());
    ui->groupVideo->setVisible(ui->radioVideo->isChecked());
    slotValidate();
}

void RenderingDialog::slotBrowseImagesDir()
{
    QFileDialog dialog(this, tr("Output directory for rendered images"));
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setFileMode(QFileDialog::Directory);
    dialog.setOption(QFileDialog::ShowDirsOnly, true);
    dialog.setDirectory(ui->imagesOutputDir->text());
    if (dialog.exec() == QDialog::Accepted) {
        ui->imagesOutputDir->setText(dialog.selectedFiles().at(0));
    }
}

void RenderingDialog::slotBrowseVideoFile()
{
    QFileDialog dialog(this, tr("Output video file"));
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setDirectory(QFileInfo(ui->videoOutputFile->text()).absolutePath());
    if (dialog.exec() == QDialog::Accepted) {
        ui->videoOutputFile->setText(dialog.selectedFiles().at(0));
    }
}

void RenderingDialog::slotSectionModeChanged()
{
    ui->timeStart->setVisible(ui->radioSection->isChecked());
    ui->timeEnd->setVisible(ui->radioSection->isChecked());
    ui->sectionMessage->setVisible(ui->radioSection->isChecked());
    ui->cbStartTag->setVisible(ui->radioTagSection->isChecked());
    ui->cbEndTag->setVisible(ui->radioTagSection->isChecked());
    ui->lblcTo->setVisible(ui->radioSection->isChecked() || ui->radioTagSection->isChecked());
    slotValidate();
}

void RenderingDialog::slotTagIndexChanged()
{
    if (QObject::sender() == ui->cbStartTag) {
        qDebug() << "Start tag";
        if (ui->cbEndTag->currentIndex() < ui->cbStartTag->currentIndex()) {
            ui->cbEndTag->setCurrentIndex(ui->cbStartTag->currentIndex());
        }
    } else {
        qDebug() << "End tag";
        if (ui->cbStartTag->currentIndex() > ui->cbEndTag->currentIndex()) {
            ui->cbStartTag->setCurrentIndex(ui->cbEndTag->currentIndex());
        }
    }
}






