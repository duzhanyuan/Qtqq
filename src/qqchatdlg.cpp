#include "qqchatdlg.h"

#include <QFileDialog>
#include <QKeyEvent>
#include <QApplication>
#include <QMenu>
#include <QAction>
#include <QPointer>

#include "core/soundplayer.h"
#include "core/imgloader.h"
#include "core/imgsender.h"
#include "core/captchainfo.h"
#include "qqchatlogwin.h"
#include "core/qqchatlog.h"
#include "core/groupchatlog.h"

QQChatDlg::QQChatDlg(QString id, QString name, FriendInfo curr_user_info, 
                     QWidget *parent) :
    QQWidget(parent),
    id_(id),
    msg_id_(4462000),
    name_(name),
    curr_user_info_(curr_user_info),
    img_loader_(NULL),
    img_sender_(NULL),
    qqface_panel_(NULL),
    msg_sender_(NULL)
{
    setObjectName("chatWindow");
    qRegisterMetaType<FileInfo>("FileInfo");

    setFontStyle(QFont(), Qt::black, 9);

    te_messages_.setReadOnly(true);
    te_input_.setMinimumHeight(70);

    send_type_menu_ = new QMenu(this);
    act_return_ = new QAction(tr("send by return"), send_type_menu_);   
    act_return_->setCheckable(true);
    act_ctrl_return_ = new QAction(tr("send by ctrl+return"), send_type_menu_);
    act_ctrl_return_->setCheckable(true);

    connect(act_return_, SIGNAL(triggered(bool)), this, SLOT(setSendByReturn(bool)));
    connect(act_ctrl_return_, SIGNAL(triggered(bool)), this, SLOT(setSendByCtrlReturn(bool)));

    send_type_menu_->addAction(act_return_);
    send_type_menu_->addAction(act_ctrl_return_);

    QSettings setting("options.ini", QSettings::IniFormat);
    send_by_return_ = setting.value("send_by_return").toBool();

    act_return_->setChecked(send_by_return_);
    act_ctrl_return_->setChecked(!send_by_return_);

    te_input_.installEventFilter(this);
}

QQChatDlg::~QQChatDlg()
{
    if (img_sender_)
    {
        img_sender_->quit(); 
        delete img_sender_;
    }
    img_sender_ = NULL;

    if (img_loader_)
    {
        img_loader_->terminate();
        img_loader_->deleteLater();
    }
    img_loader_ = NULL;

    if (qqface_panel_)
        delete qqface_panel_;
    qqface_panel_ = NULL;

    if (msg_sender_)
    {
        msg_sender_->terminate();
        delete msg_sender_;
    }
    msg_sender_ = NULL;
}

void QQChatDlg::setSendByReturn(bool checked)
{
    Q_UNUSED(checked)
    if (!send_by_return_)
    {
        QSettings setting("options.ini", QSettings::IniFormat);
        setting.setValue("send_by_return", true);
        send_by_return_ = true;

        act_ctrl_return_->setChecked(false);
    }
}

void QQChatDlg::setSendByCtrlReturn(bool checked)
{
    Q_UNUSED(checked)
    if (send_by_return_)
    {
        QSettings setting("options.ini", QSettings::IniFormat);
        setting.setValue("send_by_return", false);
        send_by_return_ = false;
        act_return_->setChecked(false);
    }
}

void QQChatDlg::closeEvent(QCloseEvent *)
{
    emit chatFinish(this);
}

bool QQChatDlg::eventFilter(QObject *obj, QEvent *e)
{
    if (obj != &te_input_ || e->type() != QEvent::KeyPress)
        return QQWidget::eventFilter(obj, e);

    QKeyEvent *key_event = static_cast<QKeyEvent *>(e);

    switch (key_event->key())
    {
    case Qt::Key_Return:
        if (key_event->modifiers()& Qt::ControlModifier)
        {
            if (send_by_return_)
            {
               te_input_.insertPlainText("\n");
            }
            else
                sendMsg();
            return true;
        }
        else
        {
            if (send_by_return_)
            {
                sendMsg();
                return true;
            }
        }
        break;
    case Qt::Key_C:
        if (key_event->modifiers()& Qt::AltModifier)
        {
            close();
        }
        break;
    }
    return false;
}

