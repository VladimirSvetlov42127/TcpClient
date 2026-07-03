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

//	Подключение модулей проекта
#include "client_namespace.h"



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
	//	Связь стрима с сокетом для безопасного чтения заголовка размера
	QDataStream in(_socket);
	in.setVersion(STREAM_VERSION);

	//	Чтение заголовка размера (выполняется 1 раз для каждого большого пакета)
	if (_total_size == 0) {
		//	Если пришло меньше 4 байт, размер узнать нельзя. Ожидание досылки
		if (_socket->bytesAvailable() < sizeof(quint32))
			return;

		// Чтение чистого размера будущего QByteArray
		in >> _total_size;

		//	Выделение памяти, чтобы избежать зависаний из-за переаллокаций при склейке массива
		_buffer.reserve(_total_size);
	}

	//	Чтение всех доступных в данный момент байт
	if (_socket->bytesAvailable() > 0)
		_buffer.append(_socket->readAll());

	//	Проверка, собран ли массив полностью
	if ((quint32)_buffer.size() >= _total_size) {

		// ДАННЫЕ УСПЕШНО СОБРАНЫ И СКЛЕЕНЫ В ОДИН МАССИВ!
		_receive_text->setText(QString(_buffer));

		// СБРОС СОСТОЯНИЯ: Очистка триггеров для приема следующего пакета от сервера
		_total_size = 0;
		_buffer.clear();

		// Защита от "слипания" пакетов: если сервер успел отправить что-то еще,
		// рекурсивно вызываем эту же функцию, чтобы обработать остаток байт
		if (_socket->bytesAvailable() > 0)
			onReadyRead();
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

	//	Проверка того, что данные не пустые и сокет подключен
	if (data.isEmpty() || !_socket || _socket->state() != QAbstractSocket::ConnectedState)
		return;

	//	Отправка заголовка с размером данных (4 байта)
	QByteArray header;
	QDataStream header_stream(&header, QIODevice::WriteOnly);
	header_stream.setVersion(STREAM_VERSION);
	
	// Запись точного размера массива
	header_stream << quint32(data.size()); 
	_socket->write(header);
	if (!_socket->waitForBytesWritten(WAIT_WRITTEN))
		return;

	//	Почанковая отправка массива данных
	qint64 written_total = 0;
	qint64 total_size = data.size();

	while (written_total < total_size) {
		qint64 bytes_left = total_size - written_total;
		qint64 bytes_send = qMin(bytes_left, CHUNK_SIZE);

		//	Указатель на данные БЕЗ копирования и создания временных QByteArray
		const char* dataPtr = data.constData() + written_total;
		qint64 written = _socket->write(dataPtr, bytes_send);
		if (written == -1)
			return;

		//	Блокировка потока и ожидание, пока операционная система реально отправит этот кусок в сеть
		if (!_socket->waitForBytesWritten(WAIT_WRITTEN))
			return;
		written_total += written;
	}

	//	Принудительная очистка буфера сокета на всякий случай
	_socket->flush();
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

