#ifndef __TCP_CLIENT_H__
#define __TCP_CLIENT_H__


//	Подключение библиотек QT
#include <QMainWindow>
#include <QWidget>
#include <QObject>
#include <QTcpSocket>
#include <QString>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QByteArray>


//===================================================================================================================================================
//  Основной класс программы
//===================================================================================================================================================
class TcpClient : public QMainWindow
{
    Q_OBJECT

public:
    TcpClient(QWidget* parent = nullptr);
    ~TcpClient();

    //	Правило пяти
    TcpClient(const TcpClient& other) = delete;
    TcpClient(TcpClient&& other) = delete;
    TcpClient& operator=(const TcpClient& other) = delete;
    TcpClient& operator=(TcpClient&& other) = delete;

private slots:
    void onReadyRead();
    void onConnect();
    void onDisconnect();
    void onSend();
    void onSocketError();

private:
    //  Вспомогательные методы класса
    void makeWindow();
    void textUpdate(const QByteArray& data);
     
    //  Свойства класса
    QLineEdit* _ip_edit;
    QLineEdit* _port_edit;
    QPushButton* _connect_button;
    QPushButton* _disconnect_button;
    QPushButton* _send_button;
    QTextEdit* _send_text;
    QTextEdit* _receive_text;

    uint16_t _port;
    QString _ip_address;
    QTcpSocket* _socket;
    quint16 _next_size;
	QByteArray _total_data;

    quint32 _total_size = 0;            // Ожидаемый размер массива от сервера
    QByteArray _buffer;                 // Буфер для сборки 100 МБ
};

#endif __TCP_CLIENT_H__