ImgLoader *QQChatDlg::getImgLoader() const
{
    return new ImgLoader();
}

QQChatLog *QQChatDlg::getChatlog() const
{
    return NULL;
}

void QQChatDlg::showQQFace(QString face_id)
{
    QImage img("images/qqface/default/"+face_id+".gif");
    QTextDocument *doc = te_messages_.document();
    QTextCursor cursor(doc);
    cursor.movePosition(QTextCursor::End);
    cursor.insertImage(img);
}

void QQChatDlg::openPathDialog(bool)
{
    QString file_path = QFileDialog::getOpenFileName(this, tr("select the image to send"), QString(), tr("Image Files(*.png *.jpg *.bmp *.gif)"));

    if (file_path.isEmpty())
        return;

    if (!img_sender_)
    {
        img_sender_ = getImgSender();
        connect(img_sender_, SIGNAL(postResult(QString,FileInfo)), this, SLOT(setFileInfo(QString, FileInfo)));
        connect(img_sender_, SIGNAL(sendDone(const QString &, const QString&)), &te_input_, SLOT(setRealImg(const QString&,const QString&)));   
    }

    QString unique_id = getUniqueId();
    img_sender_->send(unique_id, file_path, curr_user_info_.id());
    te_input_.insertImgProxy(unique_id);
}

void QQChatDlg::setFileInfo(QString unique_id, FileInfo file_info)
{
    id_file_hash_.insert(unique_id, file_info);
}

void QQChatDlg::setFontStyle(QFont font, QColor color, int size)
{
    QTextCursor cursor(te_input_.document());
    cursor.movePosition(QTextCursor::End);

    QTextBlockFormat block_format;
    block_format.setLeftMargin(8);
    block_format.setLineHeight(5, QTextBlockFormat::LineDistanceHeight);

    QTextCharFormat char_format;
    char_format.setForeground(color);
    char_format.setFont(font);
    char_format.setFontPointSize(size);

    cursor.setBlockFormat(block_format);
    cursor.setBlockCharFormat(char_format);
}

void QQChatDlg::showMsg(ShareQQMsgPtr msg)
{
    const QQChatMsg *chat_msg = static_cast<const QQChatMsg*>(msg.data());
    qint64 time = chat_msg->time();

    QDateTime date_time;
    date_time.setMSecsSinceEpoch(time * 1000);
    QString time_str = date_time.toString("dd ap hh:mm:ss");

    te_messages_.insertNameLine(convertor_.convert(chat_msg->sendUin()) + " " + time_str, Qt::blue);

    for (int i = 0; i < chat_msg->msg_.size(); ++i)
    {
        if (chat_msg->msg_[i].type() == QQChatItem::kQQFace)
        {
            te_messages_.insertQQFace(chat_msg->msg_[i].content());
        }
        else if (chat_msg->msg_[i].type() == QQChatItem::kWord)
            te_messages_.insertWord(chat_msg->msg_[i].content(), QFont(), Qt::black, 9);
        else
        {
            if (!img_loader_)
            {
                img_loader_ = getImgLoader();
                connect(img_loader_, SIGNAL(loadDone(const QString&, const QString&)), &te_messages_, SLOT(setRealImg(const QString&, const QString&)));
            }

            if (te_messages_.containsImg(chat_msg->msg_[i].content()))
            {
                te_messages_.insertExistImg(chat_msg->msg_[i].content());
            }
            else
            {
                if (chat_msg->msg_[i].type() == QQChatItem::kGroupChatImg)
                {
                    const QQGroupChatMsg *chat_msg = static_cast<const QQGroupChatMsg*>(msg.data());
                    img_loader_->loadGroupChatImg(chat_msg->msg_[i].content(), chat_msg->sendUin(), chat_msg->info_seq_,
                                                  chat_msg->msg_[i].file_id(), chat_msg->msg_[i].server_ip(),
                                                  chat_msg->msg_[i].server_port(), QString::number(chat_msg->time_));
                }
                else if (chat_msg->msg_[i].type() == QQChatItem::kFriendCface)
                    img_loader_->loadFriendCface(chat_msg->msg_[i].content(), id_, chat_msg->msg_id_);
                else
                    img_loader_->loadFriendOffpic(chat_msg->msg_[i].content(), id_);

                te_messages_.insertImgProxy(chat_msg->msg_[i].content());
            }
        }
    }

    if (this->isMinimized())
    {
        SoundPlayer::singleton()->play(SoundPlayer::kMsg);
        QApplication::alert(this, 3000);
    }
}

