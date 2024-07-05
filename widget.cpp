#include "widget.h"
#include "./ui_widget.h"
#include<QHostAddress>
#include<QString>

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);
    setWindowTitle("Server");
    initServer();

}

Widget::~Widget()
{
    closeServer();
    delete ui;
}
void Widget::initServer()
{
    //創建server對象
    server = new QTcpServer(this);

    //點擊監聽按鈕，開始監聽
    connect(ui->puB_Listen,&QPushButton::clicked,[this]{
        //判斷是否已開啟，是->close，否->listen
        if(server->isListening()){
            closeServer();
            //關閉server後恢復介面狀態
            ui->puB_Listen->setText("Listen");
            ui->AddressEdit->setEnabled(true);
            ui->PortEdit->setEnabled(true);
        }else{

            //從介面上讀取IP和端口
            const QString address_text = ui->AddressEdit->text();
            const QHostAddress address = (address_text == "Any")
                ?QHostAddress::Any
                :QHostAddress(address_text);
            const unsigned short port=ui->PortEdit->text().toShort();

            //開始監聽
            if(server->listen(address,port)){
                //連接成功->不可再編輯port、Address
                ui->puB_Listen->setText("close");
                ui->AddressEdit->setEnabled(false);
                ui->PortEdit->setEnabled(false);
            }

        }
        updateState();
    });

    //監聽到新的客戶端連線要求
    connect(server, &QTcpServer::newConnection,this,[this]{
        while(server->hasPendingConnections()){
            QTcpSocket *socket = server->nextPendingConnection();
            clientList.append(socket);
            ui->receive_text->append(QString("[%1:%2] Socket connected")
                                         .arg(socket->peerAddress().toString())
                                         .arg(socket->peerPort()));
            //收到數據 觸發readyread
            connect(socket, &QTcpSocket::readyRead,[this,socket]{
                if(socket->bytesAvailable()<=0){
                    return;
                }
                const QString recv_text = QString::fromUtf8(socket->readAll());
                ui->receive_text->append(QString("[%1:%2]")
                                             .arg(socket->peerAddress().toString())
                                           .arg(socket->peerPort()));
                ui->receive_text->append(recv_text);

                //below for test, delete after test
                qDebug()<<"|SERVER|"<<recv_text;
                QString sendText;
                if(recv_text.contains("RD R215")){
                    sendText = QString("%1").arg("0");
                }else if(recv_text.contains("WR")){
                    sendText = QString("%1").arg("OK");
                }else if(recv_text.contains("RD")){
                    sendText = QString("%1").arg("1");
                }else{
                    qDebug()<<"I don't get it";
                }
                //將發送區文本發送至客戶端
                //數據為空就返回
                QByteArray sent_data = sendText.toUtf8();
                if(sent_data.isEmpty()){
                    return;
                }
                for(QTcpSocket *socket:qAsConst(clientList)){
                    socket->write(sent_data);
                }
            });
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
            connect(socket, static_cast<void(QAbstractSocket::*)(QAbstractSocket::SocketError)>(&QAbstractSocket::error),
                    [this, socket](QAbstractSocket::SocketError){
                ui->receive_text->append(QString("[%1:%2] socket Error:%3")
                                             .arg(socket->peerAddress().toString())
                                             .arg(socket->peerPort())
                                             .arg(socket->errorString()));
            });
#else
        connect(socket,&QAbstractSocket::errorOccurred,[this, socket](QAbstractSocket::SocketError){
                ui->receive_text->append(QString("[%1:%2] socket Error:%3")
                                             .arg(socket->peerAddress().toString())
                                             .arg(socket->peerPort())
                                             .arg(socket->errorString()));
            });
#endif
            connect(socket,&QTcpSocket::disconnected,[this,socket]{
                socket->deleteLater();
                clientList.removeOne(socket);
                ui->receive_text->append(QString("[%1:%2] socket Disconnected")
                                             .arg(socket->peerAddress().toString())
                                             .arg(socket->peerPort()));
                updateState();

            });
        }
        updateState();
    });
    //server向client發送內容
    connect(ui->puB_Sent,&QPushButton::clicked, [this]{
        //判斷是否開啟server
        if(!server->isListening()){
            closeServer();
            return;
        }


        //將發送區文本發送至客戶端
        const QByteArray sent_data = ui->sent_text->toPlainText().toUtf8();
        //數據為空就返回
        if(sent_data.isEmpty()){
            return;
        }
        for(QTcpSocket *socket:qAsConst(clientList)){

            socket->write(sent_data);
        }
    });

    connect(server, &QTcpServer::acceptError, [this](QAbstractSocket::SocketError){
        ui->receive_text->append("Server Error:"+server->errorString());

    });
}

void Widget::closeServer()
{
    server->close();
    for(QTcpSocket * socket:qAsConst(clientList)){

        socket->disconnectFromHost();
        if(socket->state()!=QAbstractSocket::UnconnectedState){
            socket->abort();
        }
    }
}
void Widget::updateState()
{
    if(server->isListening()){
        setWindowTitle(QString("Server[%1:%2] connections:%3")
                           .arg(server->serverAddress().toString())
                           .arg(server->serverPort())
                           .arg(clientList.count()));

    }else{
        ui->receive_text->clear();
        ui->sent_text->clear();
        setWindowTitle("Server");
    }
}



