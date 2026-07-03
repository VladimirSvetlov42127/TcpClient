#include "tcp_client.h"


//	Подключение библиотек QT
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QDataStream>

//  Подключение библиотекк QT
#include <QThread>
#include <QFuture>
#include <QtConcurrent>



//===================================================================================================================================================
// Конструктор и деструктор класса
//===================================================================================================================================================
TcpClient::TcpClient(QWidget* parent) : QMainWindow(parent)
{
	//	Создание и привязка сокета
	_next_size = 0;
	_socket = new QTcpSocket(this);
	connect(_socket, &QTcpSocket::readyRead, this, &TcpClient::onReadyRead);
	connect(_socket, &QTcpSocket::destroyed, _socket, &QTcpSocket::deleteLater);
	connect(_socket, &QTcpSocket::errorOccurred, this, &TcpClient::onSocketError);

	//	Установка цветовой палитры
	QPalette palette = qApp->palette();
	QColor color = palette.color(QPalette::Highlight);
	color.setAlpha(80);
	palette.setColor(QPalette::Highlight, color);
	qApp->setPalette(palette);

	//	Настройка главного окна
	setWindowIcon(QIcon("icon1.png"));
	setWindowFlags(this->windowFlags() & ~Qt::WindowContextHelpButtonHint);
	setWindowModality(Qt::WindowModal);
	QApplication::setWindowIcon(windowIcon());
	setWindowTitle("Tcp Client");

	//	Вывод формы на экран
	makeWindow();
}

TcpClient::~TcpClient()
{
	onDisconnect();
	_socket->deleteLater();
}


//===================================================================================================================================================
//	Методы обработки сигналов
//===================================================================================================================================================
void TcpClient::onReadyRead()
{
	//if (!_socket) return;
	//if (_socket->state() != QAbstractSocket::ConnectedState)
	//	return;

	// QDataStream ios(_socket);
	// ios.setVersion(QDataStream::Qt_5_15);
	// QByteArray data;
	// qint64 bytes = _socket->bytesAvailable();
	// qDebug() << "Bytes available: " << bytes;	

	//ios.startTransaction();
	//ios >> data;
	//if (data.isNull()) {
	//    ios.rollbackTransaction();
	//    return;
	//}
	//ios.commitTransaction();
	////_receive_text->setText(_receive_text->toPlainText() + QString(data));
	////textUpdate(data);
	//qDebug() << "Received data: " << data.size();
	//QMetaObject::invokeMethod(this, [this, data]() {
 //   	_receive_text->setText(_receive_text->toPlainText() + QString(data));
	//}, Qt::QueuedConnection);


		// Связываем стрим с сокетом для безопасного чтения заголовка размера
	QDataStream in(_socket);
	in.setVersion(QDataStream::Qt_5_15);

	// 1. Читаем заголовок размера (выполняется 1 раз для каждого большого пакета)
	if (m_incomingTotalSize == 0) {
		// Если пришло меньше 4 байт, размер узнать нельзя. Ждем досылки.
		if (_socket->bytesAvailable() < sizeof(quint32)) {
			return;
		}

		// Считываем чистый размер будущего QByteArray
		in >> m_incomingTotalSize;

		// Важнейшая оптимизация: заранее выделяем память под 100 МБ,
		// чтобы избежать зависаний из-за переаллокаций при склейке массива
		m_receivedBuffer.reserve(m_incomingTotalSize);
	}

	// 2. Считываем все доступные в данный момент байты из сети
	if (_socket->bytesAvailable() > 0) {
		// readAll() забирает всё, что физически долетело в буфер ОС на эту секунду
		m_receivedBuffer.append(_socket->readAll());
	}

	// 3. Проверяем, собрали ли мы массив полностью
	if ((quint32)m_receivedBuffer.size() >= m_incomingTotalSize) {

		// ДАННЫЕ УСПЕШНО СОБРАНЫ И СКЛЕЕНЫ В ОДИН МАССИВ!
		// Передаем готовый QByteArray в вашу функцию обработки на клиенте
		_receive_text->setText(QString(m_receivedBuffer));

		// СБРОС СОСТОЯНИЯ: Очищаем триггеры для приема следующего пакета от сервера
		m_incomingTotalSize = 0;
		m_receivedBuffer.clear();

		// Защита от "слипания" пакетов: если сервер успел отправить что-то еще,
		// рекурсивно вызываем эту же функцию, чтобы обработать остаток байт
		if (_socket->bytesAvailable() > 0) {
			onReadyRead();
		}
	}

}