void QQChatDlg::sendMsg()
{
    if (te_input_.document()->isEmpty())
    {
        te_input_.setToolTip(tr("the message can not be empty,please input the message..."));
        te_input_.showToolTip();
        return;
    }

    if (img_sender_ && img_sender_->isSendding())
    {
        te_input_.setToolTip(tr("image uploading,please wait..."));
        te_input_.showToolTip();
        return;
    }

    QString msg = te_input_.toHtml();
    QString json_msg = converToJson(msg);

    Request req;
    req.create(kPost, send_url_);
    req.addHeaderItem("Host", "d.web2.qq.com");
    req.addHeaderItem("Cookie", CaptchaInfo::singleton()->cookie());
    req.addHeaderItem("Referer", "http://d.web2.qq.com/proxy.html?v=20110331002");
    req.addHeaderItem("Content-Length", QString::number(json_msg.length()));
    req.addHeaderItem("Content-Type", "application/x-www-form-urlencoded");
    req.addRequestContent(json_msg.toAscii());

    if (!msg_sender_)
    {
        msg_sender_ = new QQMsgSender();
    }
    
    msg_sender_->send(req);

    //���te_input,���ӵ�te_messages��
    QTextDocument *inp_doc = te_input_.document();

    for (int i = 0; i < te_input_.resourceIds().size(); ++i)
    {
        QString file_id = te_input_.resourceIds().at(i);
        te_messages_.addAnimaImg(file_id, inp_doc->resource(QTextDocument::ImageResource, QUrl(file_id)), te_input_.getMovieById(file_id));
    }

    te_messages_.insertNameLine(curr_user_info_.name() + " " + QDateTime::currentDateTime().toString("dd ap hh:mm:ss"), Qt::darkGreen);
    te_messages_.appendDocument(te_input_.document());

    te_input_.clearAll();
    te_input_.setFocus();
    setFontStyle(QFont(), Qt::black, 9);
}

void QQChatDlg::openQQFacePanel()
{
    if (!qqface_panel_)
    {
        qqface_panel_ = new QQFacePanel();
        connect(qqface_panel_, SIGNAL(qqfaceClicked(QString)), &te_input_, SLOT(insertQQFace(QString)));
    }

    //�ƶ�QQ�������λ��
    QPoint face_btn_pos = QCursor::pos();
    QRect qqface_panel_geometry = qqface_panel_->frameGeometry();
    int new_x = face_btn_pos.x() - qqface_panel_geometry.width() / 2;
    int new_y = face_btn_pos.y() - qqface_panel_geometry.height() + 5;
    qqface_panel_->setGeometry(new_x, new_y, qqface_panel_geometry.width(), qqface_panel_geometry.height());
    qqface_panel_->show();
}

void QQChatDlg::openChatLogWin()
{
    QQChatLog *chatlog = getChatlog();

    QPointer<QQChatLogWin> chatlog_win = new QQChatLogWin();
    chatlog_win->setChatLog(chatlog);
    chatlog_win->setNameConvertor(&convertor_);
    chatlog_win->getFirstPage();
    chatlog_win->show();
}

void QQChatDlg::showOldMsg(QVector<ShareQQMsgPtr> msgs)
{
    ShareQQMsgPtr msg;
    foreach(msg, msgs)
    {
        showMsg(msg);

        if (type() == kGroup)
        {
            unconvert_ids_.append(msg->sendUin());
        }
    }

    this->show();
}
