#include <QWidget>

class MainWidget final
	: public QWidget
{
	Q_OBJECT
	
public:
	MainWidget(QWidget* parent = nullptr);
	~MainWidget() override = default;
};