void TcpClient::onConnect()
{
	_port = _port_edit->text().toUShort();
	_ip_address = _ip_edit->text();
	if (_socket->state() == QAbstractSocket::UnconnectedState)
		_socket->connectToHost(_ip_address, _port);
	if (_socket->waitForConnected(3000)) {
		_connect_button->setEnabled(false);
		_disconnect_button->setEnabled(true);
		_send_button->setEnabled(true);
	}
}

void TcpClient::onDisconnect()
{
	if (_socket->state() != QAbstractSocket::ConnectedState)
		return;
	_socket->disconnectFromHost();
	if (_socket->state() == QAbstractSocket::UnconnectedState) {
		_connect_button->setEnabled(true);
		_disconnect_button->setEnabled(false);
		_send_button->setEnabled(false);
	}
}

void TcpClient::onSend()
{
	QByteArray data = _send_text->toPlainText().toUtf8();
	if (data.isEmpty())
		return;
	QDataStream ios(_socket);
	ios.setVersion(QDataStream::Qt_5_15);
	ios << data;
	_send_text->clear();
	_receive_text->clear();
}

void TcpClient::onSocketError()
{
	QMessageBox msgBox(this);
	msgBox.setText("Socket Error: " + _socket->errorString());
	msgBox.exec();
	onDisconnect();
}


//===================================================================================================================================================
//  Вспомогательные методы класса
//===================================================================================================================================================
void TcpClient::makeWindow()
{
	QWidget* central = new QWidget(this);
	QVBoxLayout* layout = new QVBoxLayout(central);

	_port = 3025;
	_ip_address = "127.0.0.1";

	//	Формирование верхнего виджета
	QHBoxLayout* top_layout = new QHBoxLayout;
	QLabel* ip_label = new QLabel("IP address:");
	QLabel* port_label = new QLabel("Port:");
	_port_edit = new QLineEdit(QString::number(_port));
	_port_edit->setFixedWidth(35);
	_ip_edit = new QLineEdit(_ip_address);
	_ip_edit->setFixedWidth(65);
	_connect_button = new QPushButton("Connect");
	_connect_button->setDefault(true);
	_disconnect_button = new QPushButton("Disconnect");
	_disconnect_button->setEnabled(false);
	_connect_button->setFixedWidth(85);
	_disconnect_button->setFixedWidth(85);
	connect(_connect_button, &QPushButton::clicked, this, &TcpClient::onConnect);
	connect(_disconnect_button, &QPushButton::clicked, this, &TcpClient::onDisconnect);

	//	Добавление верхнего виджета на макет
	top_layout->addWidget(ip_label);
	top_layout->addWidget(_ip_edit);
	top_layout->addWidget(port_label);
	top_layout->addWidget(_port_edit);
	top_layout->addStretch(1);
	top_layout->addWidget(_connect_button);
	top_layout->addWidget(_disconnect_button);
	layout->addLayout(top_layout);

	//	Добавление остальных виджетов
	_send_text = new QTextEdit;
	layout->addWidget(_send_text);
	QHBoxLayout* button_layout = new QHBoxLayout;
	_send_button = new QPushButton("Send");
	_send_button->setEnabled(false);
	_send_button->setFixedWidth(85);
	connect(_send_button, &QPushButton::clicked, this, &TcpClient::onSend);

	button_layout->addStretch();
	button_layout->addWidget(_send_button);
	layout->addLayout(button_layout);

	_receive_text = new QTextEdit;
	layout->addWidget(_receive_text);

	setCentralWidget(central);
}


void TcpClient::textUpdate(const QByteArray& data)
{
	_receive_text->setText(_receive_text->toPlainText() + QString(data));
}

