#include "mainwidget.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QPushButton>

#include <QGuiApplication>

#include <QLabel>
#include <QScreen>
#include <QDebug>
#include <QSpinBox>
#include <QElapsedTimer>
#include <QSettings>

#include "input_simulator.h"

namespace helpers
{
	namespace settings
	{
		namespace keys
		{
			const QString monitor_number = "monitor_number";
			const QString detect_area_x = "detect_area_x";
			const QString detect_area_y = "detect_area_y";
			const QString detect_area_width = "detect_area_width";
			const QString detect_area_height = "detect_area_height";
			const QString mouse_click_x = "mouse_click_x";
			const QString mouse_click_y = "mouse_click_y";
		}

		namespace default_values
		{
			const int monitor_number = 0;
			const int detect_area_x = 375;
			const int detect_area_y = 160;
			const int detect_area_width = 550;
			const int detect_area_height = 950;
			const int mouse_click_x = 2500;
			const int mouse_click_y = 1100;
		}
	}
}

MainWidget::MainWidget(QWidget *parent)
    : QWidget(parent)
{
	ReadSettings();
    CreateUi();
}

void MainWidget::ReadSettings()
{
	using namespace helpers::settings;
	QSettings settings;
	monitor_number_ = settings.value(keys::monitor_number, default_values::monitor_number).toInt();
	detect_area_.x = settings.value(keys::detect_area_x, default_values::detect_area_x).toInt();
	detect_area_.y = settings.value(keys::detect_area_y, default_values::detect_area_y).toInt();
	detect_area_.width = settings.value(keys::detect_area_width, default_values::detect_area_width).toInt();
	detect_area_.height = settings.value(keys::detect_area_height, default_values::detect_area_height).toInt();
	mouse_click_point_.rx() = settings.value(keys::mouse_click_x, default_values::mouse_click_x).toInt();
	mouse_click_point_.ry() = settings.value(keys::mouse_click_y, default_values::mouse_click_y).toInt();
}

void MainWidget::SaveSettings()
{
	using namespace helpers::settings;
	QSettings settings;
	settings.setValue(keys::monitor_number, monitor_number_);
	settings.setValue(keys::detect_area_x, detect_area_.x);
	settings.setValue(keys::detect_area_y, detect_area_.y);
	settings.setValue(keys::detect_area_width, detect_area_.width);
	settings.setValue(keys::detect_area_height, detect_area_.height);
	settings.setValue(keys::mouse_click_x, mouse_click_point_.x());
	settings.setValue(keys::mouse_click_y, mouse_click_point_.y());
	settings.sync();
}

MainWidget::~MainWidget() = default;


void MainWidget::CreateUi()
{
	QVBoxLayout* main_lay = new QVBoxLayout;

	main_lay->addLayout(CreateMonitorControl());
	main_lay->addLayout(CreateGeometryParamControl());
	main_lay->addLayout(CreateClickControl());
	
	QPushButton* start_button = new QPushButton("start");
	main_lay->addWidget(start_button);
	bool connection = connect(start_button, &QPushButton::clicked, this, &MainWidget::OnStartButtonClicked); Q_ASSERT(connection);

	setLayout(main_lay);

}

QLayout* MainWidget::CreateMonitorControl()
{
	const QList<QScreen*> screen_list = QGuiApplication::screens();

	QLabel* lbl = new QLabel;
	lbl->setText(QString::fromUtf8("Монитор"));
	QSpinBox* monitor_number_spin = new QSpinBox;
	monitor_number_spin->setMinimum(0);
	monitor_number_spin->setMaximum(screen_list.size());
	monitor_number_spin->setValue(monitor_number_);
	QPushButton* test_image = new QPushButton;
	test_image->setText(QString::fromUtf8("Тест"));
	bool connection = connect(test_image, &QPushButton::clicked, this, &MainWidget::OnTestMonitorImageButtonClicked); Q_ASSERT(connection);

	QHBoxLayout* monitor_lay = new QHBoxLayout;
	monitor_lay->addWidget(lbl);
	monitor_lay->addWidget(monitor_number_spin);
	monitor_lay->addWidget(test_image);
	connection = connect(monitor_number_spin, qOverload<int>(&QSpinBox::valueChanged), this, &MainWidget::OnMonitorNumberChanged); Q_ASSERT(connection);
	return monitor_lay;
}

