#include <QWidget>

#include <QTcpServer>
#include <QTcpSocket>

class MainWidget final
	: public QWidget
{
	Q_OBJECT
	
public:
	MainWidget(QWidget* parent = nullptr);
	~MainWidget() override = default;

	void StartServer();

private Q_SLOTS:

	void OnUserConnected();

	void OnReadyRead();

private:
	QScopedPointer<QTcpServer> sp_server_;
	QMap<int, QTcpSocket*> clients_map_;
};