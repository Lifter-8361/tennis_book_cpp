#include "mainwidget.h"

#include <QDebug>

MainWidget::MainWidget(QWidget* parent)
	: QWidget(parent)
{
}

void MainWidget::StartServer()
{
	sp_server_.reset(new QTcpServer());

	const int port_number = 62022;
	bool connection = true;
	connection = connect(sp_server_.data(), &QTcpServer::newConnection, this, &MainWidget::OnUserConnected); Q_ASSERT(connection);
	if (!sp_server_->isListening())
	{
		if (!sp_server_->listen(QHostAddress::Any, 62022))
		{
			qDebug() << "Unable to start the server with port " << 62022;
		}
	}
	else
	{
		qDebug() << "Server already listen";
	}
}

void MainWidget::OnUserConnected()
{
	qDebug() << "New user connected!";
	QTcpSocket* client_socket = sp_server_->nextPendingConnection();
	const int id_user_socs = client_socket->socketDescriptor();
	clients_map_[id_user_socs] = client_socket;
	bool connection = true;
	connection = connect(client_socket, &QTcpSocket::readyRead, this, &MainWidget::OnReadyRead); Q_ASSERT(connection);
}

void MainWidget::OnReadyRead()
{
	QTcpSocket* client_socket = qobject_cast<QTcpSocket*>(sender());
	if (!client_socket)
	{
		return;
	}

	const int id_user_socs = client_socket->socketDescriptor();
	const QByteArray data_from_client = client_socket->readAll();
	//to do...
}