QLayout* MainWidget::CreateGeometryParamControl()
{
	QLabel* lbl_x = new QLabel(QString::fromUtf8("x"));
	QSpinBox* x_spin = new QSpinBox;
	x_spin->setMinimum(0);
	x_spin->setMaximum(99999);
	x_spin->setValue(detect_area_.x);
	bool connection = connect(x_spin, qOverload<int>(&QSpinBox::valueChanged), this, [&](int value) {detect_area_.x = value; });

	QLabel* lbl_y = new QLabel(QString::fromUtf8("y"));
	QSpinBox* y_spin = new QSpinBox;
	y_spin->setMinimum(0);
	y_spin->setMaximum(99999);
	y_spin->setValue(detect_area_.y);
	connection = connect(y_spin, qOverload<int>(&QSpinBox::valueChanged), this, [&](int value) {detect_area_.y = value; });

	QLabel* lbl_width = new QLabel(QString::fromUtf8("длина"));
	QSpinBox* width_spin = new QSpinBox;
	width_spin->setMinimum(0);
	width_spin->setMaximum(99999);
	width_spin->setValue(detect_area_.width);
	connection = connect(width_spin, qOverload<int>(&QSpinBox::valueChanged), this, [&](int value) {detect_area_.width = value; });

	QLabel* lbl_height = new QLabel(QString::fromUtf8("Ширина"));
	QSpinBox* height_spin = new QSpinBox;
	height_spin->setMinimum(0);
	height_spin->setMaximum(99999);
	height_spin->setValue(detect_area_.height);
	connection = connect(height_spin, qOverload<int>(&QSpinBox::valueChanged), this, [&](int value) {detect_area_.height = value; });

	QGridLayout* geometry_param_lay = new QGridLayout;
	geometry_param_lay->addWidget(lbl_x, 0, 0);
	geometry_param_lay->addWidget(x_spin, 0, 0);
	geometry_param_lay->addWidget(lbl_y, 1, 0);
	geometry_param_lay->addWidget(y_spin, 1, 0);
	geometry_param_lay->addWidget(lbl_width, 2, 0);
	geometry_param_lay->addWidget(width_spin, 2, 0);
	geometry_param_lay->addWidget(lbl_height, 3, 0);
	geometry_param_lay->addWidget(height_spin, 3, 0);
	return geometry_param_lay;
}


QLayout* MainWidget::CreateClickControl()
{
	bool connection = true;
	QLabel* mouse_pos_x_lbl = new QLabel(QString::fromUtf8("x для щелчка"));
	QSpinBox* mouse_pos_x_spin = new QSpinBox;
	mouse_pos_x_spin->setMinimum(0);
	mouse_pos_x_spin->setMaximum(9999);
	mouse_pos_x_spin->setValue(mouse_click_point_.x());
	connection = connect(mouse_pos_x_spin, qOverload<int>(&QSpinBox::valueChanged), this, [&](int value) {mouse_click_point_.rx() = value; }); Q_ASSERT(connection);

	QLabel* mouse_pos_y_lbl = new QLabel(QString::fromUtf8("y для щелчка"));
	QSpinBox* mouse_pos_y_spin = new QSpinBox;
	mouse_pos_y_spin->setMinimum(0);
	mouse_pos_y_spin->setMaximum(9999);
	mouse_pos_y_spin->setValue(mouse_click_point_.y());
	connection = connect(mouse_pos_y_spin, qOverload<int>(&QSpinBox::valueChanged), this, [&](int value) {mouse_click_point_.ry() = value; }); Q_ASSERT(connection);

	QGridLayout* grid_lay = new QGridLayout;
	grid_lay->addWidget(mouse_pos_x_lbl, 0, 0);
	grid_lay->addWidget(mouse_pos_x_spin, 0, 1);
	grid_lay->addWidget(mouse_pos_y_lbl, 1, 0);
	grid_lay->addWidget(mouse_pos_y_spin, 1, 1);

	QPushButton* test_click = new QPushButton(QString::fromUtf8("Переместить мышь"));
	grid_lay->addWidget(test_click, 2, 0, 1, 2);
	connection = connect(test_click, &QPushButton::clicked, this, [&]() {
		InputSimulator simulator;
		simulator.moveAndClick(mouse_click_point_, 0);
		});
	Q_ASSERT(connection);
	return grid_lay;
}

