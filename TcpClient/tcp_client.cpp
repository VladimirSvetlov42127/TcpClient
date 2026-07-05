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
TcpClient::TcpClient(QWidget* parent) :
    QMainWindow(parent),
    _total_size(-1),
    _bytes(0)
{
	//	Создание и привязка сокета
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
	_send_text->clear();

    QDataStream in(_socket);
    in.setVersion(STREAM_VERSION);

    //  Получение общего размера массива (если еще не получили)
    if (_total_size == -1) {
         // Ожидание, пока придут первые 8 байт размера
        if (_socket->bytesAvailable() < (qint64)sizeof(qint64)) {
            return;
        }
        in >> _total_size;
        //  Выделение памяти заранее (оптимизация)
        _buffer.reserve(_total_size);
    }

    //  Чтение доступных в сокете данных чанками
    qint64 new_bytes = qMin(_socket->bytesAvailable(), _total_size - _bytes);
    if (new_bytes > 0) {
        QByteArray chunk = _socket->read(new_bytes);
        _buffer.append(chunk);
        _bytes += chunk.size();
    }

    //  Проверка, собран ли массив полностью
    if (_bytes == _total_size) {
        //  МАССИВ ПОЛНОСТЬЮ ПРИНЯТ. Обработка данных
        _receive_text->setText(QString(_buffer));

        //  Сброс состояние для приема следующего массива
        _buffer.clear();
        _total_size = -1;
        _bytes = 0;

        // Если в сокете осталось что-то от следующего пакета, рекурсивная обработка
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

    QDataStream out(_socket);
    out.setVersion(STREAM_VERSION);

    //  Отправка общего размера всего массива
    qint64 total_size = data.size();
    out << total_size;

    //  Отправка данные частями
    qint64 offset = 0;

    while (offset < total_size) {
        qint64 current_chunk_size = qMin(CHUNK_SIZE, total_size - offset);
        _socket->write(data.constData() + offset, current_chunk_size);
        _socket->flush();
        offset += current_chunk_size;
    }
    _send_text->clear();
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

