#include <QApplication>
#include "tcp_client.h"


int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    TcpClient client;
    client.show();


    return app.exec();
}