void MainWidget::OnStartButtonClicked()
{
    qDebug() << "device_info = " << finder_.getDeviceInfo();
    //Тут добавить проверку на то, что список устройств не пуст

    QImage target_image("://resources/input_pix.bmp");//тут загрузить изображение из ресурсов
    if (target_image.isNull())
    {
        qWarning() << QString::fromUtf8("Не удалось загрузить изображение");
        return;
    }

	QImage message_image("://resources/start_pix.bmp");//тут загрузить изображение из ресурсов
	if (message_image.isNull())
	{
		qWarning() << QString::fromUtf8("Не удалось загрузить изображение");
		return;
	}


    bool image_detected = false;
    QList<QScreen*> screen_list = QGuiApplication::screens();
    qDebug() << "screeens count = " << screen_list.size();
    QScreen* screen = screen_list[monitor_number_];

    int msec_duration = 0;
	QPoint plus_point;
    while(!image_detected)
    {
        QPixmap screenshot = screen->grabWindow(0);
        QImage source_image = screenshot.toImage();
        source_image = source_image.copy(0, source_image.height() - 200, 400, 200);

        QElapsedTimer timer;
        timer.start();
        QPoint found_pos = finder_.findFirstMatchMinimal(source_image, target_image, 0.95);
        if (found_pos.x() != -1)
        {
            image_detected = true;
            msec_duration = timer.elapsed();
			plus_point = found_pos;
        }
    }

    qDebug() << QString::fromUtf8("Plus image found. Duration : %1 msecs").arg(msec_duration);

    image_detected = false;
    while (!image_detected)
    {
		const QPixmap screenshot = screen->grabWindow(0);
		QImage source_image = screenshot.toImage();
		source_image = source_image.copy(detect_area_.x, detect_area_.y, detect_area_.width, detect_area_.height);

		qDebug() << "h = " << source_image.height() << ", w = " << source_image.width();

		QElapsedTimer timer;
		timer.start();
		QPoint found_pos = finder_.findFirstMatchMinimal(source_image, message_image, 0.95);
		if (found_pos.x() != -1)
		{
			image_detected = true;
			msec_duration = timer.elapsed();
		}
    }

    qDebug() << QString::fromUtf8("Image detected. Duration : %1 msecs").arg(msec_duration);

	ClickAndSendPlusSymbol(mouse_click_point_);
}


void MainWidget::OnMonitorNumberChanged(int monitor_number) //автоматизировать монитор. Сделать поиск окна приложения
{
    monitor_number_ = monitor_number;
}

void MainWidget::OnTestMonitorImageButtonClicked() // автоматизировать в дальнейшем
{
	QList<QScreen*> screen_list = QGuiApplication::screens();
	QScreen* screen = screen_list[monitor_number_];
	const QPixmap screenshot = screen->grabWindow(0);
	QImage source_image = screenshot.toImage();
	source_image = source_image.copy(detect_area_.x, detect_area_.y, detect_area_.width, detect_area_.height);

	QLabel* lbl = new QLabel;
	lbl->setPixmap(QPixmap::fromImage(source_image));
	lbl->setGeometry(10, 10, 100, 100);
	lbl->show();
}

void MainWidget::ClickAndSendPlusSymbol(const QPoint& point_to_click)
{
	InputSimulator simulator;
	simulator.executeFullSequence(point_to_click, 0);
}

void MainWidget::closeEvent(QCloseEvent* event)
{
	SaveSettings();
	QWidget::closeEvent(event);
}
