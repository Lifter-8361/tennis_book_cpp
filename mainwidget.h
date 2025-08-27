#pragma once

#include <QWidget>
#include "opencl_image_finder.h"

class QLabel;

class MainWidget final
    : public QWidget
{
    Q_OBJECT

public:
    MainWidget(QWidget *parent = nullptr);
    ~MainWidget();

private Q_SLOTS:

    void OnStartButtonClicked();

    void OnMonitorNumberChanged(int monitor_number);

    void OnTestMonitorImageButtonClicked();

protected:

    void CreateUi();

    QLayout* CreateMonitorControl();

    QLayout* CreateGeometryParamControl();

    QLayout* CreateClickControl();


    void ClickAndSendPlusSymbol(const QPoint& point_to_click);

    void closeEvent(QCloseEvent* event) override;

private:

    void ReadSettings();

    void SaveSettings();

private:

    OpenCLImageFinder finder_;

    int monitor_number_ = 0;

    struct geometry_area
    {
        int x = 375;
        int y = 160;
        int width = 550;
        int height = 950;
    } detect_area_;

    QPoint mouse_click_point_{2500,1100};
};
