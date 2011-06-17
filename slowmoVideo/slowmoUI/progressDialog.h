#ifndef PROGRESSDIALOG_H
#define PROGRESSDIALOG_H

#include <QDialog>

namespace Ui {
    class ProgressDialog;
}

class ProgressDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ProgressDialog(QWidget *parent = 0);
    ~ProgressDialog();

public slots:
    void slotNextTask(const QString taskDescription, int taskSize);
    void slotTaskProgress(int progress);
    void slotTaskItemDescription(const QString desc);
    void slotAllTasksFinished();
    void slotAborted(const QString& message = "");

signals:
    void signalAbort();

private:
    Ui::ProgressDialog *ui;

private slots:
    void slotAbortPressed();
};

#endif // PROGRESSDIALOG_H